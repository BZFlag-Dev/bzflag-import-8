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
#include "MeshPolySceneNode.h"

// system headers
#include <assert.h>
#include <math.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

// common implementation headers
#include "Intersect.h"

// FIXME (SceneRenderer.cxx is in src/bzflag)
#include "SceneRenderer.h"

// FIXME - no tessellation is done on for shot lighting


//
// MeshPolySceneNode::Geometry
//

MeshPolySceneNode::Geometry::Geometry(
    MeshPolySceneNode* _node,
    const std::vector<glm::vec3> &_vertices,
    const std::vector<glm::vec3> &_normals,
    const std::vector<glm::vec2> &_texcoords,
    const glm::vec4 &_plane) :
    plane(_plane), vertices(_vertices), normals(_normals), texcoords(_texcoords)
{
    sceneNode = _node;
    style = 0;
    return;
}


MeshPolySceneNode::Geometry::~Geometry()
{
    // do nothing
    return;
}

const glm::vec3 &MeshPolySceneNode::Geometry::getPosition() const
{
    return sceneNode->getSphere();
}

inline void MeshPolySceneNode::Geometry::drawV() const
{
    glBegin(GL_TRIANGLE_FAN);
    for (const auto &v : vertices)
        glVertex3fv(v);
    glEnd();
    return;
}


inline void MeshPolySceneNode::Geometry::drawVT() const
{
    const int count = vertices.size();
    glBegin(GL_TRIANGLE_FAN);
    for (int i = 0; i < count; i++)
    {
        glTexCoord2fv(texcoords[i]);
        glVertex3fv(vertices[i]);
    }
    glEnd();
    return;
}


inline void MeshPolySceneNode::Geometry::drawVN() const
{
    const int count = vertices.size();
    glBegin(GL_TRIANGLE_FAN);
    for (int i = 0; i < count; i++)
    {
        glNormal3fv(normals[i]);
        glVertex3fv(vertices[i]);
    }
    glEnd();
    return;
}


inline void MeshPolySceneNode::Geometry::drawVTN() const
{
    const int count = vertices.size();
    glBegin(GL_TRIANGLE_FAN);
    for (int i = 0; i < count; i++)
    {
        glTexCoord2fv(texcoords[i]);
        glNormal3fv(normals[i]);
        glVertex3fv(vertices[i]);
    }
    glEnd();
    return;
}


void MeshPolySceneNode::Geometry::render()
{
    sceneNode->setColor();

    if (!normals.empty())
    {
        if (style >= 2)
            drawVTN();
        else
            drawVN();
    }
    else
    {
        glNormal3fv(plane);
        if (style >= 2)
            drawVT();
        else
            drawV();
    }

    addTriangleCount(vertices.size() - 2);
    return;
}


void MeshPolySceneNode::Geometry::renderShadow()
{
    drawV();
    addTriangleCount(vertices.size() - 2);
    return;
}


//
// MeshPolySceneNode
//

