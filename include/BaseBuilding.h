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

/* BaseBuilding:
 *  Encapsulates a base in the game environment
 */

#ifndef BZF_BASE_BUILDING_H
#define BZF_BASE_BUILDING_H

// Inherits from
#include "BoxBuilding.h"

class BaseBuilding : public BoxBuilding
{

    friend class ObstacleModifier;

public:
    BaseBuilding();
    BaseBuilding(const glm::vec3 &pos, float rotation,
                 const glm::vec3 &size, int _team, bool ricochet);
    ~BaseBuilding();

    Obstacle*   copyWithTransform(const MeshTransform&) const override;

    const char*     getType() const override;
    static const char*  getClassName(); // const

    bool        inCylinder(const glm::vec3 &p, float radius, float height) const override;
    bool        inMovingBox(const glm::vec3 &oldP, float oldAngle,
                            const glm::vec3 &newP, float newAngle,
                            float halfWidth, float halfBreadth, float height) const override;
    bool        isCrossing(const glm::vec3 &p, float angle,
                           float halfWidth, float halfBreadth, float height,
                           glm::vec4 *plane) const override;

    int         getTeam() const;

    int packSize() const override;
    void *pack(void*) const override;
    const void *unpack(const void*) override;

    void print(std::ostream& out, const std::string& indent) const override;
    void printOBJ(std::ostream& out, const std::string& indent) const override;

    std::string     userTextures[2];

private:
    static const char*  typeName;
    int team;
};

#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
