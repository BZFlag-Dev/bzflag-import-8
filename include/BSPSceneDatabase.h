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

/* BSPSceneDatabase:
 *  BSP tree database of geometry to render
 */

#ifndef BZF_BSP_SCENE_DATABASE_H
#define BZF_BSP_SCENE_DATABASE_H

// common goes first
#include "common.h"

// system headers
#include <vector>
#include <glm/vec3.hpp>

// common implementation headers
#include "bzfgl.h"
#include "SceneDatabase.h"


class BSPSceneDatabase : public SceneDatabase
{
public:
    BSPSceneDatabase();
    ~BSPSceneDatabase();

    // returns true if the node would have been deleted
    bool        addStaticNode(SceneNode*, bool dontFree);
    void        addDynamicNode(SceneNode*);
    void        addDynamicSphere(SphereSceneNode*);
    void        finalizeStatics();
    void        removeDynamicNodes();
    void        removeAllNodes();
    bool        isOrdered();

    void        updateNodeStyles();
    void        addLights(SceneRenderer& renderer);
    void        addShadowNodes(SceneRenderer &renderer);
    void        addRenderNodes(SceneRenderer& renderer);
    void        renderRadarNodes(const ViewFrustum&);

    void        drawCuller();

private:
    class Node
    {
    public:
        Node(bool dynamic, SceneNode* node);
    public:
        bool        dynamic;
        int     count;
        SceneNode*  node;
        Node*       front;
        Node*       back;
    };

    void        setNodeStyle(Node*);
    void        nodeAddLights(Node*);
    void        nodeAddShadowNodes(Node*);
    void        nodeAddRenderNodes(Node*);

    // returns true if the node would have been deleted
    bool        insertStatic(int, Node*, SceneNode*, bool dontFree);
    void        insertDynamic(int, Node*, SceneNode*);
    void        insertNoPlane(int, Node*, SceneNode*);
    void        insertNoPlaneNodes();
    void        removeDynamic(Node*);
    void        free(Node*);
    void        release(Node*);
    void        setDepth(int newDepth);

private:
    Node*       root;
    int         depth;
    // the following members avoid passing parameters around
    glm::vec3   eye;
    SceneRenderer*  renderer;
    const ViewFrustum*  frustum;

    bool needNoPlaneNodes;
    std::vector<SceneNode*> noPlaneNodes;
};


#endif // BZF_BSP_SCENE_DATABASE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
