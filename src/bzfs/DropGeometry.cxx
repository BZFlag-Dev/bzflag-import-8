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


#include "common.h"

// interface header
#include "DropGeometry.h"

// system headers
#include <stdlib.h>

// common headers
#include "Intersect.h"
#include "Obstacle.h"
#include "MeshFace.h"
#include "BaseBuilding.h"
#include "ObstacleMgr.h"
#include "CollisionManager.h"
#include "PhysicsDriver.h"
#include "WorldInfo.h"
#include "BZDBCache.h"


//
// Datatype Definitions
//
class HoldingList
{
public:
    HoldingList();
    ~HoldingList();
    void copy(const ObsList* list);
public:
    int size;
    int count;
    Obstacle** list;
};

static HoldingList rayList; // ray intersection list

//
// Function Prototypes
//
static int compareAscending(const void* a, const void* b);
static int compareDescending(const void* a, const void* b);
static bool isDeathLanding(const Obstacle* landing);
static bool isOpposingTeam(const Obstacle* obs, int team);
static bool isValidLanding(const Obstacle* obs);
static bool isValidClearance(const glm::vec3 &pos, float radius,
                             float height, int team);
static bool dropIt(glm::vec3 &pos, float minZ, float maxZ,
                   float radius, float height, int team);


/******************************************************************************/

bool DropGeometry::dropPlayer(glm::vec3 &pos, float minZ, float maxZ)
{
    // fudge-it to avoid spawn stickiness on obstacles
    const float fudge = 0.001f;
    const float tankHeight = BZDBCache::tankHeight + fudge;
    bool value = dropIt(pos, minZ, maxZ, BZDBCache::tankRadius, tankHeight, -1);
    pos[2] += fudge;
    return value;
}


bool DropGeometry::dropFlag(glm::vec3 &pos, float minZ, float maxZ)
{
    const float flagHeight = BZDB.eval(StateDatabase::BZDB_FLAGHEIGHT);
    return dropIt(pos, minZ, maxZ, BZDBCache::tankRadius, flagHeight, -1);
}


bool DropGeometry::dropTeamFlag(glm::vec3 &pos, float minZ, float maxZ,
                                int team)
{
    // team flags do not get real clearance checks (radius = 0)
    // if you want to put some smarts in to check for wedging
    // (flag is stuck amongst obstacles), then add the code into
    // isValidClearance().
    const float flagHeight = BZDB.eval(StateDatabase::BZDB_FLAGHEIGHT);
    return dropIt(pos, minZ, maxZ, 0.0f, flagHeight, team);
}


/******************************************************************************/

static inline bool isDeathLanding(const Obstacle* obs)
{
    if (obs->getType() == MeshFace::getClassName())
    {
        const MeshFace* face = (const MeshFace*) obs;
        int driver = face->getPhysicsDriver();
        const PhysicsDriver* phydrv = PHYDRVMGR.getDriver(driver);
        if ((phydrv != NULL) && (phydrv->getIsDeath()))
            return true;
    }
    return false;
}


static inline bool isOpposingTeam(const Obstacle* obs, int team)
{
    if (team < 0)
        return false;

    if (obs->getType() != BaseBuilding::getClassName())
        return false;

    const BaseBuilding* base = (const BaseBuilding*) obs;
    if (base->getTeam() == team)
        return false;

    return true;
}


static inline bool isValidLanding(const Obstacle* obs)
{
    // must be a flattop buildings
    if (!obs->isFlatTop())
        return false;
    // drivethrough buildings are not potential landings
    if (obs->isDriveThrough())
        return false;

    // death buildings are not potential landings
    if (isDeathLanding(obs))
        return false;

    return true;
}


static bool isValidClearance(const glm::vec3 &pos, float radius,
                             float height, int team)
{
    const ObsList* olist = COLLISIONMGR.cylinderTest(pos, radius, height);

    // invalid if it touches a building
    for (int i = 0; i < olist->count; i++)
    {
        const Obstacle* obs = olist->list[i];
        const float zTop = obs->getExtents().maxs[2];
        if (zTop > pos[2])
        {
            if (obs->inCylinder(pos, radius, height))
                return false;
        }
        else
        {
            // do not check coplanars unless they are fatal
            if (isDeathLanding(obs) || isOpposingTeam(obs, team))
            {
                const float fudge = 0.001f; // dig in a little to make sure
                auto testPos = pos;
                testPos.z   -= fudge;
                if (obs->inCylinder(testPos, radius, height + fudge))
                    return false;
            }
        }
    }

    return true;
}


