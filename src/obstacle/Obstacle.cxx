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
#include <math.h>
#include <string.h>
#include <iostream>
#include "Obstacle.h"
#include "Intersect.h"
#include "StateDatabase.h"


// limits the maximum extent of any obstacle
const float Obstacle::maxExtent = 1.0e30f;

// for counting OBJ file objects
int Obstacle::objCounter = 0;


Obstacle::Obstacle()
{
    pos = glm::vec3(0.0f);
    size = glm::vec3(0.0f);
    angle = 0;
    driveThrough = false;
    shootThrough = false;
    ricochet     = false;
    ZFlip = false;
    source = WorldSource;

    insideNodeCount = 0;
    insideNodes = NULL;
}

Obstacle::Obstacle(const glm::vec3 &_pos, float _angle,
                   float _width, float _breadth, float _height,
                   bool drive, bool shoot, bool rico)
{
    pos = _pos;
    angle = _angle;
    size[0] = _width;
    size[1] = _breadth;
    size[2] = _height;

    driveThrough = drive;
    shootThrough = shoot;
    ricochet     = rico;
    ZFlip = false;
    source = WorldSource;

    insideNodeCount = 0;
    insideNodes = NULL;
}

Obstacle::~Obstacle()
{
    delete[] insideNodes;
    return;
}

bool            Obstacle::isValid() const
{
    for (int a = 0; a < 3; a++)
    {
        if ((extents.mins[a] < -maxExtent) || (extents.maxs[a] > maxExtent))
            return false;
    }
    return true;
}

void            Obstacle::setExtents()
{
    float xspan = (fabsf(cosf(angle)) * size[0]) + (fabsf(sinf(angle)) * size[1]);
    float yspan = (fabsf(cosf(angle)) * size[1]) + (fabsf(sinf(angle)) * size[0]);
    extents.mins = pos;
    extents.maxs = pos;
    extents.mins[0] -= xspan;
    extents.maxs[0] += xspan;
    extents.mins[1] -= yspan;
    extents.maxs[1] += yspan;
    extents.maxs[2] += size[2];
    return;
}

bool            Obstacle::isFlatTop ( void ) const
{
    return false;
}

void            Obstacle::setZFlip ( void )
{
    ZFlip = true;
}

bool            Obstacle::getZFlip ( void ) const
{
    return ZFlip;
}


void Obstacle::get3DNormal(const glm::vec3 &p, glm::vec3 &n) const
{
    getNormal(p, n);
}

bool Obstacle::inBox(const glm::vec3 &, float, float, float, float) const
{
    return false;
}

bool Obstacle::getHitNormal(const glm::vec3 &, float,
                            const glm::vec3 &, float,
                            float, float,
                            float, glm::vec3 &) const
{
    return false;
}

bool Obstacle::inMovingBox(const glm::vec3 &, float, const glm::vec3 &, float,
                           float, float, float) const
{
    assert(false);
    return false;
}

bool Obstacle::isCrossing(const glm::vec3 &, float,
                          float, float, float, glm::vec4 *) const
{
    // never crossing by default
    return false;
}

