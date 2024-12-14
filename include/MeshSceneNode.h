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

/* MeshSceneNode:
 *  Encapsulates information for rendering a mesh fragment
 *      (a collection of faces with the same material properties).
 *  Does not support level of detail.
 */

#ifndef BZF_MESH_SCENE_NODE_H
#define BZF_MESH_SCENE_NODE_H

// Inherits
#include "SceneNode.h"

// Common headers
#include "bzfgl.h"
//
// NOTES:
//
// - Make sure that "noPlane" is set to true, for Mesh can not be
//   used as occluders, and can not be culled as a simple plane
//
// - All accesses are done through indices
//

class MeshObstacle;
class MeshDrawInfo;
class MeshDrawMgr;
class BzMaterial;
class OpenGLGState;
class SceneRenderer;
class ViewFrustum;
class RenderNode;


class MeshSceneNode : public SceneNode
{
public:
    MeshSceneNode(const MeshObstacle* mesh);
    ~MeshSceneNode();

    // virtual functions from SceneNode

    void notifyStyleChange() override;

    bool cull(const ViewFrustum&) const override;
    bool inAxisBox(const Extents& exts) const override;

    void addShadowNodes(SceneRenderer&) override;
    void addRenderNodes(SceneRenderer&) override;
    void renderRadar() override;

    void getRenderNodes(std::vector<RenderSet>& rnodes) override;

    void makeXFormList();
    static void initContext(void* data);
    static void freeContext(void* data);

    static void setLodScale(int pixelsX, float fovx,
                            int pixelsY, float fovy);
    static void setRadarLodScale(float lengthPerPixel);

private:
    const MeshObstacle* mesh;

    MeshDrawMgr* drawMgr;
    const MeshDrawInfo* drawInfo;

    bool animRepos;

    // transform display list
    GLfloat xformMatrix[16];

    struct MeshMaterial
    {
        const BzMaterial* bzmat;
        OpenGLGState gstate;
        glm::vec4 color;
        const glm::vec4 *colorPtr;
        bool drawRadar;
        bool drawShadow;
        bool needsSorting;
        bool animRepos;
    };

    struct SetNode
    {
        int set;
        MeshMaterial meshMat;
        // basic render nodes
        RenderNode* node;
        RenderNode* radarNode;
    };

    struct LodNode
    {
        int count;
        SetNode* sets;
    };

    // Level Of Detail (LOD) information
    int lodCount;
    LodNode* lods;
    float* lodLengths;

    // Radar LODs
    int radarCount;
    LodNode* radarLods;
    float* radarLengths;

    static float LodScale;
    static float RadarLodScale;

private:
    void updateMaterial(MeshMaterial* mat);
    const BzMaterial* convertMaterial(const BzMaterial* bzmat);
    int calcNormalLod(const ViewFrustum&);
    int calcRadarLod();

    friend class MeshSceneNodeMgr;
};


#endif // BZF_MESH_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
