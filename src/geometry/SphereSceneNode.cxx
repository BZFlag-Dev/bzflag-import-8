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

// interface header
#include "SphereSceneNode.h"

// system headers
#include <math.h>

// common implementation headers
#include "SceneRenderer.h"
#include "StateDatabase.h"
#include "BZDBCache.h"
#include "OpenGLMaterial.h"
#include "TextureManager.h"

// local implementation headers
#include "ViewFrustum.h"


/******************************************************************************/

//
// SphereSceneNode
//

SphereSceneNode::SphereSceneNode(const GLfloat pos[3], GLfloat _radius)
{
    transparent = false;

    OpenGLGStateBuilder builder(gstate);
    builder.disableCulling();
    gstate = builder.getState();

    setColor(1.0f, 1.0f, 1.0f, 1.0f);

    // position sphere
    move(pos, _radius);

    return;
}


SphereSceneNode::~SphereSceneNode()
{
    // do nothing
}


void SphereSceneNode::setColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    color[0] = r;
    color[1] = g;
    color[2] = b;
    color[3] = a;
    transparent = (color[3] != 1.0f);
}


void SphereSceneNode::setColor(const GLfloat* rgba)
{
    color[0] = rgba[0];
    color[1] = rgba[1];
    color[2] = rgba[2];
    color[3] = rgba[3];
    transparent = (color[3] != 1.0f);
}


void SphereSceneNode::move(const GLfloat pos[3], GLfloat _radius)
{
    radius = _radius;
    setCenter(pos);
    setRadius(radius * radius);
}


void SphereSceneNode::notifyStyleChange()
{
    OpenGLGStateBuilder builder(gstate);
    if (transparent)
    {
        {
            builder.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            builder.setStipple(1.0f);
            builder.setNeedsSorting(true);
        }
    }
    else
    {
        builder.resetBlending();
        builder.setStipple(1.0f);
        builder.setNeedsSorting(false);
    }
    gstate = builder.getState();
}


/******************************************************************************/

//
// SphereLodSceneNode
//


bool SphereLodSceneNode::initialized = false;
GLuint SphereLodSceneNode::lodLists[sphereLods];
float SphereLodSceneNode::lodPixelsSqr[sphereLods];
int SphereLodSceneNode::listTriangleCount[sphereLods];


static GLuint buildSphereList(GLdouble radius, GLint slices, GLint stacks)
{
    GLuint list;

    GLUquadric* quadric = gluNewQuadric();
    gluQuadricDrawStyle(quadric, GLU_FILL);
    gluQuadricTexture(quadric, GL_TRUE);
    gluQuadricNormals(quadric, GL_SMOOTH);
    gluQuadricOrientation(quadric, GLU_OUTSIDE);

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    {
        gluSphere(quadric, radius, slices, stacks);
    }
    glEndList();

    gluDeleteQuadric(quadric);

    return list;
}


void SphereLodSceneNode::freeContext(void *)
{
    for (int i = 0; i < sphereLods; i++)
    {
        if (lodLists[i] != INVALID_GL_LIST_ID)
        {
            glDeleteLists(lodLists[i], 1);
            lodLists[i] = INVALID_GL_LIST_ID;
        }
    }
    return;
}


static int calcTriCount(int slices, int stacks)
{
    const int trifans = 2 * slices;
    const int quads = 2 * (slices * (stacks - 2));
    return (trifans + quads);
}

void SphereLodSceneNode::initContext(void *)
{
    initialized = true;

    lodLists[0] = buildSphereList(1.0, 32, 32);
    lodPixelsSqr[0] = 80.0f * 80.0f;
    listTriangleCount[0] = calcTriCount(32, 32);

    lodLists[1] = buildSphereList(1.0, 16, 16);
    lodPixelsSqr[1] = 40.0f * 40.0f;
    listTriangleCount[1] = calcTriCount(16, 16);

    lodLists[2] = buildSphereList(1.0,  8, 8);
    lodPixelsSqr[2] = 20.0f * 20.0f;
    listTriangleCount[2] = calcTriCount(8, 8);

    lodLists[3] = buildSphereList(1.0,  6, 6);
    lodPixelsSqr[3] = 10.0f * 10.0f;
    listTriangleCount[3] = calcTriCount(6, 6);

    lodLists[4] = buildSphereList(1.0,  4, 4);
    lodPixelsSqr[4] = 5.0f * 5.0f;
    listTriangleCount[4] = calcTriCount(4, 4);

    return;
}


void SphereLodSceneNode::init()
{
    initialized = false; // no lists yet
    for (int i = 0; i < sphereLods; i++)
    {
        lodLists[i] = INVALID_GL_LIST_ID;
        lodPixelsSqr[i] = 0.0f;
    }
    return;
}


void SphereLodSceneNode::kill()
{
    if (initialized)
    {
        freeContext(NULL);
        OpenGLGState::unregisterContextInitializer(freeContext, initContext, NULL);
    }
    return;
}


