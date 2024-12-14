/* bzflag
 * Copyright (c) 1993-2023 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 *
 */

// interface header
#include "RobotPlayer.h"

// common implementation headers
#include "BZDBCache.h"

// local implementation headers
#include "World.h"
#include "Intersect.h"
#include "TargetingUtils.h"

std::vector<BzfRegion*>* RobotPlayer::obstacleList = NULL;

RobotPlayer::RobotPlayer(const PlayerId& _id, const char* _name,
                         ServerLink* _server,
                         const char* _motto = "") :
    LocalPlayer(_id, _name, _motto),
    target(NULL),
    pathIndex(0),
    timerForShot(0.0f),
    drivingForward(true)
{
    gettingSound = false;
    server       = _server;
}

// estimate a player's position at now+t, similar to dead reckoning
void RobotPlayer::projectPosition(const Player *targ, const float t, glm::vec3 &tPos) const
{
    auto his  = targ->getPosition();
    auto hisv = targ->getVelocity();
    double omega=fabs(targ->getAngularVelocity());

    tPos = his;
    if ((targ->getStatus() & PlayerState::Falling) || omega < 2*M_PI / 360 * 0.5)
        tPos += hisv * t;
    else
    {
        double hisspeed = glm::length(glm::vec2(hisv));
        double alfa = omega * t;
        double r = hisspeed / fabs(omega);
        double dx = r * sin(alfa);
        double dy2 = r * (1 - cos(alfa));
        double beta = atan2(dy2, dx) * (targ->getAngularVelocity() > 0 ? 1 : -1);
        double gamma = atan2(hisv.y, hisv.x);
        double rho = gamma+beta;
        tPos.x += hisspeed * t * cos(rho);
        tPos.y += hisspeed * t * sin(rho);
        tPos.z += hisv.z * t;
    }
    if (targ->getStatus() & PlayerState::Falling)
        tPos.z += 0.5f * BZDBCache::gravity * t * t;
    if (tPos.z < 0) tPos.z = 0;
}


// get coordinates to aim at when shooting a player; steps:
// 1. estimate how long it will take shot to hit target
// 2. calc position of target at that point of time
// 3. jump to 1., using projected position, loop until result is stable
void RobotPlayer::getProjectedPosition(const Player *targ, glm::vec3 &projpos) const
{
    const auto my  = glm::vec2(getPosition());
    const auto his = glm::vec2(targ->getPosition());
    double distance = glm::distance(his, my) - BZDB.eval(StateDatabase::BZDB_MUZZLEFRONT) - BZDBCache::tankRadius;
    if (distance <= 0) distance = 0;
    const auto myVel = glm::vec2(getVelocity());
    double shotspeed = BZDB.eval(StateDatabase::BZDB_SHOTSPEED)*
                       (getFlag() == Flags::Laser ? BZDB.eval(StateDatabase::BZDB_LASERADVEL) :
                        getFlag() == Flags::RapidFire ? BZDB.eval(StateDatabase::BZDB_RFIREADVEL) :
                        getFlag() == Flags::MachineGun ? BZDB.eval(StateDatabase::BZDB_MGUNADVEL) : 1) +
                       glm::length(myVel);

    double errdistance = 1.0;
    glm::vec3 tPos;
    for (int tries=0 ; errdistance > 0.05 && tries < 4 ; tries++)
    {
        float t = (float)distance / (float)shotspeed;
        projectPosition(targ, t + 0.05f, tPos); // add 50ms for lag
        double distance2 = glm::distance(glm::vec2(tPos), my);
        errdistance = fabs(distance2-distance) / (distance + ZERO_TOLERANCE);
        distance = distance2;
    }
    projpos = tPos;

    // projected pos in building -> use current pos
    if (World::getWorld()->inBuilding(projpos, 0.0f, BZDBCache::tankHeight, 0.0f))
        projpos = targ->getPosition();
}


