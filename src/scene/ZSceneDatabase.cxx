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

// implementation header
#include "ZSceneDatabase.h"

// system headers
#include <string.h>
#include <stdlib.h>

// common headers
#include "SceneNode.h"
#include "WallSceneNode.h"
#include "SphereSceneNode.h"
#include "SceneRenderer.h"
#include "StateDatabase.h"
#include "BZDBCache.h"
#include "TimeKeeper.h"
#include "Intersect.h"
#include "Extents.h"

// local headers
#include "Octree.h"


// utility function
static int compareZExtents(const void* a, const void* b)
{
    const SceneNode* nodeA = *((const SceneNode* const *)a);
    const SceneNode* nodeB = *((const SceneNode* const *)b);
    const Extents& eA = nodeA->getExtents();
    const Extents& eB = nodeB->getExtents();
    if (eA.maxs[2] > eB.maxs[2])
        return +1;
    else if (eB.maxs[2] > eA.maxs[2])
        return -1;
    else
        return 0;
}


ZSceneDatabase::ZSceneDatabase()
{
    staticCount = staticSize = 0;
    dynamicCount = dynamicSize = 0;
    staticList = dynamicList = culledList = NULL;
    culledCount = 0;
    cullDepth = cullElements = 0;
    octree = NULL;
    return;
}


ZSceneDatabase::~ZSceneDatabase()
{
    // free static nodes
    for (int i = 0; i < staticCount; i++)
        delete staticList[i];

    // free lists
    delete[] staticList;
    delete[] dynamicList;
    if (culledList != staticList)
        delete[] culledList;

    // delete the octree
    delete octree;
}


void ZSceneDatabase::finalizeStatics()
{
    return;
}


bool ZSceneDatabase::addStaticNode(SceneNode* object, bool UNUSED(dontFree))
{
    if (staticCount == staticSize)
    {
        if (staticSize == 0) staticSize = 15;
        else staticSize += staticSize + 1;
        SceneNode** newStatic = new SceneNode*[staticSize];
        if (staticList)
            ::memcpy(newStatic, staticList, staticCount * sizeof(SceneNode*));
        delete[] staticList;
        staticList = newStatic;
    }
    staticList[staticCount++] = object;
    return false; // node would not be freed
}


void ZSceneDatabase::addDynamicNode(SceneNode* object)
{
    object->notifyStyleChange();
    if (dynamicCount == dynamicSize)
    {
        if (dynamicSize == 0) dynamicSize = 15;
        else dynamicSize += dynamicSize + 1;
        SceneNode** newDynamic = new SceneNode*[dynamicSize];
        if (dynamicList)
            ::memcpy(newDynamic, dynamicList,  dynamicCount * sizeof(SceneNode*));
        delete[] dynamicList;
        dynamicList = newDynamic;
    }
    dynamicList[dynamicCount++] = object;
}


void ZSceneDatabase::addDynamicSphere(SphereSceneNode* n)
{
    // just add sphere -- don't need to break it up for hidden surfaces
    addDynamicNode(n);
}


void ZSceneDatabase::removeDynamicNodes()
{
    dynamicCount = 0;
}


void ZSceneDatabase::removeAllNodes()
{
    staticCount = 0;
    dynamicCount = 0;
}


bool ZSceneDatabase::isOrdered()
{
    return false;
}


const Extents* ZSceneDatabase::getVisualExtents() const
{
    if (octree)
        return octree->getVisualExtents();
    else
        return NULL;
}


void ZSceneDatabase::updateNodeStyles()
{
    for (int i = 0; i < staticCount; i++)
        staticList[i]->notifyStyleChange();
    return;
}


void ZSceneDatabase::setOccluderManager(int occl)
{
    if (octree)
        octree->setOccluderManager(occl);
    return;
}


