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

#ifndef _MESH_RENDER_NODE_H
#define _MESH_RENDER_NODE_H

// Inherits from
#include "RenderNode.h"

// System headers
#include <glm/vec3.hpp>
#include <glm/fwd.hpp>

// common implementation headers
#include "bzfgl.h"


class Extents;
class MeshDrawMgr;


class OpaqueRenderNode : public RenderNode
{
public:
    OpaqueRenderNode(MeshDrawMgr* drawMgr,
                     GLfloat *xformMatrix, bool normalize,
                     const glm::vec4 *color, int lod, int set,
                     const Extents* exts, int triangles);
    void render() override;
    void renderShadow() override;
    const glm::vec3 &getPosition() const override;
private:
    void drawV() const;
    void drawVN() const;
    void drawVT() const;
    void drawVTN() const;
private:
    MeshDrawMgr* drawMgr;
    GLfloat* xformMatrix;
    bool normalize;
    int lod, set;
    const glm::vec4 *color;
    const Extents* exts;
    int triangles;
};


class AlphaGroupRenderNode : public OpaqueRenderNode
{
public:
    AlphaGroupRenderNode(MeshDrawMgr* drawMgr,
                         GLfloat *xformMatrix, bool normalize,
                         const glm::vec4 *color, int lod, int set,
                         const Extents* exts, const glm::vec3 &pos,
                         int triangles);
    const glm::vec3 &getPosition() const override;
    void setPosition(const glm::vec3 &pos);
private:
    glm::vec3 pos;
};


#endif // _MESH_RENDER_NODE_H


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