void            RobotPlayer::doUpdate(float dt)
{
    LocalPlayer::doUpdate(dt);

    float tankRadius = BZDBCache::tankRadius;
    const float shotRange  = BZDB.eval(StateDatabase::BZDB_SHOTRANGE);
    const float shotRadius = BZDB.eval(StateDatabase::BZDB_SHOTRADIUS);

    // fire shot if any available
    timerForShot  -= dt;
    if (timerForShot < 0.0f)
        timerForShot = 0.0f;

    if (getFiringStatus() != Ready)
        return;

    bool  shoot   = false;
    const float azimuth = getAngle();
    // Allow shooting only if angle is near and timer has elapsed
    if ((!path.empty()) && timerForShot <= 0.0f)
    {
        glm::vec3 p1;
        getProjectedPosition(target, p1);
        const auto &p2     = getPosition();
        float shootingAngle = atan2f(p1[1] - p2[1], p1[0] - p2[0]);
        if (shootingAngle < 0.0f)
            shootingAngle += (float)(2.0 * M_PI);
        float azimuthDiff   = shootingAngle - azimuth;
        if (azimuthDiff > M_PI)
            azimuthDiff -= (float)(2.0 * M_PI);
        else if (azimuthDiff < -M_PI)
            azimuthDiff += (float)(2.0 * M_PI);

        const float targetdistance = hypotf(p1[0] - p2[0], p1[1] - p2[1]) -
                                     BZDB.eval(StateDatabase::BZDB_MUZZLEFRONT) - tankRadius;

        const float missby = fabs(azimuthDiff) *
                             (targetdistance - BZDBCache::tankLength);
        // only shoot if we miss by less than half a tanklength and no building inbetween
        if (missby < 0.5f * BZDBCache::tankLength &&
                p1[2] < shotRadius)
        {
            auto pos = glm::vec3(getPosition()[0], getPosition()[1],
                                 getPosition()[2] +  BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT)
                                );
            auto dir = glm::vec3(cosf(azimuth), sinf(azimuth), 0.0f);
            Ray tankRay(pos, dir);
            float maxdistance = targetdistance;
            if (!ShotStrategy::getFirstBuilding(tankRay, -0.5f, maxdistance))
            {
                shoot=true;
                // try to not aim at teammates
                for (int i=0; i <= World::getWorld()->getCurMaxPlayers(); i++)
                {
                    Player *p = 0;
                    if (i < World::getWorld()->getCurMaxPlayers())
                        p = World::getWorld()->getPlayer(i);
                    else
                        p = LocalPlayer::getMyTank();
                    if (!p || p->getId() == getId() || validTeamTarget(p) ||
                            !p->isAlive()) continue;
                    auto relpos = getPosition() - p->getPosition();
                    Ray ray(relpos, dir);
                    float impact = rayAtDistanceFromOrigin(ray, 5 * BZDBCache::tankRadius);
                    if (impact > 0 && impact < shotRange)
                    {
                        shoot=false;
                        timerForShot = 0.1f;
                        break;
                    }
                }
                if (shoot && fireShot())
                {
                    // separate shot by 0.2 - 0.8 sec (experimental value)
                    timerForShot = float(bzfrand()) * 0.6f + 0.2f;
                }
            }
        }
    }
}

