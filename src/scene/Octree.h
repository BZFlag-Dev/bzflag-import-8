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

#ifndef __OCTREE_H__
#define __OCTREE_H__

#include "SceneNode.h"
#include "Frustum.h"
#include "Extents.h"


class OctreeNode;


class Octree
{

public:

    Octree();
    ~Octree();

    void clear ();

    void addNodes (SceneNode** list, int listSize, int depth, int elements);

    int getFrustumList (SceneNode** list, int listSize,
                        const Frustum* frustum) const;
    int getShadowList (SceneNode** list, int listSize,
                       int planeCount, const glm::vec4 *planes) const;
    int getRadarList (SceneNode** list, int listSize,
                      const Frustum* frustum) const;

    void setOccluderManager(int);

    void draw () const;

    const Extents* getVisualExtents() const;


private: // methods
    void getExtents(SceneNode** list, int listSize);

private: // data
    OctreeNode* root;
    Extents extents;
    Extents visualExtents;
};



class OctreeNode
{

public:

    OctreeNode(unsigned char depth, const Extents& exts,
               SceneNode** list, int listSize);
    ~OctreeNode();

    void getFrustumList () const;
    void getShadowList () const;
    void getRadarList () const;
    void getFullyVisible () const;
    void getFullyVisibleOcclude () const;
    void getFullyShadow () const;
    OctreeNode* getChild (int child);

    int getCount() const;    // number of nodes in this and subnodes
    int getChildren() const; // number of children
    int getListSize() const; // number of nodes in this node
    SceneNode** getList() const;     // list of nodes
    const Extents& getExtents() const;

    void tallyStats();
    void draw ();

private:

    void makeChildren ();
    void resizeCell ();

    enum CullLevel
    {
        NoCull,
        PartialCull,
        FullCull
    };

    unsigned char depth;
    Extents extents;
    unsigned char childCount;
    OctreeNode* children[8];
    OctreeNode* squeezed[8];
    int count;  // number of nodes in this and subnodes
    int listSize;
    SceneNode** list;

    const OctreeNode* getNode(unsigned char x) const;
    const OctreeNode* getFullNode(unsigned char x) const;
};


inline int OctreeNode::getCount() const
{
    return count;
}

inline SceneNode** OctreeNode::getList() const
{
    return list;
}

inline int OctreeNode::getListSize() const
{
    return listSize;
}

inline int OctreeNode::getChildren() const
{
    return childCount;
}

inline OctreeNode* OctreeNode::getChild (int child)
{
    return children[child];
}

inline const Extents* Octree::getVisualExtents() const
{
    return &visualExtents;
}


#endif // __OCTREE_H__

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
