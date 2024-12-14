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

#ifndef BZF_WORLD_PLAYER_H
#define BZF_WORLD_PLAYER_H

#include "common.h"
#include "Player.h"
#include "ShotPath.h"
#include <vector>

class WorldPlayer : public Player
{
public:
    WorldPlayer();
    ~WorldPlayer();

    void        addShot(const FiringInfo&);
    ShotPath*   getShot(int index) const override;
    void        updateShots(float dt);
    int         getMaxShots() const override;
    void        addShots(SceneDatabase* scene, bool colorblind) const override;

private:
    bool        doEndShot(int index, bool isHit, glm::vec3 &pos) override;

private:
    std::vector<RemoteShotPath*> shots;
};

inline int WorldPlayer::getMaxShots() const
{
    return shots.size();
}

#endif // BZF_WORLD_PLAYER_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
