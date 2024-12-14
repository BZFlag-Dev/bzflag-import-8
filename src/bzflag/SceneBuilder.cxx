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
#include "SceneBuilder.h"

// local implemenation headers
#include "ZSceneDatabase.h"
#include "BSPSceneDatabase.h"
#include "World.h"

// scene node implemenation headers
#include "MeshPolySceneNode.h"
#include "TankSceneNode.h"
#include "BoxSceneNodeGenerator.h"
#include "WallSceneNodeGenerator.h"
#include "MeshSceneNodeGenerator.h"
#include "BaseSceneNodeGenerator.h"
#include "PyramidSceneNodeGenerator.h"
#include "ObstacleSceneNodeGenerator.h"
#include "TeleporterSceneNodeGenerator.h"
#include "EighthDimShellNode.h"
#include "EighthDBoxSceneNode.h"
#include "EighthDPyrSceneNode.h"
#include "EighthDBaseSceneNode.h"

// common implementation headers
#include "StateDatabase.h"
#include "SceneRenderer.h"
#include "BZDBCache.h"
#include "TextureManager.h"
#include "ObstacleList.h"
#include "ObstacleMgr.h"


// uncomment for cheaper eighth dimension scene nodes
//#define SHELL_INSIDE_NODES


//
// SceneDatabaseBuilder
//

static const auto black = glm::vec3(0.0f);

const glm::vec4 SceneDatabaseBuilder::wallColors[4] =
{
    { 0.5f, 0.5f, 0.5f, 1.0f },
    { 0.4f, 0.4f, 0.4f, 1.0f },
    { 0.5f, 0.5f, 0.5f, 1.0f },
    { 0.6f, 0.6f, 0.6f, 1.0f }
};
const glm::vec4 SceneDatabaseBuilder::wallModulateColors[4] =
{
    { 0.5f, 0.5f, 0.5f, 1.0f },
    { 0.4f, 0.4f, 0.4f, 1.0f },
    { 0.5f, 0.5f, 0.5f, 1.0f },
    { 0.6f, 0.6f, 0.6f, 1.0f }
};
const glm::vec4 SceneDatabaseBuilder::wallLightedColors[1] =
{
    { 0.5f, 0.5f, 0.5f, 1.0f }
};
const glm::vec4 SceneDatabaseBuilder::wallLightedModulateColors[1] =
{
    { 0.5f, 0.5f, 0.5f, 1.0f }
};

const glm::vec4 SceneDatabaseBuilder::boxColors[6] =
{
    { 0.75f, 0.25f, 0.25f, 1.0f },
    { 0.63f, 0.25f, 0.25f, 1.0f },
    { 0.75f, 0.25f, 0.25f, 1.0f },
    { 0.75f, 0.375f, 0.375f, 1.0f },
    { 0.875f, 0.5f, 0.5f, 1.0f },
    { 0.275f, 0.2f, 0.2f, 1.0f }
};
const glm::vec4 SceneDatabaseBuilder::boxModulateColors[6] =
{
    { 0.75f, 0.75f, 0.75f, 1.0f },
    { 0.63f, 0.63f, 0.63f, 1.0f },
    { 0.75f, 0.75f, 0.75f, 1.0f },
    { 0.69f, 0.69f, 0.69f, 1.0f },
    { 0.875f, 0.875f, 0.875f, 1.0f },
    { 0.375f, 0.375f, 0.375f, 1.0f }
};
const glm::vec4 SceneDatabaseBuilder::boxLightedColors[6] =
{
    { 0.75f, 0.25f, 0.25f, 1.0f },
    { 0.75f, 0.25f, 0.25f, 1.0f },
    { 0.75f, 0.25f, 0.25f, 1.0f },
    { 0.75f, 0.25f, 0.25f, 1.0f },
    { 0.875f, 0.5f, 0.5f, 1.0f },
    { 0.875f, 0.5f, 0.5f, 1.0f }
};
const glm::vec4 SceneDatabaseBuilder::boxLightedModulateColors[6] =
{
    { 0.75f, 0.75f, 0.75f, 1.0f },
    { 0.75f, 0.75f, 0.75f, 1.0f },
    { 0.75f, 0.75f, 0.75f, 1.0f },
    { 0.75f, 0.75f, 0.75f, 1.0f },
    { 0.875f, 0.875f, 0.875f, 1.0f },
    { 0.875f, 0.875f, 0.875f, 1.0f }
};

