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

/* interface header */
#include "GuidedMissleStrategy.h"

// System headers
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

/* common implementation headers */
#include "BZDBCache.h"
#include "TextureManager.h"
#include "Intersect.h"
#include "OpenGLAPI.h"

/* local implementation headers */
#include "LocalPlayer.h"
#include "World.h"
#include "Roster.h"
#include "playing.h"

static float limitAngle(float a)
{
    if (a < -M_PI) a += (float)(2.0 * M_PI);
    else if (a >= M_PI) a -= (float)(2.0 * M_PI);
    return a;
}

GuidedMissileStrategy::GuidedMissileStrategy(ShotPath* _path) :
    ShotStrategy(_path),
    renderTimes(0),
    needUpdate(true)
{
    ptSceneNode = new BoltSceneNode(_path->getPosition(),_path->getVelocity(),false);
    TextureManager &tm = TextureManager::instance();
    int texture = tm.getTextureID("missile");

    if (texture >= 0)
    {
        ptSceneNode->setTexture(texture);
        ptSceneNode->setTextureAnimation(4, 4);
        ptSceneNode->setColor(1.0f, 0.2f, 0.0f);
        ptSceneNode->setTeamColor(Team::getShotColor(_path->getTeam()));
        ptSceneNode->setFlares(true);
    }

    // get initial shot info
    FiringInfo& f = getFiringInfo(_path);
    f.lifetime *= BZDB.eval(StateDatabase::BZDB_GMADLIFE);
    const auto vel = getPath().getVelocity();
    const auto dir = glm::normalize(vel);
    azimuth = limitAngle(atan2f(dir[1], dir[0]));
    elevation = limitAngle(atan2f(dir[2], hypotf(dir[1], dir[0])));

    // initialize segments
    currentTime = getPath().getStartTime();
    Ray ray = Ray(f.shot.pos, dir);
    ShotPathSegment segment(currentTime, currentTime, ray);
    segments.push_back(segment);
    segments.push_back(segment);
    segments.push_back(segment);
    segments.push_back(segment);

    // setup shot
    float shotSpeed = BZDB.eval(StateDatabase::BZDB_SHOTSPEED);
    f.shot.vel = shotSpeed * dir;

    // set next position to starting position
    nextPos = f.shot.pos;

    // check that first segment doesn't start inside a building
    float muzzleFront = BZDB.eval(StateDatabase::BZDB_MUZZLEFRONT);
    auto startPos = f.shot.pos - muzzleFront * dir;
    Ray firstRay = Ray(startPos, dir);
    prevTime = currentTime;
    prevTime += -muzzleFront / BZDB.eval(StateDatabase::BZDB_SHOTSPEED);
    checkBuildings(firstRay);
    prevTime = currentTime;

    // no last target
    lastTarget = NoPlayer;

    lastPuff = currentTime;
    rootPuff = BZDB.eval("gmPuffTime");
    puffTime = -1;
    if (RENDERER.useQuality() >= 3)
        rootPuff /= (10.0f + ((float)bzfrand()* 5.0f));
}

GuidedMissileStrategy::~GuidedMissileStrategy()
{
    delete ptSceneNode;
}

// NOTE -- ray is base of shot segment and normalized direction of flight.
//  distance traveled is ShotSpeed * dt.