float           Obstacle::getHitNormal(
    const glm::vec3 &pos1, float azimuth1,
    const glm::vec3 &pos2, float azimuth2,
    float width, float breadth,
    const glm::vec3 &oPos, float oAzimuth,
    float oWidth, float oBreadth, float oHeight,
    glm::vec3 &normal) const
{
    static const float    square[4][2] =
    {
        {  1.0f,  1.0f },
        { -1.0f,  1.0f },
        { -1.0f, -1.0f },
        {  1.0f, -1.0f }
    };

    // construct a ray between the old and new positions of each corner
    // of the moving object.  test each ray against object and record
    // the minimum valid intersection time.  valid times are [0,1].
    float c1 = cosf(azimuth1), s1 = sinf(azimuth1);
    float c2 = cosf(azimuth2), s2 = sinf(azimuth2);

    int i, bestSide = -1;
    float minTime = 1.0f;
    for (i = 0; i < 4; i++)
    {
        auto p = glm::vec3(
                     pos1[0] + square[i][0]*c1*width - square[i][1]*s1*breadth,
                     pos1[1] + square[i][0]*s1*width + square[i][1]*c1*breadth,
                     0.0f);
        auto d = glm::vec3(
                     pos2[0] + square[i][0]*c2*width - square[i][1]*s2*breadth,
                     pos2[1] + square[i][0]*s2*width + square[i][1]*c2*breadth,
                     0.0f) - p;
        int side;
        const float t = timeAndSideRayHitsRect(Ray(p, d),
                                               oPos, oAzimuth, oWidth, oBreadth, side);
        if (side >= 0 && t <= minTime)
        {
            minTime = t;
            bestSide = side;
        }
    }

    // check time to intersect roof (on the way down;  don't care about way up)
    if (pos2[2] < pos1[2])
    {
        const float t = (pos1[2] - oHeight - oPos[2]) / (pos1[2] - pos2[2]);
        if (t >= 0.0f && t <= minTime)
        {
            minTime = t;
            bestSide = 4;
        }
    }
    else if (pos2[2] == pos1[2])
    {
        if (pos1[2] == (oHeight + oPos[2]))
        {
            minTime = 0.0f;
            bestSide = 4;
        }
    }

    // now do the same with obstacle's corners against moving object.
    // we must transform the building into moving object's space.
    bool isObstacle = false;
    c1 = cosf(oAzimuth);
    s1 = sinf(oAzimuth);
    for (i = 0; i < 4; i++)
    {
        float v[2], p[2], p2[2], d[2];
        v[0] = oPos[0] + square[i][0] * c1 * oWidth - square[i][1] * s1 * oBreadth;
        v[1] = oPos[1] + square[i][0] * s1 * oWidth + square[i][1] * c1 * oBreadth;

        p[0] = (v[0]-pos1[0]) * cosf(-azimuth1) - (v[1]-pos1[1]) * sinf(-azimuth1);
        p[1] = (v[0]-pos1[0]) * sinf(-azimuth1) + (v[1]-pos1[1]) * cosf(-azimuth1);

        p2[0] = (v[0]-pos2[0]) * cosf(-azimuth2) - (v[1]-pos2[1]) * sinf(-azimuth2);
        p2[1] = (v[0]-pos2[0]) * sinf(-azimuth2) + (v[1]-pos2[1]) * cosf(-azimuth2);

        d[0] = p2[0] - p[0];
        d[1] = p2[1] - p[1];
        int side;
        const float t = timeAndSideRayHitsOrigRect(p, d, width, breadth, side);
        if (side >= 0 && t <= minTime)
        {
            minTime = t;
            bestSide = side;
            isObstacle = true;
        }
    }

    // get normal
    if (bestSide == -1) return -1.0f;
    if (bestSide == 4)
        normal = glm::vec3(0.0f, 0.0f, 1.0f);
    else if (!isObstacle)
    {
        const float _angle = (float)(0.5 * M_PI * (float)bestSide + oAzimuth);
        normal = glm::vec3(cosf(_angle), sinf(_angle), 0.0f);
    }
    else
    {
        const float _angle = (float)(0.5 * M_PI * (float)bestSide +
                                     minTime * (azimuth2 - azimuth1) + azimuth1);
        normal = glm::vec3(-cosf(_angle), -sinf(_angle), 0.0f);
    }
    return minTime;
}


void Obstacle::addInsideSceneNode(SceneNode* node)
{
    insideNodeCount++;
    SceneNode** tmp = new SceneNode*[insideNodeCount];
    if (insideNodes)
    {
        memcpy(tmp, insideNodes, (insideNodeCount - 1) * sizeof(SceneNode*));
        delete[] insideNodes;
    }
    insideNodes = tmp;
    insideNodes[insideNodeCount - 1] = node;
}


void Obstacle::freeInsideSceneNodeList()
{
    insideNodeCount = 0;
    delete[] insideNodes;
    insideNodes = NULL;
    return;
}


int Obstacle::getInsideSceneNodeCount() const
{
    return insideNodeCount;
}


SceneNode** Obstacle::getInsideSceneNodeList() const
{
    return insideNodes;
}

Obstacle* Obstacle::copyWithTransform(MeshTransform const&) const
{
    std::cout << "ERROR: Obstacle::copyWithTransform()" << std::endl;
    exit(1);
    // umm, yeah...make the compiler happy...
    return NULL;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
