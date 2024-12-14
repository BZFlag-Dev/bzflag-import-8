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
#include "Octree.h"

// system headers
#include <math.h>

// common headers
#include "Extents.h"
#include "Intersect.h"
#include "StateDatabase.h"
#include "OpenGLAPI.h"

// local headers
#include "Occluder.h"

static bool F2BSORT = true;//FIXME


// FIXME - do something about occluders vs. gridding


static const int fullListBreak = 3; // FIXME
static const float testFudge = 0.1f;

static int maxDepth = 0;
static int minElements = 16;

static int leafNodes = 0;
static int totalNodes = 0;
static int totalElements = 0;

// FIXME - make them class static members once the memory is flattened
static int CullListSize = 0;
static int CullListCount = 0;
static SceneNode** CullList = NULL;
static const Frustum* CullFrustum = NULL;
static int ShadowCount = 0;
static const glm::vec4 *ShadowPlanes;

static OccluderManager OcclMgrs[2];
static OccluderManager* OcclMgr = &OcclMgrs[0];


inline static void addCullListNode(SceneNode* node)
{
    CullList[CullListCount] = node;
    CullListCount++;
    return;
}


inline static void squeezeChildren(OctreeNode** children)
{
    for (int dst = 0; dst < 8; dst++)
    {
        if (children[dst] == NULL)
        {
            // replace with the next non-NULL
            for (int src = (dst + 1); src < 8; src++)
            {
                if (children[src] != NULL)
                {
                    children[dst] = children[src];
                    children[src] = NULL;
                    break;
                }
            }
        }
    }
    return;
}


//////////////////////////////////////////////////////////////////////////////
//
// The Octree
//

Octree::Octree()
{
    root = NULL;
    CullList = NULL;
    CullFrustum = NULL;
    CullListSize = 0;
    CullListCount = 0;
    OcclMgr = &OcclMgr[0];
    return;
}


Octree::~Octree()
{
    clear();
    return;
}


void Octree::setOccluderManager(int occlmgr)
{
    if (occlmgr == 1)
        OcclMgr = &OcclMgrs[1]; // mirror
    else
        OcclMgr = &OcclMgrs[0]; // normal
    return;
}


void Octree::clear()
{
    delete root;
    root = NULL;
    CullList = NULL;
    CullFrustum = NULL;
    CullListSize = 0;
    CullListCount = 0;
    OcclMgrs[0].clear();
    OcclMgrs[1].clear();
    return;
}


void Octree::addNodes(SceneNode** list, int listSize, int depth, int elements)
{
    int i;

    if (root)
        clear();

    // just in case
    for (i = 0; i < listSize; i++)
        list[i]->octreeState = SceneNode::OctreeCulled;

    maxDepth = depth;
    minElements = elements;

    CullList = list;
    CullListSize = listSize;

    getExtents(list, listSize);

    // making babies
    root = new OctreeNode(0, extents, list, listSize);

    leafNodes = 0;
    totalNodes = 0;
    totalElements = 0;
    root->tallyStats();

    logDebugMessage(2, "Octree scene nodes = %i\n", listSize);
    for (i = 0; i < 3; i++)
        logDebugMessage(2, "  grid extent[%i] = %f, %f\n", i, extents.mins[i], extents.maxs[i]);

    for (i = 0; i < 3; i++)
        logDebugMessage(2, "  visual extent[%i] = %f, %f\n", i, visualExtents.mins[i], visualExtents.maxs[i]);

    logDebugMessage(2, "Octree leaf nodes  = %i\n", leafNodes);
    logDebugMessage(2, "Octree total nodes = %i\n", totalNodes);
    logDebugMessage(2, "Octree total elements = %i\n", totalElements);

    return;
}


int Octree::getFrustumList(SceneNode** list, int listSize,
                           const Frustum* frustum) const
{
    if (!root)
        return 0;

    F2BSORT = BZDB.isTrue("f2bsort");

    if (listSize > CullListSize)
    {
        printf("Octree::getFrustumList() internal error!\n");
        exit(1);
    }

    CullFrustum = frustum;
    CullList = list;
    CullListCount = 0;

    // update the occluders before using them
    OcclMgr->update(CullFrustum);

    // get the nodes
    root->getFrustumList();

    // pick new occluders
    OcclMgr->select(CullList, CullListCount);

    return CullListCount;
}