static int compareAscending(const void* a, const void* b)
{
    const Obstacle* obsA = *((const Obstacle* const *)a);
    const Obstacle* obsB = *((const Obstacle* const *)b);
    const float topA = obsA->getExtents().maxs[2];
    const float topB = obsB->getExtents().maxs[2];
    if (topA < topB)
        return -1;
    else if (topA > topB)
        return +1;
    else
        return 0;
}


static int compareDescending(const void* a, const void* b)
{
    const Obstacle* obsA = *((const Obstacle* const *)a);
    const Obstacle* obsB = *((const Obstacle* const *)b);
    const float topA = obsA->getExtents().maxs[2];
    const float topB = obsB->getExtents().maxs[2];
    if (topA < topB)
        return +1;
    else if (topA > topB)
        return -1;
    else
        return 0;
}


/******************************************************************************/

static bool dropIt(glm::vec3 &pos, float minZ, float maxZ,
                   float radius, float height, int team)
{
    int i;

    // special case, just check the ground
    if (maxZ <= 0.0f)
    {
        pos[2] = 0.0f;
        if (isValidClearance(pos, radius, height, team))
            return true;
        else
            return false;
    }

    // adjust the position to the minimum level
    if (pos[2] < minZ)
        pos[2] = minZ;

    // use a downwards ray to hit the onFlatTop() buildings
    const float maxHeight = COLLISIONMGR.getWorldExtents().maxs[2];
    const auto dir = glm::vec3(0.0f, 0.0f, -1.0f);
    const auto org = glm::vec3(pos[0], pos[1], maxHeight + 1.0f);
    Ray ray(org, dir);

    // list of  possible landings
    const ObsList* olist = COLLISIONMGR.rayTest(&ray, MAXFLOAT);
    rayList.copy(olist); // copy the list, so that COLLISIONMGR can be re-used

    const float startZ = pos[2];

    // are we in the clear?
    if (isValidClearance(pos, radius, height, team))
    {
        // sort from highest to lowest
        if (rayList.count)
            qsort(rayList.list, rayList.count, sizeof(Obstacle*), compareDescending);
        // no interference, try dropping
        for (i = 0; i < rayList.count; i++)
        {
            const Obstacle* obs = rayList.list[i];
            const float zTop = obs->getExtents().maxs[2];
            // make sure that it's within the limits
            if ((zTop > startZ) || (zTop > maxZ))
                continue;
            if (zTop < minZ)
                break;
            pos[2] = zTop;

            if (obs->intersect(ray) >= 0.0f)
            {
                if (isValidLanding(obs) &&
                        isValidClearance(pos, radius, height, team))
                    return true;
                else
                {
                    // a potential hit surface was tested and failed, unless
                    // we want to pass through it, we have to return false.
                    return false;
                }
            }
        }
        // check the ground
        if (minZ <= 0.0f)
        {
            pos[2] = 0.0f;
            if (isValidClearance(pos, radius, height, team))
                return true;
        }
    }
    else
    {
        // sort from lowest to highest
        qsort(rayList.list, rayList.count, sizeof(Obstacle*), compareAscending);
        // we're blocked, try climbing
        for (i = 0; i < rayList.count; i++)
        {
            const Obstacle* obs = rayList.list[i];
            const float zTop = obs->getExtents().maxs[2];
            // make sure that it's within the limits
            if ((zTop < startZ) || (zTop < minZ))
                continue;
            if (zTop > maxZ)
                return false;
            pos[2] = zTop;

            if (isValidLanding(obs) &&
                    (obs->intersect(ray) >= 0.0f) &&
                    isValidClearance(pos, radius, height, team))
                return true;
        }
    }

    return false;
}


/******************************************************************************/

HoldingList::HoldingList()
{
    size = count = 0;
    list = NULL;
    return;
}

HoldingList::~HoldingList()
{
    delete[] list;
    return;
}

void HoldingList::copy(const ObsList* olist)
{
    if (olist->count > size)
    {
        // increase the list size
        delete[] list;
        size = olist->count;
        list = new Obstacle*[size];
    }
    count = olist->count;
    if (count)
        memcpy(list, olist->list, count * sizeof(Obstacle*));
    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