const glm::vec4 SceneDatabaseBuilder::pyramidColors[5] =
{
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.13f, 0.13f, 0.51f, 1.0f },
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.375f, 0.375f, 0.75f, 1.0f },
    { 0.175f, 0.175f, 0.35f, 1.0f }
};

const glm::vec4 SceneDatabaseBuilder::pyramidModulateColors[5] =
{
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.13f, 0.13f, 0.51f, 1.0f },
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.375f, 0.375f, 0.75f, 1.0f },
    { 0.175f, 0.175f, 0.35f, 1.0f }
};
const glm::vec4 SceneDatabaseBuilder::pyramidLightedColors[5] =
{
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.25f, 0.25f, 0.63f, 1.0f }
};
const glm::vec4 SceneDatabaseBuilder::pyramidLightedModulateColors[5] =
{
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.25f, 0.25f, 0.63f, 1.0f },
    { 0.25f, 0.25f, 0.63f, 1.0f }
};

const glm::vec4 SceneDatabaseBuilder::teleporterColors[3] =
{
    { 1.0f, 0.875f, 0.0f, 1.0f },
    { 0.9f, 0.8f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 0.5f }
};
const glm::vec4 SceneDatabaseBuilder::teleporterModulateColors[3] =
{
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 0.9f, 0.9f, 0.9f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 0.5f }
};
const glm::vec4 SceneDatabaseBuilder::teleporterLightedColors[3] =
{
    { 1.0f, 0.875f, 0.0f, 1.0f },
    { 1.0f, 0.875f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 0.5f }
};
const glm::vec4 SceneDatabaseBuilder::teleporterLightedModulateColors[3] =
{
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 0.5f }
};


SceneDatabaseBuilder::SceneDatabaseBuilder(const SceneRenderer* _renderer) :
    renderer(_renderer),
    wallMaterial(black, black, 0.0f), wallLOD(),
    boxMaterial(black, black, 0.0f), boxLOD(),
    pyramidMaterial(black, black, 0.0f), pyramidLOD(),
    baseLOD(),
    teleporterMaterial(black, black, 0.0f),
    teleporterLOD()
{
    // FIXME -- should get texture heights from resources

    TextureManager &tm = TextureManager::instance();


    // make styles -- first the outer wall
    int wallTexture = tm.getTextureID( "wall" );
    wallTexWidth = wallTexHeight = 10.0f;
    if (wallTexture>=0)
        wallTexWidth = tm.GetAspectRatio(wallTexture) * wallTexHeight;


    // make box styles
    int boxTexture = tm.getTextureID( "boxwall" );
    boxTexWidth = boxTexHeight = 0.2f * BZDB.eval(StateDatabase::BZDB_BOXHEIGHT);
    if (boxTexture>=0)
        boxTexWidth = tm.GetAspectRatio(boxTexture) * boxTexHeight;


    // lower maximum tank lod if lowdetail is true
    if (renderer->useQuality() == 0)
        TankSceneNode::setMaxLOD(2);
}


SceneDatabaseBuilder::~SceneDatabaseBuilder()
{
    // do nothing
}


SceneDatabase* SceneDatabaseBuilder::make(const World* world)
{
    // set LOD flags
    const bool doLODs = BZDBCache::lighting && BZDBCache::zbuffer;
    wallLOD = baseLOD = boxLOD = pyramidLOD = teleporterLOD = doLODs;

    // pick type of database
    SceneDatabase* db;
    if (BZDBCache::zbuffer)
        db = new ZSceneDatabase;
    else
        db = new BSPSceneDatabase;
    // FIXME -- when making BSP tree, try several shuffles for best tree

    if (!world)
        return db;

    // free any prior inside nodes
    world->freeInsideNodes();


    // add nodes to database
    unsigned int i;

    const ObstacleList& walls = OBSTACLEMGR.getWalls();
    for (i = 0; i < walls.size(); i++)
        addWall(db, *((WallObstacle*) walls[i]));

    const ObstacleList& boxes = OBSTACLEMGR.getBoxes();
    for (i = 0; i < boxes.size(); i++)
        addBox (db, *((BoxBuilding*) boxes[i]));

    const ObstacleList& bases = OBSTACLEMGR.getBases();
    for (i = 0; i < bases.size(); i++)
        addBase (db, *((BaseBuilding*) bases[i]));

    const ObstacleList& pyramids = OBSTACLEMGR.getPyrs();
    for (i = 0; i < pyramids.size(); i++)
        addPyramid (db, *((PyramidBuilding*) pyramids[i]));

    const ObstacleList& teles = OBSTACLEMGR.getTeles();
    for (i = 0; i < teles.size(); i++)
        addTeleporter (db, *((Teleporter*) teles[i]), world);

    const ObstacleList& meshes = OBSTACLEMGR.getMeshes();
    for (i = 0; i < meshes.size(); i++)
        addMesh (db, (MeshObstacle*) meshes[i]);

    // add the water level node
    addWaterLevel(db, world);

    db->finalizeStatics();

    return db;
}