MeshPolySceneNode::MeshPolySceneNode(const glm::vec4 &_plane,
                                     bool _noRadar, bool _noShadow,
                                     const std::vector<glm::vec3> &vertices,
                                     const std::vector<glm::vec3> &normals,
                                     const std::vector<glm::vec2> &texcoords) :
    node(this, vertices, normals, texcoords, plane)
{
    size_t i;
    int j;
    const size_t count = vertices.size();
    assert(texcoords.size() == count);
    assert((normals.empty()) || (normals.size() == count));

    setPlane(_plane);

    noRadar = _noRadar || (plane[2] <= 0.0f); // pre-cull if we can
    noShadow = _noShadow;

    // choose axis to ignore (the one with the largest normal component)
    int ignoreAxis;
    const auto normal = plane;
    if (fabsf(normal[0]) > fabsf(normal[1]))
    {
        if (fabsf(normal[0]) > fabsf(normal[2]))
            ignoreAxis = 0;
        else
            ignoreAxis = 2;
    }
    else
    {
        if (fabsf(normal[1]) > fabsf(normal[2]))
            ignoreAxis = 1;
        else
            ignoreAxis = 2;
    }

    // project vertices onto plane
    std::vector<glm::vec2> flat(count);
    switch (ignoreAxis)
    {
    case 0:
        for (i = 0; i < count; i++)
        {
            flat[i][0] = vertices[i][1];
            flat[i][1] = vertices[i][2];
        }
        break;
    case 1:
        for (i = 0; i < count; i++)
        {
            flat[i][0] = vertices[i][2];
            flat[i][1] = vertices[i][0];
        }
        break;
    case 2:
        for (i = 0; i < count; i++)
        {
            flat[i][0] = vertices[i][0];
            flat[i][1] = vertices[i][1];
        }
        break;
    }

    // compute area of polygon
    float* area = new float[1];
    area[0] = 0.0f;
    for (j = count - 1, i = 0; i < count; j = i, i++)
        area[0] += flat[j][0] * flat[i][1] - flat[j][1] * flat[i][0];
    area[0] = 0.5f * fabsf(area[0]) / normal[ignoreAxis];

    // set lod info
    setNumLODs(1, area);

    // compute bounding sphere, put center at average of vertices
    auto mySphere = glm::vec3(0.0f);
    for (const auto &v : vertices)
        mySphere += v;
    mySphere /= (float)count;
    setCenter(mySphere);

    float myRadius = 0.0f;
    for (const auto &v : vertices)
    {
        GLfloat r = glm::distance2(mySphere, v);
        if (r > myRadius)
            myRadius = r;
    }
    setRadius(myRadius);

    // record extents info
    for (const auto &v : vertices)
        extents.expandToPoint(v);

    return;
}


MeshPolySceneNode::~MeshPolySceneNode()
{
    return;
}


bool MeshPolySceneNode::cull(const ViewFrustum& frustum) const
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


bool MeshPolySceneNode::inAxisBox (const Extents& exts) const
{
    if (!extents.touches(exts))
        return false;

    return testPolygonInAxisBox (getVertexCount(), getVertices(), plane, exts);
}


int MeshPolySceneNode::split(const glm::vec4 &splitPlane,
                             SceneNode*& front, SceneNode*& back) const
{
    if (!node.normals.empty())
    {
        return splitWallVTN(splitPlane, node.vertices, node.normals, node.texcoords,
                            front, back);
    }
    else
        return splitWallVT(splitPlane, node.vertices, node.texcoords, front, back);
}


void MeshPolySceneNode::addRenderNodes(SceneRenderer& renderer)
{
    node.setStyle(getStyle());
    const auto dyncol = getDynamicColor();
    if ((dyncol == NULL) || (dyncol->a != 0.0f))
        renderer.addRenderNode(&node, getWallGState());
    return;
}


void MeshPolySceneNode::addShadowNodes(SceneRenderer& renderer)
{
    if (!noShadow)
    {
        const auto dyncol = getDynamicColor();
        if ((dyncol == NULL) || (dyncol->a != 0.0f))
            renderer.addShadowNode(&node);
    }
    return;
}


void MeshPolySceneNode::renderRadar()
{
    if (!noRadar)
        node.renderRadar();
    return;
}