void GuidedMissileStrategy::update(float dt)
{
    const bool isRemote = (getPath().getPlayer() !=
                           LocalPlayer::getMyTank()->getId());

    // ignore packets that arrive out of order
    if (isRemote && dt < 0.0f) return;

    // update time
    prevTime = currentTime;
    currentTime += dt;

    // if shot life ran out then send notification and expire shot.
    // only local shots are expired.
    if (!isRemote &&
            currentTime - getPath().getStartTime() >= getPath().getLifetime())
    {
        /* NOTE -- comment out to not explode when shot expires */
        addShotExplosion(nextPos);
        setExpiring();
        return;
    }

    // get target
    const Player* target = NULL;
    if (isRemote)
    {
        if (lastTarget != NoPlayer)
            target = lookupPlayer(lastTarget);
    }
    else
    {
        LocalPlayer* myTank = LocalPlayer::getMyTank();
        if (myTank)
            target = myTank->getTarget();

        // see if the target changed
        if (target)
        {
            if (lastTarget != target->getId())
            {
                needUpdate = true;
                lastTarget = target->getId();
            }
        }
        else
        {
            if (lastTarget != NoPlayer)
            {
                needUpdate = true;
                lastTarget = NoPlayer;
            }
        }
    }

    if ((target != NULL) && ((target->getFlag() == Flags::Stealth)
                             || ((target->getStatus() & short(PlayerState::Alive)) == 0)))
    {
        target = NULL;
        lastTarget = NoPlayer;
        needUpdate = true;
    }

    // compute next segment's ray
    if (target)
    {
        // turn towards target
        // find desired direction
        const auto &targetPos = target->getPosition();
        auto desiredDir = targetPos - nextPos;
        desiredDir[2] += target->getMuzzleHeight(); // right between the eyes

        // compute desired angles
        float newAzimuth = atan2f(desiredDir[1], desiredDir[0]);
        float newElevation = atan2f(desiredDir[2],
                                    hypotf(desiredDir[1], desiredDir[0]));

        float gmissileAng = BZDB.eval(StateDatabase::BZDB_GMTURNANGLE);

        // compute new azimuth
        float deltaAzimuth = limitAngle(newAzimuth - azimuth);
        if (fabsf(deltaAzimuth) <= dt * gmissileAng)
            azimuth = limitAngle(newAzimuth);
        else if (deltaAzimuth > 0.0f)
            azimuth = limitAngle(azimuth + dt * gmissileAng);
        else
            azimuth = limitAngle(azimuth - dt * gmissileAng);

        // compute new elevation
        float deltaElevation = limitAngle(newElevation - elevation);
        if (fabsf(deltaElevation) <= dt * gmissileAng)
            elevation = limitAngle(newElevation);
        else if (deltaElevation > 0.0f)
            elevation = limitAngle(elevation + dt * gmissileAng);
        else
            elevation = limitAngle(elevation - dt * gmissileAng);
    }
    auto newDirection = glm::vec3(
                            cosf(azimuth) * cosf(elevation),
                            sinf(azimuth) * cosf(elevation),
                            sinf(elevation));
    Ray ray = Ray(nextPos, newDirection);

    renderTimes++;
    if (puffTime < 0 )
        puffTime =  (float)bzfrand()*rootPuff;

    // Changed: GM smoke trail, leave it every seconds, none of this per frame crap
    if (currentTime.getSeconds() - lastPuff.getSeconds() > puffTime )
    {
        lastPuff = currentTime;
        addShotPuff(nextPos,azimuth,elevation);

        // pick a new time for the next puff so it's not so orderd.
        puffTime = (float)bzfrand()*rootPuff;
    }

    // get next position
    float shotSpeed = BZDB.eval(StateDatabase::BZDB_SHOTSPEED);
    ray.getPoint(dt * shotSpeed, nextPos);

    // see if we hit something
    TimeKeeper segmentEndTime(currentTime);
    /* if (!isRemote) */
    {
        if (nextPos[2] <= 0.0f)
        {
            // hit ground -- expire it and shorten life of segment to time of impact
            setExpiring();
            float t = ray.getOrigin()[2] / (ray.getOrigin()[2] - nextPos[2]);
            segmentEndTime = prevTime;
            segmentEndTime += t * (currentTime - prevTime);
            ray.getPoint(t / shotSpeed, nextPos);
            addShotExplosion(nextPos);
        }
        else
        {
            // see if we hit a building
            const float t = checkBuildings(ray);
            if (t >= 0.0f)
            {
                segmentEndTime = prevTime;
                segmentEndTime += t;
            }
        }
    }

    // throw out old segment and add new one
    ShotPathSegment nextSegment(prevTime, segmentEndTime, ray);
    segments.insert(segments.begin(), nextSegment);
    segments.pop_back();

    // update shot
    newDirection *= shotSpeed;
    setPosition(nextPos);
    setVelocity(newDirection);
}

