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

/* EighthDBoxSceneNode:
 *  Encapsulates information for rendering the eighth dimension
 *  of a pyramid building.
 */

#ifndef BZF_EIGHTHD_PYR_SCENE_NODE_H
#define BZF_EIGHTHD_PYR_SCENE_NODE_H

// Inherits from
#include "EighthDimSceneNode.h"

class EighthDPyrSceneNode : public EighthDimSceneNode
{
public:
    EighthDPyrSceneNode(const glm::vec3 &pos,
                        const glm::vec3 &size,
                        float rotation);
    ~EighthDPyrSceneNode();

    void        notifyStyleChange() override;
    void        addRenderNodes(SceneRenderer&) override;

protected:
    class EighthDPyrRenderNode : public RenderNode
    {
    public:
        EighthDPyrRenderNode(const EighthDPyrSceneNode*,
                             const glm::vec3 &pos,
                             const glm::vec3 &size,
                             float rotation);
        ~EighthDPyrRenderNode();
        void        render() override;
        const glm::vec3 &getPosition() const override;
    private:
        const EighthDPyrSceneNode* sceneNode;
        glm::vec3 corner[5];
    };

private:
    OpenGLGState     gstate;
    EighthDPyrRenderNode renderNode;
};

#endif // BZF_EIGHTHD_PYR_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
