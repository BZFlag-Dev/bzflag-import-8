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

#include "BaseSceneNodeGenerator.h"
#include "BaseBuilding.h"
#include "bzfgl.h"
#include "QuadWallSceneNode.h"


BaseSceneNodeGenerator::BaseSceneNodeGenerator(const BaseBuilding* _base) : base(_base)
{
    // do nothing
}

BaseSceneNodeGenerator::~BaseSceneNodeGenerator()
{
    // do nothing
}

WallSceneNode*  BaseSceneNodeGenerator::getNextNode(float uRepeats, float vRepeats, bool lod)
{
    bool fixedUVs = false;
    const float height = base->getHeight() + base->getPosition()[2];
    if (getNodeNumber() >= 1 && height == 0) return NULL;
    if (getNodeNumber() >= 6) return NULL;
    glm::vec3 bPoint, sCorner, tCorner;
    if (height == 0)
    {
        fixedUVs = true;
        incNodeNumber();
        base->getCorner(0, bPoint);
        base->getCorner(3, tCorner);
        base->getCorner(1, sCorner);
    }
    else
    {
        switch (incNodeNumber())
        {
        case 1:  // This is the top polygon
            fixedUVs = true;
            base->getCorner(4, bPoint);
            base->getCorner(5, sCorner);
            base->getCorner(7, tCorner);
            break;
        case 2:
            base->getCorner(0, bPoint);
            base->getCorner(1, sCorner);
            base->getCorner(4, tCorner);
            break;
        case 3:
            base->getCorner(1, bPoint);
            base->getCorner(2, sCorner);
            base->getCorner(5, tCorner);
            break;
        case 4:
            base->getCorner(2, bPoint);
            base->getCorner(3, sCorner);
            base->getCorner(6, tCorner);
            break;
        case 5:
            base->getCorner(3, bPoint);
            base->getCorner(0, sCorner);
            base->getCorner(7, tCorner);
            break;
        case 6:  // This is the bottom polygon
            fixedUVs = true;
            if (base->getPosition()[2] > 0.0f)
            {
                // Only generate if above ground level
                base->getCorner(0, bPoint);
                base->getCorner(3, sCorner);
                base->getCorner(1, tCorner);
            }
            else
                return NULL;
            break;

        }
    }
    glm::vec4 color;
    switch (base->getTeam())
    {
    case 1:
        color = glm::vec4(0.7f, 0.0f, 0.0f, 1.0f);
        break;
    case 2:
        color = glm::vec4(0.0f, 0.7f, 0.0f, 1.0f);
        break;
    case 3:
        color = glm::vec4(0.0f, 0.0f, 0.7f, 1.0f);
        break;
    case 4:
        color = glm::vec4(0.7f, 0.0f, 0.7f, 1.0f);
        break;
    }

    const auto sEdge = sCorner - bPoint;
    const auto tEdge = tCorner - bPoint;

    WallSceneNode *retval = new QuadWallSceneNode(bPoint, sEdge, tEdge, uRepeats, vRepeats, lod, fixedUVs);
    retval->setColor(color);
    return retval;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