int MeshPolySceneNode::splitWallVTN(const glm::vec4 &splitPlane,
                                    const std::vector<glm::vec3> &vertices,
                                    const std::vector<glm::vec3> &normals,
                                    const std::vector<glm::vec2> &texcoords,
                                    SceneNode*& front, SceneNode*& back) const
{
    int i;
    const int count = vertices.size();
    const float fudgeFactor = 0.001f;
    const unsigned char BACK_SIDE = (1 << 0);
    const unsigned char FRONT_SIDE = (1 << 1);

    // arrays for tracking each vertex's side
    // and distance from the splitting plane
    // (assuming stack allocation with be faster then heap, might be wrong)
    // wonder how that compares to static vs. stack access speeds
    const int staticSize = 64;
    float* dists;
    unsigned char* array;
    float staticDists[staticSize];
    unsigned char staticArray[staticSize];
    if (count > staticSize)
    {
        array = new unsigned char[count];
        dists = new float[count];
    }
    else
    {
        array = staticArray;
        dists = staticDists;
    }

    // determine on which side of the plane each point lies
    int bothCount = 0;
    int backCount = 0;
    int frontCount = 0;
    unsigned char *tmpArray = array;
    float *tmpDists  = dists;
    for (const auto &v : vertices)
    {
        const GLfloat d = glm::dot(glm::vec4(v, 1.0f), splitPlane);
        if (d < -fudgeFactor)
        {
            *tmpArray++ = BACK_SIDE;
            backCount++;
        }
        else if (d > fudgeFactor)
        {
            *tmpArray++ = FRONT_SIDE;
            frontCount++;
        }
        else
        {
            *tmpArray++ = BACK_SIDE | FRONT_SIDE;
            bothCount++;
            backCount++;
            frontCount++;
        }
        *tmpDists++ = d; // save for later
    }

    // see if we need to split
    if ((frontCount == 0) || (frontCount == bothCount))
    {
        if (count > staticSize)
        {
            delete[] array;
            delete[] dists;
        }
        return -1; // node is on the back side
    }
    if ((backCount == 0) || (backCount == bothCount))
    {
        if (count > staticSize)
        {
            delete[] array;
            delete[] dists;
        }
        return +1; // node is on the front side
    }

    // get the first old front and back points
    int firstFront = -1, firstBack = -1;

    for (i = 0; i < count; i++)
    {
        const int next = (i + 1) % count; // the next index
        if (array[next] & FRONT_SIDE)
        {
            if (!(array[i] & FRONT_SIDE))
                firstFront = next;
        }
        if (array[next] & BACK_SIDE)
        {
            if (!(array[i] & BACK_SIDE))
                firstBack = next;
        }
    }

    // get the last old front and back points
    int lastFront = (firstFront + frontCount - 1) % count;
    int lastBack = (firstBack + backCount - 1) % count;

    // add in extra counts for the splitting vertices
    if (firstFront != lastBack)
    {
        frontCount++;
        backCount++;
    }
    if (firstBack != lastFront)
    {
        frontCount++;
        backCount++;
    }

    // make space for new polygons
    std::vector<glm::vec3> vertexFront(frontCount);
    std::vector<glm::vec3> normalFront(frontCount);
    std::vector<glm::vec2> uvFront(frontCount);
    std::vector<glm::vec3> vertexBack(backCount);
    std::vector<glm::vec3> normalBack(backCount);
    std::vector<glm::vec2> uvBack(backCount);

    // fill in the splitting vertices
    int frontIndex = 0;
    int backIndex = 0;
    if (firstFront != lastBack)
    {
        glm::vec3 splitVertex, splitNormal;
        glm::vec2 splitUV;
        splitEdgeVTN(dists[firstFront], dists[lastBack],
                     vertices[firstFront], vertices[lastBack],
                     normals[firstFront], normals[lastBack],
                     texcoords[firstFront], texcoords[lastBack],
                     splitVertex, splitNormal, splitUV);
        vertexFront[0] = splitVertex;
        normalFront[0] = splitNormal;
        uvFront[0]     = splitUV;
        frontIndex++; // bump up the head
        const int last   = backCount - 1;
        vertexBack[last] = splitVertex;
        normalBack[last] = splitNormal;
        uvBack[last]     = splitUV;
    }
    if (firstBack != lastFront)
    {
        glm::vec3 splitVertex, splitNormal;
        glm::vec2 splitUV;
        splitEdgeVTN(dists[firstBack], dists[lastFront],
                     vertices[firstBack], vertices[lastFront],
                     normals[firstBack], normals[lastFront],
                     texcoords[firstBack], texcoords[lastFront],
                     splitVertex, splitNormal, splitUV);
        vertexBack[0] = splitVertex;
        normalBack[0] = splitNormal;
        uvBack[0]     = splitUV;
        backIndex++; // bump up the head
        const int last = frontCount - 1;
        vertexFront[last] = splitVertex;
        normalFront[last] = splitNormal;
        uvFront[last]     = splitUV;
    }

    // fill in the old front side vertices
    const int endFront = (lastFront + 1) % count;
    for (i = firstFront; i != endFront; i = (i + 1) % count)
    {
        vertexFront[frontIndex] = vertices[i];
        normalFront[frontIndex] = normals[i];
        uvFront[frontIndex]     = texcoords[i];
        frontIndex++;
    }

    // fill in the old back side vertices
    const int endBack = (lastBack + 1) % count;
    for (i = firstBack; i != endBack; i = (i + 1) % count)
    {
        vertexBack[backIndex] = vertices[i];
        normalBack[backIndex] = normals[i];
        uvBack[backIndex]     = texcoords[i];
        backIndex++;
    }

    // make new nodes
    front = new MeshPolySceneNode(*getPlane(), noRadar, noShadow,
                                  vertexFront, normalFront, uvFront);
    back = new MeshPolySceneNode(*getPlane(), noRadar, noShadow,
                                 vertexBack, normalBack, uvBack);

    // free the arrays, if required
    if (count > staticSize)
    {
        delete[] array;
        delete[] dists;
    }

    return 0; // generated new front and back nodes
}