float GuidedMissileStrategy::checkBuildings(const Ray& ray)
{
    float shotSpeed = BZDB.eval(StateDatabase::BZDB_SHOTSPEED);
    float t = float((currentTime - prevTime) * shotSpeed);
    int face;
    const Obstacle* building = getFirstBuilding(ray, Epsilon, t);
    const Teleporter* teleporter = getFirstTeleporter(ray, Epsilon, t, face);

    // check in reverse order to see what we hit first
    if (teleporter)
    {
        // entered teleporter -- teleport it
        unsigned int seed = getPath().getShotId();
        int source = World::getWorld()->getTeleporter(teleporter, face);
        int target = World::getWorld()->getTeleportTarget(source, seed);

        int outFace;
        const Teleporter* outTeleporter =
            World::getWorld()->getTeleporter(target, outFace);
        teleporter->getPointWRT(*outTeleporter, face, outFace,
                                nextPos, NULL, &azimuth);
        return t / shotSpeed;
    }
    else if (building)
    {
        // expire on next update
        setExpiring();
        glm::vec3 pos;
        ray.getPoint(t / shotSpeed, pos);
        addShotExplosion(pos);
        return t / shotSpeed;
    }
    return -1.0f;
}

float GuidedMissileStrategy::checkHit(
    const BaseLocalPlayer* tank, glm::vec3 &position) const
{
    float minTime = Infinity;
    if (getPath().isExpired()) return minTime;

    // GM is not active until activation time passes (for any tank)
    const float activationTime = BZDB.eval(StateDatabase::BZDB_GMACTIVATIONTIME);
    if ((TimeKeeper::getTick() - getPath().getStartTime()) < activationTime)
        return minTime;

    // get tank radius
    float radius = tank->getRadius();
    const float radius2 = radius * radius;

    float shotRadius = BZDB.eval(StateDatabase::BZDB_SHOTRADIUS);

    // tank is positioned from it's bottom so shift position up by
    // half a tank height.
    const float tankHeight = tank->getDimensions()[2];
    Ray tankLastMotionRaw = tank->getLastMotion();
    auto lastTankPositionRaw = tankLastMotionRaw.getOrigin();
    lastTankPositionRaw.z += 0.5f * tankHeight;
    Ray tankLastMotion(lastTankPositionRaw, tankLastMotionRaw.getDirection());

    // check each segment
    const int numSegments = segments.size();
    int i = 0;
    // only test most recent segment if shot is from my tank
    if (numSegments > 1 && tank->getId() == getPath().getPlayer())
        i = numSegments - 1;
    for (; i < numSegments; i++)
    {
        const Ray& ray = segments[i].ray;

        // construct ray with correct velocity
        const auto &dir = ray.getDirection();
        float shotSpeed = BZDB.eval(StateDatabase::BZDB_SHOTSPEED);
        auto speed = shotSpeed * dir;
        Ray speedRay(ray.getOrigin(), speed);

        // construct relative shot ray:  origin and velocity relative to
        // my tank as a function of time (t=0 is start of the interval).
        Ray relativeRay(rayMinusRay(speedRay, 0.0, tankLastMotion, 0.0));

        // get closest approach time
        float t;
        if (tank->getFlag() == Flags::Narrow)
        {
            // find closest approach to narrow box around tank.  width of box
            // is shell radius so you can actually hit narrow tank head on.
            static auto tankBase = glm::vec3(0.0f, 0.0f, -0.5f * tankHeight);
            t = timeRayHitsBlock(relativeRay, tankBase, tank->getAngle(),
                                 0.5f * BZDBCache::tankLength, shotRadius, tankHeight);
        }
        else
        {
            // find time when shot hits sphere around tank
            t = rayAtDistanceFromOrigin(relativeRay, 0.99f * radius);
        }
        if (t > minTime) continue;

        // if not in shot segment times then no hit
        if (t < 0.0f || t > segments[i].end - segments[i].start)
            continue;

        // check if shot hits tank -- get position at time t, see if in radius
        glm::vec3 closestPos;
        relativeRay.getPoint(t, closestPos);
        if (glm::length2(closestPos) < radius2)
        {
            // save best time so far
            minTime = t;

            // compute location of tank at time of hit
            glm::vec3 tankPos;
            tank->getLastMotion().getPoint(t, tankPos);

            // compute position of intersection
            position = tankPos + closestPos;
        }
    }

    return minTime;
}

