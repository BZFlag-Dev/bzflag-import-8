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

#ifndef BZF_RAY_H
#define BZF_RAY_H

// 1st
#include "common.h"

// System headers
#include <string.h> // for memset()
#include <glm/vec3.hpp>

/** Encapsulates a semi-infinite ray. */

class Ray
{
public:
    Ray();
    Ray(const glm::vec3 &o, const glm::vec3 &d);
    Ray(const Ray&);
    ~Ray();
    Ray&        operator=(const Ray&);

    const glm::vec3 &getOrigin() const;
    const glm::vec3 &getDirection() const;
    void             getPoint(float t, glm::vec3 &p) const;

private:
    glm::vec3 o;
    glm::vec3 d;
};

//
// Ray
//

inline Ray::Ray()
    : o(0.0f), d(0.0f)
{
}

inline Ray::~Ray()
{
    // do nothing
}

inline const glm::vec3 &Ray::getOrigin() const
{
    return o;
}

inline const glm::vec3 &Ray::getDirection() const
{
    return d;
}

#endif // BZF_RAY_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
