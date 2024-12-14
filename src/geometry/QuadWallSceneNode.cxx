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
#include "QuadWallSceneNode.h"

// system headers
#include <math.h>
#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

// common implementation headers
#include "Intersect.h"
#include "StateDatabase.h"

// local implementation headers
#include "ViewFrustum.h"

// FIXME (SceneRenderer.cxx is in src/bzflag)
#include "SceneRenderer.h"

//
// QuadWallSceneNode::Geometry
//

QuadWallSceneNode::Geometry::Geometry(QuadWallSceneNode* _wall,
                                      int uCount, int vCount,
                                      const glm::vec3 &base,
                                      const glm::vec3 &uEdge,
                                      const glm::vec3 &vEdge,
                                      const glm::vec4 &_plane,
                                      const glm::vec2 &uvOffset,
                                      float uRepeats, float vRepeats, bool fixedUVs) :
    wall(_wall),
    style(0),
    ds(uCount),
    dt(vCount),
    dsq(uCount / 4),
    dsr(uCount % 4),
    plane(_plane),
    vertex((uCount+1) * (vCount+1)),
    uv((uCount+1) * (vCount+1))
{
    for (int n = 0, j = 0; j <= vCount; j++)
    {
        const float t = (float)j / (float)vCount;
        for (int i = 0; i <= uCount; n++, i++)
        {
            const float s = (float)i / (float)uCount;
            vertex[n] = base + s * uEdge + t * vEdge;
            uv[n] = uvOffset + glm::vec2(s * uRepeats, t * vRepeats);
        }
    }

    if (!fixedUVs && BZDB.isTrue("remapTexCoords"))
    {
        const float uLen = glm::length(uEdge);
        const float vLen = glm::length(vEdge);
        const float uScale = 10.0f / floorf(10.0f * uLen / uRepeats);
        const float vScale = 10.0f / floorf(10.0f * vLen / vRepeats);
        const auto uvScale = glm::vec2(uScale, vScale);
        if (fabsf(plane[2]) > 0.999f)
        {
            // horizontal surface
            for (unsigned int i = 0; i < vertex.size(); i++)
                uv[i] = uvScale * glm::vec2(vertex[i]);
        }
        else
        {
            // vertical surface
            const auto n = glm::normalize(glm::vec2(plane));
            const float vs = glm::inversesqrt(1.0f - (plane[2] * plane[2]));
            for (unsigned int i = 0; i < vertex.size(); i++)
            {
                const auto v = vertex[i];
                const float uGeoScale = n.x * v[1] - n.y * v[0];
                const float vGeoScale = v[2] * vs;
                uv[i] = uvScale * glm::vec2(uGeoScale, vGeoScale);
            }
        }
    }

    triangles = 2 * (uCount * vCount);
}

QuadWallSceneNode::Geometry::~Geometry()
{
    // do nothing
}

#define RENDER(_e)                          \
  for (int k = 0, t = 0; t < dt; t++) {                 \
    glBegin(GL_TRIANGLE_STRIP);                     \
    for (int s = 0; s < dsq; k += 4, s++) {             \
      _e(k+ds+1);                           \
      _e(k);                                \
      _e(k+ds+2);                           \
      _e(k+1);                              \
      _e(k+ds+3);                           \
      _e(k+2);                              \
      _e(k+ds+4);                           \
      _e(k+3);                              \
    }                                   \
    switch (dsr) {                          \
      case 3:                               \
    _e(k+ds+1);                         \
    _e(k);                              \
    k++;                                \
    /* fall through */                      \
      case 2:                               \
    _e(k+ds+1);                         \
    _e(k);                              \
    k++;                                \
    /* fall through */                      \
      case 1:                               \
    _e(k+ds+1);                         \
    _e(k);                              \
    k++;                                \
    /* fall through */                      \
      case 0:                               \
    /* don't forget right edge of last quad on row */       \
    _e(k+ds+1);                         \
    _e(k);                              \
    k++;                                \
    }                                   \
    glEnd();                                \
  }
#define EMITV(_i)   glVertex3fv(vertex[_i])
#define EMITVT(_i)  glTexCoord2fv(uv[_i]); glVertex3fv(vertex[_i])

const glm::vec3 &QuadWallSceneNode::Geometry::getPosition() const
{
    return wall->getSphere();
}

