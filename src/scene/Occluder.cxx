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

// Interface headers
#include "Occluder.h"

// System headers
#include <stdlib.h>
#include <math.h>
#include <glm/geometric.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

// Common headers
#include "SceneNode.h"
#include "Frustum.h"
#include "Intersect.h"
#include "StateDatabase.h"
#include "OpenGLAPI.h"


//////////////////////////////////////////////////////////////////////////////
//
// The Occluder Manager
//

const int OccluderManager::MaxOccluders = MAX_OCCLUDERS;

OccluderManager::OccluderManager()
{
    activeOccluders = 0;
    allowedOccluders = 0;
    for (int i = 0; i < MaxOccluders; i++)
        occluders[i] = NULL;
}


OccluderManager::~OccluderManager()
{
    clear();
}


void OccluderManager::clear()
{
    activeOccluders = 0;
    for (int i = 0; i < MaxOccluders; i++)
    {
        delete occluders[i];
        occluders[i] = NULL;
    }
}


void OccluderManager::setMaxOccluders(int size)
{
    if (size > MaxOccluders)
        size = MaxOccluders;
    else if (size == 1)
    {
        size = 2; // minimum of two: one active, one scanning
    }
    else if (size < 0)
        size = 0;

    allowedOccluders = size;

    if (activeOccluders > allowedOccluders)
    {
        for (int i = allowedOccluders; i < activeOccluders; i++)
        {
            delete occluders[i];
            occluders[i] = NULL;
        }
        activeOccluders = allowedOccluders;
    }

    return;
}


IntersectLevel OccluderManager::occlude(const Extents& exts,
                                        unsigned int score)
{
    IntersectLevel level = Outside;

    for (int i = 0; i < activeOccluders; i++)
    {
        Occluder* oc = occluders[i];
        IntersectLevel tmp = oc->doCullAxisBox (exts);
        if (tmp == Contained)
        {
            oc->addScore (score);
            return Contained;
            // FIXME - this only makes sense for randomly selected
            //     occluders where there can be overlap
        }
        else if (tmp == Partial)
            level = Partial;
    }

    return level;
}


bool OccluderManager::occludePeek(const Extents& exts)
{
    int i;
    bool result = false;

    // doesn't adjust occluder scores
    for (i = 0; i < activeOccluders; i++)
    {
        Occluder* oc = occluders[i];
        if (oc->doCullAxisBox (exts) == Contained)
            result = true;
    }

    return result;
}


void OccluderManager::update(const Frustum* frustum)
{
//  const float* e = frustum->getEye ();
//  printf ("Eye = %f, %f, %f\n", e[0], e[1], e[2]);

    for (int i = 0; i < activeOccluders; i++)
    {
        if (!occluders[i]->makePlanes (frustum))
        {
            delete occluders[i];
            activeOccluders--;
            occluders[i] = occluders[activeOccluders];
            occluders[activeOccluders] = NULL;
            i--; // this index is a different occluder, test it again
        }
    }

    return;
}


static void print_scores (Occluder** olist, int count, const char* str)
{
    return;
    // FIXME - debugging
    bool first = true;
    for (int i = 0; i < count; i++)
    {
        int score = olist[i]->getScore();
        if (score > 0)
        {
            if (first)
            {
                printf ("%s(%i):", str, count);
                first = false;
            }
            printf (" %i", score);
        }
    }
    if (!first)
        putchar ('\n');
}


