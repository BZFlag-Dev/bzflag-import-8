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

// Its interface
#include "Ray.h"

Ray::Ray(const glm::vec3 &_o, const glm::vec3 &_d)
{
    o = _o;
    d = _d;
}

Ray::Ray(const Ray& r)
{
    o = r.o;
    d = r.d;
}

Ray&            Ray::operator=(const Ray& r)
{
    if (this != &r)
    {
        o = r.o;
        d = r.d;
    }
    return *this;
}

void Ray::getPoint(float t, glm::vec3 &p) const
{
    p = o + t * d;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
