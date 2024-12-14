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
#include "OccluderSceneNode.h"

// system headers
#include <math.h>
#include <string.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

// common implementation headers
#include "MeshFace.h"
#include "Intersect.h"
#include "ViewFrustum.h"


OccluderSceneNode::OccluderSceneNode(const MeshFace* face) :
    plane(face->getPlane())
{
    int i;

    setOccluder(true);

    // record extents info
    extents = face->getExtents();

    // record vertex info
    vertexCount = face->getVertexCount();
    vertices = new glm::vec3[vertexCount];
    for (i = 0; i < vertexCount; i++)
        vertices[i] = face->getVertex(i);

    // record sphere info
    auto mySphere = glm::vec3(0.0f);
    for (i = 0; i < vertexCount; i++)
    {
        const auto v = vertices[i];
        mySphere += v;
    }
    mySphere /= (float)vertexCount;
    setCenter(mySphere);

    float myRadius  = 0.0f;
    for (i = 0; i < vertexCount; i++)
    {
        const auto v = vertices[i];
        GLfloat r = glm::distance2(mySphere, v);
        if (r > myRadius)
            myRadius = r;
    }
    setRadius(myRadius);

    return;
}


OccluderSceneNode::~OccluderSceneNode()
{
    delete[] vertices;
    return;
}


const glm::vec4 *OccluderSceneNode::getPlane() const
{
    return &plane;
}

bool OccluderSceneNode::cull(const ViewFrustum& frustum) const
{
    // cull if eye is behind (or on) plane
    const auto eye = glm::vec4(frustum.getEye(), 1.0f);
    if (glm::dot(eye, plane) <= 0.0f)
        return true;

    // if the Visibility culler tells us that we're
    // fully visible, then skip the rest of these tests
    if (octreeState == OctreeVisible)
        return false;

    const Frustum* f = (const Frustum *) &frustum;
    if (testAxisBoxInFrustum(extents, f) == Outside)
        return true;

    // probably visible
    return false;
}


bool OccluderSceneNode::inAxisBox (const Extents& exts) const
{
    if (!extents.touches(exts))
        return false;

    return testPolygonInAxisBox (vertexCount, vertices, plane, exts);
}

void OccluderSceneNode::addShadowNodes(SceneRenderer&)
{
    return;
}

void OccluderSceneNode::addRenderNodes(SceneRenderer&)
{
    return;
}

void OccluderSceneNode::renderRadar()
{
    return;
}

int OccluderSceneNode::getVertexCount () const
{
    return vertexCount;
}

const glm::vec3 &OccluderSceneNode::getVertex (int vertex) const
{
    return vertices[vertex];
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
