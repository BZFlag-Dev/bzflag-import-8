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

// Interface
#include "WallObstacle.h"

// System headers
#include <math.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

// Common headers
#include "global.h"
#include "Pack.h"
#include "Intersect.h"

const char*     WallObstacle::typeName = "WallObstacle";

WallObstacle::WallObstacle()
{
    // do nothing
}

WallObstacle::WallObstacle(const glm::vec3 &p, float a, float b, float h, bool rico) :
    Obstacle(p, a, 0.0, b, h, false, false, rico)
{
    finalize();
}

void WallObstacle::finalize()
{
    // compute normal
    const auto &p = getPosition();
    const float a = getRotation();
    plane[0] = cosf(a);
    plane[1] = sinf(a);
    plane[2] = 0.0;
    plane[3] = -(p[0] * plane[0] + p[1] * plane[1] + p[2] * plane[2]);

    return;
}

WallObstacle::~WallObstacle()
{
    // do nothing
}

const char*     WallObstacle::getType() const
{
    return typeName;
}

const char*     WallObstacle::getClassName() // const
{
    return typeName;
}

float           WallObstacle::intersect(const Ray& r) const
{
    const auto o = glm::vec4(r.getOrigin(), 1.0f);
    const auto &d = r.getDirection();
    const float dot = -glm::dot(d, glm::vec3(plane));
    if (dot == 0.0f) return -1.0f;
    float t = glm::dot(o, plane) / dot;
    return t;
}

void WallObstacle::getNormal(const glm::vec3 &, glm::vec3 &n) const
{
    n = plane;
}

bool WallObstacle::inCylinder(const glm::vec3 &p, float r, float UNUSED( height )) const
{
    return p[0] * plane[0] + p[1] * plane[1] + p[2] * plane[2] + plane[3] < r;
}

bool            WallObstacle::inBox(const glm::vec3 &p, float _angle,
                                    float halfWidth, float halfBreadth,
                                    float UNUSED( height )) const
{
    const float xWidth = cosf(_angle);
    const float yWidth = sinf(_angle);
    const float xBreadth = -yWidth;
    const float yBreadth = xWidth;
    glm::vec3 corner;
    corner[2] = p[2];

    // check to see if any corner is inside negative half-space
    corner[0] = p[0] - xWidth * halfWidth - xBreadth * halfBreadth;
    corner[1] = p[1] - yWidth * halfWidth - yBreadth * halfBreadth;
    if (inCylinder(corner, 0.0f, 0.0f)) return true;
    corner[0] = p[0] + xWidth * halfWidth - xBreadth * halfBreadth;
    corner[1] = p[1] + yWidth * halfWidth - yBreadth * halfBreadth;
    if (inCylinder(corner, 0.0f, 0.0f)) return true;
    corner[0] = p[0] - xWidth * halfWidth + xBreadth * halfBreadth;
    corner[1] = p[1] - yWidth * halfWidth + yBreadth * halfBreadth;
    if (inCylinder(corner, 0.0f, 0.0f)) return true;
    corner[0] = p[0] + xWidth * halfWidth + xBreadth * halfBreadth;
    corner[1] = p[1] + yWidth * halfWidth + yBreadth * halfBreadth;
    if (inCylinder(corner, 0.0f, 0.0f)) return true;

    return false;
}

bool WallObstacle::inMovingBox(const glm::vec3 &UNUSED( oldP ), float UNUSED( oldAngle ),
                               const glm::vec3 &p, float _angle,
                               float halfWidth, float halfBreadth, float height) const

{
    return inBox (p, _angle, halfWidth, halfBreadth, height);
}

bool            WallObstacle::getHitNormal(
    const glm::vec3 &, float,
    const glm::vec3 &, float,
    float, float, float,
    glm::vec3 &normal) const
{
    static const auto zero = glm::vec3(0.0f);
    getNormal(zero, normal);
    return true;
}



void* WallObstacle::pack(void* buf) const
{
    buf = nboPackVector(buf, pos);
    buf = nboPackFloat(buf, angle);
    buf = nboPackFloat(buf, size[1]);
    buf = nboPackFloat(buf, size[2]);

    unsigned char stateByte = 0;
    stateByte |= canRicochet() ? _RICOCHET : 0;
    buf = nboPackUByte(buf,stateByte);

    return buf;
}


const void* WallObstacle::unpack(const void* buf)
{
    buf = nboUnpackVector(buf, pos);
    buf = nboUnpackFloat(buf, angle);
    buf = nboUnpackFloat(buf, size[1]);
    buf = nboUnpackFloat(buf, size[2]);

    unsigned char stateByte;
    buf = nboUnpackUByte(buf,stateByte);
    ricochet = (stateByte & _RICOCHET) != 0;

    finalize();

    return buf;
}


int WallObstacle::packSize() const
{
    int fullSize = 0;
    fullSize += sizeof(float[3]); // pos
    fullSize += sizeof(float);    // rotation
    fullSize += sizeof(float);    // breadth
    fullSize += sizeof(float);    // height
    fullSize += sizeof(uint8_t);
    return fullSize;
}


void WallObstacle::print(std::ostream& UNUSED(out),
                         const std::string& UNUSED(indent)) const
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
