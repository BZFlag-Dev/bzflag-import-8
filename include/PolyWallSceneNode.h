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

/* PolyWallSceneNode:
 *  Encapsulates information for rendering a planar
 *  polygonal wall.  Does not support level of detail.
 */

#ifndef BZF_POLY_WALL_SCENE_NODE_H
#define BZF_POLY_WALL_SCENE_NODE_H

// Inherits from
#include "WallSceneNode.h"

class PolyWallSceneNode : public WallSceneNode
{
public:
    PolyWallSceneNode(const std::vector<glm::vec3> &vertices,
                      const std::vector<glm::vec2> &uvs);
    ~PolyWallSceneNode();

    int         split(const glm::vec4 &, SceneNode*&, SceneNode*&)
    const override;

    void        addRenderNodes(SceneRenderer&) override;
    void        addShadowNodes(SceneRenderer&) override;
    void        renderRadar() override;

    void        getRenderNodes(std::vector<RenderSet>& rnodes) override;

protected:
    class Geometry : public RenderNode
    {
    public:
        Geometry(PolyWallSceneNode*,
                 const std::vector<glm::vec3> &vertices,
                 const std::vector<glm::vec2> &uvs,
                 const glm::vec4 &plane);
        ~Geometry();
        void        setStyle(int _style)
        {
            style = _style;
        }
        void        render() override;
        const glm::vec3 &getPosition() const override;
    private:
        void        drawV() const;
        void        drawVT() const;
    private:
        PolyWallSceneNode* wall;
        int     style;
        const glm::vec4 &plane;
    public:
        const std::vector<glm::vec3> vertex;
        const std::vector<glm::vec2> uv;
    };

private:
    Geometry*       node;
    Geometry*       shadowNode;
};

#endif // BZF_POLY_WALL_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