int Octree::getRadarList(SceneNode** list, int listSize,
                         const Frustum* frustum) const
{
    // This is basically the same as Octree::getFrustumList(),
    // except that it doesn't use the occluders. This duplication
    // was done to try and speed-up this low level code.

    if (!root)
        return 0;

    if (listSize > CullListSize)
    {
        printf("Octree::getRadarList() internal error!\n");
        exit(1);
    }

    CullFrustum = frustum;
    CullList = list;
    CullListCount = 0;

    // get the nodes
    root->getRadarList();

    return CullListCount;
}


int Octree::getShadowList(SceneNode** list, int listSize,
                          int planeCount, const glm::vec4 *planes) const
{
    if (!root)
        return 0;

    if (listSize > CullListSize)
    {
        printf("Octree::getShadowList() internal error!\n");
        exit(1);
    }

    ShadowCount = planeCount;
    ShadowPlanes = planes;

    CullList = list;
    CullListCount = 0;

    // update the occluders before using them
    // FIXME: use occluders later?  OcclMgr->update(CullFrustum);

    // get the nodes
    root->getShadowList();

    return CullListCount;
}


void Octree::getExtents(SceneNode** list, int listSize)
{
    int i;

    Extents tmpExts;

    for (i = 0; i < listSize; i++)
    {
        SceneNode* node = list[i];
        const Extents& exts = node->getExtents();
        tmpExts.expandToBox(exts);
    }

    visualExtents = tmpExts;

    // find the longest axis
    float width = -MAXFLOAT;
    for (i = 0; i < 3; i++)
    {
        float axisWidth = tmpExts.getWidth(i);
        if (axisWidth > width)
            width = axisWidth;
    }

    extents = tmpExts;

    // make it a cube, with Z on its minimum
    for (i = 0; i < 2; i++)
    {
        const float axisWidth = tmpExts.getWidth(i);
        if (axisWidth < width)
        {
            const float adjust = 0.5f * (width - axisWidth);
            extents.mins[i] = extents.mins[i] - adjust;
            extents.maxs[i] = extents.maxs[i] + adjust;
        }
    }
    extents.maxs[2] = extents.mins[2] + width;

    return;
}


void Octree::draw() const
{
    if (!root)
        return;

    GLboolean usingTextures;
    glGetBooleanv(GL_TEXTURE_2D, &usingTextures);
    glDisable(GL_TEXTURE_2D);

    // CullFrustum needs to still be valid here
    // It should still exist in SceneRender.cxx
    // when this function is called.
    root->draw();
    OcclMgr->update(CullFrustum);
    OcclMgr->draw();

    if (usingTextures)
        glEnable(GL_TEXTURE_2D);

    return;
}


//////////////////////////////////////////////////////////////////////////////
//
// The Nodes
//


OctreeNode::OctreeNode(unsigned char _depth, const Extents& exts,
                       SceneNode** _list, int _listSize)
{
    int i;

    depth = _depth;

    for (i = 0; i < 8; i++)
    {
        children[i] = NULL;
        squeezed[i] = NULL;
    }
    childCount = 0;

    // copy the incoming list
    const int listBytes = _listSize * sizeof(SceneNode*);
    list = (SceneNode**)malloc(listBytes);
    if (listBytes)
        memcpy(list, _list, listBytes);

    // copy the extents, and make a slightly puffed up version
    extents = exts;
    Extents testExts;
    testExts = exts;
    testExts.addMargin(testFudge);

    // find all of the intersecting nodes
    listSize = 0;
    for (i = 0; i < _listSize; i++)
    {
        SceneNode* node = _list[i];
        if (node->inAxisBox(testExts))
        {
            list[listSize] = node;
            listSize++;
        }
    }

    // count will remain as the total numbers of
    // scene nodes that intersect with this cell
    count = listSize;

    // resize the list to save space
    // FIXME: Correctly handle the situation of the realloc failing (which would return a NULL pointer)
    list = (SceneNode**)realloc(list, count * sizeof(SceneNode*));

    // return if this is a leaf node
    if (((int)depth >= maxDepth) || (listSize <= minElements))
    {
        resizeCell();
        //logDebugMessage(4, ("LEAF NODE: depth = %d, items = %i\n", depth, count);
        return;
    }

    // sow the seeds
    depth++;
    makeChildren();

    // non NULLs first
    squeezeChildren(squeezed);

    // resize this branch cell
    resizeCell();

    // leave some lists for FullyVisible grabs
    if ((depth % fullListBreak) != 0)
    {
        listSize = 0;
        free(list);
        list = NULL;
    }

    //logDebugMessage(4, ("BRANCH NODE: depth = %d, squeezed = %i\n", depth, childCount);

    return;
}


