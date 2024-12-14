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

#include "SceneNode.h"
#include "Frustum.h"
#include "Intersect.h"
#include "Extents.h"

class Occluder
{
public:
    Occluder(const SceneNode *node);
    ~Occluder();
    bool makePlanes(const Frustum* frustum);
    IntersectLevel doCullAxisBox(const Extents& exts);
    void addScore(unsigned int score);
    void divScore();
    int getScore() const;
    int getVertexCount() const;
    const SceneNode* getSceneNode() const;
    void draw() const;
    void print(const char* string) const; // for debugging

private:
    const SceneNode* sceneNode;
    unsigned int cullScore;
    int planeCount;  // one more then the vertex count
    int vertexCount; // vertex count of the occluding plane
    glm::vec4 *planes;
    glm::vec3 *vertices;
    static const bool DrawEdges;
    static const bool DrawNormals;
    static const bool DrawVertices;
};

#define MAX_OCCLUDERS 64

class OccluderManager
{

public:
    OccluderManager();
    ~OccluderManager();

    void clear();
    void update(const Frustum* frustum);
    void select(const SceneNode* const* list, int listCount);

    IntersectLevel occlude(const Extents& exts, unsigned int score);
    bool occludePeek(const Extents& exts);

    int getOccluderCount() const;

    void draw() const;

private:
    void setMaxOccluders(int size);
    void sort();
    int activeOccluders;
    int allowedOccluders;
    static const int MaxOccluders;
    Occluder* occluders[MAX_OCCLUDERS];
};

inline void Occluder::addScore(unsigned int score)
{
    unsigned int tmp = cullScore + score;
    if (tmp > cullScore)
        cullScore = tmp;
    return;
}

inline void Occluder::divScore()
{
    cullScore = cullScore >> 1;
    return;
}

inline int Occluder::getScore() const
{
    return cullScore;
}

inline const SceneNode* Occluder::getSceneNode()const
{
    return sceneNode;
}

inline int Occluder::getVertexCount() const
{
    return vertexCount;
}

inline int OccluderManager::getOccluderCount () const
{
    return activeOccluders;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
