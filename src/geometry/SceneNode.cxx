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
#include "SceneNode.h"

// system implementation headers
#include <string.h>
#include <math.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

// common implementation headers
#include "Extents.h"
#include "RenderNode.h"
#include "StateDatabase.h"

// FIXME (SceneRenderer.cxx is in src/bzflag)
#include "SceneRenderer.h"

void            (*SceneNode::stipple)(GLfloat);

SceneNode::SceneNode()
{
    static bool init = false;

    if (!init)
    {
        init = true;
        setColorOverride(false);
    }

    setCenter(glm::vec3(0.0f));
    setRadius(0.0f);

    occluder = false;
    octreeState = OctreeCulled;

    return;
}

SceneNode::~SceneNode()
{
    // do nothing
}


bool            SceneNode::colorOverride = true;
void            SceneNode::noStipple(GLfloat) {}

void            SceneNode::setColorOverride(bool on)
{
    colorOverride = on;
    if (on)
        stipple  = &noStipple;
    else
        stipple  = &OpenGLGState::setStipple;
}

void            SceneNode::setRadius(GLfloat radiusSquared)
{
    radius2 = radiusSquared;
}

void            SceneNode::setCenter(const glm::vec3 &_center)
{
    sphere = _center;
}

void            SceneNode::notifyStyleChange()
{
    // do nothing
}

void            SceneNode::addRenderNodes(SceneRenderer&)
{
    // do nothing
}

void            SceneNode::addShadowNodes(SceneRenderer&)
{
    // do nothing
}

void            SceneNode::addLight(SceneRenderer&)
{
    // do nothing
}

GLfloat SceneNode::getDistance(const glm::vec3 &eye) const
{
    return glm::distance2(eye, sphere);
}

int SceneNode::split(const glm::vec4 &, SceneNode*&, SceneNode*&) const
{
    // can't split me
    return 1;
}

bool            SceneNode::cull(const ViewFrustum& view) const
{
    // if center of object is outside view frustum and distance is
    // greater than radius of object then cull.
    const int planeCount = view.getPlaneCount();
    const auto sphere4 = glm::vec4(sphere, 1.0f);
    for (int i = 0; i < planeCount; i++)
    {
        const auto norm = view.getSide(i);
        const GLfloat d = glm::dot(sphere4, norm);
        if ((d < 0.0f) && (d * d > radius2))
            return true;
    }
    return false;
}


bool SceneNode::cullShadow(int, const glm::vec4 []) const
{
    // currently only used for dynamic nodes by ZSceneDatabase
    // we let the octree deal with the static nodes
    return true;
}


bool SceneNode::inAxisBox (const Extents& exts) const
{
    if (!extents.touches(exts))
        return false;
    return true;
}

int SceneNode::getVertexCount () const
{
    return 0;
}

const auto zeroVector = glm::vec3(0.0f);
const glm::vec3 &SceneNode::getVertex (int) const
{
    return zeroVector;
}

const glm::vec4 *SceneNode::getPlane() const
{
    return NULL;
}


//
// GLfloat2Array
//

GLfloat2Array::GLfloat2Array(const GLfloat2Array& a) :
    size(a.size)
{
    data = new GLfloat2[size];
    ::memcpy(data, a.data, size * sizeof(GLfloat2));
}

GLfloat2Array&      GLfloat2Array::operator=(const GLfloat2Array& a)
{
    if (this != &a)
    {
        delete[] data;
        size = a.size;
        data = new GLfloat2[size];
        ::memcpy(data, a.data, size * sizeof(GLfloat2));
    }
    return *this;
}


//
// GLfloat3Array
//

GLfloat3Array::GLfloat3Array(const GLfloat3Array& a) :
    size(a.size)
{
    data = new GLfloat3[size];
    ::memcpy(data, a.data, size * sizeof(GLfloat3));
}

GLfloat3Array&      GLfloat3Array::operator=(const GLfloat3Array& a)
{
    if (this != &a)
    {
        delete[] data;
        size = a.size;
        data = new GLfloat3[size];
        ::memcpy(data, a.data, size * sizeof(GLfloat3));
    }
    return *this;
}


void SceneNode::getRenderNodes(std::vector<RenderSet>&)
{
    return; // do nothing
}


void SceneNode::renderRadar()
{
    printf ("SceneNode::renderRadar() called, implement in subclass\n");
    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
