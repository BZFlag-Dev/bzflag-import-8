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
#include "LocalPlayer.h"

/* system implementation headers */
#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

/* common implementation headers */
#include "CommandManager.h"
#include "BZDBCache.h"
#include "FlagSceneNode.h"
#include "CollisionManager.h"
#include "PhysicsDriver.h"
#include "BzfEvent.h"
#include "WallObstacle.h"
#include "MeshObstacle.h"

/* local implementation headers */
#include "World.h"
#include "sound.h"
#include "ForceFeedback.h"
#include "effectsRenderer.h"

LocalPlayer*        LocalPlayer::mainPlayer = NULL;

LocalPlayer::LocalPlayer(const PlayerId& _id,
                         const char* name, const char* _motto) :
    BaseLocalPlayer(_id, name, _motto),
    gettingSound(true),
    server(ServerLink::getServer()),
    location(Dead),
    firingStatus(Deceased),
    flagShakingTime(0.0f),
    flagShakingWins(0),
    antidoteFlag(NULL),
    desiredSpeed(0.0f),
    desiredAngVel(0.0f),
    lastSpeed(0.0f),
    anyShotActive(false),
    target(NULL),
    nemesis(NULL),
    recipient(NULL),
    inputChanged(false),
    stuckFrameCount(0),
    spawning(false),
    wingsFlapCount(0),
    left(false),
    right(false),
    up(false),
    down(false),
    entryDrop(true),
    wantJump(false),
    jumpPressed(false),
    deathPhyDrv(-1)
{
    // initialize shots array to no shots fired
    const int numShots = World::getWorld()->getMaxShots();
    shots = new LocalShotPath*[numShots];
    for (int i = 0; i < numShots; i++)
        shots[i] = NULL;
    // set input method
    if (BZDB.isTrue("allowInputChange"))
        inputMethod = Mouse;
    else
        setInputMethod(BZDB.get("activeInputDevice"));
}

LocalPlayer::~LocalPlayer()
{
    // free shots
    const int numShots = World::getWorld()->getMaxShots();
    for (int i = 0; i < numShots; i++)
        if (shots[i])
            delete shots[i];
    delete[] shots;

    // free antidote flag
    delete antidoteFlag;
}

void            LocalPlayer::setMyTank(LocalPlayer* player)
{
    mainPlayer = player;
}

void            LocalPlayer::doUpdate(float dt)
{
    const bool hadShotActive = anyShotActive;
    const int numShots = World::getWorld()->getMaxShots();
    static TimeKeeper pauseTime = TimeKeeper::getNullTime();
    static bool wasPaused = false;

    // if paused then boost the reload times by dt (so that, effectively,
    // reloading isn't performed)
    int i;
    if (isPaused())
    {
        for (i = 0; i < numShots; i++)
        {
            if (shots[i])
                shots[i]->boostReloadTime(dt);
        }

        // if we've been paused for a long time, drop our flag
        if (!wasPaused)
        {
            pauseTime = TimeKeeper::getTick();
            wasPaused = true;
        }
        if (TimeKeeper::getTick() -  pauseTime > BZDB.eval(StateDatabase::BZDB_PAUSEDROPTIME))
        {
            server->sendDropFlag(getPosition());
            setStatus(getStatus() & ~PlayerState::FlagActive);
            pauseTime = TimeKeeper::getSunExplodeTime();
        }

    }
    else
    {
        pauseTime = TimeKeeper::getNullTime();
        wasPaused = false;
    }

    // reap dead (reloaded) shots
    for (i = 0; i < numShots; i++)
        if (shots[i] && shots[i]->isReloaded())
        {
            if (!shots[i]->isExpired())
                shots[i]->setExpired();
            delete shots[i];
            shots[i] = NULL;
        }

    // update shots
    anyShotActive = false;
    for (i = 0; i < numShots; i++)
        if (shots[i])
        {
            shots[i]->update(dt);
            if (!shots[i]->isExpired()) anyShotActive = true;
        }

    // if no shots now out (but there had been) then reset target
    if (!anyShotActive && hadShotActive)
        target = NULL;

    // drop bad flag if timeout has expired
    if (!isPaused() && dt > 0.0f && World::getWorld()->allowShakeTimeout() &&
            getFlag() != Flags::Null && getFlag()->endurance == FlagSticky &&
            flagShakingTime > 0.0f)
    {
        flagShakingTime -= dt;
        if (flagShakingTime <= 0.0f)
        {
            flagShakingTime = 0.0f;
            server->sendDropFlag(getPosition());
        }
    }
}


void LocalPlayer::doSlideMotion(float dt, float slideTime,
                                float newAngVel, glm::vec3 &newVelocity)
{
    const float oldAzimuth = getAngle();
    const auto &oldVelocity = getVelocity();

    const float angle = oldAzimuth + (0.5f * dt * newAngVel);
    const float cos_val = cosf(angle);
    const float sin_val = sinf(angle);
    const float scale = (dt / slideTime);
    const float speedAdj = desiredSpeed * scale;
    const auto  ov = glm::vec2(oldVelocity);
    auto &nv = newVelocity;
    nv[0] = ov[0] + (cos_val * speedAdj);
    nv[1] = ov[1] + (sin_val * speedAdj);
    const float newSpeed = glm::length(glm::vec2(nv));

    float maxSpeed = getMaxSpeed();

    if (newSpeed > maxSpeed)
    {
        float adjSpeed;
        const float oldSpeed = glm::length(ov);
        if (oldSpeed > maxSpeed)
        {
            adjSpeed = oldSpeed - maxSpeed * scale;
            if (adjSpeed < 0.0f)
                adjSpeed = 0.0f;
        }
        else
            adjSpeed = maxSpeed;
        const float speedScale = adjSpeed / newSpeed;
        nv[0] *= speedScale;
        nv[1] *= speedScale;
    }
    return;
}


float LocalPlayer::getNewAngVel(float old, float desired)
{
    float newAngVel;

    if ((inputMethod != Keyboard) || (getPhysicsDriver() >= 0))
    {
        // mouse and joystick users
        newAngVel = desired;

    }
    else
    {

        /* keyboard users
         * the larger the oldAngVel contribution, the more slowly an
         * angular velocity converges to the desired "max" velocity; the
         * contribution of the desired and old velocity should add up to
         * one for a linear convergence rate.
         */
        newAngVel = (old * 0.8f) + (desired * 0.2f);

        // instant stop
        if ((old * desired < 0.0f) ||
                (NEAR_ZERO(desired, ZERO_TOLERANCE)))
            newAngVel = desired;
    }
    return newAngVel;
}


