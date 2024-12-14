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

/* MeshFragSceneNode:
 *  Encapsulates information for rendering a mesh fragment
 *      (a collection of faces with the same material properties).
 *  Does not support level of detail.
 */

#pragma once

// Inherits from
#include "WallSceneNode.h"

// Global headers
#include "BzMaterial.h"

//
// NOTES:
//
// - Make sure that "noPlane" is set to true, for Mesh Fragments can not be
//   used as occluders, and can not be culled as a simple plane
//
// - The lists are all GL_TRIANGLE lists
//

class MeshFace;

class MeshFragSceneNode : public WallSceneNode
{

public:
    MeshFragSceneNode(int faceCount, const MeshFace** faces);
    ~MeshFragSceneNode();

    // virtual functions from SceneNode
    const glm::vec4 *getPlane() const override;
    bool cull(const ViewFrustum&) const override;
    void addShadowNodes(SceneRenderer&) override;
    void addRenderNodes(SceneRenderer&) override;
    void renderRadar() override;

    // virtual functions from WallSceneNode
    bool inAxisBox(const Extents& exts) const override;

    void getRenderNodes(std::vector<RenderSet>& rnodes) override;

protected:
    class Geometry : public RenderNode
    {
    public:
        Geometry(MeshFragSceneNode &node);
        ~Geometry();

        void init();
        void setStyle(int style);
        void render() override;
        void renderShadow() override;
        const glm::vec3 &getPosition() const override;

    private:
        void drawV() const; // draw with just vertices
        void drawVT() const; // draw with texcoords
        void drawVN() const; // draw with normals
        void drawVTN() const; // draw with texcoords and normals

        void initDisplayList();
        void freeDisplayList();
        static void initContext(void *data);
        static void freeContext(void *data);

    private:
        int style;
        GLuint list;
        MeshFragSceneNode &sceneNode;
    };

private:
    Geometry renderNode;

    GLint faceCount;
    const MeshFace** faces;

    bool noRadar;
    bool noShadow;
    GLint arrayCount;
    glm::vec3 *vertices;
    glm::vec3 *normals;
    glm::vec2 *texcoords;

    friend class MeshFragSceneNode::Geometry;
};


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