void            QuadWallSceneNode::Geometry::render()
{
    wall->setColor();
    glNormal3fv(plane);
    if (style >= 2)
        drawVT();
    else
        drawV();
    addTriangleCount(triangles);
    return;
}

void            QuadWallSceneNode::Geometry::renderShadow()
{
    int last = (ds + 1) * dt;
    glBegin(GL_TRIANGLE_STRIP);
    glVertex3fv(vertex[last]);
    glVertex3fv(vertex[0]);
    glVertex3fv(vertex[last + ds]);
    glVertex3fv(vertex[ds]);
    glEnd();
    addTriangleCount(2);
}

void            QuadWallSceneNode::Geometry::drawV() const
{
    RENDER(EMITV)
}

void            QuadWallSceneNode::Geometry::drawVT() const
{
    RENDER(EMITVT)
}

const glm::vec3 &QuadWallSceneNode::Geometry::getVertex(int i) const
{
    return vertex[i];
}

//
// QuadWallSceneNode
//

QuadWallSceneNode::QuadWallSceneNode(const glm::vec3 &base,
                                     const glm::vec3 &uEdge,
                                     const glm::vec3 &vEdge,
                                     const glm::vec2 &uvOffset,
                                     float uRepeats,
                                     float vRepeats,
                                     bool makeLODs)
{
    init(base, uEdge, vEdge, uvOffset, uRepeats, vRepeats, makeLODs, false);
}

QuadWallSceneNode::QuadWallSceneNode(const glm::vec3 &base,
                                     const glm::vec3 &uEdge,
                                     const glm::vec3 &vEdge,
                                     float uRepeats,
                                     float vRepeats,
                                     bool makeLODs, bool fixedUVs)
{
    init(base, uEdge, vEdge, glm::vec2(0.0f), uRepeats, vRepeats, makeLODs, fixedUVs);
}

void            QuadWallSceneNode::init(const glm::vec3 &base,
                                        const glm::vec3 &uEdge,
                                        const glm::vec3 &vEdge,
                                        const glm::vec2 &uvOffset,
                                        float uRepeats,
                                        float vRepeats,
                                        bool makeLODs,
                                        bool fixedUVs)
{
    // record plane and bounding sphere info
    auto n = glm::cross(uEdge, vEdge);
    auto myPlane = glm::vec4(n, -glm::dot(n, base));
    setPlane(myPlane);

    auto mySphere = 0.5f * (uEdge + vEdge);

    setRadius(glm::length2(mySphere));

    mySphere += base;
    setCenter(mySphere);

    // get length of sides
    const float uLength = glm::length(uEdge);
    const float vLength = glm::length(vEdge);
    float area = uLength * vLength;

    // If negative then these values aren't a number of times to repeat
    // the texture along the surface but the width, or a desired scaled
    // width, of the texture itself. Repeat the texture as many times
    // as necessary to fit the surface.
    if (uRepeats < 0.0f)
        uRepeats = - uLength / uRepeats;

    if (vRepeats < 0.0f)
        vRepeats = - vLength / vRepeats;

    // compute how many LODs required to get smaller edge down to
    // elements no bigger than 4 units on a side.
    int uElements = int(uLength) / 2;
    int vElements = int(vLength) / 2;
    int uLevels = 1, vLevels = 1;
    while (uElements >>= 1) uLevels++;
    while (vElements >>= 1) vLevels++;
    int numLevels = std::min(uLevels, vLevels);

    // if overly rectangular then add levels to square it up
    bool needsSquaring = false;
    if (makeLODs)
    {
        if (uLevels >= vLevels+2)
        {
            needsSquaring = true;
            numLevels += (uLevels - vLevels) / 2;
        }
        else if (vLevels >= uLevels+2)
        {
            needsSquaring = true;
            numLevels += (vLevels - uLevels) / 2;
        }
    }

    // if no lod's required then don't make any except most coarse
    if (!makeLODs)
        numLevels = 1;

    // make level of detail and element area arrays
    nodes = new Geometry*[numLevels];
    float* areas = new float[numLevels];

    // make top level (single polygon)
    int level = 0;
    uElements = 1;
    vElements = 1;
    areas[level] = area;
    nodes[level++] = new Geometry(this, uElements, vElements,
                                  base, uEdge, vEdge,
                                  plane, uvOffset,
                                  uRepeats, vRepeats, fixedUVs);
    shadowNode = new Geometry(this, uElements, vElements,
                              base, uEdge, vEdge,
                              plane, uvOffset,
                              uRepeats, vRepeats, fixedUVs);
    shadowNode->setStyle(0);

    // make squaring levels if necessary
    if (needsSquaring)
    {
        uElements = 1;
        vElements = 1;
        if (uLevels > vLevels)
        {
            int count = (uLevels - vLevels) / 2;
            while (count-- > 0)
            {
                uElements <<= 2;
                areas[level] = area / (float)uElements;
                nodes[level++] = new Geometry(this, uElements, vElements,
                                              base, uEdge, vEdge,
                                              plane, uvOffset,
                                              uRepeats, vRepeats, fixedUVs);

            }
            area /= (float)uElements;
        }
        else
        {
            int count = (vLevels - uLevels) / 2;
            while (count-- > 0)
            {
                vElements <<= 2;
                areas[level] = area / (float)vElements;
                nodes[level++] = new Geometry(this, uElements, vElements,
                                              base, uEdge, vEdge,
                                              plane, uvOffset,
                                              uRepeats, vRepeats, fixedUVs);

            }
            area /= (float)vElements;
        }
    }

    // make remaining levels by doubling elements in each dimension
    while (level < numLevels)
    {
        uElements <<= 1;
        vElements <<= 1;
        area *= 0.25f;
        areas[level] = area;
        nodes[level++] = new Geometry(this, uElements, vElements,
                                      base, uEdge, vEdge,
                                      plane, uvOffset,
                                      uRepeats, vRepeats, fixedUVs);
    }

    // record extents info
    for (int i = 0; i < 4; i++)
    {
        const auto point = getVertex(i);
        extents.expandToPoint(point);
    }

    // record LOD info
    setNumLODs(numLevels, areas);
}