void            LocalPlayer::doUpdateMotion(float dt)
{
    static const float MinSearchStep = 0.0001f;
    static const int MaxSearchSteps = 7;
    static const int MaxSteps = 4;
    static const float TinyDistance = 0.001f;

    // save old state
    const Location oldLocation = location;
    const auto &oldPosition = getPosition();
    const float oldAzimuth = getAngle();
    const float oldAngVel = getAngularVelocity();
    const auto &oldVelocity = getVelocity();

    // prepare new state
    auto newVelocity = oldVelocity;
    float newAngVel = 0.0f;

    clearRemoteSounds();

    // if was teleporting and exceeded teleport time then not teleporting anymore
    if (isTeleporting() &&
            ((lastTime - getTeleportTime()) >= BZDB.eval(StateDatabase::BZDB_TELEPORTTIME)))
        setStatus(getStatus() & ~short(PlayerState::Teleporting));

    // phased means we can pass through buildings
    const bool phased = ((location == Dead) || (location == Exploding) ||
                         (getFlag() == Flags::OscillationOverthruster) ||
                         isPhantomZoned());

    float groundLimit = 0.0f;
    if (getFlag() == Flags::Burrow)
        groundLimit = BZDB.eval(StateDatabase::BZDB_BURROWDEPTH);

    // get linear and angular speed at start of time step
    if (!NEAR_ZERO(dt,ZERO_TOLERANCE))
    {
        if (location == Dead || isPaused())
        {
            // can't move if paused or dead -- set dt to zero instead of
            // clearing velocity and newAngVel for when we resume (if paused)
            dt = 0.0f;
            newAngVel = oldAngVel;
        }
        else if (location == Exploding)
        {
            // see if explosing time has expired
            if (lastTime - getExplodeTime() >= BZDB.eval(StateDatabase::BZDB_EXPLODETIME))
            {
                dt -= float((lastTime - getExplodeTime()) - BZDB.eval(StateDatabase::BZDB_EXPLODETIME));
                if (dt < 0.0f)
                    dt = 0.0f;
                setStatus(PlayerState::DeadStatus);
                location = Dead;
                if (isAutoPilot())
                    CMDMGR.run("restart");
            }

            // can't control explosion motion
            newVelocity[2] += BZDBCache::gravity * dt;
            newAngVel = 0.0f; // or oldAngVel to spin while exploding
        }
        else if ((location == OnGround) || (location == OnBuilding) ||
                 (location == InBuilding && oldPosition[2] == groundLimit))
        {
            // full control
            float speed = desiredSpeed;

            // angular velocity
            newAngVel = getNewAngVel(oldAngVel, desiredAngVel);

            // limit acceleration
            doMomentum(dt, speed, newAngVel);

            // compute velocity so far
            const float angle = oldAzimuth + 0.5f * dt * newAngVel;
            newVelocity[0] = speed * cosf(angle);
            newVelocity[1] = speed * sinf(angle);
            newVelocity[2] = 0.0f;

            // now friction, if any
            doFriction(dt, oldVelocity, newVelocity);

            // reset our flap count if we have wings
            if (getFlag() == Flags::Wings)
                wingsFlapCount = (int) BZDB.eval(StateDatabase::BZDB_WINGSJUMPCOUNT);

            if ((oldPosition[2] < 0.0f) && (getFlag() == Flags::Burrow))
                newVelocity[2] += 4 * BZDBCache::gravity * dt;
            else if (oldPosition[2] > groundLimit)
                newVelocity[2] += BZDBCache::gravity * dt;

            // save speed for next update
            lastSpeed = speed;
        }
        else
        {
            // can't control motion in air unless have wings
            if (getFlag() == Flags::Wings)
            {
                float speed = desiredSpeed;

                // angular velocity
                newAngVel = getNewAngVel(oldAngVel, desiredAngVel);

                // compute horizontal velocity so far
                const float slideTime = BZDB.eval(StateDatabase::BZDB_WINGSSLIDETIME);
                if (slideTime > 0.0)
                    doSlideMotion(dt, slideTime, newAngVel, newVelocity);
                else
                {
                    const float angle = oldAzimuth + 0.5f * dt * newAngVel;
                    newVelocity[0] = speed * cosf(angle);
                    newVelocity[1] = speed * sinf(angle);
                }

                newVelocity[2] += BZDB.eval(StateDatabase::BZDB_WINGSGRAVITY) * dt;
                lastSpeed = speed;
            }
            else
            {
                newVelocity[2] += BZDBCache::gravity * dt;
                newAngVel = oldAngVel;
            }
        }


        // now apply outside forces
        doForces(dt, newVelocity, newAngVel);

        // below the ground: however I got there, creep up
        if (oldPosition[2] < groundLimit)
            newVelocity[2] = std::max(newVelocity[2], -oldPosition[2] / 2.0f + 0.5f);
    }

    // jump here, we allow a little change in horizontal motion
    if (wantJump)
    {
        doJump();
        if (!wantJump)
        {
            newVelocity[2] = oldVelocity[2];
            if ((lastObstacle != NULL) && !lastObstacle->isFlatTop()
                    && BZDB.isTrue(StateDatabase::BZDB_NOCLIMB))
            {
                newVelocity[0] = 0.0f;
                newVelocity[1] = 0.0f;
            }
        }
    }

    // do the physics driver stuff
    const int driverId = getPhysicsDriver();
    const PhysicsDriver* phydrv = PHYDRVMGR.getDriver(driverId);
    if (phydrv != NULL)
    {
        const float* v = phydrv->getLinearVel();

        newVelocity[2] += v[2];

        if (phydrv->getIsSlide())
        {
            const float slideTime = phydrv->getSlideTime();
            doSlideMotion(dt, slideTime, newAngVel, newVelocity);
        }
        else
        {
            // adjust the horizontal velocity
            newVelocity[0] += v[0];
            newVelocity[1] += v[1];

            const float av = phydrv->getAngularVel();
            const float* ap = phydrv->getAngularPos();

            if (av != 0.0f)
            {
                // the angular velocity is in radians/sec
                newAngVel += av;
                const float dx = oldPosition[0] - ap[0];
                const float dy = oldPosition[1] - ap[1];
                newVelocity[0] -= av * dy;
                newVelocity[1] += av * dx;
            }
        }
    }
    lastObstacle = NULL;

    // get new position so far (which is just the current position)
    auto newPos      = oldPosition;
    float newAzimuth = oldAzimuth;

    // move tank through the time step.  if there's a collision then
    // move the tank up to the collision, adjust the velocity to
    // prevent interpenetration, and repeat.  avoid infinite loops
    // by only allowing a maximum number of repeats.
    bool expelled;
    const Obstacle* obstacle;
    float timeStep = dt;
    int stuck = false;
    if (location != Dead && location != Exploding)
    {
        location = OnGround;

        // anti-stuck code is useful only when alive
        // then only any 100 frames while stuck, take an action

        // try to see if we are stuck on a building
        obstacle = getHitBuilding(newPos, newAzimuth, newPos, newAzimuth,
                                  phased, expelled);

        if (obstacle && expelled)
        {
            stuckFrameCount++;
            stuck = true;
        }
        else
            stuckFrameCount = 0;

        if (stuckFrameCount > 100)
        {
            stuckFrameCount = 0;
            // we are using a maximum value on time for frame to avoid lagging problem
            setDesiredSpeed(0.25f);
            float delta = dt > 0.1f ? 0.1f : dt;
            glm::vec3 normalStuck;
            obstacle->getNormal(newPos, normalStuck);
            // use all the given speed to exit
            float movementMax = desiredSpeed * delta;

            newVelocity = movementMax * normalStuck;
            if ((World::getWorld()->allowJumping() || (getFlag() == Flags::Jumping) || (getFlag() == Flags::Wings)) &&
                    (getFlag() != Flags::NoJumping))
                ;
            else
                newVelocity[2] = 0.0f;

            // exit will be in the normal direction
            newPos += newVelocity;
            // compute time for all other kind of movements
            timeStep -= delta;
        }
    }

    float nominalPlanarSpeed2 = glm::length2(glm::vec2(newVelocity));

    for (int numSteps = 0; numSteps < MaxSteps; numSteps++)
    {
        // record position at beginning of time step
        float tmpAzimuth;
        tmpAzimuth = newAzimuth;
        auto tmpPos = newPos;

        // get position at end of time step
        newAzimuth = tmpAzimuth + timeStep * newAngVel;
        newPos = tmpPos + timeStep * newVelocity;
        if ((newPos[2] < groundLimit) && (newVelocity[2] < 0))
        {
            // Hit lower limit, stop falling
            newPos[2] = groundLimit;
            if (location == Exploding)
            {
                // tank pieces reach the ground, friction
                // stop them, & mainly player view
                newPos[0] = tmpPos[0];
                newPos[1] = tmpPos[1];
            }
        }

        // see if we hit anything.  if not then we're done.
        obstacle = getHitBuilding(tmpPos, tmpAzimuth, newPos, newAzimuth,
                                  phased, expelled);

        if (!obstacle || !expelled) break;

        float obstacleTop = obstacle->getPosition()[2] + obstacle->getHeight();
        if ((oldLocation != InAir) && obstacle->isFlatTop() &&
                (obstacleTop != tmpPos[2]) &&
                (obstacleTop < (tmpPos[2] + BZDB.eval(StateDatabase::BZDB_MAXBUMPHEIGHT))))
        {
            newPos[0] = oldPosition[0];
            newPos[1] = oldPosition[1];
            newPos[2] = obstacleTop;

            // drive over bumps
            const Obstacle* bumpObstacle = getHitBuilding(newPos, tmpAzimuth,
                                           newPos, newAzimuth,
                                           phased, expelled);
            if (bumpObstacle == NULL)
            {
                move(newPos, getAngle());
                newPos[0] += newVelocity[0] * (dt * 0.5f);
                newPos[1] += newVelocity[1] * (dt * 0.5f);
                break;
            }
        }

        // record position when hitting
        float hitAzimuth = newAzimuth;
        auto hitPos = newPos;

        // find the latest time before the collision
        float searchTime = 0.0f, searchStep = 0.5f * timeStep;
        for (int i = 0; searchStep > MinSearchStep && i < MaxSearchSteps;
                searchStep *= 0.5f, i++)
        {
            // get intermediate position
            const float t = searchTime + searchStep;
            newAzimuth = tmpAzimuth + (t * newAngVel);
            newPos = tmpPos + t * newVelocity;
            if ((newPos[2] < groundLimit) && (newVelocity[2] < 0))
                newPos[2] = groundLimit;

            // see if we hit anything
            bool searchExpelled;
            const Obstacle* searchObstacle =
                getHitBuilding(tmpPos, tmpAzimuth, newPos, newAzimuth,
                               phased, searchExpelled);

            if (!searchObstacle || !searchExpelled)
            {
                // if no hit then search latter half of time step
                searchTime = t;
            }
            else if (searchObstacle)
            {
                // if we hit a building then record which one and where
                obstacle = searchObstacle;

                expelled = searchExpelled;
                hitAzimuth = newAzimuth;
                hitPos = newPos;
            }
        }

        // get position just before impact
        newAzimuth = tmpAzimuth + (searchTime * newAngVel);
        newPos = tmpPos + searchTime * newVelocity;
        if (oldPosition[2] < groundLimit)
            newVelocity[2] = std::max(newVelocity[2], -oldPosition[2] / 2.0f + 0.5f);


        // record how much time is left in time step
        timeStep -= searchTime;

        // get normal at intersection.  sometimes fancy test says there's
        // no intersection but we're expecting one so, in that case, fall
        // back to simple normal calculation.
        glm::vec3 normal;
        if (!getHitNormal(obstacle, newPos, newAzimuth, hitPos, hitAzimuth, normal))
            obstacle->getNormal(newPos, normal);

        // check for being on a building
        if ((newPos[2] > 0.0f) && (normal[2] > 0.001f))
        {
            if (location != Dead && location != Exploding && expelled)
            {
                location = OnBuilding;
                lastObstacle = obstacle;
            }
            newVelocity[2] = 0.0f;
        }
        else
        {
            // get component of velocity in normal direction (in horizontal plane)
            float mag = (normal[0] * newVelocity[0]) +
                        (normal[1] * newVelocity[1]);

            // handle upward normal component to prevent an upward force
            if (!NEAR_ZERO(normal[2], ZERO_TOLERANCE))
            {
                // if going down then stop falling
                if (newVelocity[2] < 0.0f && newVelocity[2] -
                        (mag + normal[2] * newVelocity[2]) * normal[2] > 0.0f)
                    newVelocity[2] = 0.0f;

                // normalize force magnitude in horizontal plane
                float horNormal = normal[0] * normal[0] + normal[1] * normal[1];
                if (!NEAR_ZERO(horNormal, ZERO_TOLERANCE))
                    mag /= horNormal;
            }

            // cancel out component in normal direction (if velocity and
            // normal point in opposite directions).  also back off a tiny
            // amount to prevent a spurious collision against the same
            // obstacle.
            if (mag < 0.0f)
            {
                newVelocity[0] -= mag * normal[0];
                newVelocity[1] -= mag * normal[1];

                newPos[0] -= TinyDistance * mag * normal[0];
                newPos[1] -= TinyDistance * mag * normal[1];
            }
            if (mag > -0.01f)
            {
                // assume we're not allowed to turn anymore if there's no
                // significant velocity component to cancel out.
                newAngVel = 0.0f;
            }
        }
    }

    // pick new location if we haven't already done so
    if (location == OnGround)
    {
        if (obstacle && (!expelled || stuck))
            location = InBuilding;
        else if (newPos[2] > 0.0f)
            location = InAir;
    }

    // see if we're crossing a wall
    if (location == InBuilding && getFlag() == Flags::OscillationOverthruster)
    {
        if (obstacle->isCrossing(newPos, newAzimuth,
                                 0.5f * BZDBCache::tankLength,
                                 0.5f * BZDBCache::tankWidth,
                                 BZDBCache::tankHeight, NULL))
            setStatus(getStatus() | int(PlayerState::CrossingWall));
        else
            setStatus(getStatus() & ~int(PlayerState::CrossingWall));
    }
    else if (World::getWorld()->crossingTeleporter( newPos, newAzimuth,
             0.5f * BZDBCache::tankLength,
             0.5f * BZDBCache::tankWidth,
             BZDBCache::tankHeight, crossingPlane))
        setStatus(getStatus() | int(PlayerState::CrossingWall));
    else
        setStatus(getStatus() & ~int(PlayerState::CrossingWall));

    // compute actual velocities.  do this before teleportation.
    if (!NEAR_ZERO(dt, ZERO_TOLERANCE))
    {
        const float oodt = 1.0f / dt;
        newAngVel = (newAzimuth - oldAzimuth) * oodt;
        newVelocity = (newPos - oldPosition) * oodt;

        if (nominalPlanarSpeed2)
        {
            float newPlanarSpeed2 = glm::length2(glm::vec2(newVelocity));
            float scaling = newPlanarSpeed2 / nominalPlanarSpeed2;
            if (scaling > 1.0f)
            {
                scaling = sqrtf(scaling);
                newVelocity[0] /= scaling;
                newVelocity[1] /= scaling;
            }
        }
    }

    // see if we teleported
    int face;
    const Teleporter* teleporter;
    if (!isAlive())
        teleporter = NULL;
    else
        teleporter = World::getWorld()->crossesTeleporter(oldPosition, newPos, face);

    if (teleporter)
    {
        if (getFlag() == Flags::PhantomZone)
        {
            // change zoned state
            setStatus(getStatus() ^ PlayerState::FlagActive);
            if (gettingSound)
                playLocalSound(SFX_PHANTOM);
        }
        else
        {
            // teleport
            const int source = World::getWorld()->getTeleporter(teleporter, face);
            int targetTele = World::getWorld()->getTeleportTarget(source);

            int outFace;
            const Teleporter* outPort =
                World::getWorld()->getTeleporter(targetTele, outFace);
            teleporter->getPointWRT(*outPort, face, outFace,
                                    newPos, &newVelocity, &newAzimuth);

            // check for a hit on the other side
            const Obstacle* teleObs =
                getHitBuilding(newPos, newAzimuth, newPos, newAzimuth, phased, expelled);
            if (teleObs != NULL)
            {
                // revert
                newPos = oldPosition;
                newVelocity[0] = newVelocity[1] = 0.0f;
                newVelocity[2] = oldVelocity[2];
                newAzimuth = oldAzimuth;
            }
            else
            {
                // save teleport info
                setTeleport(lastTime, source, targetTele);
                server->sendTeleport(source, targetTele);
                if (gettingSound)
                    playLocalSound(SFX_TELEPORT);
            }
        }
    }

    // setup the physics driver
    setPhysicsDriver(-1);
    if ((lastObstacle != NULL) &&
            (lastObstacle->getType() == MeshFace::getClassName()))
    {
        const MeshFace* meshFace = (const MeshFace*) lastObstacle;
        int driverIdent = meshFace->getPhysicsDriver();
        const PhysicsDriver* phydriver = PHYDRVMGR.getDriver(driverIdent);
        if (phydriver != NULL)
            setPhysicsDriver(driverIdent);
    }

    // deal with drop sounds and effects
    if (entryDrop)
    {
        // because the starting position that the server sends can result
        // in an initial InAir condition, we use this bool to avoid having
        // a false jump mess with the spawnEffect()
        // FIXME: this isn't a clean way to do it
        if ((oldLocation == InAir) == (location == InAir))
            entryDrop = false;
    }

    const bool justLanded =
        (oldLocation == InAir) &&
        ((location == OnGround) || (location == OnBuilding));

    if (justLanded)
    {
        setLandingSpeed(oldVelocity[2]);
        EFFECTS.addLandEffect(getColor(),newPos,getAngle());
    }
    if (gettingSound)
    {
        const PhysicsDriver* phydriver = PHYDRVMGR.getDriver(getPhysicsDriver());
        if ((phydriver != NULL) && (phydriver->getLinearVel()[2] > 0.0f))
        {
            playLocalSound(SFX_BOUNCE);
            addRemoteSound(PlayerState::BounceSound);
        }
        else if (justLanded && !entryDrop)
            playLocalSound(SFX_LAND);
        else if ((location == OnGround) &&
                 (oldPosition[2] == 0.0f) && (newPos[2] < 0.f))
            playLocalSound(SFX_BURROW);
    }

    // set falling status
    if (location == OnGround || location == OnBuilding ||
            (location == InBuilding && newPos[2] == 0.0f))
        setStatus(getStatus() & ~short(PlayerState::Falling));
    else if (location == InAir || location == InBuilding)
        setStatus(getStatus() | short(PlayerState::Falling));

    // set UserInput status (determines how animated treads are drawn)
    const PhysicsDriver* phydrv2 = PHYDRVMGR.getDriver(getPhysicsDriver());
    if (((phydrv2 != NULL) && phydrv2->getIsSlide()) ||
            ((getFlag() == Flags::Wings) && (location == InAir) &&
             (BZDB.eval(StateDatabase::BZDB_WINGSSLIDETIME) > 0.0f)))
        setStatus(getStatus() | short(PlayerState::UserInputs));
    else
        setStatus(getStatus() & ~short(PlayerState::UserInputs));

    // compute firing status
    switch (location)
    {
    case Dead:
    case Exploding:
        firingStatus = Deceased;
        break;
    case InBuilding:
        firingStatus = (getFlag() == Flags::PhantomZone) ? Zoned : Sealed;
        break;
    default:
        if (isPhantomZoned())
            firingStatus = Zoned;
        else if ((getReloadTime() > 0.0f) && (getFlag() != Flags::TriggerHappy))
            firingStatus = Loading;
        else
            firingStatus = Ready;
        break;
    }

    // calculate the list of inside buildings
    insideBuildings.clear();
    if (location == InBuilding)
        collectInsideBuildings();

    // move tank
    move(newPos, newAzimuth);
    setVelocity(newVelocity);
    setAngularVelocity(newAngVel);
    setRelativeMotion();
    newAzimuth = getAngle(); // pickup the limited angle range from move()

    // see if I'm over my antidote
    if (antidoteFlag && location == OnGround)
    {
        float dist = glm::distance2(glm::vec2(flagAntidotePos), glm::vec2(newPos));
        const float twoRads = getRadius() + BZDBCache::flagRadius;
        if (dist < (twoRads * twoRads))
            server->sendDropFlag(getPosition());
    }

    if ((getFlag() == Flags::Bouncy) && ((location == OnGround) || (location == OnBuilding)))
    {
        if (oldLocation != InAir)
        {
            if ((TimeKeeper::getTick() - bounceTime) > 0)
                doJump();
        }
        else
        {
            bounceTime = TimeKeeper::getTick();
            bounceTime += 0.2f;
        }
    }

    if (gettingSound)
    {
        if (oldPosition[0] != newPos[0] || oldPosition[1] != newPos[1] ||
                oldPosition[2] != newPos[2] || oldAzimuth != newAzimuth)
        {
            moveSoundReceiver(newPos[0], newPos[1], newPos[2], newAzimuth,
                              NEAR_ZERO(dt, ZERO_TOLERANCE) ||
                              ((teleporter != NULL) && (getFlag() != Flags::PhantomZone)));
        }
        if (NEAR_ZERO(dt, ZERO_TOLERANCE))
            speedSoundReceiver(newVelocity);
        else
            speedSoundReceiver((newPos - oldPosition) / dt);
    }
}


