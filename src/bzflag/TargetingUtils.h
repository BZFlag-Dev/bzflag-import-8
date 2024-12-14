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

#ifndef _TARGETINGUTILS__H_
#define _TARGETINGUTILS__H_

// common - 1st
#include "common.h"

// System headers
#include <glm/fwd.hpp>

class TargetingUtils
{
public:
    static float normalizeAngle( float ang );
    static float getTargetDistance(const glm::vec2 &src,
                                   const glm::vec2 &target);
    static glm::vec2 getUnitVector(const glm::vec2 &src,
                                   const glm::vec2 &target);
    static glm::vec3 get3DUnitVector(const glm::vec3 &src,
                                     const glm::vec3 &target);
    static float getTargetAzimuth(const glm::vec3 &src, const glm::vec3 &target);
    static float getTargetRotation( const float startAzimuth, float targetAzimuth );
    static float getTargetAngleDifference(const glm::vec2 &src,
                                          float srcAzimuth,
                                          const glm::vec3 &target);
    static bool isLocationObscured(const glm::vec3 &src,
                                   const glm::vec2 &target);
    static float getOpenDistance(const glm::vec3 &src, const float azimuth);
    static bool getFirstCollisionPoint(
        const glm::vec3 &src,
        const glm::vec3 &target,
        glm::vec3 &collisionPt);
};

#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
