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

#ifndef __GUIDEDMISSLESTRATEGY_H__
#define __GUIDEDMISSLESTRATEGY_H__

/* interface header */
#include "ShotStrategy.h"

/* system interface headers */
#include <vector>
#include <glm/vec3.hpp>

/* common interface headers */
#include "SceneDatabase.h"
#include "TimeKeeper.h"
#include "BoltSceneNode.h"

/* local interface headers */
#include "BaseLocalPlayer.h"
#include "ShotPathSegment.h"


class GuidedMissileStrategy : public ShotStrategy
{
public:
    GuidedMissileStrategy(ShotPath*);
    ~GuidedMissileStrategy();

    void        update(float dt);
    float       checkHit(const BaseLocalPlayer*, glm::vec3 &) const;
    void        sendUpdate(const FiringInfo&) const;
    void        readUpdate(uint16_t, const void*);
    void        addShot(SceneDatabase*, bool colorblind);
    void        expire();
    void        radarRender() const;

private:
    float       checkBuildings(const Ray& ray);

private:
    TimeKeeper      prevTime;
    TimeKeeper      currentTime;
    std::vector<ShotPathSegment>    segments;
    int         renderTimes;
    float       azimuth;
    float       elevation;
    glm::vec3   nextPos;
    BoltSceneNode*  ptSceneNode;

    float   puffTime,rootPuff;
    TimeKeeper lastPuff;
    mutable bool    needUpdate;
    PlayerId        lastTarget;
};


#endif /* __GUIDEDMISSLESTRATEGY_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