const Obstacle* LocalPlayer::getHitBuilding(const glm::vec3 &oldP, float oldA,
        const glm::vec3 &p, float a,
        bool phased, bool& expelled)
{
    const bool hasOOflag = getFlag() == Flags::OscillationOverthruster;
    const float* dims = getDimensions();
    const Obstacle* obstacle = World::getWorld()->
                               hitBuilding(oldP, oldA, p, a, dims[0], dims[1], dims[2], !hasOOflag);

    expelled = (obstacle != NULL);
    if (expelled && phased)
        expelled = (obstacle->getType() == WallObstacle::getClassName() ||
                    obstacle->getType() == Teleporter::getClassName() ||
                    (hasOOflag && desiredSpeed < 0.0f && p[2] == 0.0f));

    if (obstacle != NULL)
    {
        if (obstacle->getType() == MeshFace::getClassName())
        {
            const MeshFace* face = (const MeshFace*) obstacle;
            const int driver = face->getPhysicsDriver();
            const PhysicsDriver* phydrv = PHYDRVMGR.getDriver(driver);
            if ((phydrv != NULL) && phydrv->getIsDeath())
                deathPhyDrv = driver;
        }
    }

    return obstacle;
}


bool LocalPlayer::getHitNormal(const Obstacle* o,
                               const glm::vec3 &pos1, float azimuth1,
                               const glm::vec3 &pos2, float azimuth2,
                               glm::vec3 &normal) const
{
    const float* dims = getDimensions();
    return o->getHitNormal(pos1, azimuth1, pos2, azimuth2,
                           dims[0], dims[1], dims[2], normal);
}