void GuidedMissileStrategy::sendUpdate(const FiringInfo& firingInfo) const
{
    // only send an update when needed
    if (!needUpdate) return;
    needUpdate = false;

    // construct and send packet
    char packet[ShotUpdatePLen + PlayerIdPLen];
    void *buf = (void*)packet;
    buf = firingInfo.shot.pack(buf);
    buf = nboPackUByte(buf, lastTarget);
    ServerLink::getServer()->send(MsgGMUpdate, sizeof(packet), packet);
}

void GuidedMissileStrategy::readUpdate(uint16_t code, const void* msg)
{
    // ignore non-guided missile messages (we shouldn't get them)
    if (code != MsgGMUpdate) return;

    // position and velocity have been replaced by the remote system's
    // concept of the position and velocity.  this may cause a discontinuity
    // in the shot's position but it's probably better to have the shot in
    // the right place than to maintain smooth motion.  these updates ought
    // to be rare anyway.

    // read the lastTarget
    nboUnpackUByte(msg, lastTarget);

    // fix up dependent variables
    const auto vel = getPath().getVelocity();
    auto dir = glm::normalize(vel);
    azimuth = limitAngle(atan2f(dir[1], dir[0]));
    elevation = limitAngle(atan2f(dir[2], hypotf(dir[1], dir[0])));
    const auto pos = getPath().getPosition();
    nextPos = pos;

    // note that we do not call update(float).  let that happen on the
    // next time step.
}

void GuidedMissileStrategy::addShot(SceneDatabase* scene, bool)
{
    ptSceneNode->move(getPath().getPosition(), getPath().getVelocity());
    scene->addDynamicNode(ptSceneNode);
}

void GuidedMissileStrategy::expire()
{
    if (getPath().getPlayer() == LocalPlayer::getMyTank()->getId())
    {
        const ShotPath& shot = getPath();
        /* NOTE -- change 0 to 1 to not explode when shot expires (I think) */
        ServerLink::getServer()->sendEndShot(shot.getPlayer(), shot.getShotId(), 0);
    }
}

void GuidedMissileStrategy::radarRender() const
{
    const auto &orig = getPath().getPosition();
    const int length = (int)BZDBCache::linedRadarShots;
    const int size   = (int)BZDBCache::sizedRadarShots;

    float shotTailLength = BZDB.eval(StateDatabase::BZDB_SHOTTAILLENGTH);
    // Display leading lines
    if (length > 0)
    {
        const auto vel = getPath().getVelocity();
        const auto dir = glm::normalize(vel) * shotTailLength * float(length);
        glBegin(GL_LINES);
        glVertex(orig);
        if (BZDBCache::leadingShotLine == 1)   //leading
        {
            glVertex2f(orig[0] + dir[0], orig[1] + dir[1]);
            glEnd();
        }
        else if (BZDBCache::leadingShotLine == 0)     //lagging
        {
            glVertex2f(orig[0] - dir[0], orig[1] - dir[1]);
            glEnd();
        }
        else if (BZDBCache::leadingShotLine == 2)     //both
        {
            glVertex2f(orig[0] + dir[0], orig[1] + dir[1]);
            glEnd();
            glBegin(GL_LINES);
            glVertex(orig);
            glVertex2f(orig[0] - dir[0], orig[1] - dir[1]);
            glEnd();
        }

        // draw a "bright reddish" missle tip
        if (size > 0)
        {
            glColor3f(1.0f, 0.75f, 0.75f);
            glPointSize((float)size);
            glBegin(GL_POINTS);
            glVertex2f(orig[0], orig[1]);
            glEnd();
            glPointSize(1.0f);
        }
    }
    else
    {
        if (size > 0)
        {
            // draw a sized missle
            glPointSize((float)size);
            glBegin(GL_POINTS);
            glVertex(orig);
            glEnd();
            glPointSize(1.0f);
        }
        else
        {
            // draw the tiny missle
            glBegin(GL_POINTS);
            glVertex(orig);
            glEnd();
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