void OctreeNode::makeChildren()
{
    int side[3];    // the axis sides  (0 or 1)
    Extents exts;

    // setup the center point
    auto center = 0.5f * (extents.maxs + extents.mins);

    childCount = 0;
    const glm::vec3 extentSet[3] = { extents.mins, center, extents.maxs };

    for (side[0] = 0; side[0] < 2; side[0]++)
    {
        for (side[1] = 0; side[1] < 2; side[1]++)
        {
            for (side[2] = 0; side[2] < 2; side[2]++)
            {

                // calculate the child's extents
                for (int a = 0; a < 3; a++)
                {
                    exts.mins[a] = extentSet[side[a] + 0][a];
                    exts.maxs[a] = extentSet[side[a] + 1][a];
                }

                int kid = side[0] + (2 * side[1]) + (4 * side[2]);

                squeezed[kid] = new OctreeNode(depth, exts, list, count);

                if (squeezed[kid]->getCount() == 0)
                {
                    delete squeezed[kid];
                    squeezed[kid] = NULL;
                }
                else
                    childCount++;
                children[kid] = squeezed[kid];
            }
        }
    }

    return;
}


void OctreeNode::resizeCell()
{
    int i;
    Extents absExts;

    for (i = 0; i < count; i++)
    {
        SceneNode* node = list[i];
        const Extents& nodeExts = node->getExtents();
        absExts.expandToBox(nodeExts);
    }

    for (i = 0; i < 3; i++)
    {
        if (absExts.mins[i] > extents.mins[i])
            extents.mins[i] = absExts.mins[i];
        if (absExts.maxs[i] < extents.maxs[i])
            extents.maxs[i] = absExts.maxs[i];
    }

    return;
}


OctreeNode::~OctreeNode()
{
    for (int i = 0; i < 8; i++)
        delete squeezed[i];

    free(list);
    return;
}

const OctreeNode* OctreeNode::getNode(unsigned char x) const
{
    const OctreeNode* onode = children[(x)];
    if (onode != NULL)
        onode->getFrustumList();
    return onode;
}

const OctreeNode* OctreeNode::getFullNode(unsigned char x) const
{
    const OctreeNode* onode = children[(x)];
    if (onode != NULL)
        onode->getFullyVisibleOcclude();
    return onode;
}


