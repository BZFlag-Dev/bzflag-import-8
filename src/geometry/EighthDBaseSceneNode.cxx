/* bzflag
 * Copyright (c) 1993-2018 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

// bzflag common header
#include "common.h"

// inteface header
#include "EighthDBaseSceneNode.h"

// system headers
#include <stdlib.h>
#include <math.h>
#include <glm/gtc/type_ptr.hpp>

// common implementation header
#include "StateDatabase.h"

// FIXME (SceneRenderer.cxx is in src/bzflag)
#include "SceneRenderer.h"

const int       BasePolygons = 60;

EighthDBaseSceneNode::EighthDBaseSceneNode(const float pos[3],
        const float size[3], float rotation) :
    EighthDimSceneNode(BasePolygons),
    renderNode(this, glm::make_vec3(pos), size, rotation)
{
    // get rotation stuff
    const float c = cosf(rotation);
    const float s = sinf(rotation);

    // compute polygons
    const GLfloat polySize = size[0] / powf(float(BasePolygons), 0.3333f);
    for (int i = 0; i < BasePolygons; i++)
    {
        GLfloat base[3], vertex[3][3];
        base[0] = (size[0] - 0.5f * polySize) * (2.0f * (float) bzfrand() - 1.0f);
        base[1] = (size[1] - 0.5f * polySize) * (2.0f * (float) bzfrand() - 1.0f);
        base[2] = (size[2] - 0.5f * polySize) * (float) bzfrand();
        for (int j = 0; j < 3; j++)
        {
            // pick point around origin
            GLfloat p[3];
            p[0] = base[0] + polySize * ((float) bzfrand() - 0.5f);
            p[1] = base[1] + polySize * ((float) bzfrand() - 0.5f);
            p[2] = base[2] + polySize * ((float) bzfrand() - 0.5f);

            // make sure it's inside the base
            if (p[0] < -size[0]) p[0] = -size[0];
            else if (p[0] > size[0]) p[0] = size[0];
            if (p[1] < -size[1]) p[1] = -size[1];
            else if (p[1] > size[1]) p[1] = size[1];
            if (p[2] < -size[2]) p[2] = -size[2];
            else if (p[2] > size[2]) p[2] = size[2];

            // rotate it
            vertex[j][0] = pos[0] + c * p[0] - s * p[1];
            vertex[j][1] = pos[1] + s * p[0] + c * p[1];
            vertex[j][2] = pos[2] + p[2];
        }
        setPolygon(i, vertex);
    }

    // set sphere
    setCenter(pos);
    setRadius(0.25f * (size[0] * size[0] + size[1] * size[1] + size[2] * size[2]));
}

EighthDBaseSceneNode::~EighthDBaseSceneNode()
{
    // do nothing
}

void EighthDBaseSceneNode::notifyStyleChange()
{
    EighthDimSceneNode::notifyStyleChange();

    OpenGLGStateBuilder builder(gstate);
    if (BZDB.isTrue("smooth"))
    {
        builder.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        builder.setSmoothing();
    }
    else
    {
        builder.resetBlending();
        builder.resetSmoothing();
    }
    gstate = builder.getState();
}

void EighthDBaseSceneNode::addRenderNodes(SceneRenderer& renderer)
{
    EighthDimSceneNode::addRenderNodes(renderer);
    renderer.addRenderNode(&renderNode, &gstate);
}

EighthDBaseSceneNode::EighthDBaseRenderNode::EighthDBaseRenderNode(
    const EighthDBaseSceneNode * _sceneNode,
    const glm::vec3 pos,
    const float size[3], float rotation) :
    sceneNode(_sceneNode)
{
    // get rotation stuff
    const float c = cosf(rotation);
    const float s = sinf(rotation);

    // compute corners
    const float b = size[0];
    const float w = size[1];
    corner[0] = glm::vec3(+c * b - s * w, +s * b + c * w, 0.0f);
    corner[1] = glm::vec3(-c * b - s * w, -s * b + c * w, 0.0f);
    corner[2] = -corner[0];
    corner[3] = -corner[1];
    int i;
    for (i = 0; i < 4; i++)
        corner[i + 4] = corner[i];
    for (i = 4; i < 8; i++)
        corner[i].z = size[2];
    for (i = 0; i < 8; i++)
        corner[i] += pos;

    vboIndex = -1;
    vboManager.registerClient(this);
}

EighthDBaseSceneNode::EighthDBaseRenderNode::~EighthDBaseRenderNode()
{
    vboV.vboFree(vboIndex);
    vboManager.unregisterClient(this);
}

void EighthDBaseSceneNode::EighthDBaseRenderNode::initVBO()
{
    glm::vec3 vertex[16];
    vertex[0]  = corner[0];
    vertex[1]  = corner[1];
    vertex[2]  = corner[2];
    vertex[3]  = corner[3];

    vertex[4]  = corner[4];
    vertex[5]  = corner[5];
    vertex[6]  = corner[6];
    vertex[7]  = corner[7];

    vertex[8]  = corner[0];
    vertex[9]  = corner[4];
    vertex[10] = corner[1];
    vertex[11] = corner[5];
    vertex[12] = corner[2];
    vertex[13] = corner[6];
    vertex[14] = corner[3];
    vertex[15] = corner[7];

    vboIndex = vboV.vboAlloc(16);
    vboV.vertexData(vboIndex, 16, vertex);
}

void EighthDBaseSceneNode::EighthDBaseRenderNode::render()
{
    myColor3f(1.0f, 1.0f, 1.0f);
    vboV.enableArrays();
    glDrawArrays(GL_LINE_LOOP, vboIndex,     4);
    glDrawArrays(GL_LINE_LOOP, vboIndex + 4, 4);
    glDrawArrays(GL_LINES,     vboIndex + 8, 8);
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
