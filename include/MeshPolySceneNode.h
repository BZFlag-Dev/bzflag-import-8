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

/* MeshPolySceneNode:
 *  Encapsulates information for rendering a planar
 *  polygonal wall.  Does not support level of detail.
 */

#ifndef BZF_MESH_POLY_SCENE_NODE_H
#define BZF_MESH_POLY_SCENE_NODE_H

// Inherits from
#include "WallSceneNode.h"

// System headers
#include <glm/gtc/type_ptr.hpp>

class MeshPolySceneNode : public WallSceneNode
{
public:
    MeshPolySceneNode(const glm::vec4 &plane,
                      bool noRadar, bool noShadow,
                      const std::vector<glm::vec3> &vertices,
                      const std::vector<glm::vec3> &normals,
                      const std::vector<glm::vec2> &texcoords);
    ~MeshPolySceneNode();

    bool cull(const ViewFrustum& frustum) const override;
    bool inAxisBox (const Extents& exts) const override;
    int getVertexCount () const override;
    const glm::vec3 &getVertex (int vertex) const override;
    const glm::vec3 *getVertices() const;

    int split(const glm::vec4 &plane, SceneNode*&, SceneNode*&) const override;

    void addRenderNodes(SceneRenderer&) override;
    void addShadowNodes(SceneRenderer&) override;
    void renderRadar() override;

    void getRenderNodes(std::vector<RenderSet>& rnodes) override;


protected:
    class Geometry : public RenderNode
    {
    public:
        Geometry(MeshPolySceneNode*,
                 const std::vector<glm::vec3> &vertices,
                 const std::vector<glm::vec3> &normals,
                 const std::vector<glm::vec2> &texcoords,
                 const glm::vec4 &plane);
        ~Geometry();
        void setStyle(int _style)
        {
            style = _style;
        }
        void render() override;
        void renderShadow() override;
        const glm::vec3 &getVertex(int i) const;
        const glm::vec3 *getVertices() const;
        int getVertexCount() const;
        const glm::vec3 &getPosition() const override;
    private:
        void drawV() const; // draw with just vertices
        void drawVT() const; // draw with texcoords
        void drawVN() const; // draw with normals
        void drawVTN() const; // draw with texcoords and normals
    private:
        MeshPolySceneNode* sceneNode;
        int style;
        const glm::vec4 &plane;
    public:
        const std::vector<glm::vec3> vertices;
        const std::vector<glm::vec3> normals;
        const std::vector<glm::vec2> texcoords;
    };

private:
    int splitWallVTN(const glm::vec4 &plane,
                     const std::vector<glm::vec3> &vertices,
                     const std::vector<glm::vec3> &normals,
                     const std::vector<glm::vec2> &texcoords,
                     SceneNode*& front, SceneNode*& back) const;

    void splitEdgeVTN(float d1, float d2,
                      const glm::vec3 &p1, const glm::vec3 &p2,
                      const glm::vec3 &n1, const glm::vec3 &n2,
                      const glm::vec2 &uv1, const glm::vec2 &uv2,
                      glm::vec3 &p, glm::vec3 &n, glm::vec2 &uv) const;

    int splitWallVT(const glm::vec4 &plane,
                    const std::vector<glm::vec3> &vertices,
                    const std::vector<glm::vec2> &texcoords,
                    SceneNode*& front, SceneNode*& back) const;

    void splitEdgeVT(float d1, float d2,
                     const glm::vec3 &p1,  const glm::vec3 &p2,
                     const glm::vec2 &uv1, const glm::vec2 &uv2,
                     glm::vec3 &p, glm::vec2 &uv) const;

    Geometry node;
    bool noRadar;
    bool noShadow;
};

inline int MeshPolySceneNode::Geometry::getVertexCount() const
{
    return vertices.size();
}

inline int MeshPolySceneNode::getVertexCount () const
{
    return node.getVertexCount();
}

inline const glm::vec3 &MeshPolySceneNode::Geometry::getVertex(int i) const
{
    return vertices[i];
}

inline const glm::vec3 *MeshPolySceneNode::Geometry::getVertices() const
{
    return &vertices[0];
}

inline const glm::vec3 &MeshPolySceneNode::getVertex(int i) const
{
    return node.getVertex(i);
}

inline const glm::vec3 *MeshPolySceneNode::getVertices() const
{
    return node.getVertices();
}


#endif // BZF_MESH_POLY_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