void OctreeNode::getFrustumList() const
{
    IntersectLevel level = testAxisBoxInFrustum(extents, CullFrustum);

    if (level == Outside)
        return;

    IntersectLevel occLevel = OcclMgr->occlude(extents, count);
    if (occLevel == Contained)
        return;

    if ((level == Contained) && (occLevel == Outside))
        getFullyVisible();

    // this cell is only partially contained within
    // the frustum and is not being fully occluded

    // is the viewer in this node?
    // add the closest nodes first

    if (childCount > 0)
    {
        if (F2BSORT)
        {
            const auto &dir = CullFrustum->getDirection();
            unsigned char dirbits = 0;
            if (dir[0] < 0.0f) dirbits |= (1 << 0);
            if (dir[1] < 0.0f) dirbits |= (1 << 1);
            if (dir[2] < 0.0f) dirbits |= (1 << 2);

            if (occLevel == Outside)
            {
                getNode(dirbits);               // 0:  0,0,0
                dirbits ^= (1 << 0);
                getNode(dirbits);               // 1:  1,0,0
                dirbits ^= (1 << 0) | (1 << 1);
                getNode(dirbits);               // 2:  0,1,0
                dirbits ^= (1 << 1) | (1 << 2);
                getNode(dirbits);               // 3:  0,0,1
                dirbits ^= (1 << 0) | (1 << 1) | (1 << 2);
                getNode(dirbits);               // 4:  1,1,0
                dirbits ^= (1 << 1) | (1 << 2);
                getNode(dirbits);               // 5:  1,0,1
                dirbits ^= (1 << 0) | (1 << 1);
                getNode(dirbits);               // 6:  0,1,1
                dirbits ^= (1 << 0);
                getNode(dirbits);               // 7:  1,1,1
            }
            else
            {
                getFullNode(dirbits);               // 0:  0,0,0
                dirbits ^= (1 << 0);
                getFullNode(dirbits);               // 1:  1,0,0
                dirbits ^= (1 << 0) | (1 << 1);
                getFullNode(dirbits);               // 2:  0,1,0
                dirbits ^= (1 << 1) | (1 << 2);
                getFullNode(dirbits);               // 3:  0,0,1
                dirbits ^= (1 << 0) | (1 << 1) | (1 << 2);
                getFullNode(dirbits);               // 4:  1,1,0
                dirbits ^= (1 << 1) | (1 << 2);
                getFullNode(dirbits);               // 5:  1,0,1
                dirbits ^= (1 << 0) | (1 << 1);
                getFullNode(dirbits);               // 6:  0,1,1
                dirbits ^= (1 << 0);
                getFullNode(dirbits);               // 7:  1,1,1
            }
        }
        else
        {
            if (occLevel == Outside)
            {
                for (int i = 0; i < childCount; i++)
                    squeezed[i]->getFrustumList();
            }
            else
            {
                for (int i = 0; i < childCount; i++)
                    squeezed[i]->getFullyVisibleOcclude();
            }
        }
    }
    else
    {
        for (int i = 0; i < listSize; i++)
        {
            SceneNode* node = list[i];
            if (node->octreeState == SceneNode::OctreeCulled)
            {
                addCullListNode(node);
                node->octreeState = SceneNode::OctreePartial;
            }
        }
    }

    return;
}
/*
// FIXME - faster then the recursive version?

static struct NodeStack {
OctreeNode* node;
int child;
} nodeStack[OctreeNode::maxDepth];
static int level;
static int childNum;
static OctreeNode* working;

level = 0;
nodeStack[0].child = 0;
working = (OctreeNode*) this;

while (level >= 0) {
childNum = nodeStack[level].child;
if (working->getChildren() > childNum) {
nodeStack[level].node = working;
nodeStack[level].child++;
working = working->getChild(childNum);
level++;
nodeStack[level].child = 0;
} else {
static int i;
for (i = 0; i < working->getListSize(); i++) {
static SceneNode* node;
node = working->getList()[i];
if (node->octreeState == SceneNode::OctreeCulled) {
addCullListNode (node);
}
node->octreeState = SceneNode::OctreeVisible;
}
level--;
working = nodeStack[level].node;
}
}
*/


void OctreeNode::getRadarList() const
{
    IntersectLevel level = testAxisBoxInFrustum(extents, CullFrustum);

    if (level == Outside)
        return;

    if (level == Contained)
    {
        getFullyVisible();
        return;
    }

    // this cell is only partially contained within
    // the frustum and is not being fully occluded

    if (childCount > 0)
    {
        for (int i = 0; i < childCount; i++)
            squeezed[i]->getRadarList();
    }
    else
    {
        for (int i = 0; i < listSize; i++)
        {
            SceneNode* node = list[i];
            if (node->octreeState == SceneNode::OctreeCulled)
            {
                addCullListNode(node);
                node->octreeState = SceneNode::OctreePartial;
            }
        }
    }

    return;
}


void OctreeNode::getFullyVisible() const
{
    if ((childCount > 0) && (listSize == 0))
    {
        for (int i = 0; i < childCount; i++)
            squeezed[i]->getFullyVisible();
    }
    else
    {
        for (int i = 0; i < listSize; i++)
        {
            SceneNode* node = list[i];
            SceneNode::CullState& state = node->octreeState;
            if (state == SceneNode::OctreeCulled)
                addCullListNode(node);

            state = SceneNode::OctreeVisible;
        }
    }
    return;
}


void OctreeNode::getFullyVisibleOcclude() const
{
    IntersectLevel occLevel = OcclMgr->occlude(extents, count);
    if (occLevel == Contained)
        return;


    if (occLevel == Outside)
        getFullyVisible();

    // this cell is only partially contained within
    // the frustum and is not being fully occluded

    if (childCount > 0)
    {
        for (int i = 0; i < childCount; i++)
            squeezed[i]->getFullyVisibleOcclude();
    }
    else
    {
        for (int i = 0; i < listSize; i++)
        {
            SceneNode* node = list[i];
            if (node->octreeState == SceneNode::OctreeCulled)
            {
                addCullListNode(node);
                node->octreeState = SceneNode::OctreePartial;
            }
        }
    }

    return;
}


