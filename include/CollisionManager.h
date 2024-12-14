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

#ifndef __COLLISION_GRID__
#define __COLLISION_GRID__

#include "common.h"

// system headers
#include <vector>

// common headers
#include "Extents.h"

class Ray;
class Obstacle;
class MeshObstacle;
class BoxBuilding;
class PyramidBuilding;
class BaseBuilding;
class Teleporter;


typedef struct
{
    int count;
    Obstacle** list;
} ObsList;

typedef union
{
    ObsList array[6];
    struct
    {
        ObsList boxes;
        ObsList bases;
        ObsList pyrs;
        ObsList teles;
        ObsList faces;
        ObsList meshes;
    } named;
} SplitObsList;

typedef struct
{
    int count;
    const class ColDetNode** list;
} ColDetNodeList;


// well you know my name is Simon, and I like to do drawings
typedef void (*DrawLinesFunc)
(int pointCount, const glm::vec3 points[], int color);


class CollisionManager
{

public:

    CollisionManager();
    ~CollisionManager();

    void load ();
    void clear ();

    // some basics
    bool needReload() const;     // octree parameter has changed
    int getObstacleCount() const;    // total number of obstacles
    const Extents& getWorldExtents() const;


    // test against an axis aligned bounding box
    const ObsList* axisBoxTest (const Extents& extents);

    // test against a cylinder
    const ObsList* cylinderTest (const glm::vec3 &pos,
                                 float radius, float height) const;
    // test against a box
    const ObsList* boxTest (const glm::vec3 &pos, float angle,
                            float dx, float dy, float dz) const;
    // test against a moving box
    const ObsList* movingBoxTest (const glm::vec3 &oldPos, float oldAngle,
                                  const glm::vec3 &pos, float angle,
                                  float dx, float dy, float dz) const;
    // test against a Ray
    const ObsList* rayTest (const Ray* ray, float timeLeft) const;

    // test against a Ray (and return a list of ColDetNodes)
    const ColDetNodeList* rayTestNodes (const Ray* ray, float timeLeft) const;

    // test against a box and return a split list
    //const SplitObsList *boxTestSplit (const float* pos, float angle,
    //                float dx, float dy, float dz) const;

    // drawing function
    void draw (DrawLinesFunc drawLinesFunc);

private:

    void setExtents(ObsList* list); // gather the extents

    class ColDetNode* root;   // the root of the octree

    float worldSize;
    Extents gridExtents;
    Extents worldExtents;
};

extern CollisionManager COLLISIONMGR;


class ColDetNode
{
public:
    ColDetNode(unsigned char depth, const Extents& exts, ObsList *fullList);
    ~ColDetNode();

    int getCount() const;
    const ObsList* getList() const;
    const Extents& getExtents() const;
    float getInTime() const;
    float getOutTime() const;

    // these fill in the FullList return list
    void axisBoxTest (const Extents& extents) const;
    void boxTest (const float* pos, float angle, float dx, float dy, float dz) const;
    void rayTest (const Ray* ray, float timeLeft) const;
    void rayTestNodes (const Ray* ray, float timeLeft) const;

    // this fills in the SplitList return list
    // (FIXME: not yet implemented, boxTestSplit might be useful for radar)
    //void boxTestSplit (const float* pos, float angle, float dx, float dy, float dz) const;

    void tallyStats();
    void draw(DrawLinesFunc drawLinesFunc);

private:
    void makeChildren ();
    void resizeCell ();

    unsigned char depth;
    int count;
    Extents extents;
    unsigned char childCount;
    ColDetNode* children[8];
    ObsList fullList;
    mutable float inTime, outTime;
};


inline int ColDetNode::getCount() const
{
    return count;
}

inline const ObsList* ColDetNode::getList() const
{
    return &fullList;
}

inline const Extents& ColDetNode::getExtents() const
{
    return extents;
}

inline float ColDetNode::getInTime() const
{
    return inTime;
}

inline float ColDetNode::getOutTime() const
{
    return outTime;
}


inline int CollisionManager::getObstacleCount() const
{
    if (root == NULL)
        return 0;
    else
        return root->getCount();
}

inline const Extents& CollisionManager::getWorldExtents() const
{
    return worldExtents;
}


#endif /* __COLLISION_GRID__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
