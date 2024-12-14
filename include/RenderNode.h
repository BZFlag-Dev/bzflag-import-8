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

/* RenderNode:
 *  Encapsulates information for rendering geometry with a
 *  single gstate.
 *
 * RenderNodeList:
 *  Keeps a list of RenderNode* and can render them in order.
 */

#pragma once

// Before everything
#include "common.h"

// System headers
#include <vector>
#include <glm/fwd.hpp>

// Global headers
#include "OpenGLGState.h"


class RenderNode
{
public:
    RenderNode() = default;
    virtual ~RenderNode() = default;

    virtual void    render() = 0;
    virtual void    renderShadow();
    virtual void    renderRadar();
    // Used for sorting objects by distance
    virtual const glm::vec3 &getPosition() const = 0;

    static int      getTriangleCount();
    static void     resetTriangleCount();

protected:
    static void     addTriangleCount(int triCount);

private:
    static int      triangleCount;
};


inline void RenderNode::addTriangleCount(int count)
{
    triangleCount += count;
}

// do not tally unless debugging (for now)
#ifndef DEBUG_RENDERING
#  define addTriangleCount(x)
#endif


class RenderNodeList
{
public:
    RenderNodeList() = default;
    RenderNodeList(const RenderNodeList&) = delete;
    RenderNodeList& operator=(const RenderNodeList&) = delete;

    void        clear();
    void        append(RenderNode*);
    void        render() const;

private:
    std::vector<RenderNode*> list;
};

class RenderNodeGStateList
{
public:
    RenderNodeGStateList() = default;
    RenderNodeGStateList(const RenderNodeGStateList&) = delete;
    RenderNodeGStateList& operator=(const RenderNodeGStateList&) = delete;

    void        clear();
    void        append(RenderNode*, const OpenGLGState*);
    void        append(RenderNode*, const OpenGLGState*, float depth);
    void        render() const;

    void        sort(const glm::vec3 &eye);

private:
    struct Item
    {
    public:
        using GStatePtr = OpenGLGState const *;
        RenderNode* node;
        GStatePtr   gstate;
        float       depth;
    };

    std::vector<Item> list;
};


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