void OccluderManager::select(const SceneNode* const* list, int listCount)
{
    int oc;

    // see if our limit has changed
    int max = BZDB.evalInt (StateDatabase::BZDB_CULLOCCLUDERS);
    if (max != allowedOccluders)
        setMaxOccluders (max);

    // don't need more occluders then scenenodes
    if (activeOccluders > listCount)
    {
        for (int i = listCount; i < activeOccluders; i++)
        {
            delete occluders[i];
            occluders[i] = NULL;
        }
        activeOccluders = listCount;
    }

    // remove the useless occluders
    for (oc = 0; oc < activeOccluders; oc++)
    {
        if ((occluders[oc]->getScore() <= 0) ||
                (oc == (allowedOccluders - 1)))   // always have a spare
        {
            delete occluders[oc];
            activeOccluders--;
            occluders[oc] = occluders[activeOccluders];
            occluders[activeOccluders] = NULL;
        }
    }

    // sort before picking a new occluder
    // they are sorted in descending order  (occluders[0] has the best score)
    if (activeOccluders > 1)
        sort();

    // pick a new one, this will obviously require a better algorithm,
    // i'm hoping to use a GL query extension (preferably ARB, then NV)
    // could also use the cross-sign value from update(), distance to eye,
    // area of scene node, etc...
    int target = allowedOccluders;
    if (listCount < allowedOccluders)
        target = listCount;
    while (activeOccluders < target)
    {
        const SceneNode* sceneNode = list[rand() % listCount];
        occluders[activeOccluders] = new Occluder(sceneNode);
        if (occluders[activeOccluders]->getVertexCount() == 0)
        {
            delete occluders[activeOccluders];
            occluders[activeOccluders] = NULL;
            target--; // protect against a list full of nonvalid occluders.
            // could also tally the valid occluder sceneNodes, but
            // that would eat up CPU time in a large list
        }
        else
            activeOccluders++;
    }

    // FIXME  - debugging
    print_scores (occluders, activeOccluders, "prediv");

    // decrease the scores
    for (oc = 0; oc < activeOccluders; oc++)
        occluders[oc]->divScore();

    return;
}


static int compareOccluders (const void* a, const void* b)
{
    const Occluder* const * ptrA = (const Occluder* const *)a;
    const Occluder* const * ptrB = (const Occluder* const *)b;
    int scoreA = (*ptrA)->getScore();
    int scoreB = (*ptrB)->getScore();

    if (scoreA > scoreB)
        return -1;
    else if (scoreA < scoreB)
        return +1;
    else
        return 0;
}


void OccluderManager::sort()
{
    qsort (occluders, activeOccluders, sizeof (Occluder*), compareOccluders);
}


void OccluderManager::draw() const
{
    for (int i = 0; i < activeOccluders; i++)
        occluders[i]->draw();
    return;
}


//////////////////////////////////////////////////////////////////////////////
//
// The Occluders
//

const bool Occluder::DrawEdges = true;
const bool Occluder::DrawNormals = false;
const bool Occluder::DrawVertices = true;

Occluder::Occluder(const SceneNode* node)
{
    sceneNode = node;
    planes = NULL;
    vertices = NULL;
    cullScore = 0;

    vertexCount = node->getVertexCount();
    if (!node->isOccluder())
    {
        vertexCount = 0; // used to flag a bad occluder
        return;
    }
    vertices = new glm::vec3[vertexCount];

    planeCount = vertexCount + 1; // the occluder's plane normal
    planes = new glm::vec4[planeCount];

    // counter-clockwise order as viewed from the front face
    for (int i = 0; i < vertexCount; i++)
    {
        const auto vertex = node->getVertex(i);
        vertices[i] = vertex;
    }

    return;
}


Occluder::~Occluder()
{
    // do nothing
    delete[] planes;
    delete[] vertices;
}


IntersectLevel Occluder::doCullAxisBox(const Extents& exts)
{
    return testAxisBoxOcclusion (exts, planes, planeCount);
}


static bool makePlane (const glm::vec3 &p1, const glm::vec3 &p2, const glm::vec3 &pc,
                       glm::vec4 &r)
{
    // make vectors from points
    auto x = p1 - pc;
    auto y = p2 - pc;

    // cross product to get the normal
    auto n = glm::cross(x, y);

    // normalize
    float len = glm::length2(n);
    if (len < +0.001f)
        return false;
    else
        len = glm::inversesqrt(len);
    n *= len;

    // finish the plane equation: {rx*px + ry*py + rz+pz + rd = 0}
    r = glm::vec4(n, -glm::dot(pc, n));

    return true;
}

