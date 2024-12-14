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
#include "LaserSceneNode.h"

// system headers
#include <math.h>

// common implementation headers
#include "StateDatabase.h"
#include "BZDBCache.h"
#include "OpenGLAPI.h"

// FIXME (SceneRenderer.cxx is in src/bzflag)
#include "SceneRenderer.h"

const GLfloat       LaserRadius = 0.1f;

LaserSceneNode::LaserSceneNode(const glm::vec3 &pos, const glm::vec3 &forward) :
    texturing(false),
    renderNode(this)
{
    // prepare rendering info
    const float xy_dist = hypotf(forward.x, forward.y);
    azimuth = (float)(180.0 / M_PI*atan2f(forward[1], forward[0]));
    elevation = (float)(-180.0 / M_PI * atan2f(forward.z, xy_dist));
    length = hypotf(xy_dist, forward.z);

    // setup sphere
    setCenter(pos);
    setRadius(length * length);

    OpenGLGStateBuilder builder(gstate);
    builder.disableCulling();
    gstate = builder.getState();

    first = false;
    setColor(1,1,1);
    setCenterColor(1,1,1);
}

void LaserSceneNode::setColor(float r, float g, float b)
{
    color = glm::vec4(r, g, b, 1.0f);
}


void LaserSceneNode::setCenterColor(float r, float g, float b)
{
    centerColor = glm::vec4(r, g, b, 1.0f);
}

LaserSceneNode::~LaserSceneNode()
{
    // do nothing
}

void            LaserSceneNode::setTexture(const int texture)
{
    OpenGLGStateBuilder builder(gstate);
    builder.setTexture(texture);
    builder.enableTexture(texture>=0);
    gstate = builder.getState();
}

bool            LaserSceneNode::cull(const ViewFrustum&) const
{
    // no culling
    return false;
}

void            LaserSceneNode::notifyStyleChange()
{
    texturing = BZDBCache::texture && BZDBCache::blend;
    OpenGLGStateBuilder builder(gstate);
    builder.enableTexture(texturing);
    if (BZDBCache::blend)
    {
        // add in contribution from laser
        builder.setBlending(GL_SRC_ALPHA, GL_ONE);
        builder.setSmoothing(BZDB.isTrue("smooth"));
    }
    else
    {
        builder.resetBlending();
        builder.setSmoothing(false);
    }
    gstate = builder.getState();
}

void            LaserSceneNode::addRenderNodes(
    SceneRenderer& renderer)
{
    renderer.addRenderNode(&renderNode, &gstate);
}

//
// LaserSceneNode::LaserRenderNode
//

glm::vec2 LaserSceneNode::LaserRenderNode::geom[6];

LaserSceneNode::LaserRenderNode::LaserRenderNode(
    const LaserSceneNode* _sceneNode) :
    sceneNode(_sceneNode)
{
    // initialize geometry if first instance
    static bool init = false;
    if (!init)
    {
        init = true;
        for (int i = 0; i < 6; i++)
        {
            const float angle = 2.0 * M_PI * double(i) / 6.0;
            geom[i] = LaserRadius * glm::vec2(-cosf(angle), sinf(angle));
        }
    }
}

LaserSceneNode::LaserRenderNode::~LaserRenderNode()
{
    // do nothing
}

const glm::vec3 &LaserSceneNode::LaserRenderNode::getPosition() const
{
    return sceneNode->getSphere();
}

void LaserSceneNode::LaserRenderNode::render()
{
    const bool blackFog = BZDBCache::blend && RENDERER.isFogActive();
    if (blackFog)
        glSetFogColor(glm::vec4(0.0f));

    if (RENDERER.useQuality() >= 3)
        renderGeoLaser();
    else
        renderFlatLaser();

    if (blackFog)
        glSetFogColor(RENDERER.getFogColor());
}

