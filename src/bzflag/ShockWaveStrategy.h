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

#ifndef __SHOCKWAVESTRATEGY_H__
#define __SHOCKWAVESTRATEGY_H__

/* interface header */
#include "ShotStrategy.h"

/* common interface headers */
#include "SceneDatabase.h"
#include "SphereSceneNode.h"

/* local interface headers */
#include "BaseLocalPlayer.h"
#include "ShotPath.h"


class ShockWaveStrategy : public ShotStrategy
{
public:
    ShockWaveStrategy(ShotPath*);
    ~ShockWaveStrategy();

    void        update(float dt);
    float       checkHit(const BaseLocalPlayer*, glm::vec3 &) const;
    bool        isStoppedByHit() const;
    void        addShot(SceneDatabase*, bool colorblind);
    void        radarRender() const;

private:
    SphereSceneNode*    shockNode;
    float       radius;
    float       radius2;
    TeamColor       team;
};


#endif /* __SHOCKWAVESTRATEGY_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