QuadWallSceneNode::~QuadWallSceneNode()
{
    // free LODs
    const int numLevels = getNumLODs();
    for (int i = 0; i < numLevels; i++)
        delete nodes[i];
    delete[] nodes;
    delete shadowNode;
}

int         QuadWallSceneNode::split(const glm::vec4 &_plane,
                                     SceneNode*& front, SceneNode*& back) const
{
    // need to reorder vertices into counterclockwise order
    std::vector<glm::vec3> vertex(4);
    std::vector<glm::vec2> uv(4);
    for (int i = 0; i < 4; i++)
    {
        int j = i;
        if (j == 2 || j == 3) j = 5 - j;
        vertex[i] = nodes[0]->vertex[j];
        uv[i] = nodes[0]->uv[j];
    }
    return WallSceneNode::splitWall(_plane, vertex, uv, front, back);
}

void            QuadWallSceneNode::addRenderNodes(
    SceneRenderer& renderer)
{
    const int lod = pickLevelOfDetail(renderer);
    nodes[lod]->setStyle(getStyle());
    renderer.addRenderNode(nodes[lod], getWallGState());
}

void            QuadWallSceneNode::addShadowNodes(
    SceneRenderer& renderer)
{
    renderer.addShadowNode(shadowNode);
}

bool            QuadWallSceneNode::inAxisBox(const Extents& exts) const
{
    if (!extents.touches(exts))
        return false;

    // NOTE: inefficient
    glm::vec3 vertices[4];
    vertices[0] = nodes[0]->getVertex(0);
    vertices[1] = nodes[0]->getVertex(1);
    vertices[2] = nodes[0]->getVertex(2);
    vertices[3] = nodes[0]->getVertex(3);

    return testPolygonInAxisBox (4, vertices, plane, exts);
}

int         QuadWallSceneNode::getVertexCount () const
{
    return 4;
}

const glm::vec3 &QuadWallSceneNode::getVertex (int vertex) const
{
    // re-map these to a counter-clockwise order
    const int order[4] = {0, 1, 3, 2};
    return nodes[0]->getVertex(order[vertex]);
}


void            QuadWallSceneNode::getRenderNodes(std::vector<RenderSet>& rnodes)
{
    RenderSet rs = { nodes[0], getWallGState() };
    rnodes.push_back(rs);
    return;
}


void            QuadWallSceneNode::renderRadar()
{
    if (plane[2] > 0.0f)
        nodes[0]->renderRadar();
    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
