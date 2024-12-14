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

/* TetraBuilding:
 *  Encapsulates a tetrahederon in the game environment.
 */

#ifndef BZF_TETRA_BUILDING_H
#define BZF_TETRA_BUILDING_H

// Inherits from
#include "Obstacle.h"

// System headers
#include <string>

// Common headers
#include "MeshObstacle.h"
#include "MeshTransform.h"
#include "BzMaterial.h"

class TetraBuilding : public Obstacle
{
public:

    TetraBuilding();
    TetraBuilding(const MeshTransform& transform,
                  const glm::vec3 vertices[4], const float normals[4][3][3],
                  const float texCoords[4][3][2], const bool useNormals[4],
                  const bool useTexCoords[4], const BzMaterial* materials[4],
                  bool drive = false, bool shoot = false, bool ricochet = false);
    ~TetraBuilding();

    Obstacle* copyWithTransform(const MeshTransform&) const override;

    MeshObstacle* makeMesh();

    void        finalize();

    const char*     getType() const override;
    static const char*  getClassName(); // const
    bool        isValid() const override;

    float       intersect(const Ray&) const override;
    void        getNormal(const glm::vec3 &p, glm::vec3 &n) const override;
    void        get3DNormal(const glm::vec3 &p, glm::vec3 &n) const override;

    bool        inCylinder(const glm::vec3 &p, float radius, float height) const override;

    void        getCorner(int index, float* pos) const;

    int packSize() const override;
    void *pack(void*) const override;
    const void *unpack(const void*) override;

    void print(std::ostream& out, const std::string& indent) const override;

private:
    void checkVertexOrder();

private:
    static const char*  typeName;

    MeshTransform transform;
    glm::vec3 vertices[4];
    float normals[4][3][3];
    float texcoords[4][3][2];
    bool useNormals[4];
    bool useTexcoords[4];
    const BzMaterial* materials[4];
};


#endif // BZF_TETRA_BUILDING_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