void MeshPolySceneNode::splitEdgeVTN(float d1, float d2,
                                     const glm::vec3 &p1,  const glm::vec3 &p2,
                                     const glm::vec3 &n1,  const glm::vec3 &n2,
                                     const glm::vec2 &uv1, const glm::vec2 &uv2,
                                     glm::vec3 &p, glm::vec3 &n, glm::vec2 &uv) const
{
    // compute fraction along edge where split occurs
    float t1 = (d2 - d1);
    if (t1 != 0.0f)   // shouldn't happen
        t1 = -(d1 / t1);

    // compute vertex
    p = glm::mix(p1, p2, t1);

    // compute normal
    n = glm::mix(n1, n2, t1);
    // normalize
    if (n.x || n.y || n.z) // otherwise, let it go...
        n = glm::normalize(n);

    // compute texture coordinate
    uv = glm::mix(uv1, uv2, t1);

    return;
}


int MeshPolySceneNode::splitWallVT(const glm::vec4 &splitPlane,
                                   const std::vector<glm::vec3> &vertices,
                                   const std::vector<glm::vec2> &texcoords,
                                   SceneNode*& front, SceneNode*& back) const
{
    int i;
    const int count = vertices.size();
    const float fudgeFactor = 0.001f;
    const unsigned char BACK_SIDE = (1 << 0);
    const unsigned char FRONT_SIDE = (1 << 1);

    // arrays for tracking each vertex's side
    // and distance from the splitting plane
    // (assuming stack allocation with be faster then heap, might be wrong)
    // wonder how that compares to static vs. stack access speeds
    const int staticSize = 64;
    float* dists;
    unsigned char* array;
    float staticDists[staticSize];
    unsigned char staticArray[staticSize];
    if (count > staticSize)
    {
        array = new unsigned char[count];
        dists = new float[count];
    }
    else
    {
        array = staticArray;
        dists = staticDists;
    }

    // determine on which side of the plane each point lies
    int bothCount = 0;
    int backCount = 0;
    int frontCount = 0;
    unsigned char *tmpArray = array;
    float *tmpDists  = dists;
    for (const auto &v : vertices)
    {
        const GLfloat d = glm::dot(glm::vec4(v, 1.0f), splitPlane);
        if (d < -fudgeFactor)
        {
            *tmpArray++ = BACK_SIDE;
            backCount++;
        }
        else if (d > fudgeFactor)
        {
            *tmpArray++ = FRONT_SIDE;
            frontCount++;
        }
        else
        {
            *tmpArray++ = BACK_SIDE | FRONT_SIDE;
            bothCount++;
            backCount++;
            frontCount++;
        }
        *tmpDists++ = d; // save for later
    }

    // see if we need to split
    if ((frontCount == 0) || (frontCount == bothCount))
    {
        if (count > staticSize)
        {
            delete[] array;
            delete[] dists;
        }
        return -1; // node is on the back side
    }
    if ((backCount == 0) || (backCount == bothCount))
    {
        if (count > staticSize)
        {
            delete[] array;
            delete[] dists;
        }
        return +1; // node is on the front side
    }

    // get the first old front and back points
    int firstFront = -1, firstBack = -1;

    for (i = 0; i < count; i++)
    {
        const int next = (i + 1) % count; // the next index
        if (array[next] & FRONT_SIDE)
        {
            if (!(array[i] & FRONT_SIDE))
                firstFront = next;
        }
        if (array[next] & BACK_SIDE)
        {
            if (!(array[i] & BACK_SIDE))
                firstBack = next;
        }
    }

    // get the last old front and back points
    int lastFront = (firstFront + frontCount - 1) % count;
    int lastBack = (firstBack + backCount - 1) % count;

    // add in extra counts for the splitting vertices
    if (firstFront != lastBack)
    {
        frontCount++;
        backCount++;
    }
    if (firstBack != lastFront)
    {
        frontCount++;
        backCount++;
    }

    // make space for new polygons
    std::vector<glm::vec3> vertexFront(frontCount);
    std::vector<glm::vec3> normalFront(0);
    std::vector<glm::vec2> uvFront(frontCount);
    std::vector<glm::vec3> vertexBack(backCount);
    std::vector<glm::vec3> normalBack(0);
    std::vector<glm::vec2> uvBack(backCount);

    // fill in the splitting vertices
    int frontIndex = 0;
    int backIndex = 0;
    if (firstFront != lastBack)
    {
        glm::vec3 splitVertex;
        glm::vec2 splitUV;
        splitEdgeVT(dists[firstFront], dists[lastBack],
                    vertices[firstFront], vertices[lastBack],
                    texcoords[firstFront], texcoords[lastBack],
                    splitVertex, splitUV);
        vertexFront[0] = splitVertex;
        uvFront[0]     = splitUV;
        frontIndex++; // bump up the head
        const int last = backCount - 1;
        vertexBack[last] = splitVertex;
        uvBack[last]     = splitUV;
    }
    if (firstBack != lastFront)
    {
        glm::vec3 splitVertex;
        glm::vec2 splitUV;
        splitEdgeVT(dists[firstBack], dists[lastFront],
                    vertices[firstBack], vertices[lastFront],
                    texcoords[firstBack], texcoords[lastFront],
                    splitVertex, splitUV);
        vertexBack[0] = splitVertex;
        uvBack[0]     = splitUV;
        backIndex++; // bump up the head
        const int last = frontCount - 1;
        vertexFront[last] = splitVertex;
        uvFront[last]     = splitUV;
    }

    // fill in the old front side vertices
    const int endFront = (lastFront + 1) % count;
    for (i = firstFront; i != endFront; i = (i + 1) % count)
    {
        vertexFront[frontIndex] = vertices[i];
        uvFront[frontIndex]     = texcoords[i];
        frontIndex++;
    }

    // fill in the old back side vertices
    const int endBack = (lastBack + 1) % count;
    for (i = firstBack; i != endBack; i = (i + 1) % count)
    {
        vertexBack[backIndex] = vertices[i];
        uvBack[backIndex]     = texcoords[i];
        backIndex++;
    }

    // make new nodes
    front = new MeshPolySceneNode(*getPlane(), noRadar, noShadow,
                                  vertexFront, normalFront, uvFront);
    back = new MeshPolySceneNode(*getPlane(), noRadar, noShadow,
                                 vertexBack, normalBack, uvBack);

    // free the arrays, if required
    if (count > staticSize)
    {
        delete[] array;
        delete[] dists;
    }

    return 0; // generated new front and back nodes
}


void MeshPolySceneNode::splitEdgeVT(float d1, float d2,
                                    const glm::vec3 &p1, const glm::vec3 &p2,
                                    const glm::vec2 &uv1, const glm::vec2 &uv2,
                                    glm::vec3 &p, glm::vec2 &uv) const
{
    // compute fraction along edge where split occurs
    float t1 = (d2 - d1);
    if (t1 != 0.0f)   // shouldn't happen
        t1 = -(d1 / t1);

    // compute vertex
    p = glm::mix(p1, p2, t1);

    // compute texture coordinate
    uv = glm::mix(uv1, uv2, t1);

    return;
}


void MeshPolySceneNode::getRenderNodes(std::vector<RenderSet>& rnodes)
{
    RenderSet rs = { &node, getWallGState() };
    rnodes.push_back(rs);
    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