static bool notInObstacleList(const Obstacle* obs,
                              const std::vector<const Obstacle*>& list)
{
    for (unsigned int i = 0; i < list.size(); i++)
    {
        if (obs == list[i])
            return false;
    }
    return true;
}


void LocalPlayer::collectInsideBuildings()
{
    const auto &pos = getPosition();
    const float angle = getAngle();
    const float* dims = getDimensions();

    // get the list of possible inside buildings
    const ObsList* olist =
        COLLISIONMGR.boxTest (pos, angle, dims[0], dims[1], dims[2]);

    for (int i = 0; i < olist->count; i++)
    {
        const Obstacle* obs = olist->list[i];
        if (obs->inBox(pos, angle, dims[0], dims[1], dims[2]))
        {
            if (obs->getType() == MeshFace::getClassName())
            {
                const MeshFace* face = (const MeshFace*) obs;
                const MeshObstacle* mesh = (const MeshObstacle*) face->getMesh();
                // check it for the death touch
                if (deathPhyDrv < 0)
                {
                    const int driver = face->getPhysicsDriver();
                    const PhysicsDriver* phydrv = PHYDRVMGR.getDriver(driver);
                    if ((phydrv != NULL) && (phydrv->getIsDeath()))
                        deathPhyDrv = driver;
                }
                // add the mesh if not already present
                if (!obs->isDriveThrough() &&
                        notInObstacleList(mesh, insideBuildings))
                    insideBuildings.push_back(mesh);
            }
            else if (!obs->isDriveThrough())
            {
                if (obs->getType() == MeshObstacle::getClassName())
                {
                    // add the mesh if not already present
                    if (notInObstacleList(obs, insideBuildings))
                        insideBuildings.push_back(obs);
                }
                else
                    insideBuildings.push_back(obs);
            }
        }
    }

    return;
}


