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


// implementation header
#include "Triangulate.h"

// system headers
#include <string.h>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/mixed_product.hpp>
#include <glm/gtx/vector_query.hpp>

// triangulation parameters
static glm::vec3 Normal; // FIXME, uNormal, vNormal;
static const glm::vec3 * const *Verts = NULL;
static int Count = 0;
static int* WorkSet = NULL;


static inline void makeNormal()
{
    // Newell method
    Normal[0] = Normal[1] = Normal[2] = 0.0f;
    for (int i = 0; i < Count; i++)
    {
        const auto &v0 = *Verts[i];
        const auto &v1 = *Verts[(i + 1) % Count];
        Normal[0] += ((v0[1] - v1[1]) * (v0[2] + v1[2]));
        Normal[1] += ((v0[2] - v1[2]) * (v0[0] + v1[0]));
        Normal[2] += ((v0[0] - v1[0]) * (v0[1] + v1[1]));
    }

    // normalize
    if (glm::isNull(Normal, 1.0e-6f))
    {
        Normal = glm::vec3(0.0f);
        return;
    }

    Normal = glm::normalize(Normal);
}


static inline bool isConvex(int w0, int w1, int w2)
{
    // caution: faces can fold around the normal
    const int v0 = WorkSet[w0];
    const int v1 = WorkSet[w1];
    const int v2 = WorkSet[w2];
    auto &vert0 = *Verts[v0];
    auto &vert1 = *Verts[v1];
    auto &vert2 = *Verts[v2];
    auto dotcross = glm::mixedProduct(vert1 - vert0, vert2 - vert1, Normal);
    return dotcross > 0.0f;
}


static inline bool isFaceClear(int w0, int w1, int w2)
{
    int i;
    const int v0 = WorkSet[w0];
    const int v1 = WorkSet[w1];
    const int v2 = WorkSet[w2];

    auto &vert0 = *Verts[v0];
    auto &vert1 = *Verts[v1];
    auto &vert2 = *Verts[v2];

    // setup the edges
    glm::vec3 edges[3];
    edges[0] = vert1 - vert0;
    edges[1] = vert2 - vert1;
    edges[2] = vert0 - vert2;

    // get the triangle normal
    auto normal = glm::cross(edges[0], edges[1]);

    // setup the planes
    glm::vec3 planes[3];
    float planesW[3];
    planes[0] = glm::cross(edges[0], normal);
    planes[1] = glm::cross(edges[1], normal);
    planes[2] = glm::cross(edges[2], normal);
    planesW[0] = -glm::dot(planes[0], vert0);
    planesW[1] = -glm::dot(planes[1], vert1);
    planesW[2] = -glm::dot(planes[2], vert2);

    for (int w = 0; w < Count; w++)
    {
        if ((w == w0) || (w == w1) || (w == w2))
        {
            continue; // FIXME: lazy
        }
        const int v = WorkSet[w];
        auto &vertV = *Verts[v];
        for (i = 0; i < 3; i++)
        {
            const float dist = glm::dot(planes[i], vertV) + planesW[i];
            if (dist > 0.0f)
            {
                break; // this point is clear
            }
        }
        if (i == 3)
            return false;
    }
    return true;
}


static inline float getDot(int w0, int w1, int w2)
{
    const int v0 = WorkSet[w0];
    const int v1 = WorkSet[w1];
    const int v2 = WorkSet[w2];

    auto &vert0 = *Verts[v0];
    auto &vert1 = *Verts[v1];
    auto &vert2 = *Verts[v2];

    return glm::dot(glm::normalize(vert1 - vert0), glm::normalize(vert2 - vert1));
}


void triangulateFace(int count, const glm::vec3 * const verts[],
                     std::vector<TriIndices>& tris)
{
    tris.clear();

    Verts = verts;
    Count = count;
    WorkSet = new int[Count];
    for (int i = 0; i < Count; i++)
        WorkSet[i] = i;
    makeNormal();

    int best = 0;
    bool left = false;
    bool first = true;
    float score = 0.0f;

    while (Count >= 3)
    {
        bool convex = false;
        bool faceClear = false;

        int offset;
        if (best == Count)
            offset = Count - 1;
        else
            offset = (best % Count);

        // stripping pattern
        if (left)
            offset = (offset + (Count - 1)) % Count;
        left = !left;

        // find the best triangle
        for (int w = offset; w < offset + (Count - 2); w++)
        {
            const int w0 = (w + 0) % Count;
            const int w1 = (w + 1) % Count;
            const int w2 = (w + 2) % Count;

            const bool convex2 = isConvex(w0, w1, w2);
            if (convex && !convex2)
                continue;

            const bool faceClear2 = isFaceClear(w0, w1, w2);
            if ((faceClear && !faceClear2) && (convex || !convex2))
                continue;

            if (first)
            {
                const float score2 = 2.0f - getDot(w0, w1, w2);
                if ((score2 < score) &&
                        (convex || !convex2) && (faceClear || !faceClear2))
                    continue;
                else
                    score = score2;
            }

            best = w0;
            if (convex && faceClear)
                break;
            convex = convex2;
            faceClear = faceClear2;
        }

        first = false;

        // add the triangle
        TriIndices ti;
        ti.indices[0] = WorkSet[(best + 0) % Count];
        ti.indices[1] = WorkSet[(best + 1) % Count];
        ti.indices[2] = WorkSet[(best + 2) % Count];
        tris.push_back(ti);

        // remove the middle vertex
        const int m = (best + 1) % Count;
        memmove(WorkSet + m, WorkSet + m + 1, (Count - m - 1) * sizeof(int));
        Count--;
    }

    delete[] WorkSet;

    return;
}


// Local variables: ***
// mode: C++ ***
// tab-width: 4***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
