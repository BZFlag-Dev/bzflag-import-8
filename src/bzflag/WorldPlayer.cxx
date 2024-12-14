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
#include "WorldPlayer.h"

WorldPlayer::WorldPlayer() :
    Player(ServerPlayer, RogueTeam, "world weapon", "", ComputerPlayer)
{
}

WorldPlayer::~WorldPlayer()
{
    for (std::vector<RemoteShotPath*>::iterator it = shots.begin(); it != shots.end(); ++it)
    {
        RemoteShotPath *shot = *it;
        delete shot;
    }

    shots.clear();
}

void            WorldPlayer::addShot(const FiringInfo& info)
{
    RemoteShotPath* newShot = new RemoteShotPath(info);
    int shotNum = int(newShot->getShotId() & 255);
    if (shotNum >= (int)shots.size())
        shots.resize(shotNum+1);
    else
    {
        if (shots[shotNum] != NULL)
            delete shots[shotNum];
    }
    shots[shotNum] = newShot;
}

ShotPath*       WorldPlayer::getShot(int index) const
{
    if ((index & 255) >= (int)shots.size()) return NULL;
    return shots[index & 255];
}

bool            WorldPlayer::doEndShot(
    int ident, bool isHit, glm::vec3 &pos)
{
    const int index = ident & 255;
    const int salt = (ident >> 8) & 127;

    // special id used in some messages (and really shouldn't be sent here)
    if (ident == -1)
        return false;

    // ignore bogus shots (those with a bad index or for shots that don't exist)
    if (index < 0 || index >= (int)shots.size() || !shots[index])
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

void            WorldPlayer::updateShots(float dt)
{
    for (int i = 0; i < (int)shots.size(); i++)
        if (shots[i])
            shots[i]->update(dt);
}

void            WorldPlayer::addShots(SceneDatabase* scene,
                                      bool colorblind) const
{
    const int count = shots.size();
    for (int i = 0; i < count; i++)
    {
        ShotPath* shot = getShot(i);
        if (shot && !shot->isExpiring() && !shot->isExpired())
            shot->addShot(scene, colorblind);
    }
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
