/* bzflag
 * Copyright (c) 1993-2021 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* EighthDBaseSceneNode:
 *  Encapsulates information for rendering the eighth dimension
 *  of a base building.
 */

#ifndef BZF_EIGHTHD_BASE_SCENE_NODE_H
#define BZF_EIGHTHD_BASE_SCENE_NODE_H

// Inherit from
#include "EighthDimSceneNode.h"

class EighthDBaseSceneNode : public EighthDimSceneNode
{
public:
    EighthDBaseSceneNode(const glm::vec3 &pos,
                         const glm::vec3 &size, float rotation);
    ~EighthDBaseSceneNode();
    void        notifyStyleChange() override;
    void        addRenderNodes(SceneRenderer&) override;
protected:
    class EighthDBaseRenderNode : public RenderNode
    {
    public:
        EighthDBaseRenderNode(const EighthDBaseSceneNode *,
                              const glm::vec3 &pos,
                              const glm::vec3 &size, float rotation);
        ~EighthDBaseRenderNode();
        void        render() override;
        const glm::vec3 getPosition() const override;
    private:
        const EighthDBaseSceneNode *sceneNode;
        glm::vec3 corner[8];
    };
private:
    OpenGLGState      gstate;
    EighthDBaseRenderNode renderNode;
};

#endif // BZF_EIGHTHD_BASE_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