void            RobotPlayer::doUpdateMotion(float dt)
{
    if (isAlive())
    {
        bool evading = false;
        // record previous position
        const float oldAzimuth = getAngle();
        const auto &oldPosition = getPosition();
        auto position = oldPosition;
        float azimuth = oldAzimuth;
        float tankAngVel = BZDB.eval(StateDatabase::BZDB_TANKANGVEL);
        float tankSpeed = BZDBCache::tankSpeed;


        // basically a clone of Roger's evasive code
        for (int t=0; t <= World::getWorld()->getCurMaxPlayers(); t++)
        {
            Player *p = 0;
            if (t < World::getWorld()->getCurMaxPlayers())
                p = World::getWorld()->getPlayer(t);
            else
                p = LocalPlayer::getMyTank();
            if (!p || p->getId() == getId())
                continue;
            const int maxShots = p->getMaxShots();
            for (int s = 0; s < maxShots; s++)
            {
                ShotPath* shot = p->getShot(s);
                if (!shot || shot->isExpired())
                    continue;
                // ignore invisible bullets completely for now (even when visible)
                if (shot->getFlag() == Flags::InvisibleBullet)
                    continue;

                const auto shotPos = shot->getPosition();
                if ((fabs(shotPos[2] - position[2]) > BZDBCache::tankHeight) && (shot->getFlag() != Flags::GuidedMissile))
                    continue;
                const float dist = TargetingUtils::getTargetDistance(position, shotPos);
                if (dist < 150.0f)
                {
                    const auto shotVel = shot->getVelocity();
                    float shotAngle = atan2f(shotVel[1], shotVel[0]);
                    float shotUnitVec[2] = {cosf(shotAngle), sinf(shotAngle)};

                    float trueVec[2] = {(position[0]-shotPos[0])/dist,(position[1]-shotPos[1])/dist};
                    float dotProd = trueVec[0]*shotUnitVec[0]+trueVec[1]*shotUnitVec[1];

                    if (dotProd > 0.97f)
                    {
                        float rotation;
                        float rotation1 = (float)((shotAngle + M_PI/2.0) - azimuth);
                        if (rotation1 < -1.0f * M_PI) rotation1 += (float)(2.0 * M_PI);
                        if (rotation1 > 1.0f * M_PI) rotation1 -= (float)(2.0 * M_PI);

                        float rotation2 = (float)((shotAngle - M_PI/2.0) - azimuth);
                        if (rotation2 < -1.0f * M_PI) rotation2 += (float)(2.0 * M_PI);
                        if (rotation2 > 1.0f * M_PI) rotation2 -= (float)(2.0 * M_PI);

                        if (fabs(rotation1) < fabs(rotation2))
                            rotation = rotation1;
                        else
                            rotation = rotation2;
                        setDesiredSpeed(1.0f);
                        setDesiredAngVel(rotation);
                        evading = true;
                    }
                }
            }
        }

        // when we are not evading, follow the path
        if (!evading && dt > 0.0 && pathIndex < (int)path.size())
        {
            float distance;
            const auto &endPoint = path[pathIndex].get();
            // find how long it will take to get to next path segment
            auto v = endPoint - glm::vec2(position);
            distance = glm::length(v);
            float tankRadius = BZDBCache::tankRadius;
            // smooth path a little by turning early at corners, might get us stuck, though
            if (distance <= 2.5f * tankRadius)
                pathIndex++;

            float segmentAzimuth = atan2f(v[1], v[0]);
            float azimuthDiff = segmentAzimuth - azimuth;
            if (azimuthDiff > M_PI) azimuthDiff -= (float)(2.0 * M_PI);
            else if (azimuthDiff < -M_PI) azimuthDiff += (float)(2.0 * M_PI);
            if (fabs(azimuthDiff) > 0.01f)
            {
                // drive backward when target is behind, try to stick to last direction
                if (drivingForward)
                    drivingForward = fabs(azimuthDiff) < M_PI/2*0.9 ? true : false;
                else
                    drivingForward = fabs(azimuthDiff) < M_PI/2*0.3 ? true : false;
                setDesiredSpeed(drivingForward ? 1.0f : -1.0f);
                // set desired turn speed
                if (azimuthDiff >= dt * tankAngVel)
                    setDesiredAngVel(1.0f);
                else if (azimuthDiff <= -dt * tankAngVel)
                    setDesiredAngVel(-1.0f);
                else
                    setDesiredAngVel(azimuthDiff / dt / tankAngVel);
            }
            else
            {
                drivingForward = true;
                // tank doesn't turn while moving forward
                setDesiredAngVel(0.0f);
                // find how long it will take to get to next path segment
                if (distance <= dt * tankSpeed)
                {
                    pathIndex++;
                    // set desired speed
                    setDesiredSpeed(distance / dt / tankSpeed);
                }
                else
                    setDesiredSpeed(1.0f);
            }
        }
    }
    LocalPlayer::doUpdateMotion(dt);
}

void            RobotPlayer::explodeTank()
{
    LocalPlayer::explodeTank();
    target = NULL;
    path.clear();
}

void RobotPlayer::restart(const glm::vec3 &pos, float _azimuth)
{
    LocalPlayer::restart(pos, _azimuth);
    // no target
    path.clear();
    target = NULL;
    pathIndex = 0;

}

float           RobotPlayer::getTargetPriority(const
        Player* _target) const
{
    // don't target teammates or myself
    if (!this->validTeamTarget(_target))
        return 0.0f;

    // go after closest player
    // FIXME -- this is a pretty stupid heuristic
    const float worldSize = BZDBCache::worldSize;
    const auto &p1 = getPosition();
    const auto &p2 = _target->getPosition();

    float basePriority = 1.0f;
    // give bonus to non-paused player
    if (!_target->isPaused())
        basePriority += 2.0f;
    // give bonus to non-deadzone targets
    if (obstacleList)
    {
        glm::vec2 nearest;
        const BzfRegion* targetRegion = findRegion (p2, nearest);
        if (targetRegion && targetRegion->isInside(p2))
            basePriority += 1.0f;
    }
    return basePriority
           - 0.5f * hypotf(p2[0] - p1[0], p2[1] - p1[1]) / worldSize;
}

