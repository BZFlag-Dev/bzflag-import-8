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

#ifndef __MESHSCENENODEGENERATOR_H__
#define __MESHSCENENODEGENERATOR_H__

#include "SceneNode.h"
#include "MeshFace.h"
#include "MeshObstacle.h"
#include "BzMaterial.h"
#include <vector>

class WallSceneNode;
class MeshPolySceneNode;

class MeshSceneNodeGenerator
{

    friend class SceneDatabaseBuilder;

public:
    ~MeshSceneNodeGenerator();

    WallSceneNode* getNextNode(bool lod);

    static void setupNodeMaterial(WallSceneNode* node,
                                  const BzMaterial* mat);
    static MeshPolySceneNode* getMeshPolySceneNode(const MeshFace* face);

    static void makeTexcoords(const glm::vec4 &plane,
                              const std::vector<glm::vec3> &vertices,
                              std::vector<glm::vec2> &texcoords);

protected:
    MeshSceneNodeGenerator(const MeshObstacle*);

private:
    void setupOccluders();
    void setupFacesAndFrags();

private:
    int currentNode;
    bool useDrawInfo;
    bool returnOccluders;
    const MeshObstacle* mesh;

    typedef struct
    {
        bool isFace;
        std::vector<const MeshFace*> faces;
    } MeshNode;
    std::vector<MeshNode> nodes;
    std::vector<SceneNode*> occluders;
};

#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
