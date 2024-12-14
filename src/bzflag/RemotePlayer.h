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

#ifndef __REMOTEPLAYER_H__
#define __REMOTEPLAYER_H__

/* interface header */
#include "Player.h"

/* common interface headers */
#include "global.h"
#include "ShotUpdate.h"

/* local interface headers */
#include "ShotPath.h"


class RemotePlayer : public Player
{
public:
    RemotePlayer(const PlayerId&, TeamColor team,
                 const char* name, const char* motto,
                 const PlayerType);
    ~RemotePlayer();

    void addShot(const FiringInfo&);
    ShotPath* getShot(int index) const override;
    void updateShots(float dt);

private:
    bool doEndShot(int index, bool isHit, glm::vec3 &pos) override;

private:
    int numShots;
    RemoteShotPath** shots;
};


#endif /* __REMOTE_PLAYER_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
