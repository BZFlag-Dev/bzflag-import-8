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

/* interface header */
#include "Region.h"

/* system implementation headers */
#include <math.h>
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>


RegionPoint::RegionPoint(float x, float y)
{
    p[0] = x;
    p[1] = y;
}

RegionPoint::RegionPoint(const glm::vec2 &v)
{
    p = v;
}

RegionPoint::~RegionPoint()
{
    // do nothing
}

const glm::vec2 &RegionPoint::get() const
{
    return p;
}

//
// BzfRegion
//
// Note: edge from corners[0] to corners[1] has neighbor stored in
// neighbors[0].  all regions should have corners ordered counter-
// clockwise.
//

BzfRegion::BzfRegion() : mailbox(0), target(0), distance(0.0), A(0.0, 0.0)
{
}

BzfRegion::BzfRegion(int sides, const glm::vec2 p[]) :
    mailbox(0), target(0), distance(), A(0.0, 0.0)
{
    for (int i = 0; i < sides; i++)
    {
        corners.push_back(RegionPoint(p[i]));
        neighbors.push_back((BzfRegion*)0);
    }
}

BzfRegion::~BzfRegion()
{
    // tell neighbors I'm going away
    const int count = corners.size();
    for (int i = 0; i < count; i++)
        if (neighbors[i])
            neighbors[i]->setNeighbor(this, 0);
}

bool BzfRegion::isInside(const glm::vec2 &p) const
{
    // see if testPoint is inside my edges
    const int count = corners.size();
    if (count < 3) return false;
    bool inside = false;
    auto p1 = corners[count - 1].get();
    glm::vec2 p2;
    for (int i = 0; i < count; p1 = p2, i++)
    {
        p2 = corners[i].get();
        if ((p1[1] >= p[1] && p2[1] >= p[1]) || (p1[1] < p[1] && p2[1] < p[1]))
            continue;
        if (p1[0] < p[0] && p2[0] < p[0])
            continue;
        if (p1[0] >= p[0] && p2[0] >= p[0]) inside = !inside;
        else
        {
            float tolerance = (p1[1] < p2[1]) ? -0.001f : 0.001f;
            if ((p[1] - p1[1]) * (p2[0] - p1[0]) / (p2[1] - p1[1]) >=
                    p[0] - p1[0] + tolerance)
                inside = !inside;
        }
    }
    return inside;
}

float BzfRegion::getDistance(const glm::vec2 &p, glm::vec2 &nearest) const
{
    const int count = corners.size();
    float currentDistance = maxDistance;
    float pointDistance;

    //compute distance from any edge
    auto p1 = corners[count - 1].get();
    glm::vec2 d;
    glm::vec2 m;
    float t;
    float edgeSquareDist;
    glm::vec2 temp;
    for (int c = 0; c < count; c++)
    {
        const auto p2 = corners[c].get();
        d = p2 - p1;
        m = p  - p1;
        edgeSquareDist = glm::length2(d);
        t = glm::dot(m, d) / edgeSquareDist;
        if (t <= 0)
        {
            pointDistance = glm::length(m);
            temp = p1;
        }
        else if (t >= 1)
        {
            pointDistance = glm::distance(m, d);
            temp = p2;
        }
        else
        {
            pointDistance = glm::distance(m, t * d);
            temp = p1 + t * d;
        }
        if (pointDistance < currentDistance)
        {
            currentDistance = pointDistance;
            nearest = temp;
        }
        p1 = p2;
    }
    return currentDistance;
}

int BzfRegion::classify(const float e1[2], const float e2[2]) const
{
    // return true if all points lie to right side of edge
    const float dx = e2[0] - e1[0];
    const float dy = e2[1] - e1[1];
    const float d = dy * e1[0] - dx * e1[1];
    int toRight = 0, onEdge = 0;
    const int count = corners.size();
    for (int i = 0; i < count; i++)
    {
        const auto &p = corners[i].get();
        const float e = -dy * p[0] + dx * p[1] + d;
        if (e < -0.00001) toRight++;
        else if (e <= 0.00001) onEdge++;
    }
    if (toRight + onEdge == count) return 1;  // all to right
    if (toRight != 0) return 0;           // not all to left -- split
    return -1;                    // all to left
}

