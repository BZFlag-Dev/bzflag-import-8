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

#ifndef BZF_MESH_OBSTACLE_H
#define BZF_MESH_OBSTACLE_H

// Inherits from
#include "Obstacle.h"

// System headers
#include <string>
#include <vector>
#include <iostream>
#include <glm/fwd.hpp>

// Common headers
#include "Ray.h"
#include "MeshFace.h"
#include "MeshTransform.h"

class MeshDrawInfo;

class MeshObstacle : public Obstacle
{
public:
    MeshObstacle();
    MeshObstacle(const MeshTransform& transform,
                 const std::vector<char>& checkTypes,
                 const std::vector<glm::vec3>& checkPoints,
                 const std::vector<glm::vec3>& vertices,
                 const std::vector<glm::vec3>& normals,
                 const std::vector<glm::vec2>& texcoords,
                 int faceCount, bool noclusters,
                 bool bounce, bool drive, bool shoot, bool ricochet);

    bool addFace (const std::vector<int>& vertices,
                  const std::vector<int>& normals,
                  const std::vector<int>& texcoords,
                  const BzMaterial* bzMaterial, int physics,
                  bool noclusters, bool bounce, bool drive, bool shoot,
                  bool ricochet, bool triangulate);

    ~MeshObstacle();

    void finalize();

    Obstacle* copyWithTransform(const MeshTransform&) const override;
    void copyFace(int face, MeshObstacle* mesh) const;

    void setName(const std::string& name);
    const std::string&  getName() const;

    enum CheckType
    {
        CheckInside =  0,
        CheckOutside = 1,
        InsideParity = 2,
        OutsidePartiy = 3
    };

    const char* getType() const override;
    static const char* getClassName(); // const
    bool isValid() const override;

    float intersect(const Ray&) const override;
    void getNormal(const glm::vec3 &p, glm::vec3 &n) const override;
    void get3DNormal(const glm::vec3 &p, glm::vec3 &n) const override;

    bool inCylinder(const glm::vec3 &p, float radius, float height) const override;
    bool inBox(const glm::vec3 &p, float angle,
               float halfWidth, float halfBreadth, float height) const override;
    bool inMovingBox(const glm::vec3 &oldP, float oldAngle,
                     const glm::vec3 &newP, float newAngle,
                     float halfWidth, float halfBreadth, float height) const override;

    bool getHitNormal(const glm::vec3 &pos1, float azimuth1,
                      const glm::vec3 &pos2, float azimuth2,
                      float halfWidth, float halfBreadth,
                      float height, glm::vec3 &normal) const override;

    bool containsPoint(const glm::vec3 &point) const;
    bool containsPointNoOctree(const glm::vec3 &point) const;

    const char *getCheckTypes() const;
    const glm::vec3 *getCheckPoints() const;
    glm::vec3 *getVertices() const;
    glm::vec3 *getNormals() const;
    glm::vec2 *getTexcoords() const;
    int getCheckCount() const;
    int getVertexCount() const;
    int getNormalCount() const;
    int getTexcoordCount() const;
    int getFaceCount() const;
    MeshFace* getFace(int face) const;
    bool useSmoothBounce() const;
    bool noClusters() const;

    MeshDrawInfo* getDrawInfo() const;
    void setDrawInfo(MeshDrawInfo*);

    int packSize() const override;
    void *pack(void*) const override;
    const void *unpack(const void*) override;

    void print(std::ostream& out, const std::string& indent) const override;
    void printOBJ(std::ostream& out, const std::string& indent) const override;

private:
    void makeFacePointers(const std::vector<int>& _vertices,
                          const std::vector<int>& _normals,
                          const std::vector<int>& _texcoords,
                          glm::vec3 *v[], glm::vec3 **n, glm::vec2 **t);

private:
    static const char* typeName;

    std::string name;

    int checkCount;
    char* checkTypes;
    glm::vec3 *checkPoints;
    int vertexCount;
    glm::vec3 *vertices;
    int normalCount;
    glm::vec3 *normals;
    int texcoordCount;
    glm::vec2 *texcoords;
    int faceCount, faceSize;
    MeshFace** faces;
    bool smoothBounce;
    bool noclusters;
    bool inverted; // used during building. can be ditched if
    // edge tables are setup with bi-directional
    // ray-vs-face tests and parity counts.

    MeshDrawInfo* drawInfo; // hidden data stored in extra texcoords
};

inline const char *MeshObstacle::getCheckTypes() const
{
    return checkTypes;
}

inline const glm::vec3 *MeshObstacle::getCheckPoints() const
{
    return checkPoints;
}

inline glm::vec3 *MeshObstacle::getVertices() const
{
    return vertices;
}

inline glm::vec3 *MeshObstacle::getNormals() const
{
    return normals;
}

inline glm::vec2 *MeshObstacle::getTexcoords() const
{
    return texcoords;
}

inline int MeshObstacle::getCheckCount() const
{
    return checkCount;
}

inline int MeshObstacle::getVertexCount() const
{
    return vertexCount;
}

inline int MeshObstacle::getNormalCount() const
{
    return normalCount;
}

inline int MeshObstacle::getTexcoordCount() const
{
    return texcoordCount;
}

inline int MeshObstacle::getFaceCount() const
{
    return faceCount;
}

inline MeshFace* MeshObstacle::getFace(int face) const
{
    return faces[face];
}

inline bool MeshObstacle::useSmoothBounce() const
{
    return smoothBounce;
}

inline bool MeshObstacle::noClusters() const
{
    return noclusters;
}

inline MeshDrawInfo* MeshObstacle::getDrawInfo() const
{
    return drawInfo;
}

inline const std::string& MeshObstacle::getName() const
{
    return name;
}

inline void MeshObstacle::setName(const std::string& str)
{
    name = str;
    return;
}

#endif // BZF_MESH_OBSTACLE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
