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
#include "EighthDPyrSceneNode.h"

// system headers
#include <stdlib.h>
#include <math.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <glm/gtc/random.hpp>

// common implementation header
#include "StateDatabase.h"

// FIXME (SceneRenderer.cxx is in src/bzflag)
#include "SceneRenderer.h"

const int       PyrPolygons = 20;

EighthDPyrSceneNode::EighthDPyrSceneNode(
    const glm::vec3 &pos,
    const glm::vec3 &size,
    float rotation) :
    EighthDimSceneNode(PyrPolygons),
    renderNode(this, pos, size, rotation)
{
    // get rotation stuff
    const float c = cosf(rotation);
    const float s = sinf(rotation);

    // compute polygons
    const auto polySize = size[0] / powf(float(PyrPolygons), 0.3333f) / 2.0f;
    const GLfloat slope = size[2] / size[0];
    const auto low  = glm::vec3(size.x - polySize,
                                size.y - polySize,
                                0.0f);
    const auto high = size - glm::vec3(polySize,
                                       polySize,
                                       slope * hypotf(low.x, low.y));
    const auto ave  = glm::vec3(polySize);

    for (int i = 0; i < PyrPolygons; i++)
    {
        glm::vec3 vertex[3];
        const auto base = glm::linearRand(-low, high);
        for (int j = 0; j < 3; j++)
        {
            // pick point around origin
            auto p = base + glm::linearRand(-ave, +ave);

            // make sure it's inside the box
            GLfloat height = size[2] - slope * hypotf(p[0], p[1]);
            p = glm::clamp(p,
                           glm::vec3(-size.x, -size.y, 0.0f),
                           glm::vec3( size.x,  size.y, height));

            // rotate it
            vertex[j] = pos + glm::vec3(c * p[0] - s * p[1],
                                        s * p[0] + c * p[1],
                                        p[2]);
        }

        setPolygon(i, vertex);
    }

    // set sphere
    setCenter(pos);
    setRadius(0.25f * glm::length2(size));
}

EighthDPyrSceneNode::~EighthDPyrSceneNode()
{
    // do nothing
}

void            EighthDPyrSceneNode::notifyStyleChange()
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

void            EighthDPyrSceneNode::addRenderNodes(
    SceneRenderer& renderer)
{
    EighthDimSceneNode::addRenderNodes(renderer);
    renderer.addRenderNode(&renderNode, &gstate);
}

//
// EighthDPyrSceneNode::EighthDPyrRenderNode
//

EighthDPyrSceneNode::EighthDPyrRenderNode::EighthDPyrRenderNode(
    const EighthDPyrSceneNode* _sceneNode,
    const glm::vec3 &pos,
    const glm::vec3 &size,
    float rotation) :
    sceneNode(_sceneNode)
{
    // get rotation stuff
    const float c = cosf(rotation);
    const float s = sinf(rotation);

    // compute corners
    corner[0][0] = pos[0] + c * size[0] - s * size[1];
    corner[0][1] = pos[1] + s * size[0] + c * size[1];
    corner[1][0] = pos[0] - c * size[0] - s * size[1];
    corner[1][1] = pos[1] - s * size[0] + c * size[1];
    corner[2][0] = pos[0] - c * size[0] + s * size[1];
    corner[2][1] = pos[1] - s * size[0] - c * size[1];
    corner[3][0] = pos[0] + c * size[0] + s * size[1];
    corner[3][1] = pos[1] + s * size[0] - c * size[1];
    corner[0][2] = corner[1][2] = corner[2][2] = corner[3][2] = pos[2];
    corner[4][0] = pos[0];
    corner[4][1] = pos[1];
    corner[4][2] = pos[2] + size[2];
}

EighthDPyrSceneNode::EighthDPyrRenderNode::~EighthDPyrRenderNode()
{
    // do nothing
}

const glm::vec3 &EighthDPyrSceneNode::EighthDPyrRenderNode::getPosition() const
{
    return sceneNode->getSphere();
}

void            EighthDPyrSceneNode::EighthDPyrRenderNode::render()
{
    myColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3fv(corner[0]);
    glVertex3fv(corner[1]);
    glVertex3fv(corner[2]);
    glVertex3fv(corner[3]);
    glEnd();
    glBegin(GL_LINES);
    glVertex3fv(corner[0]);
    glVertex3fv(corner[4]);
    glVertex3fv(corner[1]);
    glVertex3fv(corner[4]);
    glVertex3fv(corner[2]);
    glVertex3fv(corner[4]);
    glVertex3fv(corner[3]);
    glVertex3fv(corner[4]);
    glEnd();
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
