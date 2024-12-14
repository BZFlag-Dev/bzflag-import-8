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

/*
 * SceneDatabaseBuilder:
 *  Encapsulates creation of display database
 */

#ifndef BZF_SCENE_BUILDER_H
#define BZF_SCENE_BUILDER_H

// 1st
#include "common.h"

// System interfaces
#include <glm/vec4.hpp>

// Common headers
#include "OpenGLMaterial.h"

class SceneRenderer;
class SceneDatabase;
class WallObstacle;
class MeshObstacle;
class BoxBuilding;
class PyramidBuilding;
class BaseBuilding;
class Teleporter;
class World;

class SceneDatabaseBuilder
{
public:
    SceneDatabaseBuilder(const SceneRenderer*);
    ~SceneDatabaseBuilder();

    SceneDatabase*  make(const World*);

protected:
    void        addWall(SceneDatabase*, const WallObstacle&);
    void        addMesh(SceneDatabase*, MeshObstacle*);
    void        addBox(SceneDatabase*, BoxBuilding&);
    void        addPyramid(SceneDatabase*, PyramidBuilding&);
    void        addBase(SceneDatabase*, BaseBuilding&);
    void        addTeleporter(SceneDatabase*, const Teleporter&, const World*);
    void        addWaterLevel(SceneDatabase*, const World*);

private:
    // disallow duplication
    SceneDatabaseBuilder(const SceneDatabaseBuilder&);
    SceneDatabaseBuilder& operator=(const SceneDatabaseBuilder&);

private:
    const SceneRenderer *renderer;

    OpenGLMaterial  wallMaterial;
    float       wallTexWidth, wallTexHeight;
    bool        wallLOD;

    OpenGLMaterial  boxMaterial;
    float       boxTexWidth, boxTexHeight;
    bool        boxLOD;

    OpenGLMaterial  pyramidMaterial;
    bool        pyramidLOD;

    bool        baseLOD;

    OpenGLMaterial  teleporterMaterial;
    bool        teleporterLOD;

    static const glm::vec4 wallColors[4];
    static const glm::vec4 wallModulateColors[4];
    static const glm::vec4 wallLightedColors[1];
    static const glm::vec4 wallLightedModulateColors[1];
    static const glm::vec4 boxColors[6];
    static const glm::vec4 boxModulateColors[6];
    static const glm::vec4 boxLightedColors[6];
    static const glm::vec4 boxLightedModulateColors[6];
    static const glm::vec4 pyramidColors[5];
    static const glm::vec4 pyramidModulateColors[5];
    static const glm::vec4 pyramidLightedColors[5];
    static const glm::vec4 pyramidLightedModulateColors[5];
    static const glm::vec4 teleporterColors[3];
    static const glm::vec4 teleporterModulateColors[3];
    static const glm::vec4 teleporterLightedColors[3];
    static const glm::vec4 teleporterLightedModulateColors[3];
};

#endif // BZF_SCENE_BUILDER_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