void SceneDatabaseBuilder::addWaterLevel(SceneDatabase* db,
        const World* world)
{
    const float level = world->getWaterLevel();

    // don't draw it if it isn't active
    if (level < 0.0f)
        return;

    auto plane = glm::vec4(0.0f, 0.0f, 1.0f, -level);

    // setup the vertex and texture coordinates
    float size = BZDBCache::worldSize;
    std::vector<glm::vec3> v(4);
    std::vector<glm::vec3> n(0);
    std::vector<glm::vec2> t(4);
    v[0] = glm::vec3(-size/2.0f, -size/2.0f, level);
    v[1] = glm::vec3(+size/2.0f, -size/2.0f, level);
    v[2] = glm::vec3(+size/2.0f, +size/2.0f, level);
    v[3] = glm::vec3(-size/2.0f, +size/2.0f, level);
    t[0] = glm::vec2(0.0f, 0.0f);
    t[1] = glm::vec2(2.0f, 0.0f);
    t[2] = glm::vec2(2.0f, 2.0f);
    t[3] = glm::vec2(0.0f, 2.0f);

    // get the material
    const BzMaterial* mat = world->getWaterMaterial();
    const bool noRadar = mat->getNoRadar();
    const bool noShadow = mat->getNoShadow();

    MeshPolySceneNode* node =
        new MeshPolySceneNode(plane, noRadar, noShadow, v, n, t);

    // setup the material
    MeshSceneNodeGenerator::setupNodeMaterial(node, mat);

    db->addStaticNode(node, false);

    return;
}


void SceneDatabaseBuilder::addWall(SceneDatabase* db, const WallObstacle& o)
{
    if (o.getHeight() <= 0.0f)
        return;

    int part = 0;
    WallSceneNode* node;
    ObstacleSceneNodeGenerator* nodeGen = new WallSceneNodeGenerator (&o);

    TextureManager &tm = TextureManager::instance();
    int wallTexture = -1;

    bool  useColorTexture = false;

    // try object, standard, then default
    if (o.userTextures[0].size())
        wallTexture = tm.getTextureID(o.userTextures[0].c_str(),false);
    if (wallTexture < 0)
        wallTexture = tm.getTextureID( "wall" );
    else
        useColorTexture = wallTexture >= 0;

    while ((node = nodeGen->getNextNode(o.getBreadth() / wallTexWidth,
                                        o.getHeight() / wallTexHeight, wallLOD)))
    {
        node->setColor(wallColors[part]);
        node->setModulateColor(wallModulateColors[part]);
        node->setLightedColor(wallLightedColors[0]);
        node->setLightedModulateColor(wallLightedModulateColors[0]);
        node->setMaterial(wallMaterial);
        node->setTexture(wallTexture);
        node->setUseColorTexture(useColorTexture);

        db->addStaticNode(node, false);
        part = (part + 1) % 5;
    }
    delete nodeGen;
}


void SceneDatabaseBuilder::addMesh(SceneDatabase* db, MeshObstacle* mesh)
{
    WallSceneNode* node;
    MeshSceneNodeGenerator* nodeGen = new MeshSceneNodeGenerator (mesh);

    while ((node = nodeGen->getNextNode(wallLOD)))
    {
        // make the inside node
        const bool ownTheNode = db->addStaticNode(node, true);
        // The BSP can split a MeshPolySceneNode and then delete it, which is
        // not good for the EighthDimShellNode's referencing scheme. If the
        // BSP would have split and then deleted this node, ownTheNode will
        // return true, and the EighthDimShellNode will then own the node.
        EighthDimShellNode* inode = new EighthDimShellNode(node, ownTheNode);
        mesh->addInsideSceneNode(inode);
    }
    delete nodeGen;
}


