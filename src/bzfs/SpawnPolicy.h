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

#ifndef __SPAWNPOLICY_H__
#define __SPAWNPOLICY_H__

#include "common.h"

// System headers
#include <glm/vec3.hpp>

/* common interface headers */
#include "global.h"  /* for TeamColor */


/** a SpawnPolicy is used to determine a new SpawnPosition.  Policies
 *  are defined to describe various spawning behaviors such as purely
 *  random, aggressive, or classical behavior.  Factors that can be
 *  taken into account are the proximity to other players, bullets,
 *  flags, etc.
 */
class SpawnPolicy
{

public:
    SpawnPolicy();
    virtual ~SpawnPolicy();

    virtual void getPosition(glm::vec3 &pos, int playerId, bool onGroundOnly, bool notNearEdges);
    virtual void getAzimuth(float &azimuth);

protected:
    virtual bool isImminentlyDangerous() const;

private:
    float enemyProximityCheck(float &enemyAngle) const;
    float distanceFrom(const glm::vec3 &farPos) const;
    bool  isFacing(const glm::vec3 &enemyPos, const float enemyAzimuth, const float deviation) const;

    /* temp, internal use */
    TeamColor   team;
    glm::vec3   testPos;

    float safeSWRadius;
    float safeSRRadius;
    float safeDistance;
};

#endif  /*__SPAWNPOLICY_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