SphereLodSceneNode::SphereLodSceneNode(const GLfloat pos[3], GLfloat _radius) :
    SphereSceneNode(pos, _radius),
    renderNode(this)
{
    if (!initialized)
    {
        initialized = true;
        initContext(NULL);
        OpenGLGState::registerContextInitializer(freeContext, initContext, NULL);
    }

    inside = false;
    shockWave = false;

    renderNode.setLod(0);

    // adjust the gstate for this type of sphere
    OpenGLGStateBuilder builder(gstate);
    builder.setCulling(GL_BACK);
    builder.setShading(GL_SMOOTH);
    const float spec[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const float emis[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    OpenGLMaterial glmat(spec, emis, 64.0f);
    builder.setMaterial(glmat);
    gstate = builder.getState();
    return;
}


SphereLodSceneNode::~SphereLodSceneNode()
{
    return;
}


void SphereLodSceneNode::setShockWave(bool value)
{
    shockWave = value;
    return;
}


void SphereLodSceneNode::addRenderNodes(SceneRenderer& renderer)
{
    const ViewFrustum& view = renderer.getViewFrustum();
    const float* s = getSphere();
    const float* e = view.getEye();
    const float dx = e[0] - s[0];
    const float dy = e[1] - s[1];
    const float dz = e[2] - s[2];

    float distSqr = (dx*dx) + (dy*dy) + (dz*dz);
    if (distSqr <= 0.0f)
        distSqr = 1.0e-6f;

    const float lpp = renderer.getLengthPerPixel();
    float ppl;
    if (lpp <= 0.0f)
        ppl = +MAXFLOAT;
    else
        ppl = 1.0f / lpp;
    const float pixelsSqr = (s[3] * (ppl * ppl)) / distSqr;

    int lod;
    for (lod = 0; lod < (sphereLods - 1); lod++)
    {
        if (lodPixelsSqr[lod] < pixelsSqr)
            break;
    }
    renderNode.setLod(lod);

    inside = (distSqr < s[3]);

    renderer.addRenderNode(&renderNode, &gstate);

    return;
}


void SphereLodSceneNode::addShadowNodes(SceneRenderer&)
{
    return;
}


//
// SphereLodSceneNode::SphereLodRenderNode
//

SphereLodSceneNode::SphereLodRenderNode::SphereLodRenderNode(
    const SphereLodSceneNode* _sceneNode) :
    sceneNode(_sceneNode)
{
    return;
}


SphereLodSceneNode::SphereLodRenderNode::~SphereLodRenderNode()
{
    return;
}


void SphereLodSceneNode::SphereLodRenderNode::setLod(int _lod)
{
    lod = _lod;
    return;
}


static inline void drawFullScreenRect()
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f(+1.0f, -1.0f);
    glVertex2f(-1.0f, +1.0f);
    glVertex2f(+1.0f, +1.0f);
    glEnd();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    return;
}


void SphereLodSceneNode::SphereLodRenderNode::render()
{
    const GLfloat radius = sceneNode->radius;
    const GLfloat* sphere = sceneNode->getSphere();

    glEnable(GL_CLIP_PLANE0);

    glEnable(GL_RESCALE_NORMAL);

    const bool transparent = sceneNode->transparent;

    const GLuint list = SphereLodSceneNode::lodLists[lod];

    glPushMatrix();
    {
        glTranslatef(sphere[0], sphere[1], sphere[2]);
        glScalef(radius, radius, radius);

        // invert the color within contained volume
        if (sceneNode->shockWave)
        {
            if (transparent)
                glDisable(GL_BLEND);
            glDisable(GL_LIGHTING);

            glLogicOp(GL_INVERT);
            glEnable(GL_COLOR_LOGIC_OP);
            {
                glCullFace(GL_FRONT);
                glCallList(list);
                addTriangleCount(listTriangleCount[lod]);
                glCullFace(GL_BACK);
                if (!sceneNode->inside)
                {
                    glCallList(list);
                    addTriangleCount(listTriangleCount[lod]);
                }
                else
                {
                    drawFullScreenRect();
                    addTriangleCount(2);
                }
            }
            glDisable(GL_COLOR_LOGIC_OP);

            if (transparent)
                glEnable(GL_BLEND);
            glEnable(GL_LIGHTING);
        }

        // draw the surface
        myColor4fv(sceneNode->color);
        {
            glCullFace(GL_FRONT);
            glCallList(list);
            addTriangleCount(listTriangleCount[lod]);
        }
        glCullFace(GL_BACK);
        if (!sceneNode->inside)
        {
            glCallList(list);
            addTriangleCount(listTriangleCount[lod]);
        }
        else
        {
            glDisable(GL_LIGHTING);
            drawFullScreenRect();
            glEnable(GL_LIGHTING);
            addTriangleCount(2);
        }
    }
    glPopMatrix();

    glDisable(GL_RESCALE_NORMAL);

    glDisable(GL_CLIP_PLANE0);

    return;
}


/******************************************************************************/


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
