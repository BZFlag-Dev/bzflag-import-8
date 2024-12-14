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

/*
 *
 */

// Interface header
#include "BSPSceneDatabase.h"

// System headers
#include <assert.h>
#include <string.h>

// Common headers
#include "SceneNode.h"
#include "ViewFrustum.h"
#include "SphereSceneNode.h"
#include "WallSceneNode.h"
#include "SceneRenderer.h"


//
// BSPSceneDatabase::Node
//

void BSPSceneDatabase::renderRadarNodes(const ViewFrustum&)
{
}



BSPSceneDatabase::Node::Node(bool _dynamic, SceneNode* _node):
    dynamic(_dynamic),
    count(0),
    node(_node),
    front(NULL),
    back(NULL)
{
    // do nothing
}


//
// BSPSceneDatabase
//

BSPSceneDatabase::BSPSceneDatabase() :
    root(NULL),
    depth(0),
    eye(0.0f)
{
    needNoPlaneNodes = true;
}


BSPSceneDatabase::~BSPSceneDatabase()
{
    free(root);
}


void BSPSceneDatabase::finalizeStatics()
{
    if (needNoPlaneNodes)
        insertNoPlaneNodes();
    return;
}


bool BSPSceneDatabase::addStaticNode(SceneNode* node, bool dontFree)
{
    // store for later insertion if the node has no plane
    if (node->getPlane() == NULL)
    {
        noPlaneNodes.push_back(node);
        return false; // node would not be freed
    }

    // insert static node
    if (!root)
    {
        root = new Node(false, node);
        setDepth(1);
        return false; // node would not be freed
    }
    else
        return insertStatic(1, root, node, dontFree);
}


void BSPSceneDatabase::addDynamicNode(SceneNode* node)
{
    node->notifyStyleChange();
    // insert dynamic node
    if (!root)
    {
        root = new Node(true, node);
        setDepth(1);
    }
    else
        insertDynamic(1, root, node);
}


void BSPSceneDatabase::addDynamicSphere(SphereSceneNode* n)
{
    // add each part of sphere separately
    int numParts;
    SceneNode** parts = n->getParts(numParts);
    if (!parts)
    {
        // wouldn't split itself up -- add whole thing
        addDynamicNode(n);
    }
    else
    {
        for (int i = 0; i < numParts; i++)
            addDynamicNode(parts[i]);
    }
}


void BSPSceneDatabase::removeDynamicNodes()
{
    // scan tree removing dynamic nodes
    if (root && root->dynamic)
    {
        release(root);
        root = NULL;
    }
    else
        removeDynamic(root);
}


void BSPSceneDatabase::removeAllNodes()
{
    free(root);
    root = NULL;
    depth = 0;
    noPlaneNodes.clear();
    needNoPlaneNodes = true;
}


bool BSPSceneDatabase::isOrdered()
{
    return true;
}


void BSPSceneDatabase::free(Node* node)
{
    if (!node) return;
    delete node->node;
    free(node->front);
    free(node->back);
    delete node;
}


void BSPSceneDatabase::release(Node* node)
{
    if (!node) return;
    release(node->front);
    release(node->back);
    delete node;
}


bool BSPSceneDatabase::insertStatic(int level, Node* _root,
                                    SceneNode* node, bool dontFree)
{
    // dynamic nodes should only be inserted after all static nodes
    assert(_root->dynamic == false);

    bool wouldFree = false;

    // split against root's plane
    SceneNode* front = NULL, *back = NULL;
    switch (node->split(*_root->node->getPlane(), front, back))
    {
    case 0:
        // copy style to new nodes
        // FIXME -- only WallSceneNodes are static but should make type safe
        ((WallSceneNode*)front)->copyStyle((WallSceneNode*)node);
        ((WallSceneNode*)back)->copyStyle((WallSceneNode*)node);

        // done with split node so get rid of it
        if (dontFree)
            wouldFree = true;
        else
            delete node;
        break;
    case 1:
        // completely in front
        front = node;
        break;
    case -1:
        // completely in back
        back = node;
        break;
    }

    const bool dontFreeNext = (dontFree && !wouldFree);

    // add nodes
    if (front)
    {
        if (_root->front)
            wouldFree = insertStatic(level + 1, _root->front, front, dontFreeNext);
        else
        {
            _root->front = new Node(false, front);
            setDepth(level + 1);
        }
        _root->count++;
    }
    if (back)
    {
        if (_root->back)
            wouldFree = insertStatic(level + 1, _root->back, back, dontFreeNext);
        else
        {
            _root->back = new Node(false, back);
            setDepth(level + 1);
        }
        _root->count++;
    }

    return wouldFree;
}


void BSPSceneDatabase::insertDynamic(int level, Node* _root,
                                     SceneNode* node)
{
    GLfloat d;
    if (!_root->dynamic && _root->node->getPlane())
    {
        const auto plane = _root->node->getPlane();
        const auto pos = glm::vec4(node->getSphere(), 1.0f);
        d = glm::dot(pos, *plane);
    }
    else
        d = _root->node->getDistance(eye) - node->getDistance(eye);

    if (d >= 0.0f)
    {
        if (_root->front)
            insertDynamic(level + 1, _root->front, node);
        else
        {
            _root->front = new Node(true, node);
            setDepth(level + 1);
        }
    }
    else
    {
        if (_root->back)
            insertDynamic(level + 1, _root->back, node);
        else
        {
            _root->back = new Node(true, node);
            setDepth(level + 1);
        }
    }
}


