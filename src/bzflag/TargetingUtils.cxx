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

// interface header
#include "TargetingUtils.h"

// system headers
#include <math.h>
#include <glm/geometric.hpp>

// common headers
#include "Ray.h"

// local headers
#include "ShotStrategy.h"

// These routines are 2 dimensional

float TargetingUtils::normalizeAngle(float ang)
{
    if (ang < -1.0f * M_PI) ang += (float)(2.0 * M_PI);
    if (ang > 1.0f * M_PI) ang -= (float)(2.0 * M_PI);
    return ang;
}

glm::vec2 TargetingUtils::getUnitVector(const glm::vec2 &src,
                                        const glm::vec2 &target)
{
    const auto unitVector = target - src;

    if (!(unitVector.x || unitVector.y))
        return glm::vec2(0.0f);

    return glm::normalize(unitVector);
}

glm::vec3 TargetingUtils::get3DUnitVector(
    const glm::vec3 &src,
    const glm::vec3 &target)
{
    const auto unitVector = target - src;

    if (!(unitVector.x || unitVector.y || unitVector.z))
        return glm::vec3(0.0f);

    return glm::normalize(unitVector);
}

float TargetingUtils::getTargetDistance(const glm::vec2 &src,
                                        const glm::vec2 &target)
{
    return glm::distance(src, target);
}

float TargetingUtils::getTargetAzimuth(const glm::vec3 &src,
                                       const glm::vec3 &target)
{
    return atan2f((target[1] - src[1]), (target[0] - src[0]));
}

float TargetingUtils::getTargetRotation( const float startAzimuth, float targetAzimuth )
{
    float targetRotation = targetAzimuth - startAzimuth;
    if (targetRotation < -1.0f * M_PI) targetRotation += (float)(2.0 * M_PI);
    if (targetRotation > 1.0f * M_PI) targetRotation -= (float)(2.0 * M_PI);

    return targetRotation;
}

float TargetingUtils::getTargetAngleDifference(const glm::vec2 &src, float srcAzimuth, const glm::vec3 &target)
{
    const auto targetUnitVector = getUnitVector(src, target);

    const auto srcUnitVector = glm::vec2(cosf(srcAzimuth), sinf(srcAzimuth));

    return acos(glm::dot(targetUnitVector, srcUnitVector));
}

bool TargetingUtils::isLocationObscured(const glm::vec3 &src, const glm::vec2 &target)
{
    const auto src2 = glm::vec2(src);
    auto dir = glm::vec3(getUnitVector(src2, target), 0.0f);

    Ray tankRay(src, dir);
    float targetDistance = glm::distance(src2, target);
    const Obstacle *building = ShotStrategy::getFirstBuilding(tankRay, -0.5f, targetDistance);
    return building != NULL;
}

float TargetingUtils::getOpenDistance(const glm::vec3 &src, const float azimuth )
{
    float t = MAXFLOAT; //Some constant?

    const auto dir = glm::vec3(cosf(azimuth), sinf(azimuth), 0.0f);
    auto pos = src;   // Don't hit building because you're sitting on one.
    pos.z += 0.1f;
    Ray tankRay( pos, dir );
    ShotStrategy::getFirstBuilding(tankRay, -0.5f, t);
    return t;
}

bool TargetingUtils::getFirstCollisionPoint(
    const glm::vec3 &src, const glm::vec3 &target, glm::vec3 &collisionPt)
{
    float t = MAXFLOAT;
    const auto dir = get3DUnitVector(src, target);

    Ray tankRay(src, dir);
    const Obstacle *building = ShotStrategy::getFirstBuilding(tankRay, 0.0f, t);
    if (building == NULL)
        return false;

    collisionPt = src + dir * t;
    return true;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