void SceneDatabaseBuilder::addBox(SceneDatabase* db, BoxBuilding& o)
{
    // this assumes boxes have six parts:  four sides, a roof, and a bottom.
    int part = 0;
    WallSceneNode* node;
    ObstacleSceneNodeGenerator* nodeGen = new BoxSceneNodeGenerator(&o);
    TextureManager &tm = TextureManager::instance();
    int    boxTexture = -1;
    bool  useColorTexture[2] = {false,false};

    // try object, standard, then default
    if (o.userTextures[0].size())
        boxTexture = tm.getTextureID(o.userTextures[0].c_str(),false);
    if (boxTexture < 0)
        boxTexture = tm.getTextureID(BZDB.get("boxWallTexture").c_str(),true);

    useColorTexture[0] = boxTexture >= 0;

    int boxTopTexture = -1;

    if (o.userTextures[1].size())
        boxTopTexture = tm.getTextureID(o.userTextures[1].c_str(),false);
    if (boxTopTexture < 0)
        boxTopTexture = tm.getTextureID(BZDB.get("boxTopTexture").c_str(),true);

    useColorTexture[1] = boxTopTexture >= 0;

    float texutureFactor = BZDB.eval("boxWallTexRepeat");
    if (renderer->useQuality() >= 2)
        texutureFactor = BZDB.eval("boxWallHighResTexRepeat");

    while ((node = ((part < 4) ? nodeGen->getNextNode(
                        -texutureFactor*boxTexWidth,
                        -texutureFactor*boxTexWidth, boxLOD) :
                    // I'm using boxTexHeight for roof since textures are same
                    // size and this number is available
                    nodeGen->getNextNode(
                        -boxTexHeight,
                        -boxTexHeight, boxLOD))))
    {
        node->setColor(boxColors[part]);
        node->setModulateColor(boxModulateColors[part]);
        node->setLightedColor(boxLightedColors[part]);
        node->setLightedModulateColor(boxLightedModulateColors[part]);
        node->setMaterial(boxMaterial);
        if (part < 4)
        {
            node->setTexture(boxTexture);
            node->setUseColorTexture(useColorTexture[0]);
        }
        else
        {
            node->setTexture(boxTopTexture);
            node->setUseColorTexture(useColorTexture[1]);
        }

#ifdef SHELL_INSIDE_NODES
        const bool ownTheNode = db->addStaticNode(node, true);
        EighthDimShellNode* inode = new EighthDimShellNode(node, ownTheNode);
        o.addInsideSceneNode(inode);
#else
        db->addStaticNode(node, false);
#endif // SHELL_INSIDE_NODES

        part = (part + 1) % 6;
    }

#ifndef SHELL_INSIDE_NODES
    // add the inside node
    glm::vec3 obstacleSize;
    obstacleSize[0] = o.getWidth();
    obstacleSize[1] = o.getBreadth();
    obstacleSize[2] = o.getHeight();
    SceneNode* inode =
        new EighthDBoxSceneNode(o.getPosition(), obstacleSize, o.getRotation());
    o.addInsideSceneNode(inode);
#endif // SHELL_INSIDE_NODES

    delete nodeGen;
}


