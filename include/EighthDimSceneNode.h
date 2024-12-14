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

/* EighthDimSceneNode:
 *  Encapsulates information for rendering the eighth dimension
 */

#ifndef BZF_EIGHTH_DIM_SCENE_NODE_H
#define BZF_EIGHTH_DIM_SCENE_NODE_H

// Inherits from
#include "SceneNode.h"

class EighthDimSceneNode : public SceneNode
{
public:
    ~EighthDimSceneNode();

    bool        cull(const ViewFrustum&) const override;
    void        notifyStyleChange() override;
    void        addRenderNodes(SceneRenderer&) override;

protected:
    EighthDimSceneNode(int numPolys);

    void        setPolygon(int index, const glm::vec3 vertex[3]);

protected:
    class EighthDimRenderNode : public RenderNode
    {
    public:
        EighthDimRenderNode(
            const EighthDimSceneNode*,
            int numPolygons);
        ~EighthDimRenderNode();
        void        render() override;
        void  setPolygon(int index, const glm::vec3[3]);
        const glm::vec3 &getPosition() const override;
    private:
        const EighthDimSceneNode* sceneNode;
        int     numPolygons;
        glm::vec4   *color;
        glm::vec3   (*poly)[3];
    };

private:
    OpenGLGState    gstate;
    EighthDimRenderNode renderNode;
};

#endif // BZF_EIGHTH_DIM_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