void ZSceneDatabase::setupCullList()
{
    const int currentDepth = BZDB.evalInt(StateDatabase::BZDB_CULLDEPTH);
    const int currentElements = BZDB.evalInt(StateDatabase::BZDB_CULLELEMENTS);

    if ((culledList == NULL) ||
            (currentDepth != cullDepth) || (currentElements != cullElements))
    {

        cullDepth = currentDepth;
        cullElements = currentElements;

        delete octree;
        octree = NULL;

        if (cullDepth > 0)
            makeCuller();
        else
        {
            if (culledList != staticList)
                delete [] culledList;
            culledList = staticList;
            culledCount = staticCount;
        }
    }

    return;
}


void ZSceneDatabase::makeCuller()
{
    delete octree;
    octree = new Octree;

    TimeKeeper startTime = TimeKeeper::getCurrent();

    // sorted from lowest to highest
    if (staticCount)
        qsort(staticList, staticCount, sizeof(SceneNode*), compareZExtents);

    // make the tree
    octree->addNodes (staticList, staticCount, cullDepth, cullElements);

    float elapsed = (float)(TimeKeeper::getCurrent() - startTime);
    logDebugMessage(2,"SceneNode Octree processed in %.3f seconds.\n", elapsed);

    if (culledList != staticList)
        delete [] culledList;

    // make scratch pad for the culler
    culledList = new SceneNode*[staticCount];
}


void ZSceneDatabase::addLights(SceneRenderer& renderer)
{
    // add the lights from the dynamic nodes
    for (int i = 0; i < dynamicCount; i++)
        dynamicList[i]->addLight(renderer);

    return;
}


static void setupShadowPlanes(const Frustum* frustum, const glm::vec3 &sunDir,
                              int& planeCount, glm::vec4 planes[4])
{
    // FIXME: As a first cut, we'll assume that
    //        the frustum top points towards Z.

    const auto &eye = frustum->getEye();
    if (frustum->getUp()[2] < 0.999f)
    {
        planeCount = 0;
        return;
    }

    // we project the frustum onto the ground plane, and then
    // use those lines to generate planes in the direction of
    // the sun's light. that is the potential shadow volume.

    // The frustum planes are as follows:
    // 0: front
    // 1: left
    // 2: right
    // 3: bottom
    // 4: top

    planeCount = 2;
    float edge[2];
    // left edge
    edge[0] = -frustum->getSide(1)[1];
    edge[1] = +frustum->getSide(1)[0];
    planes[0][0] =  (edge[1] * sunDir[2]);
    planes[0][1] = -(edge[0] * sunDir[2]);
    planes[0][2] =  (edge[0] * sunDir[1]) - (edge[1] * sunDir[0]);
    planes[0][3] = -((planes[0][0] * eye[0]) + (planes[0][1] * eye[1]));
    // right edge
    edge[0] = -frustum->getSide(2)[1];
    edge[1] = +frustum->getSide(2)[0];
    planes[1][0] =  (edge[1] * sunDir[2]);
    planes[1][1] = -(edge[0] * sunDir[2]);
    planes[1][2] =  (edge[0] * sunDir[1]) - (edge[1] * sunDir[0]);
    planes[1][3] = -((planes[1][0] * eye[0]) + (planes[1][1] * eye[1]));
    // only use the bottom edge if we have some height (about one jump's worth)
    if (eye[2] > 20.0f)
    {
        // bottom edge
        edge[0] = -frustum->getSide(3)[1];
        edge[1] = +frustum->getSide(3)[0];
        planes[2][0] =  (edge[1] * sunDir[2]);
        planes[2][1] = -(edge[0] * sunDir[2]);
        planes[2][2] =  (edge[0] * sunDir[1]) - (edge[1] * sunDir[0]);
        const float hlen = sqrtf ((frustum->getSide(3)[0] * frustum->getSide(3)[0]) +
                                  (frustum->getSide(3)[1] * frustum->getSide(3)[1]));
        const float slope = frustum->getSide(3)[2] / hlen;
        float point[2];
        point[0] = eye[0] + (eye[2] * frustum->getSide(3)[0] * slope);
        point[1] = eye[1] + (eye[2] * frustum->getSide(3)[1] * slope);
        planes[2][3] = -((planes[2][0] * point[0]) + (planes[2][1] * point[1]));
        planeCount++;
    }

    return;
}