void SceneDatabaseBuilder::addPyramid(SceneDatabase* db, PyramidBuilding& o)
{
    // this assumes pyramids have four parts:  four sides
    int part = 0;
    WallSceneNode* node;
    ObstacleSceneNodeGenerator* nodeGen = new PyramidSceneNodeGenerator(&o);

    TextureManager &tm = TextureManager::instance();
    int pyramidTexture = -1;

    bool useColorTexture = false;
    // try object, standard, then default
    if (o.userTextures[0].size())
        pyramidTexture = tm.getTextureID(o.userTextures[0].c_str(),false);
    if (pyramidTexture < 0)
        pyramidTexture = tm.getTextureID(BZDB.get("pyrWallTexture").c_str(),false);

    useColorTexture = pyramidTexture >= 0;

    // Using boxTexHeight since it's (currently) the same and it's already available
    float textureFactor = BZDB.eval("pyrWallTexRepeat");
    if (renderer->useQuality() >= 2)
        textureFactor = BZDB.eval("pyrWallHighResTexRepeat");

    while ((node = nodeGen->getNextNode(-textureFactor * boxTexHeight,
                                        -textureFactor * boxTexHeight,
                                        pyramidLOD)))
    {
        node->setColor(pyramidColors[part]);
        node->setModulateColor(pyramidModulateColors[part]);
        node->setLightedColor(pyramidLightedColors[part]);
        node->setLightedModulateColor(pyramidLightedModulateColors[part]);
        node->setMaterial(pyramidMaterial);
        node->setTexture(pyramidTexture);
        node->setUseColorTexture(useColorTexture);

#ifdef SHELL_INSIDE_NODES
        const bool ownTheNode = db->addStaticNode(node, true);
        EighthDimShellNode* inode = new EighthDimShellNode(node, ownTheNode);
        o.addInsideSceneNode(inode);
#else
        db->addStaticNode(node, false);
#endif // SHELL_INSIDE_NODES

        part = (part + 1) % 5;
    }

#ifndef SHELL_INSIDE_NODES
    // add the inside node
    glm::vec3 obstacleSize;
    obstacleSize[0] = o.getWidth();
    obstacleSize[1] = o.getBreadth();
    obstacleSize[2] = o.getHeight();
    SceneNode* inode =
        new EighthDPyrSceneNode(o.getPosition(), obstacleSize, o.getRotation());
    o.addInsideSceneNode(inode);
#endif // SHELL_INSIDE_NODES

    delete nodeGen;
}


void SceneDatabaseBuilder::addBase(SceneDatabase *db, BaseBuilding &o)
{
    WallSceneNode* node;
    ObstacleSceneNodeGenerator* nodeGen = new BaseSceneNodeGenerator(&o);

    TextureManager &tm = TextureManager::instance();
    int   boxTexture = -1;

    bool  useColorTexture[2] = {false,false};

    // try object, standard, then default
    if (o.userTextures[0].size())
        boxTexture = tm.getTextureID(o.userTextures[0].c_str(),false);
    if (boxTexture < 0)
    {
        std::string teamBase = Team::getImagePrefix((TeamColor)o.getTeam());
        teamBase += BZDB.get("baseWallTexture");
        boxTexture = tm.getTextureID(teamBase.c_str(),false);
    }
    if (boxTexture < 0)
        boxTexture = tm.getTextureID( BZDB.get("boxWallTexture").c_str() );

    useColorTexture[0] = boxTexture >= 0;

    int   baseTopTexture = -1;

    if (o.userTextures[1].size())
        baseTopTexture = tm.getTextureID(o.userTextures[1].c_str(),false);
    if (baseTopTexture < 0)
    {
        std::string teamBase = Team::getImagePrefix((TeamColor)o.getTeam());
        teamBase += BZDB.get("baseTopTexture").c_str();
        baseTopTexture = tm.getTextureID(teamBase.c_str(),false);
    }
    if (baseTopTexture < 0)
        baseTopTexture = -1;
    else
        useColorTexture[1] = baseTopTexture >= 0;

    // this assumes bases have 6 parts - if they don't, it still works
    unsigned int part = 0;
    // repeat the textue once for the top and bottom, else use the old messed up way
    // There are 3 cases for the texture ordering:
    // 1. getNextNode() only returns the top texture
    // 2. getNextNode() returns the top texture(0), and the 4 sides(1-4)
    // 3. getNextNode() returns the top texture(0), and the 4 sides(1-4), and the bottom(5)
    while ((node = ( ((part % 5) == 0) ? nodeGen->getNextNode(1,1, boxLOD) :
                     nodeGen->getNextNode(o.getBreadth(),
                                          o.getHeight(),
                                          boxLOD))))
    {
        if ((part % 5) != 0)
        {
            const unsigned int colorIndex = (part % 5) - 1;
            node->setColor(boxColors[colorIndex]);
            node->setModulateColor(boxModulateColors[colorIndex]);
            node->setLightedColor(boxLightedColors[colorIndex]);
            node->setLightedModulateColor(boxLightedModulateColors[colorIndex]);
            node->setMaterial(boxMaterial);
            node->setTexture(boxTexture);
            node->setUseColorTexture(useColorTexture[0]);
        }
        else
        {
            if (useColorTexture[1])    // only set the texture if we have one and are using it
            {
                node->setTexture(baseTopTexture);
                node->setUseColorTexture(useColorTexture[1]);
            }
        }
        part++;

#ifdef SHELL_INSIDE_NODES
        const bool ownTheNode = db->addStaticNode(node, true);
        EighthDimShellNode* inode = new EighthDimShellNode(node, ownTheNode);
        o.addInsideSceneNode(inode);
#else
        db->addStaticNode(node, false);
#endif // SHELL_INSIDE_NODES
    }

#ifndef SHELL_INSIDE_NODES
    // add the inside node
    glm::vec3 obstacleSize;
    obstacleSize[0] = o.getWidth();
    obstacleSize[1] = o.getBreadth();
    obstacleSize[2] = o.getHeight();
    SceneNode* inode = new
    EighthDBaseSceneNode(o.getPosition(), obstacleSize, o.getRotation());
    o.addInsideSceneNode(inode);
#endif // SHELL_INSIDE_NODES

    delete nodeGen;
}

