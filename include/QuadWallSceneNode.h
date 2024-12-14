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

/* QuadWallSceneNode:
 *  Encapsulates information for rendering a quadrilateral wall.
 */

#ifndef BZF_QUAD_WALL_SCENE_NODE_H
#define BZF_QUAD_WALL_SCENE_NODE_H

// Inherits from
#include "WallSceneNode.h"

class QuadWallSceneNode : public WallSceneNode
{
public:
    QuadWallSceneNode(const glm::vec3 &base,
                      const glm::vec3 &sEdge,
                      const glm::vec3 &tEdge,
                      float uRepeats = 1.0,
                      float vRepeats = 1.0,
                      bool makeLODs = true,
                      bool fixedUVs = false);
    QuadWallSceneNode(const glm::vec3 &base,
                      const glm::vec3 &sEdge,
                      const glm::vec3 &tEdge,
                      const glm::vec2 &uvOffset,
                      float uRepeats,
                      float vRepeats,
                      bool makeLODs);
    ~QuadWallSceneNode();

    int         split(const glm::vec4 &, SceneNode*&, SceneNode*&) const override;

    void        addRenderNodes(SceneRenderer&) override;
    void        addShadowNodes(SceneRenderer&) override;
    void        renderRadar() override;


    bool        inAxisBox (const Extents& exts) const override;

    int         getVertexCount () const override;
    const glm::vec3 &getVertex (int vertex) const override;

    void    getRenderNodes(std::vector<RenderSet>& rnodes) override;

private:
    void        init(const glm::vec3 &base,
                     const glm::vec3 &uEdge,
                     const glm::vec3 &vEdge,
                     const glm::vec2 &uvOffset,
                     float uRepeats,
                     float vRepeats,
                     bool makeLODs, bool fixedUVs);

protected:
    class Geometry : public RenderNode
    {
    public:
        Geometry(QuadWallSceneNode*,
                 int uCount, int vCount,
                 const glm::vec3 &base,
                 const glm::vec3 &uEdge,
                 const glm::vec3 &vEdge,
                 const glm::vec4 &plane,
                 const glm::vec2 &uvOffset,
                 float uRepeats, float vRepeats,
                 bool fixedUVs);
        ~Geometry();
        void        setStyle(int _style)
        {
            style = _style;
        }
        void        render() override;
        void        renderShadow() override;
        const glm::vec3 &getVertex(int i) const;
        const glm::vec3 &getPosition() const override;
    private:
        void        drawV() const;
        void        drawVT() const;
    private:
        WallSceneNode*  wall;
        int     style;
        int     ds, dt;
        int     dsq, dsr;
        const glm::vec4 &plane;
    public:
        std::vector<glm::vec3> vertex;
        std::vector<glm::vec2> uv;
        int      triangles;
    };

private:
    Geometry**      nodes;
    Geometry*       shadowNode;
};

#endif // BZF_QUAD_WALL_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