int BzfRegion::getNumSides() const
{
    return neighbors.size();
}

const RegionPoint& BzfRegion::getCorner(int index) const
{
    return corners[index];
}

BzfRegion* BzfRegion::getNeighbor(int index) const
{
    return neighbors[index];
}

BzfRegion* BzfRegion::orphanSplitRegion(const float e1[2], const float e2[2])
{
    // if edge p1,p2 intersects me then split myself along that edge.
    // return new region (the other half of the split), or NULL if no
    // split occurred.  the new region (if it exists) will be to the
    // right of the cutting edge (when moving from e1 to e2).
    const int count = corners.size();
    if (count == 0) return NULL;
    int i, split = 0, edge[2];
    float tsplit[2], etsplit[2];
    const float dx = e2[0] - e1[0];
    const float dy = e2[1] - e1[1];
    const float d = dy * e1[0] - dx * e1[1];
    auto p1 = corners[0].get();

    // Vector Product between splitter and corner : to know if right or left
    float lastPVect = d - dy * p1[0] + dx * p1[1];
    // keep sign of PVector, too risky to recompute
    bool lastSign = (lastPVect >= 0);
    // Sign (Left or Right) of first corner and hence first transition
    bool fistCornerRight = lastSign;
    for (i = 0; split < 2 && i < count; i++)
    {
        const auto &p2 = corners[(i + 1) % count].get();
        const float pVect = d - dy * p2[0] + dx * p2[1];
        const bool newSign = (pVect >= 0);

        // if both ends of region edge are on same side of cutting edge then
        // can't intersect the edge.
        if (lastSign != newSign)
        {
            const float delta = lastPVect - pVect;
            // compute distance along region edge where intersection occurs
            tsplit[split] = lastPVect / delta;
            edge[split] = i;
            etsplit[split] = ((p1[1] - p2[1]) * (p1[0] - e1[0]) +
                              (p2[0] - p1[0]) * (p1[1] - e1[1])) / delta;
            split++;
        }
        lastPVect = pVect;
        lastSign = newSign;
        p1 = p2;
    }

    // done if no intersections
    if (split != 2 ||
            (etsplit[0] <= 0.0 && etsplit[1] <= 0.0) ||
            (etsplit[0] >= 1.0 && etsplit[1] >= 1.0))
        return 0;

    // corner is t the left of cutting edge -- new region between edge 1 and 0
    if (!fistCornerRight)
    {
        i = edge[0];
        edge[0] = edge[1];
        edge[1] = i;
        float tsplitTemp = tsplit[0];
        tsplit[0] = tsplit[1];
        tsplit[1] = tsplitTemp;
    }

    // make new corners and region
    p1 = corners[edge[1]].get();
    auto p2 = corners[(edge[1]+1) % count].get();
    RegionPoint n2(p1 + tsplit[1] * (p2 - p1));
    p1 = corners[edge[0]].get();
    p2 = corners[(edge[0]+1) % count].get();
    RegionPoint n1(p1 + tsplit[0] * (p2 - p1));
    BzfRegion* newRegion = new BzfRegion;

    // add sides to new region and remove them from me.  the new region
    // must be to the right of the edge.  see if the corner after the
    // split on the first split edge is to the right or left of the
    // cutting edge.
    {
        // add sides to new region
        newRegion->addSide(n1, neighbors[edge[0]]);
        i = edge[0];
        while (true)
        {
            i++;
            if (i == count)
                i = 0;
            newRegion->addSide(corners[i], neighbors[i]);
            if (i == edge[1])
                break;
            if (neighbors[i]) neighbors[i]->setNeighbor(this, newRegion);
        }
        newRegion->addSide(n2, this);

        // tell neighbors on split edges to split the shared edge at the
        // same place and to point to their new neighbor
        // tell them about their new neighbor.
        if (neighbors[edge[0]])
            neighbors[edge[0]]->splitEdge(this, newRegion, n1, true);
        if (neighbors[edge[1]])
            neighbors[edge[1]]->splitEdge(this, newRegion, n2, false);

        // remove old edges from myself and add new ones
        std::vector<RegionPoint> newCorners;
        std::vector<BzfRegion*> newNeighbors;
        newCorners.push_back(n2);
        newNeighbors.push_back(neighbors[edge[1]]);
        i = edge[1];
        while (true)
        {
            i++;
            if (i == count)
                i = 0;
            newCorners.push_back(corners[i]);
            newNeighbors.push_back(neighbors[i]);
            if (i == edge[0])
                break;
        }
        newCorners.push_back(n1);
        newNeighbors.push_back(newRegion);
        corners = newCorners;
        neighbors = newNeighbors;
    }

    // throw out degenerate edges in myself and newRegion
    tidy();
    newRegion->tidy();

    // all done
    return newRegion;
}