bool Occluder::makePlanes(const Frustum* frustum)
{
    // occluders can't have their back towards the camera
    const auto eye = glm::vec4(frustum->getEye(), 1.0f);
    const auto &p = *sceneNode->getPlane();
    float tmp = glm::dot(p, eye);
    if (tmp < +0.1f)
        return false;
    // FIXME - store/flag this so we don't have to do it again?

    // make the occluder's normal plane
    const auto &plane = *sceneNode->getPlane();
    planes[0] = -plane;

    // make the edges planes
    for (int i = 0; i < vertexCount; i++)
    {
        int second = (i + vertexCount - 1) % vertexCount;
        if (!makePlane (vertices[i], vertices[second], eye, planes[i + 1]))
            return false;
    }

    return true;
}


void Occluder::draw() const
{
    int v;
    glm::vec4 colors[5] =
    {
        {1.0f, 0.0f, 1.0f, 1.0f}, // purple  (occluder's normal)
        {1.0f, 0.0f, 0.0f, 1.0f}, // red
        {0.0f, 1.0f, 0.0f, 1.0f}, // green
        {0.0f, 0.0f, 1.0f, 1.0f}, // blue
        {1.0f, 1.0f, 0.0f, 1.0f}, // yellow
    };
    const float length = 5.0f;

    glLineWidth (3.0f);
    glPointSize (10.0f);
    glEnable (GL_POINT_SMOOTH);

    if (DrawNormals)
    {
        // the tri-wall 'getSphere()' center sucks...
        glm::vec3 center;
        for (int a = 0; a < 3; a++)
        {
            center[a] = 0.0f;
            for (v = 0; v < vertexCount; v++)
                center[a] += vertices[v][a];
            center[a] = center[a] / (float) vertexCount;
        }

        auto outwards = center - length * glm::vec3(planes[0]);

        // draw the plane normal
        glBegin (GL_LINES);
        glColor(colors[0]);
        glVertex(center);
        glVertex(outwards);
        glEnd ();
    }

    // drawn the edges and normals
    if (DrawEdges || DrawNormals)
    {
        for (v = 0; v < vertexCount; v++)
        {
            glm::vec3 midpoint;
            int vn = (v + 1) % vertexCount;
            for (int a = 0; a < 3; a++)
                midpoint[a] = 0.5f * (vertices[v][a] + vertices[vn][a]);
            auto outwards = midpoint - length * glm::vec3(planes[vn + 1]);
            glBegin (GL_LINES);
            glColor(colors[(v % 4) + 1]);
            if (DrawEdges)
            {
                glVertex(vertices[v]);
                glVertex(vertices[vn]);
            }
            if (DrawNormals)
            {
                glVertex(midpoint);
                glVertex(outwards);
            }
            glEnd();
        }
    }

    // draw some nice vertex points
    if (DrawVertices)
    {
        for (v = 0; v < vertexCount; v++)
        {
            glBegin (GL_POINTS);
            glColor(colors[(v % 4) + 1]);
            glVertex(vertices[v]);
            glEnd();
        }
    }

    glDisable (GL_POINT_SMOOTH);
    glPointSize (1.0f);
    glLineWidth (1.0f);

//  print("Occluder::draw");

    return;
}


void Occluder::print(const char* string) const
{
    // FIXME - debugging
    printf ("%s: %p, V = %i, P = %i\n", string, (const void *)sceneNode, vertexCount, planeCount);
    for (int v = 0; v < vertexCount; v++)
        printf ("  v%i: %f, %f, %f\n", v, vertices[v][0], vertices[v][1],vertices[v][2]);
    for (int p = 0; p < planeCount; p++)
        printf ("  p%i: %f, %f, %f, %f\n", p, planes[p][0], planes[p][1],planes[p][2],planes[p][3]);

    return;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
