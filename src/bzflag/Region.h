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

#ifndef __REGION_H__
#define __REGION_H__

// 1st
#include "common.h"

/* system interface headers */
#include <vector>
#include <glm/vec2.hpp>


const float      maxDistance = 1.0e6;

class RegionPoint
{
public:
    RegionPoint(float x, float y);
    RegionPoint(const glm::vec2 &v);
    ~RegionPoint();

    const glm::vec2 &get() const;

private:
    glm::vec2 p;
};


class BzfRegion
{
public:
    BzfRegion(int sides, const glm::vec2 p[2]);
    ~BzfRegion();

    bool        isInside(const glm::vec2 &p) const;
    // get point distance from Region. Point should be outside Region!
    float       getDistance(const glm::vec2 &p, glm::vec2 &nearest) const;
    int         classify(const float p1[2], const float p2[2]) const;
    BzfRegion*      orphanSplitRegion(const float p1[2], const float p2[2]);

    int         getNumSides() const;
    const RegionPoint&  getCorner(int index) const;
    BzfRegion*      getNeighbor(int index) const;

    bool        test(int mailboxIndex) const;
    void        setPathStuff(float distance, BzfRegion* target,
                             const glm::vec2 &p, int mailboxIndex);
    float       getDistance() const;
    BzfRegion*      getTarget() const;
    const glm::vec2 &getA() const;

protected:
    BzfRegion();
    void        splitEdge(const BzfRegion* oldNeighbor,
                          BzfRegion* newNeighbor,
                          const RegionPoint& p,
                          bool onRight);
    void        addSide(const RegionPoint&, BzfRegion* neighbor);
    void        setNeighbor(const BzfRegion* oldNeighbor,
                            BzfRegion* newNeighbor);
    void        tidy();

private:
    std::vector<RegionPoint>    corners;
    std::vector<BzfRegion*>     neighbors;
    int         mailbox;

    BzfRegion*      target;
    float       distance;
    RegionPoint     A;
};


#endif /* __REGION_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