void BzfRegion::splitEdge(const BzfRegion* oldNeighbor,
                          BzfRegion* newNeighbor,
                          const RegionPoint& p,
                          bool onRight)
{
    // split my edge which has neighbor oldNeighbor at point p.
    // set the neighbor for the edge on the right if onRight is true
    // or on the left if onRight is false to newNeighbor.
    const int count = corners.size();
    for (int i = 0; i < count; i++)
        if (neighbors[i] == oldNeighbor)
        {
            std::vector<RegionPoint>::iterator it1 = corners.begin();
            for (int j = 0; j < i + 1; j++) ++it1;
            corners.insert(it1, p);
            if (onRight)
            {
                std::vector<BzfRegion*>::iterator it2 = neighbors.begin();
                for (int j = 0; j < i; j++) ++it2;
                neighbors.insert(it2, newNeighbor);
            }
            else
            {
                std::vector<BzfRegion*>::iterator it2 = neighbors.begin();
                for (int j = 0; j < i + 1; j++) ++it2;
                neighbors.insert(it2, newNeighbor);
            }
            tidy();
            break;
        }
}

void BzfRegion::addSide(const RegionPoint& p, BzfRegion* neighbor)
{
    corners.push_back(p);
    neighbors.push_back(neighbor);
}

void BzfRegion::setNeighbor(const BzfRegion* oldNeighbor,
                            BzfRegion* newNeighbor)
{
    const int count = corners.size();
    for (int i = 0; i < count; i++)
        if (neighbors[i] == oldNeighbor)
        {
            neighbors[i] = newNeighbor;
            break;
        }
}

void BzfRegion::tidy()
{
    // throw out degenerate edges
    int count = corners.size();
    for (int i = 0; i < count; i++)
    {
        const auto &p1 = corners[i].get();
        const auto &p2 = corners[(i+1)%count].get();
        if (fabs(p1[0] - p2[0]) < ZERO_TOLERANCE && fabs(p1[1] - p2[1]) < ZERO_TOLERANCE)
        {
            std::vector<RegionPoint>::iterator it1 = corners.begin();
            for (int j = 0; j < i; j++) ++it1;
            corners.erase(it1);
            std::vector<BzfRegion*>::iterator it2 = neighbors.begin();
            for (int k = 0; k < i; k++) ++it2;
            neighbors.erase(it2);
            i--;
            count--;
        }
    }
}

bool BzfRegion::test(int mailboxIndex) const
{
    return (mailbox != mailboxIndex);
}

void BzfRegion::setPathStuff(float _distance,
                             BzfRegion* _target,
                             const glm::vec2 &_a, int mailboxIndex)
{
    distance = _distance;
    target = _target;
    A = RegionPoint(_a);
    mailbox = mailboxIndex;
}

float BzfRegion::getDistance() const
{
    return distance;
}

BzfRegion* BzfRegion::getTarget() const
{
    return target;
}

const glm::vec2 &BzfRegion::getA() const
{
    return A.get();
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
