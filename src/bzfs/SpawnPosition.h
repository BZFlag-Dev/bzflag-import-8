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

// object that creates and contains a spawn position

#ifndef __SPAWNPOSITION_H__
#define __SPAWNPOSITION_H__

#include "common.h"

// System headers
#include <glm/vec3.hpp>

/* local interface headers */
#include "SpawnPolicy.h"


/** SpawnPosition represents a single spawn position and encapsulates
 *  the logic for selecting that new position based on the map, world
 *  configuration settings, and current game activity.
 */
class SpawnPosition
{

public:
    SpawnPosition(int playerId, bool onGroundOnly, bool notNearEdges);
    ~SpawnPosition();

    /** used to override the default spawn policy.  call this before
     *  creating SpawnPosition objects.
     */
    static void setSpawnPolicy(SpawnPolicy* policy);

    float getX() const;
    float getY() const;
    float getZ() const;
    float getAzimuth() const;

private:
    float       azimuth;
    glm::vec3   pos;

    /* class data - determines how the pos and azimuth are determined */
    static SpawnPolicy *policy;
};

inline float SpawnPosition::getX() const
{
    return pos[0];
}

inline float SpawnPosition::getY() const
{
    return pos[1];
}

inline float SpawnPosition::getZ() const
{
    return pos[2];
}

/** returns the rotational orientation of this spawn position, from
 *  facing 'North' in radians.
 */
inline float SpawnPosition::getAzimuth() const
{
    return azimuth;
}

#else
class SpawnPosition;
#endif  /* __SPAWNPOSITION_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
