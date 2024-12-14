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

// interface header
#include "ShotPath.h"

/* system headers */
#include <assert.h>

/* local implementation headers */
#include "SegmentedShotStrategy.h"
#include "GuidedMissleStrategy.h"
#include "ShockWaveStrategy.h"


//
// FiringInfo (with BaseLocalPlayer)
//

FiringInfo::FiringInfo(const BaseLocalPlayer& tank, int id)
{
    shot.player = tank.getId();
    shot.id = uint16_t(id);
    shot.pos = tank.getMuzzle();
    const auto &dir = tank.getForward();
    const auto &tankVel = tank.getVelocity();
    float shotSpeed = BZDB.eval(StateDatabase::BZDB_SHOTSPEED);
    shot.vel = tankVel;
    shot.vel[0] += shotSpeed * dir[0];
    shot.vel[1] += shotSpeed * dir[1];
    shot.dt = 0.0f;

    flagType = tank.getFlag();
    // wee bit o hack -- if phantom flag but not phantomized
    // the shot flag is normal -- otherwise FiringInfo will have
    // to be changed to add a real bitwise status variable
    if (tank.getFlag() == Flags::PhantomZone && !tank.isFlagActive())
        flagType = Flags::Null;
    lifetime = BZDB.eval(StateDatabase::BZDB_RELOADTIME);
}

//
// ShotPath
//

ShotPath::ShotPath(const FiringInfo& info) :
    firingInfo(info),
    reloadTime(BZDB.eval(StateDatabase::BZDB_RELOADTIME)),
    startTime(TimeKeeper::getTick()),
    currentTime(TimeKeeper::getTick()),
    expiring(false),
    expired(false)
{
    // eek!  a giant switch statement, how un-object-oriented!
    // each flag should be a flyweight object derived from a
    // base Flag class with a virtual makeShotStrategy() member.
    // just remember -- it's only a game.
    if (firingInfo.flagType->flagShot == NormalShot)
        strategy = new NormalShotStrategy(this);
    else
    {
        if (firingInfo.flagType == Flags::RapidFire)
            strategy = new RapidFireStrategy(this);
        else if (firingInfo.flagType == Flags::MachineGun)
            strategy = new MachineGunStrategy(this);
        else if (firingInfo.flagType == Flags::GuidedMissile)
            strategy = new GuidedMissileStrategy(this);
        else if (firingInfo.flagType == Flags::Laser)
            strategy = new LaserStrategy(this);
        else if (firingInfo.flagType == Flags::Ricochet)
            strategy = new RicochetStrategy(this);
        else if (firingInfo.flagType == Flags::SuperBullet)
            strategy = new SuperBulletStrategy(this);
        else if (firingInfo.flagType == Flags::ShockWave)
            strategy = new ShockWaveStrategy(this);
        else if (firingInfo.flagType == Flags::Thief)
            strategy = new ThiefStrategy(this);
        else if (firingInfo.flagType == Flags::PhantomZone)
            strategy = new PhantomBulletStrategy(this);
        else
            assert(0);    // shouldn't happen
    }
}

ShotPath::~ShotPath()
{
    delete strategy;
}

float           ShotPath::checkHit(const BaseLocalPlayer* player,
                                   glm::vec3 &position) const
{
    return strategy->checkHit(player, position);
}

bool            ShotPath::isStoppedByHit() const
{
    return strategy->isStoppedByHit();
}

void            ShotPath::addShot(SceneDatabase* scene,
                                  bool colorblind)
{
    strategy->addShot(scene, colorblind);
}

void            ShotPath::radarRender() const
{
    if (!isExpired()) strategy->radarRender();
}

void            ShotPath::updateShot(float dt)
{
    // get new time step and set current time
    currentTime += dt;

    // update shot
    if (!expired)
    {
        if (expiring) setExpired();
        else getStrategy()->update(dt);
    }
}

void            ShotPath::setReloadTime(float _reloadTime)
{
    reloadTime = _reloadTime;
}

void            ShotPath::setPosition(const glm::vec3 &p)
{
    firingInfo.shot.pos = p;
}

void            ShotPath::setVelocity(const glm::vec3 &v)
{
    firingInfo.shot.vel = v;
}

void            ShotPath::setExpiring()
{
    expiring = true;
}

void            ShotPath::setExpired()
{
    expiring = true;
    expired = true;
    getStrategy()->expire();
}

void            ShotPath::boostReloadTime(float dt)
{
    reloadTime += dt;
}

//
// LocalShotPath
//

LocalShotPath::LocalShotPath(const FiringInfo& info) :
    ShotPath(info)
{
    // do nothing
}

LocalShotPath::~LocalShotPath()
{
    // do nothing
}

void            LocalShotPath::update(float dt)
{
    getFiringInfo().shot.dt += dt;
    updateShot(dt);

    // send updates if necessary
    getStrategy()->sendUpdate(getFiringInfo());
}

//
// RemoteShotPath
//

RemoteShotPath::RemoteShotPath(const FiringInfo& info) :
    ShotPath(info)
{
    // do nothing
}

RemoteShotPath::~RemoteShotPath()
{
    // do nothing
}

void            RemoteShotPath::update(float dt)
{
    // update shot
    updateShot(dt);
}

void            RemoteShotPath::update(const ShotUpdate& shot,
                                       uint16_t code, const void* msg)
{
    // update shot info
    getFiringInfo().shot = shot;

    // let the strategy see the message
    getStrategy()->readUpdate(code, msg);
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
