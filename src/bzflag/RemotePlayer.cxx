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
#include "RemotePlayer.h"

/* common implementation headers */
#include "World.h"


RemotePlayer::RemotePlayer(const PlayerId& _id, TeamColor _team,
                           const char* _name, const char* _motto,
                           const PlayerType _type) :
    Player(_id, _team, _name, _motto, _type)
{
    numShots = World::getWorld()->getMaxShots();
    shots = new RemoteShotPath*[numShots];
    for (int i = 0; i < numShots; i++)
        shots[i] = NULL;
}

RemotePlayer::~RemotePlayer()
{
    for (int i = 0; i < numShots; i++)
        delete shots[i];
    delete[] shots;
}

void            RemotePlayer::addShot(const FiringInfo& info)
{
    glm::vec3 newpos;
    const auto &f = getForward();
    RemoteShotPath* newShot = new RemoteShotPath(info);
    int shotNum = int(newShot->getShotId() & 255);

    if (shotNum >= numShots)
        return;
    if (shots[shotNum]) delete shots[shotNum];
    shots[shotNum] = newShot;
    // Update tanks position and set dead reckoning for better lag handling
    // shot origin is center of tank for shockwave
    newpos = info.shot.pos;
    if (info.flagType == Flags::ShockWave)
    {
    }
    // shot origin is muzzle for other shots
    else
    {
        float front = BZDB.eval(StateDatabase::BZDB_MUZZLEFRONT);
        if (info.flagType == Flags::Obesity) front *= BZDB.eval(StateDatabase::BZDB_OBESEFACTOR);
        else if (info.flagType == Flags::Tiny) front *= BZDB.eval(StateDatabase::BZDB_TINYFACTOR);
        else if (info.flagType == Flags::Thief) front *= BZDB.eval(StateDatabase::BZDB_THIEFTINYFACTOR);
        newpos[0] -= front * f[0];
        newpos[1] -= front * f[1];
        newpos[2] -= BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
    }
    shotStatistics.recordFire(info.flagType,f,info.shot.vel);
    // FIXME - with dynamic dimensions, this may not be a good idea
    //     (flag each shot with a 'default dimensions' state?)
    move(newpos, getAngle());
    // FIXME - timestamp of shots are handled slightly different than for
    //     tank position updates, better so ignore them for now
    //     detail: settick | shot: current() | move | draw | tankpos: tick()
    //setDeadReckoning(info.timeSent);
    setDeadReckoning(-1.0f);
}

ShotPath*       RemotePlayer::getShot(int index) const
{
    index &= 0x00FF;
    if ((index < 0) || (index >= World::getWorld()->getMaxShots()))
        return NULL;
    return shots[index];
}

bool            RemotePlayer::doEndShot(
    int ident, bool isHit, glm::vec3 &pos)
{
    const int index = ident & 255;
    const int salt = (ident >> 8) & 127;

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
    if (salt != ((shots[index]->getShotId() >> 8) & 127))
        return false;

    // keep statistics
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

void            RemotePlayer::updateShots(float dt)
{
    for (int i = 0; i < numShots; i++)
        if (shots[i])
            shots[i]->update(dt);
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
