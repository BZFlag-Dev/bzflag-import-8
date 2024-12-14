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

#ifndef __RANDOMSPAWNPOLICY_H__
#define __RANDOMSPAWNPOLICY_H__

#include "common.h"

/* common interface headers */
#include "SpawnPolicy.h"


/** a RandomSpawnPolicy is a SpawnPolicy that just generates a purely
 *  random (yet hopefully still valid) position and drops you there.
 */
class RandomSpawnPolicy : public SpawnPolicy
{

public:
    RandomSpawnPolicy();
    virtual ~RandomSpawnPolicy();

    void getPosition(glm::vec3 &pos, int playerId, bool onGroundOnly, bool notNearEdges) override;
    void getAzimuth(float &azimuth) override;
};

#endif  /*__RANDOMSPAWNPOLICY_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
