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
#include "SphereSceneNode.h"

// system headers
#include <math.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

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

SphereSceneNode::SphereSceneNode(const glm::vec3 &pos, GLfloat _radius)
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
    color = glm::vec4(r, g, b, a);
    transparent = (color[3] != 1.0f);
}


void SphereSceneNode::setColor(const glm::vec4 &rgba)
{
    color = rgba;
    transparent = (color[3] != 1.0f);
}


void SphereSceneNode::move(const glm::vec3 &pos, GLfloat _radius)
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
        if (BZDBCache::blend)
        {
            builder.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            builder.setStipple(1.0f);
            builder.setNeedsSorting(true);
        }
        else
        {
            builder.resetBlending();
            builder.setStipple(0.5f);
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


SphereLodSceneNode::SphereLodSceneNode(const glm::vec3 &pos, GLfloat _radius) :
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
    const auto spec = glm::vec3(1.0f);
    const auto emis = glm::vec3(0.0f);
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
    if (BZDBCache::texture && false)   //FIXME
    {
        OpenGLGStateBuilder builder(gstate);
        TextureManager &tm = TextureManager::instance();
        int texId = tm.getTextureID("mesh");
        builder.setTexture(texId);
        gstate = builder.getState();
    }
    return;
}


void SphereLodSceneNode::addRenderNodes(SceneRenderer& renderer)
{
    const ViewFrustum& view = renderer.getViewFrustum();
    const auto &s = getSphere();
    const auto &e = view.getEye();

    float distSqr = glm::distance2(e, s);
    if (distSqr <= 0.0f)
        distSqr = 1.0e-6f;

    const float lpp = renderer.getLengthPerPixel();
    float ppl;
    if (lpp <= 0.0f)
        ppl = +MAXFLOAT;
    else
        ppl = 1.0f / lpp;
    const float r = getRadius2();
    const float pixelsSqr = (r * ppl * ppl) / distSqr;

    int lod;
    for (lod = 0; lod < (sphereLods - 1); lod++)
    {
        if (lodPixelsSqr[lod] < pixelsSqr)
            break;
    }
    renderNode.setLod(lod);

    inside = (distSqr < r);

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

const glm::vec3 &SphereLodSceneNode::SphereLodRenderNode::getPosition() const
{
    return sceneNode->getSphere();
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
    glRectf(-1.0f, -1.0f, +1.0f, +1.0f);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    return;
}


void SphereLodSceneNode::SphereLodRenderNode::render()
{
    const GLfloat radius = sceneNode->radius;
    const auto &sphere = getPosition();

    glEnable(GL_CLIP_PLANE0);

#ifdef GL_VERSION_1_2
    glEnable(GL_RESCALE_NORMAL);
#else
    glEnable(GL_NORMALIZE);
#endif

    const bool transparent = sceneNode->transparent;
    const bool stippled = transparent && !BZDBCache::blend;

    const GLuint list = SphereLodSceneNode::lodLists[lod];

    glPushMatrix();
    {
        glTranslatef(sphere[0], sphere[1], sphere[2]);
        glScalef(radius, radius, radius);

        // invert the color within contained volume
        if (sceneNode->shockWave)
        {
            if (transparent)
            {
                if (BZDBCache::blend)
                    glDisable(GL_BLEND);
                else
                    myStipple(1.0f);
            }
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
            {
                if (BZDBCache::blend)
                    glEnable(GL_BLEND);
                else
                    myStipple(0.5f);
            }
            glEnable(GL_LIGHTING);
        }

        // draw the surface
        myColor4fv(sceneNode->color);
        if (stippled)
            myStipple(sceneNode->color[3]);
        if (!stippled)
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
        if (stippled)
            myStipple(0.5f);
    }
    glPopMatrix();

#ifdef GL_VERSION_1_2
    glDisable(GL_RESCALE_NORMAL);
#else
    glDisable(GL_NORMALIZE);
#endif

    glDisable(GL_CLIP_PLANE0);

    return;
}


/******************************************************************************/

//
// SphereBspSceneNode
//

const int       NumSlices = 2 * SphereRes;
const int       NumParts = SphereLowRes * SphereLowRes;

SphereBspSceneNode::SphereBspSceneNode(const glm::vec3 &pos, GLfloat _radius) :
    SphereSceneNode(pos, _radius),
    renderNode(this),
    parts(NULL)
{
}

SphereBspSceneNode::~SphereBspSceneNode()
{
    if (parts)
    {
        for (int i = 0; i < NumParts; i++)
            delete parts[i];
        delete[] parts;
    }
}

SceneNode**     SphereBspSceneNode::getParts(int& numParts)
{
    if (!parts)
    {
        // make parts -- always use low detail sphere (if your zbuffer is
        // slow, then you probably don't want to render lots o' polygons)
        parts = new SphereFragmentSceneNode*[NumParts];
        for (int i = 0; i < SphereLowRes; i++)
            for (int j = 0; j < SphereLowRes; j++)
                parts[SphereLowRes * i + j] = new SphereFragmentSceneNode(j, i, this);
    }

    // choose number of parts to cut off bottom at around ground level
    int i;
    const auto &mySphere = getSphere();
    for (i = 0; i < SphereLowRes; i++)
        if (radius * SphereBspRenderNode::lgeom[SphereLowRes*i][2]
                + mySphere[2] < 0.01f)
            break;
    numParts = SphereLowRes * i;

    return (SceneNode**)parts;
}

void            SphereBspSceneNode::addRenderNodes(
    SceneRenderer& renderer)
{
    const float myRadius2 = getRadius2();
    const ViewFrustum& view = renderer.getViewFrustum();
    const auto &eye = view.getEye();
    const float size = myRadius2 * view.getAreaFactor() / getDistance(eye);
    const int lod = (size < 100.0f) ? 0 : 1;

    renderNode.setHighResolution(lod != 0);

    if (BZDBCache::blend)
    {
        const auto &mySphere = getSphere();
        const float azimuth = atan2f(mySphere[1] - eye[1], eye[0] - mySphere[0]);
        const int numSlices = (lod == 1) ? NumSlices : SphereLowRes;
        renderNode.setBaseIndex(int(float(numSlices) *
                                    (1.0f + 0.5f * azimuth / M_PI)) % numSlices);
    }

    renderer.addRenderNode(&renderNode, &gstate);
}

void            SphereBspSceneNode::addShadowNodes(SceneRenderer& UNUSED(renderer))
{
    return;
    /*
      renderNode.setHighResolution(false);
      renderNode.setBaseIndex(0);
      renderer.addShadowNode(&renderNode);
    */
}

//
// SphereBspSceneNode::SphereBspRenderNode
//

glm::vec3 SphereBspSceneNode::SphereBspRenderNode
::geom[NumSlices * (SphereRes + 1)];
glm::vec3 SphereBspSceneNode::SphereBspRenderNode
::lgeom[SphereLowRes * (SphereLowRes + 1)];

SphereBspSceneNode::SphereBspRenderNode::SphereBspRenderNode(
    const SphereBspSceneNode* _sceneNode) :
    sceneNode(_sceneNode),
    highResolution(false),
    baseIndex(0)
{
    // initialize geometry if first instance
    static bool init = false;
    if (!init)
    {
        init = true;

        // high resolution sphere
        int i, j;
        for (i = 0; i <= SphereRes; i++)
        {
            const float phi = (float)(M_PI * (0.5f - double(i) / SphereRes));
            for (j = 0; j < NumSlices; j++)
            {
                const float theta = (float)(2.0 * M_PI * double(j) / NumSlices);
                geom[NumSlices * i + j][0] = cosf(theta) * cosf(phi);
                geom[NumSlices * i + j][1] = sinf(theta) * cosf(phi);
                geom[NumSlices * i + j][2] = sinf(phi);
            }
        }

        // low resolution sphere
        for (i = 0; i <= SphereLowRes; i++)
        {
            const float phi = (float)(M_PI * (0.5 - double(i) / SphereLowRes));
            for (j = 0; j < SphereLowRes; j++)
            {
                const float theta = (float)(2.0 * M_PI * double(j) / SphereLowRes);
                lgeom[SphereLowRes * i + j][0] = cosf(theta) * cosf(phi);
                lgeom[SphereLowRes * i + j][1] = sinf(theta) * cosf(phi);
                lgeom[SphereLowRes * i + j][2] = sinf(phi);
            }
        }
    }
}

SphereBspSceneNode::SphereBspRenderNode::~SphereBspRenderNode()
{
    // do nothing
}

const glm::vec3 &SphereBspSceneNode::SphereBspRenderNode::getPosition() const
{
    return sceneNode->getSphere();
}

void            SphereBspSceneNode::SphereBspRenderNode::
setHighResolution(bool _highResolution)
{
    highResolution = _highResolution;
}

void            SphereBspSceneNode::SphereBspRenderNode::
setBaseIndex(int _baseIndex)
{
    baseIndex = _baseIndex;
}

void            SphereBspSceneNode::SphereBspRenderNode::render()
{
    int i, j;
    const GLfloat radius = sceneNode->radius;
    const auto &sphere = getPosition();

    glEnable(GL_CLIP_PLANE0);

    glPushMatrix();
    glTranslatef(sphere[0], sphere[1], sphere[2]);
    glScalef(radius, radius, radius);

    myColor4fv(sceneNode->color);
    if (!BZDBCache::blend && sceneNode->transparent)
        myStipple(sceneNode->color[3]);
    if (BZDBCache::lighting)
    {
#ifdef GL_VERSION_1_2
        glEnable(GL_RESCALE_NORMAL);
#else
        glEnable(GL_NORMALIZE);
#endif
        // draw with normals (normal is same as vertex!
        // one of the handy properties of a sphere.)
        if (highResolution)
        {
            for (i = 0; i < SphereRes; i++)
            {
                glBegin(GL_TRIANGLE_STRIP);
                for (j = baseIndex; j < NumSlices; j++)
                {
                    glNormal3fv(geom[NumSlices * i + j]);
                    glVertex3fv(geom[NumSlices * i + j]);
                    glNormal3fv(geom[NumSlices * i + j + NumSlices]);
                    glVertex3fv(geom[NumSlices * i + j + NumSlices]);
                }
                for (j = 0; j <= baseIndex; j++)
                {
                    glNormal3fv(geom[NumSlices * i + j]);
                    glVertex3fv(geom[NumSlices * i + j]);
                    glNormal3fv(geom[NumSlices * i + j + NumSlices]);
                    glVertex3fv(geom[NumSlices * i + j + NumSlices]);
                }
                glEnd();
            }
            addTriangleCount(SphereRes * NumSlices * 2);
        }
        else
        {
            for (i = 0; i < SphereLowRes; i++)
            {
                glBegin(GL_TRIANGLE_STRIP);
                for (j = baseIndex; j < SphereLowRes; j++)
                {
                    glNormal3fv(lgeom[SphereLowRes * i + j]);
                    glVertex3fv(lgeom[SphereLowRes * i + j]);
                    glNormal3fv(lgeom[SphereLowRes * i + j + SphereLowRes]);
                    glVertex3fv(lgeom[SphereLowRes * i + j + SphereLowRes]);
                }
                for (j = 0; j <= baseIndex; j++)
                {
                    glNormal3fv(lgeom[SphereLowRes * i + j]);
                    glVertex3fv(lgeom[SphereLowRes * i + j]);
                    glNormal3fv(lgeom[SphereLowRes * i + j + SphereLowRes]);
                    glVertex3fv(lgeom[SphereLowRes * i + j + SphereLowRes]);
                }
                glEnd();
            }
            addTriangleCount(SphereLowRes * SphereLowRes * 2);
        }
#ifdef GL_VERSION_1_2
        glDisable(GL_RESCALE_NORMAL);
#else
        glDisable(GL_NORMALIZE);
#endif
    }
    else
    {
        // draw without normals
        if (highResolution)
        {
            for (i = 0; i < SphereRes; i++)
            {
                glBegin(GL_TRIANGLE_STRIP);
                for (j = baseIndex; j < NumSlices; j++)
                {
                    glVertex3fv(geom[NumSlices * i + j]);
                    glVertex3fv(geom[NumSlices * i + j + NumSlices]);
                }
                for (j = 0; j <= baseIndex; j++)
                {
                    glVertex3fv(geom[NumSlices * i + j]);
                    glVertex3fv(geom[NumSlices * i + j + NumSlices]);
                }
                glEnd();
            }
            addTriangleCount(SphereRes * NumSlices * 2);
        }
        else
        {
            for (i = 0; i < SphereLowRes; i++)
            {
                glBegin(GL_TRIANGLE_STRIP);
                for (j = baseIndex; j < SphereLowRes; j++)
                {
                    glVertex3fv(lgeom[SphereLowRes * i + j]);
                    glVertex3fv(lgeom[SphereLowRes * i + j + SphereLowRes]);
                }
                for (j = 0; j <= baseIndex; j++)
                {
                    glVertex3fv(lgeom[SphereLowRes * i + j]);
                    glVertex3fv(lgeom[SphereLowRes * i + j + SphereLowRes]);
                }
                glEnd();
            }
            addTriangleCount(SphereLowRes * SphereLowRes * 2);
        }
    }

    if (!BZDBCache::blend && sceneNode->transparent)
        myStipple(0.5f);

    glPopMatrix();

    glDisable(GL_CLIP_PLANE0);
}

//
// SphereFragmentSceneNode
//

SphereFragmentSceneNode::SphereFragmentSceneNode(int _theta, int _phi,
        SphereBspSceneNode* _parentSphere) :
    parentSphere(_parentSphere),
    renderNode(_parentSphere, _theta, _phi)
{
    // position sphere fragment
    move();
}

SphereFragmentSceneNode::~SphereFragmentSceneNode()
{
    // do nothing
}

void            SphereFragmentSceneNode::move()
{
    const auto &pSphere   = parentSphere->getSphere();
    const GLfloat pRadius = parentSphere->getRadius();
    const auto vertex     = renderNode.getVertex();
    setCenter(pSphere + pRadius * vertex);

    setRadius((GLfloat)(4.0 * M_PI * M_PI * pRadius) /
              GLfloat(SphereLowRes * SphereLowRes));
}

void            SphereFragmentSceneNode::addRenderNodes
(SceneRenderer& renderer)
{
    renderer.addRenderNode(&renderNode, &parentSphere->gstate);
}

void            SphereFragmentSceneNode::addShadowNodes(
    SceneRenderer& UNUSED(renderer))
{
    return;
    /*
      renderer.addShadowNode(&renderNode);
    */
}

//
// SphereFragmentSceneNode::FragmentRenderNode
//

SphereFragmentSceneNode::FragmentRenderNode::FragmentRenderNode(
    const SphereBspSceneNode* _sceneNode,
    int _theta, int _phi) :
    sceneNode(_sceneNode),
    theta(_theta),
    phi(_phi)
{
    // compute incremented theta and phi
    theta2 = (theta + 1) % SphereLowRes;
    phi2 = phi + 1;
}

SphereFragmentSceneNode::FragmentRenderNode::~FragmentRenderNode()
{
    // do nothing
}

const glm::vec3 &SphereFragmentSceneNode::FragmentRenderNode::getVertex() const
{
    return SphereBspSceneNode::SphereBspRenderNode
           ::lgeom[phi * SphereLowRes + theta];
}

const glm::vec3 &SphereFragmentSceneNode::FragmentRenderNode::
getPosition() const
{
    return sceneNode->getSphere();
}

void            SphereFragmentSceneNode::FragmentRenderNode::render()
{
    const GLfloat pRadius = sceneNode->getRadius();
    const auto &pSphere = getPosition();

    glPushMatrix();
    {
        glTranslatef(pSphere[0], pSphere[1], pSphere[2]);
        glScalef(pRadius, pRadius, pRadius);

        myColor4fv(sceneNode->color);
        if (!BZDBCache::blend && sceneNode->transparent)
            myStipple(sceneNode->color[3]);
        glBegin(GL_TRIANGLE_STRIP);
        {
            if (BZDBCache::lighting)
            {
                glNormal3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi + theta]);
                glVertex3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi + theta]);
                glNormal3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi2 + theta]);
                glVertex3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi2 + theta]);
                glNormal3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi + theta2]);
                glVertex3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi + theta2]);
                glNormal3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi2 + theta2]);
                glVertex3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi2 + theta2]);
                addTriangleCount(2);
            }
            else
            {
                glVertex3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi + theta]);
                glVertex3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi2 + theta]);
                glVertex3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi + theta2]);
                glVertex3fv(SphereBspSceneNode::SphereBspRenderNode::lgeom[SphereLowRes * phi2 + theta2]);
                addTriangleCount(2);
            }
        }
        glEnd(); // 4 verts -> 2 tris
    }
    glPopMatrix();

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
