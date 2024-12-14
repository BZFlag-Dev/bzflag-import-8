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

/* Extents
 *  Encapsulates the data to record the minimum and maximum
 *      values along each axis of an axis-aligned bounding box.
 */

#ifndef BZF_EXTENTS_H
#define BZF_EXTENTS_H

// 1st
#include "common.h"

// System headers
#include <glm/vec3.hpp>
#include <glm/common.hpp>

class Extents
{
public:
    Extents();
    Extents(const glm::vec3 &mins, const glm::vec3 &maxs);

    void reset();

    Extents& operator=(const Extents&);
    void set(const glm::vec3 &mins, const glm::vec3 &maxs);

    void expandToBox(const Extents& box); // expand to contain the box
    void expandToPoint(const glm::vec3&); // expand to contain the point
    void addMargin(float margin);     // widen the extents by "margin"

    float getWidth(int axis) const;

    bool touches(const Extents& orig) const;
    bool contains(const Extents& orig) const;

private:
    // force passing by reference
    Extents(const Extents& orig);

public:
    glm::vec3 mins;
    glm::vec3 maxs;
};


inline void Extents::reset()
{
    mins = glm::vec3(+MAXFLOAT);
    maxs = glm::vec3(-MAXFLOAT);
    return;
}


inline void Extents::set(const glm::vec3 &_mins, const glm::vec3 &_maxs)
{
    mins = _mins;
    maxs = _maxs;
    return;
}


inline Extents::Extents()
{
    reset();
    return;
}


inline Extents::Extents(const glm::vec3 &_mins, const glm::vec3 &_maxs)
{
    set(_mins, _maxs);
    return;
}


inline Extents& Extents::operator=(const Extents& orig)
{
    mins = orig.mins;
    maxs = orig.maxs;
    return *this;
}


inline void Extents::expandToBox(const Extents& test)
{
    // test mins
    mins = glm::min(mins, test.mins);
    // test maxs
    maxs = glm::max(maxs, test.maxs);
    return;
}


inline void Extents::expandToPoint(const glm::vec3 &point)
{
    // test mins
    mins = glm::min(mins, point);
    // test maxs
    maxs = glm::max(maxs, point);
    return;
}


inline bool Extents::touches(const Extents& test) const
{
    return glm::all(glm::lessThanEqual(mins, test.maxs)) &&
           glm::all(glm::greaterThanEqual(maxs, test.mins));
}


inline bool Extents::contains(const Extents& test) const
{
    return glm::all(glm::lessThan(mins, test.mins)) &&
           glm::all(glm::greaterThan(maxs, test.maxs));
}


inline void Extents::addMargin(float margin)
{
    // subtract from the mins
    mins -= margin;
    // add to the maxs
    maxs += margin;
    return;
}


inline float Extents::getWidth(int axis) const
{
    return (maxs[axis] - mins[axis]);
}


#endif // BZF_EXTENTS_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