void SceneDatabaseBuilder::addTeleporter(SceneDatabase* db,
        const Teleporter& o,
        const World* world)
{
    // this assumes teleporters have fourteen parts:  12 border sides, 2 faces
    int part = 0;
    WallSceneNode* node;
    ObstacleSceneNodeGenerator* nodeGen = new TeleporterSceneNodeGenerator(&o);

    TextureManager &tm = TextureManager::instance();
    int        teleporterTexture = -1;

    bool  useColorTexture = false;

    // try object, standard, then default
    if (o.userTextures[0].size())
        teleporterTexture = tm.getTextureID(o.userTextures[0].c_str(),false);
    if (teleporterTexture < 0)
        teleporterTexture = tm.getTextureID(BZDB.get("cautionTexture").c_str(),true);

    useColorTexture = teleporterTexture >= 0;

    int numParts = o.isHorizontal() ? 18 : 14;

    while ((node = nodeGen->getNextNode(1.0, o.getHeight() / o.getBreadth(),
                                        teleporterLOD)))
    {
        if (o.isHorizontal ())
        {
            if (part >= 0 && part <= 15)
            {
                node->setColor (teleporterColors[0]);
                node->setModulateColor (teleporterModulateColors[0]);
                node->setLightedColor (teleporterLightedColors[0]);
                node->setLightedModulateColor (teleporterLightedModulateColors[0]);
                node->setMaterial (teleporterMaterial);
                node->setTexture (teleporterTexture);
                node->setUseColorTexture (useColorTexture);
            }
        }
        else
        {
            if (part >= 0 && part <= 1)
            {
                node->setColor (teleporterColors[0]);
                node->setModulateColor (teleporterModulateColors[0]);
                node->setLightedColor (teleporterLightedColors[0]);
                node->setLightedModulateColor (teleporterLightedModulateColors[0]);
                node->setMaterial (teleporterMaterial);
                node->setTexture (teleporterTexture);
                node->setUseColorTexture (useColorTexture);
            }
            else if (part >= 2 && part <= 11)
            {
                node->setColor (teleporterColors[1]);
                node->setModulateColor (teleporterModulateColors[1]);
                node->setLightedColor (teleporterLightedColors[1]);
                node->setLightedModulateColor (teleporterLightedModulateColors[1]);
                node->setMaterial (teleporterMaterial);
                node->setTexture (teleporterTexture);
                node->setUseColorTexture (useColorTexture);
            }
        }

        db->addStaticNode(node, false);
        part = (part + 1) % numParts;
    }

    MeshPolySceneNode* linkNode;
    const BzMaterial* mat = world->getLinkMaterial();

    linkNode = MeshSceneNodeGenerator::getMeshPolySceneNode(o.getBackLink());
    MeshSceneNodeGenerator::setupNodeMaterial(linkNode, mat);
    db->addStaticNode(linkNode, false);

    linkNode = MeshSceneNodeGenerator::getMeshPolySceneNode(o.getFrontLink());
    MeshSceneNodeGenerator::setupNodeMaterial(linkNode, mat);
    db->addStaticNode(linkNode, false);

    delete nodeGen;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