void ZSceneDatabase::addShadowNodes(SceneRenderer& renderer)
{
    int i;
    const auto *sunDir = renderer.getSunDirection();
    const ViewFrustum& vf = renderer.getViewFrustum();
    const Frustum* frustum = (const Frustum*) &vf;

    if (sunDir == NULL)
    {
        // no sun = no shadows, simple
        return;
    }

    // see if we need an octree, or if it needs to be rebuilt
    setupCullList();

    // setup the shadow clipping planes
    int planeCount = 0;
    glm::vec4 planes[4];
    setupShadowPlanes(frustum, *sunDir, planeCount, planes);

    // cull if we're supposed to
    if (octree)
    {
        culledCount = octree->getShadowList (culledList, staticCount,
                                             planeCount, planes);
    }

    // add the static nodes
    for (i = 0; i < culledCount; i++)
    {
        SceneNode* node = culledList[i];
        node->addShadowNodes(renderer);

        // clear the state
        node->octreeState = SceneNode::OctreeCulled;
    }

    // add the dynamic nodes
    for (i = 0; i < dynamicCount; i++)
    {
        SceneNode* node = dynamicList[i];
        if (!node->cullShadow(planeCount, planes))
            node->addShadowNodes(renderer);
    }

    return;
}


void ZSceneDatabase::renderRadarNodes(const ViewFrustum& vf)
{
    // see if we need an octree, or if it needs to be rebuilt
    setupCullList();

    // cull if we're supposed to
    if (octree)
    {
        const Frustum* f = (const Frustum *) &vf;
        culledCount = octree->getRadarList (culledList, staticCount, f);

        // sort based on heights
        if (BZDBCache::radarStyle == 2)
            qsort(culledList, culledCount, sizeof(SceneNode*), compareZExtents);
    }

    // render through the sceneNodes
    for (int i = 0; i < culledCount; i++)
    {
        SceneNode* snode = culledList[i];
        snode->renderRadar();
        snode->octreeState = SceneNode::OctreeCulled; // clear the state
    }

    return;
}


void ZSceneDatabase::addRenderNodes(SceneRenderer& renderer)
{
    int i;
    const ViewFrustum& frustum = renderer.getViewFrustum();
    const auto eye = glm::vec4(frustum.getEye(), 1.0f);

    // see if we need an octree, or if it needs to be rebuilt
    setupCullList();

    // cull if we're supposed to
    if (octree)
    {
        const Frustum* f = (const Frustum *) &frustum;
        culledCount = octree->getFrustumList (culledList, staticCount, f);
    }

    const Frustum* frustumPtr = (const Frustum *) &frustum;

    // add the static nodes
    for (i = 0; i < culledCount; i++)
    {
        SceneNode* node = culledList[i];

        const auto plane = node->getPlane();

        if (plane != NULL)
        {
            // see if our eye is behind the plane
            if (glm::dot(eye, *plane) <= 0.0f)
            {
                node->octreeState = SceneNode::OctreeCulled;
                continue;
            }
        }

        // if the Visibility culler tells us that we're
        // fully visible, then skip the extents test
        if (node->octreeState != SceneNode::OctreeVisible)
        {
            const Extents& exts = node->getExtents();
            if (testAxisBoxInFrustum(exts, frustumPtr) == Outside)
            {
                node->octreeState = SceneNode::OctreeCulled;
                continue;
            }
        }

        // add the node
        node->addRenderNodes(renderer);

        // clear the state
        node->octreeState = SceneNode::OctreeCulled;
    }

    // add the dynamic nodes
    for (i = 0; i < dynamicCount; i++)
    {
        SceneNode* node = dynamicList[i];
        if (!node->cull(frustum))
            node->addRenderNodes(renderer);
    }

    return;
}


void ZSceneDatabase::drawCuller()
{
    if (octree)
        octree->draw ();
    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
