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

/* ArcObstacle:
 *  Encapsulates an arc in the game environment.
 */

#ifndef BZF_ARC_OBSTACLE_H
#define BZF_ARC_OBSTACLE_H

// Inherits from
#include "Obstacle.h"

// System headers
#include <string>

// Common headers
#include "MeshObstacle.h"
#include "MeshTransform.h"
#include "BzMaterial.h"


class ArcObstacle : public Obstacle
{
public:

    enum
    {
        Top,
        Bottom,
        Inside,
        Outside_,
        StartFace,
        EndFace,
        MaterialCount
    };

    ArcObstacle();
    ArcObstacle(const MeshTransform& transform,
                const glm::vec3 &_pos, const glm::vec3 &_size,
                float _rotation, float _angle, float _ratio,
                const float _texsize[4], bool _useNormals,
                int _divisions, const BzMaterial* mats[MaterialCount],
                int physics, bool bounce, bool drive, bool shoot, bool ricochet);
    ~ArcObstacle();

    Obstacle* copyWithTransform(const MeshTransform&) const override;

    MeshObstacle* makeMesh();

    const char* getType() const override;
    static const char* getClassName(); // const
    bool isValid() const override;
    bool isFlatTop() const override;

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
    MeshObstacle* makePie(bool isCircle, float a, float r, float h,
                          float radius, float squish, float texsz[4]);
    MeshObstacle* makeRing(bool isCircle, float a, float r, float h,
                           float inrad, float outrad, float squish,
                           float texsz[4]);
private:
    static const char* typeName;

    MeshTransform transform;
    int divisions;
    float sweepAngle;
    float ratio;
    int phydrv;
    bool smoothBounce;
    bool useNormals;
    float texsize[4];
    const BzMaterial* materials[MaterialCount];
};


#endif // BZF_ARC_OBSTACLE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
