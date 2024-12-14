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

/* FlagWarpSceneNode:
 *  Encapsulates information for rendering the little cloud
 *  that appears when a flag is coming or going.
 */

#ifndef BZF_FLAG_WARP_SCENE_NODE_H
#define BZF_FLAG_WARP_SCENE_NODE_H

// Inherits
#include "SceneNode.h"

// System headers
#include <glm/vec3.hpp>

class FlagWarpSceneNode : public SceneNode
{
public:
    FlagWarpSceneNode(const glm::vec3 &pos);
    ~FlagWarpSceneNode();

    void        setSizeFraction(GLfloat);

    GLfloat     getDistance(const glm::vec3 &) const override;
    void        move(const glm::vec3 &pos);

    void        notifyStyleChange() override;
    void        addRenderNodes(SceneRenderer&) override;

protected:
    class FlagWarpRenderNode : public RenderNode
    {
    public:
        FlagWarpRenderNode(const FlagWarpSceneNode*);
        ~FlagWarpRenderNode();
        void        render() override;
        const glm::vec3 &getPosition() const override;
    private:
        const FlagWarpSceneNode* sceneNode;
        static glm::vec3 ring[12];
    };
    friend class FlagWarpRenderNode;

private:
    GLfloat     size;
    OpenGLGState    gstate;
    FlagWarpRenderNode  renderNode;
    static const glm::vec3 color[7];
};

#endif // BZF_FLAG_WARP_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
