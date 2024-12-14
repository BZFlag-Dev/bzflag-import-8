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

/* OccluderSceneNode:
 *  Encapsulates information for an occluder
 *      plane. Occluders are never rendered.
 */

#ifndef BZF_OCCLUDER_SCENE_NODE_H
#define BZF_OCCLUDER_SCENE_NODE_H

// Inherits from
#include "SceneNode.h"

// common implementation headers
#include "MeshFace.h"

class OccluderSceneNode : public SceneNode
{

public:
    OccluderSceneNode(const MeshFace* face);
    ~OccluderSceneNode();

    // virtual functions from SceneNode
    bool cull(const ViewFrustum&) const override;
    bool inAxisBox(const Extents& exts) const override;
    void addShadowNodes(SceneRenderer&) override;
    void addRenderNodes(SceneRenderer&) override;
    void renderRadar() override;

    int getVertexCount () const override;

    const glm::vec4 *getPlane() const override;

    const glm::vec3 &getVertex (int vertex) const override;

private:
    int vertexCount;
    glm::vec3  *vertices;
    const glm::vec4 plane;   // unit normal, distance to origin
};


#endif // BZF_OCCLUDER_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