void LaserSceneNode::LaserRenderNode::renderGeoLaser()
{
    const float len = sceneNode->length;
    const auto &sphere = getPosition();
    glPushMatrix();
    glTranslate(sphere);
    glRotatef(sceneNode->azimuth, 0.0f, 0.0f, 1.0f);
    glRotatef(sceneNode->elevation, 0.0f, 1.0f, 0.0f);
    glRotatef(90, 0.0f, 1.0f, 0.0f);

    glDisable(GL_TEXTURE_2D);

    GLUquadric *q = gluNewQuadric();

    auto coreColor = sceneNode->centerColor;
    auto mainColor = sceneNode->color;

    myColor4f(coreColor.r, coreColor.g, coreColor.b, 0.85f);
    gluCylinder(q, 0.0625f, 0.0625f, len, 10, 1);
    addTriangleCount(20);

    myColor4f(mainColor.r, mainColor.g, mainColor.b, 0.125f);
    gluCylinder(q, 0.1f, 0.1f, len, 16, 1);
    addTriangleCount(32);

    gluCylinder(q, 0.2f, 0.2f, len, 24, 1);
    addTriangleCount(48);

    gluCylinder(q, 0.4f, 0.4f, len, 32, 1);
    addTriangleCount(64);

    if (sceneNode->first)
    {
        gluSphere(q, 0.5f, 32, 32);
        addTriangleCount(32 * 32 * 2);
    }
    else
    {
        gluSphere(q, 0.5f, 12, 12);
        addTriangleCount(12 * 12 * 2);
    }

    gluDeleteQuadric(q);

    glEnable(GL_TEXTURE_2D);
    glPopMatrix();
}


void LaserSceneNode::LaserRenderNode::renderFlatLaser()
{
    const float len = sceneNode->length;
    const auto &sphere = getPosition();
    glPushMatrix();
    glTranslate(sphere);
    glRotatef(sceneNode->azimuth, 0.0f, 0.0f, 1.0f);
    glRotatef(sceneNode->elevation, 0.0f, 1.0f, 0.0f);

    if (sceneNode->texturing)
    {
        myColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_TRIANGLE_FAN);
        glTexCoord2f(0.5f,  0.5f);
        glVertex3f(  0.0f,  0.0f,  0.0f);
        glTexCoord2f(0.0f,  0.0f);
        glVertex3f(  0.0f,  0.0f,  1.0f);
        glVertex3f(  0.0f,  1.0f,  0.0f);
        glVertex3f(  0.0f,  0.0f, -1.0f);
        glVertex3f(  0.0f, -1.0f,  0.0f);
        glVertex3f(  0.0f,  0.0f,  1.0f);
        glEnd(); // 6 verts -> 4 tris

        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0.0f,  0.0f);
        glVertex3f(  0.0f,  0.0f,  1.0f);
        glTexCoord2f(0.0f,  1.0f);
        glVertex3f(   len,  0.0f,  1.0f);
        glTexCoord2f(1.0f,  0.0f);
        glVertex3f(  0.0f,  0.0f, -1.0f);
        glTexCoord2f(1.0f,  1.0f);
        glVertex3f(   len,  0.0f, -1.0f);
        glEnd();

        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0.0f,  0.0f);
        glVertex3f(  0.0f,  1.0f,  0.0f);
        glTexCoord2f(0.0f,  1.0f);
        glVertex3f(   len,  1.0f,  0.0f);
        glTexCoord2f(1.0f,  0.0f);
        glVertex3f(  0.0f, -1.0f,  0.0f);
        glTexCoord2f(1.0f,  1.0f);
        glVertex3f(   len, -1.0f,  0.0f);
        glEnd(); // 8 verts -> 4 tris

        addTriangleCount(8);
    }

    else
    {
        // draw beam
        myColor4f(1.0f, 0.25f, 0.0f, 0.85f);
        glBegin(GL_TRIANGLE_STRIP);
        {
            for (int i = 0; i < 6; i++)
            {
                glVertex(glm::vec3(0.0f, geom[i]));
                glVertex(glm::vec3(len,  geom[i]));
            }
            glVertex(glm::vec3(0.0f, geom[0]));
            glVertex(glm::vec3(len,  geom[0]));
        }
        glEnd(); // 14 verts -> 12 tris

        // also draw a line down the middle (so the beam is visible even
        // if very far away).  this will also give the beam an extra bright
        // center.
        glBegin(GL_LINES);
        {
            glVertex3f(  0.0f, 0.0f, 0.0f);
            glVertex3f(   len, 0.0f, 0.0f);
        }
        glEnd(); // count 1 line as 1 tri

        addTriangleCount(13);
    }

    glPopMatrix();
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