void            RobotPlayer::setObstacleList(std::vector<BzfRegion*>*
        _obstacleList)
{
    obstacleList = _obstacleList;
}

const Player*       RobotPlayer::getTarget() const
{
    return target;
}

void            RobotPlayer::setTarget(const Player* _target)
{
    static int mailbox = 0;

    path.clear();
    target = _target;
    if (!target) return;

    // work backwards (from target to me)
    glm::vec3 proj;
    getProjectedPosition(target, proj);
    auto p1 = glm::vec2(proj);
    const auto &p2 = getPosition();
    glm::vec2 q1, q2;
    BzfRegion* headRegion = findRegion(p1, q1);
    BzfRegion* tailRegion = findRegion(p2, q2);
    if (!headRegion || !tailRegion)
    {
        // if can't reach target then forget it
        return;
    }

    mailbox++;
    headRegion->setPathStuff(0.0f, NULL, q1, mailbox);
    RegionPriorityQueue queue;
    queue.insert(headRegion, 0.0f);
    BzfRegion* next;
    while (!queue.isEmpty() && (next = queue.remove()) != tailRegion)
        findPath(queue, next, tailRegion, q2, mailbox);

    // get list of points to go through to reach the target
    next = tailRegion;
    do
    {
        p1 = next->getA();
        path.push_back(p1);
        next = next->getTarget();
    }
    while (next && next != headRegion);
    if (next || tailRegion == headRegion)
        path.push_back(q1);
    else
        path.clear();
    pathIndex = 0;
}

BzfRegion*      RobotPlayer::findRegion(const glm::vec2 &p,
                                        glm::vec2 &nearest) const
{
    nearest = p;
    const int count = obstacleList->size();
    for (int o = 0; o < count; o++)
        if ((*obstacleList)[o]->isInside(p))
            return (*obstacleList)[o];

    // point is outside: find nearest region
    float      distance      = maxDistance;
    BzfRegion* nearestRegion = NULL;
    for (int i = 0; i < count; i++)
    {
        glm::vec2 currNearest;
        float currDistance = (*obstacleList)[i]->getDistance(p, currNearest);
        if (currDistance < distance)
        {
            nearestRegion = (*obstacleList)[i];
            distance = currDistance;
            nearest = currNearest;
        }
    }
    return nearestRegion;
}

float           RobotPlayer::getRegionExitPoint(
    const glm::vec2 &p1, const glm::vec2 &p2,
    const glm::vec2 &a, const glm::vec2 &targetPoint,
    glm::vec2 &mid, float& priority)
{
    auto b = targetPoint - a;
    auto d = p2 - p1;

    float vect = d[0] * b[1] - d[1] * b[0];
    float t    = 0.0f;  // safe value
    if (fabs(vect) > ZERO_TOLERANCE)
    {
        // compute intersection along (p1,d) with (a,b)
        t = (a[0] * b[1] - a[1] * b[0] - p1[0] * b[1] + p1[1] * b[0]) / vect;
        if (t > 1.0f)
            t = 1.0f;
        else if (t < 0.0f)
            t = 0.0f;
    }

    mid = p1 + t * d;

    const float distance = glm::distance(a, mid);
    // priority is to minimize distance traveled and distance left to go
    priority = distance + glm::distance(targetPoint, mid);
    // return distance traveled
    return distance;
}

void            RobotPlayer::findPath(RegionPriorityQueue& queue,
                                      BzfRegion* region,
                                      BzfRegion* targetRegion,
                                      const glm::vec2 &targetPoint,
                                      int mailbox)
{
    const int numEdges = region->getNumSides();
    for (int i = 0; i < numEdges; i++)
    {
        BzfRegion* neighbor = region->getNeighbor(i);
        if (!neighbor) continue;

        const auto &p1 = region->getCorner(i).get();
        const auto &p2 = region->getCorner((i+1)%numEdges).get();
        glm::vec2 mid;
        float priority;
        float total = getRegionExitPoint(p1, p2, region->getA(),
                                         targetPoint, mid, priority);
        priority += region->getDistance();
        if (neighbor == targetRegion)
            total += glm::distance(targetPoint, mid);
        total += region->getDistance();
        if (neighbor->test(mailbox) || total < neighbor->getDistance())
        {
            neighbor->setPathStuff(total, region, mid, mailbox);
            queue.insert(neighbor, priority);
        }
    }
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
