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

/* WallObstacle:
 *  Encapsulates an infinite wall in the game environment.
 */

#ifndef BZF_WALL_OBSTACLE_H
#define BZF_WALL_OBSTACLE_H

// Inherits from
#include "Obstacle.h"

// System interfaces
#include <string>
#include <glm/vec4.hpp>

class WallObstacle : public Obstacle
{
public:
    WallObstacle();
    WallObstacle(const glm::vec3 &pos, float rotation,
                 float breadth, float height, bool ricochet);
    ~WallObstacle();

    const char*     getType() const override;
    static const char*  getClassName(); // const

    float       intersect(const Ray&) const override;
    void        getNormal(const glm::vec3 &p, glm::vec3 &n) const override;

    bool        inCylinder(const glm::vec3 &p, float radius, float height) const override;
    bool        inBox(const glm::vec3 &p, float angle,
                      float halfWidth, float halfBreadth, float height) const override;
    bool        inMovingBox(const glm::vec3 &oldP, float oldAngle,
                            const glm::vec3 &newP, float newAngle,
                            float halfWidth, float halfBreadth, float height) const override;

    bool        getHitNormal(
        const glm::vec3 &pos1, float azimuth1,
        const glm::vec3 &pos2, float azimuth2,
        float halfWidth, float halfBreadth,
        float height,
        glm::vec3 &normal) const override;

    int packSize() const override;
    void *pack(void*) const override;
    const void *unpack(const void*) override;

    void print(std::ostream& out, const std::string& indent) const override;

    std::string     userTextures[1];

private:
    void finalize();

private:
    glm::vec4 plane;
    static const char*  typeName;
};

#endif // BZF_WALL_OBSTACLE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
