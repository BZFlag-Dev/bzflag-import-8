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

/* SphereObstacle:
 *  Encapsulates a cone in the game environment.
 */

#ifndef BZF_SPHERE_OBSTACLE_H
#define BZF_SPHERE_OBSTACLE_H

#include "common.h"
#include <string>
#include "Obstacle.h"
#include "MeshObstacle.h"
#include "MeshTransform.h"
#include "BzMaterial.h"


class SphereObstacle : public Obstacle
{
public:

    enum
    {
        Edge,
        Bottom,
        MaterialCount
    };

    SphereObstacle();
    SphereObstacle(const MeshTransform& transform,
                   const glm::vec3 &_pos, const glm::vec3 &_size,
                   float _rotation, const float _texsize[2],
                   bool _useNormals, bool hemisphere,
                   int _divisions, const BzMaterial* mats[MaterialCount],
                   int physics, bool bounce, bool drive, bool shoot, bool ricochet);
    ~SphereObstacle();

    Obstacle* copyWithTransform(const MeshTransform&) const override;

    MeshObstacle* makeMesh();

    const char* getType() const override;
    static const char* getClassName(); // const
    bool isValid() const override;

    float intersect(const Ray&) const override;
    void getNormal(const glm::vec3 &p, glm::vec3 &n) const override;
    void get3DNormal(const glm::vec3 &p, glm::vec3 &n) const override;

    bool inCylinder(const glm::vec3 &p, float radius, float height) const override;

    int packSize() const override;
    void *pack(void*) const override;
    const void *unpack(const void*) override;

    void print(std::ostream& out, const std::string& indent) const override;

private:
    void finalize();

private:
    static const char* typeName;

    MeshTransform transform;
    int divisions;
    bool hemisphere;
    int phydrv;
    bool smoothBounce;
    bool useNormals;
    float texsize[2];
    const BzMaterial* materials[MaterialCount];
};


#endif // BZF_SPHERE_OBSTACLE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