float           LocalPlayer::getFlagShakingTime() const
{
    return flagShakingTime;
}

int         LocalPlayer::getFlagShakingWins() const
{
    return flagShakingWins;
}

const glm::vec3 *LocalPlayer::getAntidoteLocation() const
{
    return antidoteFlag ? &antidoteFlag->getSphere() : NULL;
}

ShotPath*       LocalPlayer::getShot(int index) const
{
    index &= 0x00FF;
    if ((index < 0) || (index >= World::getWorld()->getMaxShots()))
        return NULL;
    return shots[index];
}

void LocalPlayer::restart(const glm::vec3 &pos, float _azimuth)
{
    // put me in limbo
    setStatus(short(PlayerState::DeadStatus));

    // can't have a flag
    setFlag(Flags::Null);

    // get rid of existing shots
    const int numShots = World::getWorld()->getMaxShots();
    // get rid of existing shots
    for (int i = 0; i < numShots; i++)
        if (shots[i])
        {
            delete shots[i];
            shots[i] = NULL;
        }
    anyShotActive = false;

    // no target
    target = NULL;

    // no death
    deathPhyDrv = -1;

    // initialize position/speed state
    static const auto zero = glm::vec3(0.0f);
    location = (pos[2] > 0.0f) ? OnBuilding : OnGround;
    lastObstacle = NULL;
    lastSpeed = 0.0f;
    desiredSpeed = 0.0f;
    desiredAngVel = 0.0f;
    move(pos, _azimuth);
    setVelocity(zero);
    setAngularVelocity(0.0f);
    left  = false;
    right = false;
    up    = false;
    down  = false;
    wantJump = false;
    doUpdateMotion(0.0f);
    entryDrop = true;

    // make me alive now
    setStatus(getStatus() | short(PlayerState::Alive));
}

void            LocalPlayer::setTeam(TeamColor _team)
{
    changeTeam(_team);
}

void            LocalPlayer::setDesiredSpeed(float fracOfMaxSpeed)
{
    FlagType* flag = getFlag();
    // can't go faster forward than at top speed, and backward at half speed
    if (fracOfMaxSpeed > 1.0f) fracOfMaxSpeed = 1.0f;
    else if (fracOfMaxSpeed < -0.5f) fracOfMaxSpeed = -0.5f;

    // oscillation overthruster tank in building can't back up
    if (fracOfMaxSpeed < 0.0f && getLocation() == InBuilding &&
            flag == Flags::OscillationOverthruster)
        fracOfMaxSpeed = 0.0f;

    // boost speed for certain flags
    if (flag == Flags::Velocity)
        fracOfMaxSpeed *= BZDB.eval(StateDatabase::BZDB_VELOCITYAD);
    else if (flag == Flags::Thief)
        fracOfMaxSpeed *= BZDB.eval(StateDatabase::BZDB_THIEFVELAD);
    else if ((flag == Flags::Burrow) && (getPosition()[2] < 0.0f))
        fracOfMaxSpeed *= BZDB.eval(StateDatabase::BZDB_BURROWSPEEDAD);
    else if ((flag == Flags::ForwardOnly) && (fracOfMaxSpeed < 0.0))
        fracOfMaxSpeed = 0.0f;
    else if ((flag == Flags::ReverseOnly) && (fracOfMaxSpeed > 0.0))
        fracOfMaxSpeed = 0.0f;
    else if (flag == Flags::Agility)
    {
        if ((TimeKeeper::getTick() - agilityTime) < BZDB.eval(StateDatabase::BZDB_AGILITYTIMEWINDOW))
            fracOfMaxSpeed *= BZDB.eval(StateDatabase::BZDB_AGILITYADVEL);
        else
        {
            float oldFrac = desiredSpeed / BZDBCache::tankSpeed;
            if (oldFrac > 1.0f)
                oldFrac = 1.0f;
            else if (oldFrac < -0.5f)
                oldFrac = -0.5f;
            float limit = BZDB.eval(StateDatabase::BZDB_AGILITYVELDELTA);
            if (fracOfMaxSpeed < 0.0f)
                limit /= 2.0f;
            if (fabs(fracOfMaxSpeed - oldFrac) > limit)
            {
                fracOfMaxSpeed *= BZDB.eval(StateDatabase::BZDB_AGILITYADVEL);
                agilityTime = TimeKeeper::getTick();
            }
        }
    }

    // apply handicap advantage to tank speed
    fracOfMaxSpeed *= (1.0f + (handicap * (BZDB.eval(StateDatabase::BZDB_HANDICAPVELAD) - 1.0f)));

    // set desired speed
    desiredSpeed = BZDBCache::tankSpeed * fracOfMaxSpeed;
    Player::setUserSpeed(desiredSpeed);

    return;
}


