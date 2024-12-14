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

// Interface headers
#include "RenderNode.h"

// System headers
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <glm/vec3.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

//
// RenderNode
//

int RenderNode::triangleCount = 0;

void RenderNode::renderShadow()
{
    render();
}

void RenderNode::renderRadar()
{
    renderShadow();
}

int RenderNode::getTriangleCount()
{
    return triangleCount;
}


void RenderNode::resetTriangleCount()
{
    triangleCount = 0;
}


//
// RenderNodeList
//

void RenderNodeList::clear()
{
    list.clear();
}


void RenderNodeList::append(RenderNode* node)
{
    list.push_back(node);
}

void RenderNodeList::render() const
{
    for (auto item : list)
        item->renderShadow();
}

//
// RenderNodeGStateList
//

void RenderNodeGStateList::clear()
{
    list.clear();
}


void RenderNodeGStateList::append(RenderNode* node,
                                  const OpenGLGState* gstate)
{
    Item item;
    item.node   = node;
    item.gstate = gstate;
    item.depth  = 0.0f;
    list.push_back(item);
}

void RenderNodeGStateList::render() const
{
    for (auto &item : list)
    {
        item.gstate->setState();
        item.node->render();
    }
}


void RenderNodeGStateList::sort(const glm::vec3 &e)
{
    // calculate distances from the eye (squared)
    for (auto &item : list)
    {
        const auto &p = item.node->getPosition();
        item.depth = glm::distance2(p, e);
    }

    // sort from farthest to closest
    // Note: std::sort is guaranteed O(n log n). qsort (std::qsort) has no guarantee
    std::sort( list.begin(), list.end(),
               [](const Item& lhs, const Item& rhs)
    {
        return lhs.depth > rhs.depth;
    }
             );

    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