void BSPSceneDatabase::insertNoPlane(int level, Node* _root,
                                     SceneNode* node)
{
    // dynamic nodes should only be inserted after all static nodes
    assert(_root->dynamic == false);

    GLfloat d;
    if (_root->node->getPlane())
    {
        const auto plane = _root->node->getPlane();
        const auto pos = glm::vec4(node->getSphere(), 1.0f);
        d = glm::dot(pos, *plane);
    }
    else
    {
        // it's a crap shoot  (draw smaller items first)
        d = node->getRadius2() - _root->node->getRadius2();
    }

    if (d >= 0.0f)
    {
        if (_root->front)
            insertNoPlane(level + 1, _root->front, node);
        else
        {
            _root->front = new Node(false, node);
            setDepth(level + 1);
        }
    }
    else
    {
        if (_root->back)
            insertNoPlane(level + 1, _root->back, node);
        else
        {
            _root->back = new Node(false, node);
            setDepth(level + 1);
        }
    }
}


void BSPSceneDatabase::insertNoPlaneNodes()
{
    int i = 0;

    needNoPlaneNodes = false;

    const int count = noPlaneNodes.size();
    if (!root)
    {
        if (count > 0)
        {
            root = new Node(false, noPlaneNodes[0]);
            setDepth(1);
            i++;
        }
        else
            return;
    }

    for (; i < count; i++)
    {
        SceneNode* node = noPlaneNodes[i];
        node->notifyStyleChange();
        insertNoPlane(1, root, node);
    }

    noPlaneNodes.clear();

    return;
}


void BSPSceneDatabase::removeDynamic(Node* node)
{
    if (!node) return;
    if (node->front && node->front->dynamic)
    {
        release(node->front);
        node->front = NULL;
    }
    else
        removeDynamic(node->front);
    if (node->back && node->back->dynamic)
    {
        release(node->back);
        node->back = NULL;
    }
    else
        removeDynamic(node->back);
}


void BSPSceneDatabase::setDepth(int newDepth)
{
    if (newDepth <= depth)
        return;
    depth = newDepth;
}


void BSPSceneDatabase::updateNodeStyles()
{
    if (root)
        setNodeStyle(root);
    return;
}


void BSPSceneDatabase::addLights(SceneRenderer& _renderer)
{
    if (root)
    {
        renderer = &_renderer;
        nodeAddLights(root);
    }
    return;
}


void BSPSceneDatabase::addShadowNodes(SceneRenderer& _renderer)
{
    if (root)
    {
        renderer = &_renderer;
        nodeAddShadowNodes(root);
    }
    return;
}


void BSPSceneDatabase::addRenderNodes(SceneRenderer& _renderer)
{
    if (root)
    {
        renderer = &_renderer;
        frustum = &renderer->getViewFrustum();
        eye = frustum->getEye();
        nodeAddRenderNodes(root);
    }
    return;
}


void BSPSceneDatabase::setNodeStyle(Node *node)
{
    Node* back = node->back;
    Node* front = node->front;
    // dive into the child BSP nodes
    if (front)
        setNodeStyle(node->front);
    if (back)
        setNodeStyle(node->back);
    // add this node's style
    node->node->notifyStyleChange();

    return;
}


void BSPSceneDatabase::nodeAddLights(Node* node)
{
    Node* back = node->back;
    Node* front = node->front;
    // dive into the child BSP nodes
    if (front)
        nodeAddLights(node->front);
    if (back)
        nodeAddLights(node->back);
    // add this node's light, if it's dynamic
    if (node->dynamic)
        node->node->addLight(*renderer);
    return;
}


void BSPSceneDatabase::nodeAddShadowNodes(Node* node)
{
    Node* back = node->back;
    Node* front = node->front;
    // dive into the child BSP nodes
    if (front)
        nodeAddShadowNodes(node->front);
    if (back)
        nodeAddShadowNodes(node->back);
    // add this node's shadows
    node->node->addShadowNodes(*renderer);
    return;
}


void BSPSceneDatabase::nodeAddRenderNodes(Node* node)
{
    Node* back = node->back;
    Node* front = node->front;
    SceneNode* snode = node->node;

    const auto plane = snode->getPlane();
    if (plane)
    {
        const auto eye4 = glm::vec4(eye, 1.0f);
        if (glm::dot(*plane, eye4) >= 0.0f)
        {
            // eye is in front so render:  back, node, front
            if (back)
                nodeAddRenderNodes(back);
            if (!snode->cull(*frustum))
                snode->addRenderNodes(*renderer);
            if (front)
                nodeAddRenderNodes(front);
        }
        else
        {
            // eye is in back so render:  front, node, back
            if (front)
                nodeAddRenderNodes(front);
            if (!snode->cull(*frustum))
                snode->addRenderNodes(*renderer);
            if (back)
                nodeAddRenderNodes(back);
        }
    }
    else
    {
        // nodes without split planes should be rendered back, node, front
        if (back)
            nodeAddRenderNodes(back);
        if (!snode->cull(*frustum))
            snode->addRenderNodes(*renderer);
        if (front)
            nodeAddRenderNodes(front);
    }

    return;
}


void BSPSceneDatabase::drawCuller()
{
    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