void            LocalPlayer::setDesiredAngVel(float fracOfMaxAngVel)
{
    FlagType* flag = getFlag();

    // limit turn speed to maximum
    if (fracOfMaxAngVel > 1.0f) fracOfMaxAngVel = 1.0f;
    else if (fracOfMaxAngVel < -1.0f) fracOfMaxAngVel = -1.0f;

    // further limit turn speed for certain flags
    if (fracOfMaxAngVel < 0.0f && getFlag() == Flags::LeftTurnOnly)
        fracOfMaxAngVel = 0.0f;
    else if (fracOfMaxAngVel > 0.0f && getFlag() == Flags::RightTurnOnly)
        fracOfMaxAngVel = 0.0f;

    // boost turn speed for other flags
    if (flag == Flags::QuickTurn)
        fracOfMaxAngVel *= BZDB.eval(StateDatabase::BZDB_ANGULARAD);
    else if ((flag == Flags::Burrow) && (getPosition()[2] < 0.0f))
        fracOfMaxAngVel *= BZDB.eval(StateDatabase::BZDB_BURROWANGULARAD);

    // apply handicap advantage to tank speed
    fracOfMaxAngVel *= (1.0f + (handicap * (BZDB.eval(StateDatabase::BZDB_HANDICAPANGAD) - 1.0f)));

    // set desired turn speed
    desiredAngVel = fracOfMaxAngVel * BZDB.eval(StateDatabase::BZDB_TANKANGVEL);
    Player::setUserAngVel(desiredAngVel);

    return;
}


void            LocalPlayer::setPause(bool pause)
{
    if (isAlive())
    {
        if (pause && !isPaused())
        {
            setStatus(getStatus() | short(PlayerState::Paused));
            server->sendPaused(true);
        }
        else if (!pause && isPaused())
        {
            setStatus(getStatus() & ~short(PlayerState::Paused));
            server->sendPaused(false);
        }
    }
}

void            LocalPlayer::activateAutoPilot(bool autopilot)
{
    // If desired and actual state is the same, ignore the request
    if (autopilot == isAutoPilot())
        return;

    setAutoPilot(autopilot);
    server->sendAutoPilot(autopilot);
    if (!autopilot)
        setTarget(NULL);
}

bool            LocalPlayer::fireShot()
{
    if (! (firingStatus == Ready || firingStatus == Zoned))
        return false;

    // find an empty slot
    const int numShots = World::getWorld()->getMaxShots();
    int i;
    for (i = 0; i < numShots; i++)
        if (!shots[i])
            break;
    if (i == numShots) return false;

    // make sure we're allowed to shoot
    if (!isAlive() || isPaused() ||
            ((location == InBuilding) && !isPhantomZoned()))
        return false;

    // prepare shot
    FiringInfo firingInfo(*this, i + getSalt());
    // FIXME team coloring of shot is never used; it was meant to be used
    // for rabbit mode to correctly calculate team kills when rabbit changes
    firingInfo.shot.team = getTeam();
    if (firingInfo.flagType == Flags::ShockWave)
    {
        // move shot origin under tank and make it stationary
        const auto &pos = getPosition();
        firingInfo.shot.pos = pos;
        firingInfo.shot.vel = glm::vec3(0.0f);
    }
    else
    {
        // apply any handicap advantage to shot speed
        if (handicap > 0.0f)
        {
            const float speedAd = 1.0f + (handicap * (BZDB.eval(StateDatabase::BZDB_HANDICAPSHOTAD) - 1.0f));
            auto dir = getForward();
            dir     *= speedAd * BZDB.eval(StateDatabase::BZDB_SHOTSPEED);

            firingInfo.shot.vel = getVelocity();
            firingInfo.shot.vel[0] += dir[0];
            firingInfo.shot.vel[1] += dir[1];
        }
        // Set _shotsKeepVerticalVelocity on the server if you want shots
        // to have the same vertical velocity as the tank when fired.
        // keeping shots moving horizontally makes the game more playable.
        if (!BZDB.isTrue(StateDatabase::BZDB_SHOTSKEEPVERTICALV)) firingInfo.shot.vel[2] = 0.0f;
    }

    // make shot and put it in the table
    shots[i] = new LocalShotPath(firingInfo);

    // Insert timestamp, useful for dead reckoning jitter fixing
    // TODO should maybe use getTick() instead? must double check
    const float timeStamp = float(TimeKeeper::getCurrent() - TimeKeeper::getNullTime());
    firingInfo.timeSent = timeStamp;

    // always send a player-update message. To synchronize movement and
    // shot start. They should generally travel on the same frame, when
    // flushing the output queues.
    server->sendPlayerUpdate(this);
    server->sendBeginShot(firingInfo);

    if (BZDB.isTrue("enableLocalShotEffect") && SceneRenderer::instance().useQuality() >= 2)
        EFFECTS.addShotEffect(getColor(), firingInfo.shot.pos, getAngle(), getVelocity());

    if (gettingSound)
    {
        if (firingInfo.flagType == Flags::ShockWave)
        {
            playLocalSound(SFX_SHOCK);
            ForceFeedback::shockwaveFired();
        }
        else if (firingInfo.flagType == Flags::Laser)
        {
            playLocalSound(SFX_LASER);
            ForceFeedback::laserFired();
        }
        else if (firingInfo.flagType == Flags::GuidedMissile)
        {
            playLocalSound(SFX_MISSILE);
            ForceFeedback::shotFired();
        }
        else if (firingInfo.flagType == Flags::Thief)
        {
            playLocalSound(SFX_THIEF);
            ForceFeedback::shotFired();
        }
        else
        {
            playLocalSound(SFX_FIRE);
            ForceFeedback::shotFired();
        }
    }

    shotStatistics.recordFire(firingInfo.flagType,getForward(),firingInfo.shot.vel);

    if (getFlag() == Flags::TriggerHappy)
    {
        // make sure all the shots don't go off at once
        forceReload(BZDB.eval(StateDatabase::BZDB_RELOADTIME) / numShots);
    }
    return true;
}