void OctreeNode::getShadowList() const
{
    IntersectLevel level = testAxisBoxOcclusion(extents, ShadowPlanes, ShadowCount);

    if (level == Outside)
        return;

    if (level == Contained)
    {
        getFullyShadow();
        return;
    }

    // this cell is only partially contained within the shadow
    // volume we'll have to test all of its sub-cells as well.

    if (childCount > 0)
    {
        for (int i = 0; i < childCount; i++)
            squeezed[i]->getShadowList();
    }
    else
    {
        for (int i = 0; i < listSize; i++)
        {
            SceneNode* node = list[i];
            if (node->octreeState == SceneNode::OctreeCulled)
            {
                addCullListNode(node);
                node->octreeState = SceneNode::OctreePartial;
            }
        }
    }

    return;
}


void OctreeNode::getFullyShadow() const
{
    if ((childCount > 0) && (listSize == 0))
    {
        for (int i = 0; i < childCount; i++)
            squeezed[i]->getFullyShadow();
    }
    else
    {
        for (int i = 0; i < listSize; i++)
        {
            SceneNode* node = list[i];
            SceneNode::CullState& state = node->octreeState;
            if (state == SceneNode::OctreeCulled)
                addCullListNode(node);

            state = SceneNode::OctreeVisible;
        }
    }
    return;
}


const Extents& OctreeNode::getExtents() const
{
    return extents;
}


void OctreeNode::tallyStats()
{
    totalNodes++;
    totalElements += listSize;

    if (childCount > 0)
    {
        for (int i = 0; i < childCount; i++)
            squeezed[i]->tallyStats();
    }
    else
        leafNodes++;

    return;
}


void OctreeNode::draw()
{
    glm::vec4 red = { 1.0f, 0.0f, 0.0f, 0.75f };    // red
    glm::vec4 blue = { 0.0f, 0.0f, 1.0f, 0.75f };   // blue
    glm::vec4 green = { 0.0f, 1.0f, 0.0f, 0.75f };  // green
    glm::vec4 yellow = { 1.0f, 1.0f, 0.0f, 0.75f }; // yellow
    glm::vec4 purple = { 1.0f, 0.0f, 1.0f, 0.75f }; // purple
    glm::vec4 *color = &purple;
    int x, y, z, c;
    glm::vec3 points[5];
    IntersectLevel frustumCull = Contained;
    bool occludeCull = false;

    if (CullFrustum != NULL)
    {
        frustumCull = testAxisBoxInFrustum(extents, CullFrustum);
        occludeCull = OcclMgr->occludePeek(extents);
    }

    // choose the color
    switch (frustumCull)
    {
    case Outside:
        color = &purple;
        break;
    case Partial:
        if (!occludeCull)
            color = &blue;
        else
            color = &green;
        break;
    case Contained:
        if (!occludeCull)
            color = &red;
        else
            color = &yellow;
        break;
    }
    glColor(*color);

    const glm::vec3 exts[2] = { extents.mins, extents.maxs };

    // draw Z-normal squares
    for (z = 0; z < 2; z++)
    {
        for (c = 0; c < 4; c++)
        {
            x = ((c + 0) % 4) / 2;
            y = ((c + 1) % 4) / 2;
            points[c][0] = exts[x][0];
            points[c][1] = exts[y][1];
            points[c][2] = exts[z][2];
        }
        points[4] = points[0];
        glBegin(GL_LINE_STRIP);

        for (int i = 0; i < 5; i++)
            glVertex(points[i]);

        glEnd();
    }

    // draw the corner edges
    for (c = 0; c < 4; c++)
    {
        x = ((c + 0) % 4) / 2;
        y = ((c + 1) % 4) / 2;
        for (z = 0; z < 2; z++)
        {
            points[z][0] = exts[x][0];
            points[z][1] = exts[y][1];
            points[z][2] = exts[z][2];
        }
        glBegin(GL_LINE_STRIP);
        glVertex(points[0]);
        glVertex(points[1]);
        glEnd();
    }

    // draw the kids
    for (c = 0; c < childCount; c++)
        squeezed[c]->draw();

    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
