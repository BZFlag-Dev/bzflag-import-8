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
#include "RoofTops.h"

// local interface headers
#include "Obstacle.h"
#include "Ray.h"
#include "CollisionManager.h"
#include "Teleporter.h"


/****************************************************************************/

// NOTE - this could also be used with a 'rain on windshield' effect

// FIXME - this should use a pregenerated list of 3D polygons
//       - use something like a split quadtree  (16 divs top layer, etc...)
//       - splitting polygons is only helpful if they break into diff cells
//   - return the isFlatTop() state?  (for making puddles)
//   - return the plane normal?  (also for making puddles)
//       - check the hit position for under-the-wall leaks?

void RoofTops::load()
{
    return;
}


void RoofTops::clear()
{
    return;
}


float RoofTops::getTopHeight(float x, float y, float maxHeight)
{
    const float zSpeed = -1.0f;

    // setup the test ray
    const auto dir = glm::vec3(0.0f, 0.0f, zSpeed);
    const auto org = glm::vec3(x, y, maxHeight);
    Ray ray(org, dir);

    // get the obstacle list
    const ObsList* olist = COLLISIONMGR.rayTest (&ray, MAXFLOAT);

    float minTime = MAXFLOAT;

    for (int i = 0; i < olist->count; i++)
    {
        const Obstacle* obs = olist->list[i];
        const float t = obs->intersect(ray);
        if (obs->getType() == Teleporter::getClassName())
        {
            // the physics for teles is whacked, imho
            continue;
        }
        else if ((t > 0.0f) && (t < minTime))
            minTime = t;
    }

    float height = maxHeight + (minTime * zSpeed);

    if (height < 0.0f)
        height = 0.0f;

    return height;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