float           LocalPlayer::getReloadTime() const
{
    const int numShots = World::getWorld()->getMaxShots();
    if (numShots <= 0)
        return 0.0f;

    float time = float(jamTime - TimeKeeper::getTick());
    if (time > 0.0f)
        return time;

    // look for an empty slot
    int i;
    for (i = 0; i < numShots; i++)
    {
        if (!shots[i])
            return 0.0f;
    }

    // look for the shot fired least recently
    float minTime = float(shots[0]->getReloadTime() -
                          (shots[0]->getCurrentTime() - shots[0]->getStartTime()));
    for (i = 1; i < numShots; i++)
    {
        const float t = float(shots[i]->getReloadTime() -
                              (shots[i]->getCurrentTime() - shots[i]->getStartTime()));
        if (t < minTime)
            minTime = t;
    }

    if (minTime < 0.0f)
        minTime = 0.0f;

    return minTime;
}

bool LocalPlayer::doEndShot(int ident, bool isHit, glm::vec3 &pos)
{
    const int index = ident & 255;
    const int slt   = (ident >> 8) & 127;

    // special id used in some messages (and really shouldn't be sent here)
    if (ident == -1)
        return false;

    // ignore bogus shots (those with a bad index or for shots that don't exist)
    if (index < 0 || index >= World::getWorld()->getMaxShots() || !shots[index])
        return false;

    // ignore shots that already ending
    if (shots[index]->isExpired() || shots[index]->isExpiring())
        return false;

    // ignore shots that have the wrong salt.  since we reuse shot indices
    // it's possible for an old MsgShotEnd to arrive after we've started a
    // new shot.  that's where the salt comes in.  it changes for each shot
    // so we can identify an old shot from a new one.
    if (slt != ((shots[index]->getShotId() >> 8) & 127))
        return false;

    // keep shot statistics
    shotStatistics.recordHit(shots[index]->getFlag());

    // don't stop if it's because were hitting something and we don't stop
    // when we hit something.
    if (isHit && !shots[index]->isStoppedByHit())
        return false;

    // end it
    const auto &shotPos = shots[index]->getPosition();
    pos = shotPos;
    shots[index]->setExpired();
    return true;
}

void            LocalPlayer::setJump()
{
    wantJump = jumpPressed;
}

void            LocalPlayer::setJumpPressed(bool value)
{
    jumpPressed = value;
}

void            LocalPlayer::doJump()
{
    FlagType* flag = getFlag();

    // can't jump while burrowed
    if (getPosition()[2] < 0.0f)
        return;

    if (flag == Flags::Wings)
    {
        if (wingsFlapCount <= 0)
            return;
        wingsFlapCount--;
    }
    else if ((location != OnGround) && (location != OnBuilding))
    {
        // can't jump unless on the ground or a building
        if (flag != Flags::Wings)
            return;
        if (wingsFlapCount <= 0)
            return;
        wingsFlapCount--;
    }
    else if ((flag != Flags::Bouncy) &&
             ((flag != Flags::Jumping && !World::getWorld()->allowJumping()) ||
              (flag == Flags::NoJumping)))
        return;

    // jump velocity
    const auto &oldVelocity = getVelocity();
    glm::vec3 newVelocity;
    newVelocity[0] = oldVelocity[0];
    newVelocity[1] = oldVelocity[1];
    if (flag == Flags::Wings)
    {
        newVelocity[2] = BZDB.eval(StateDatabase::BZDB_WINGSJUMPVELOCITY);
        // if you're falling, wings will just slow you down
        if (oldVelocity[2] < 0)
        {
            newVelocity[2] += oldVelocity[2];
            // if you're already going up faster, just keep doing that
        }
        else if (oldVelocity[2] > newVelocity[2])
            newVelocity[2] = oldVelocity[2];
    }
    else if (flag == Flags::Bouncy)
    {
        const float factor = 0.25f + ((float)bzfrand() * 0.75f);
        newVelocity[2] = factor * BZDB.eval(StateDatabase::BZDB_JUMPVELOCITY);
    }
    else
        newVelocity[2] = BZDB.eval(StateDatabase::BZDB_JUMPVELOCITY);
    setVelocity(newVelocity);
    location = InAir;

    // setup the graphics
    fireJumpJets();

    // setup the sound
    if (gettingSound)
    {
        if (flag == Flags::Wings)
        {
            playLocalSound(SFX_FLAP);
            addRemoteSound(PlayerState::WingsSound);
        }
        else
        {
            playLocalSound(SFX_JUMP);
            addRemoteSound(PlayerState::JumpSound);
        }
    }

    wantJump = false;
}

void            LocalPlayer::setTarget(const Player* _target)
{
    target = _target;
}

void            LocalPlayer::setNemesis(const Player* _nemesis)
{
    if ((_nemesis == NULL) || _nemesis->getPlayerType() == TankPlayer)
        nemesis = _nemesis;
}

void            LocalPlayer::setRecipient(const Player* _recipient)
{
    if ((_recipient == NULL) || (_recipient->getId() <= LastRealPlayer))
        recipient = _recipient;
}

void            LocalPlayer::explodeTank()
{
    if (location == Dead || location == Exploding) return;
    float gravity      = BZDBCache::gravity;
    float explodeTim   = BZDB.eval(StateDatabase::BZDB_EXPLODETIME);
    setExplodePos(getPosition());
    // Limiting max height increment to this value (the old default value)
    const float zMax  = 49.0f;
    setExplode(TimeKeeper::getTick());
    const auto &oldVelocity = getVelocity();
    auto newVelocity = oldVelocity;
    if (gravity < 0)
    {
        // comparing 2 speed:
        //   to have a symmetric path (ending at same height as starting)
        //   to reach the apex of parabola, under the max height established
        // take the less
        newVelocity[2] = - 0.5f * gravity * explodeTim;
        float maxSpeed = sqrtf(- 2.0f * zMax * gravity);
        if (newVelocity[2] > maxSpeed)
            newVelocity[2] = maxSpeed;
    }
    TankDeathOverride* death = getDeathEffect();
    if (death)
    {
        auto v = newVelocity;
        if (death->GetDeathVector(v))
            newVelocity = v;
    }
    setVelocity(newVelocity);
    location = Exploding;
    target = NULL;        // lose lock when dead
}

void            LocalPlayer::doMomentum(float dt,
                                        float& speed, float& angVel)
{
    // get maximum linear and angular accelerations
    float linearAcc = (getFlag() == Flags::Momentum) ? BZDB.eval(StateDatabase::BZDB_MOMENTUMLINACC) :
                      World::getWorld()->getLinearAcceleration();
    float angularAcc = (getFlag() == Flags::Momentum) ? BZDB.eval(StateDatabase::BZDB_MOMENTUMANGACC) :
                       World::getWorld()->getAngularAcceleration();

    // limit linear acceleration
    if (linearAcc > 0.0f)
    {
        const float acc = (speed - lastSpeed) / dt;
        if (acc > 20.0f * linearAcc) speed = lastSpeed + dt * 20.0f*linearAcc;
        else if (acc < -20.0f * linearAcc) speed = lastSpeed - dt * 20.0f*linearAcc;
    }

    // limit angular acceleration
    if (angularAcc > 0.0f)
    {
        const float oldAngVel = getAngularVelocity();
        const float angAcc = (angVel - oldAngVel) / dt;
        if (angAcc > angularAcc) angVel = oldAngVel + dt * angularAcc;
        else if (angAcc < -angularAcc) angVel = oldAngVel - dt * angularAcc;
    }
}

void LocalPlayer::doFriction(float dt,
                             const glm::vec2 &oldVelocity,
                             glm::vec3 &newVelocity)
{
    const float friction = (getFlag() == Flags::Momentum) ? BZDB.eval(StateDatabase::BZDB_MOMENTUMFRICTION) :
                           BZDB.eval(StateDatabase::BZDB_FRICTION);

    if (friction > 0.0f)
    {
        // limit vector acceleration

        auto delta = glm::vec2(newVelocity) - oldVelocity;
        float acc2 = glm::length2(delta);
        float accLimit = 20.0f * friction * dt;

        if (acc2 > accLimit*accLimit)
        {
            float ratio = accLimit / sqrtf(acc2);
            newVelocity = glm::vec3(oldVelocity + delta * ratio, 0.0f);
        }
    }
}

void            LocalPlayer::doForces(float UNUSED(dt),
                                      glm::vec3 &UNUSED(velocity),
                                      float& UNUSED(angVel))
{
    // apply external forces
    // do nothing -- no external forces right now
}

// NOTE -- minTime should be initialized to Infinity by the caller
bool            LocalPlayer::checkHit(const Player* source,
                                      const ShotPath*& hit, float& minTime) const
{
    bool goodHit = false;

    // if firing tank is paused then it doesn't count
    if (source->isPaused()) return goodHit;

    const int maxShots = source->getMaxShots();
    for (int i = 0; i < maxShots; i++)
    {
        // get shot
        const ShotPath* shot = source->getShot(i);
        if (!shot || shot->isExpired()) continue;

        // my own shock wave cannot kill me
        if (source == this && ((shot->getFlag() == Flags::ShockWave) || (shot->getFlag() == Flags::Thief))) continue;

        // if no team kills, shots of my team cannot kill me. Thief can still take
        // a teammate's flag.
        if (getTeam() != RogueTeam && !World::getWorld()->allowTeamKills() &&
                shot->getFlag() != Flags::Thief && shot->getTeam() == getTeam() &&
                source != this) continue;

        // short circuit test if shot can't possibly hit.
        // only superbullet or shockwave can kill zoned dude
        const FlagType* shotFlag = shot->getFlag();
        if (isPhantomZoned() &&
                (shotFlag != Flags::ShockWave) &&
                (shotFlag != Flags::SuperBullet) &&
                (shotFlag != Flags::PhantomZone))
            continue;

        // laser can't hit a cloaked tank
        if ((getFlag() == Flags::Cloaking) && (shotFlag == Flags::Laser))
            continue;

        // zoned shots only kill zoned tanks
        if ((shotFlag == Flags::PhantomZone) && !isPhantomZoned())
            continue;

        // test myself against shot
        glm::vec3 position;
        const float t = shot->checkHit(this, position);
        if (t >= minTime) continue;

        // test if shot hit a part of my tank that's through a teleporter.
        // hit is no good if hit point is behind crossing plane.
        if (isCrossingWall() &&
                (glm::dot(position, glm::vec3(crossingPlane)) + crossingPlane[3]
                 < 0.0))
            continue;

        // okay, shot hit
        goodHit = true;
        hit = shot;
        minTime = t;
    }
    return goodHit;
}

void            LocalPlayer::setFlag(FlagType* flag)
{
    Player::setFlag(flag);

    float worldSize = BZDBCache::worldSize;
    // if it's bad then reset countdowns and set antidote flag
    if (getFlag() != Flags::Null && getFlag()->endurance == FlagSticky)
    {
        if (World::getWorld()->allowShakeTimeout())
            flagShakingTime = World::getWorld()->getFlagShakeTimeout();
        if (World::getWorld()->allowShakeWins())
            flagShakingWins = World::getWorld()->getFlagShakeWins();
        if (World::getWorld()->allowAntidote())
        {
            float tankRadius = BZDBCache::tankRadius;
            float baseSize = BZDB.eval(StateDatabase::BZDB_BASESIZE);
            int tryCount = 0;
            do
            {
                tryCount++;
                if (tryCount > 100) // if it takes this long, just screw it.
                    break;

                if (World::getWorld()->allowTeamFlags())
                {
                    flagAntidotePos[0] = 0.5f * worldSize * ((float)bzfrand() - 0.5f);
                    flagAntidotePos[1] = 0.5f * worldSize * ((float)bzfrand() - 0.5f);
                    flagAntidotePos[2] = 0.0f;
                }
                else
                {
                    flagAntidotePos[0] = (worldSize - baseSize) * ((float)bzfrand() - 0.5f);
                    flagAntidotePos[1] = (worldSize - baseSize) * ((float)bzfrand() - 0.5f);
                    flagAntidotePos[2] = 0.0f;
                }
            }
            while (World::getWorld()->inBuilding(flagAntidotePos, tankRadius,
                                                 BZDBCache::tankHeight));
            antidoteFlag = new FlagSceneNode(flagAntidotePos);
            antidoteFlag->setColor(1.0f, 1.0f, 0.0f);
            World::setFlagTexture(antidoteFlag);
        }
    }
    else
    {
        delete antidoteFlag;
        antidoteFlag = NULL;
        flagShakingTime = 0.0f;
        flagShakingWins = 0;
    }
}

void            LocalPlayer::changeScore(short deltaWins,
        short deltaLosses,
        short deltaTks)
{
    Player::changeScore(deltaWins, deltaLosses, deltaTks);
    if (deltaWins > 0 && World::getWorld()->allowShakeWins() &&
            flagShakingWins > 0)
    {
        flagShakingWins -= deltaWins;
        if (flagShakingWins <= 0)
        {
            flagShakingWins = 0;
            server->sendDropFlag(getPosition());
        }
    }
}

void            LocalPlayer::addAntidote(SceneDatabase* scene)
{
    if (antidoteFlag)
        scene->addDynamicNode(antidoteFlag);
}

std::string     LocalPlayer::getInputMethodName(InputMethod whatInput)
{
    switch (whatInput)
    {
    case Keyboard:
        return std::string("Keyboard");
        break;
    case Mouse:
        return std::string("Mouse");
        break;
    case Joystick:
        return std::string("Joystick");
        break;
    default:
        return std::string("Unnamed Device");
    }
}

void LocalPlayer::setKey(int button, bool pressed)
{
    switch (button)
    {
    case BzfKeyEvent::Left:
        left = pressed;
        break;
    case BzfKeyEvent::Right:
        right = pressed;
        break;
    case BzfKeyEvent::Up:
        up = pressed;
        break;
    case BzfKeyEvent::Down:
        down = pressed;
        break;
    }
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
