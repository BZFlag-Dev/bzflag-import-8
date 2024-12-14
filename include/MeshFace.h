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

#ifndef BZF_MESH_FACE_OBSTACLE_H
#define BZF_MESH_FACE_OBSTACLE_H

// Inherits from
#include "Obstacle.h"

// System headers
#include <string>
#include <iostream>
#include <glm/vec4.hpp>

// Common headers
#include "Ray.h"
#include "global.h"
#include "BzMaterial.h"

class MeshFace : public Obstacle
{

    friend class MeshObstacle;
    friend class ObstacleModifier;

public:
    MeshFace(class MeshObstacle* mesh);
    MeshFace(MeshObstacle* mesh, int vertexCount,
             glm::vec3 *vertices[], glm::vec3 **normals, glm::vec2 **texcoords,
             const BzMaterial* material, int physics,
             bool noclusters, bool smoothBounce, bool drive, bool shoot, bool ricochet);
    ~MeshFace();

    const char* getType() const override;
    static const char* getClassName(); // const
    bool isValid() const override;
    bool isFlatTop() const override;

    float intersect(const Ray&) const override;
    void getNormal(const glm::vec3 &p, glm::vec3 &n) const override;
    void get3DNormal(const glm::vec3 &p, glm::vec3 &n) const override;

    bool inCylinder(const glm::vec3 &p, float radius, float height) const override;
    bool inBox(const glm::vec3 &p, float angle,
               float halfWidth, float halfBreadth, float height) const override;
    bool inMovingBox(const glm::vec3 &oldP, float oldAngle,
                     const glm::vec3 &newP, float newAngle,
                     float halfWidth, float halfBreadth, float height) const override;
    bool isCrossing(const glm::vec3 &p, float angle,
                    float halfWidth, float halfBreadth, float height,
                    glm::vec4 *plane) const override;

    bool getHitNormal(const glm::vec3 &pos1, float azimuth1,
                      const glm::vec3 &pos2, float azimuth2,
                      float halfWidth, float halfBreadth,
                      float height, glm::vec3 &normal) const override;

    MeshObstacle* getMesh() const;
    int getVertexCount() const;
    bool useNormals() const;
    bool useTexcoords() const;
    const glm::vec3 &getVertex(int index) const;
    const glm::vec3 &getNormal(int index) const;
    const glm::vec2 &getTexcoord(int index) const;
    const glm::vec4 &getPlane() const;
    const BzMaterial* getMaterial() const;
    int getPhysicsDriver() const;
    bool noClusters() const;
    bool isSmoothBounce() const;

    bool isSpecial() const;
    bool isBaseFace() const;
    bool isLinkToFace() const;
    bool isLinkFromFace() const;
    bool isZPlane() const;
    bool isUpPlane() const;
    bool isDownPlane() const;

    void setLink(const MeshFace* link);
    const MeshFace* getLink() const;

    int packSize() const override;
    void *pack(void*) const override;
    const void *unpack(const void*) override;

    void print(std::ostream& out, const std::string& indent) const override;

public:
    mutable float scratchPad;

private:
    void finalize();

private:
    static const char* typeName;

    class MeshObstacle* mesh;
    int vertexCount;
    glm::vec3 **vertices;
    glm::vec3 **normals;
    glm::vec2 **texcoords;
    const BzMaterial* bzMaterial;
    bool smoothBounce;
    bool noclusters;
    int phydrv;

    glm::vec4 plane;
    glm::vec4 *edgePlanes;

    MeshFace* edges; // edge 0 is between vertex 0 and 1, etc...
    // not currently used for anything

    enum
    {
        XPlane    = (1 << 0),
        YPlane    = (1 << 1),
        ZPlane    = (1 << 2),
        UpPlane   = (1 << 3),
        DownPlane = (1 << 4),
        WallPlane = (1 << 5)
    } PlaneBits;

    char planeBits;


    enum
    {
        LinkToFace      = (1 << 0),
        LinkFromFace    = (1 << 1),
        BaseFace        = (1 << 2),
        IcyFace    = (1 << 3),
        StickyFace      = (1 << 5),
        DeathFace       = (1 << 6),
        PortalFace      = (1 << 7)
    } SpecialBits;

    // combining all types into one struct, because I'm lazy
    typedef struct
    {
        // linking data
        const MeshFace* linkFace;
        bool linkFromFlat;
        float linkFromSide[3]; // sideways vector
        float linkFromDown[3]; // downwards vector
        float linkFromCenter[3];
        bool linkToFlat;
        float linkToSide[3]; // sideways vector
        float linkToDown[3]; // downwards vector
        float linkToCenter[3];
        // base data
        TeamColor teamColor;
    } SpecialData;

    uint16_t specialState;
    SpecialData* specialData;
};

inline MeshObstacle* MeshFace::getMesh() const
{
    return mesh;
}

inline const BzMaterial* MeshFace::getMaterial() const
{
    return bzMaterial;
}

inline int MeshFace::getPhysicsDriver() const
{
    return phydrv;
}

inline const glm::vec4 &MeshFace::getPlane() const
{
    return plane;
}

inline int MeshFace::getVertexCount() const
{
    return vertexCount;
}

inline const glm::vec3 &MeshFace::getVertex(int index) const
{
    return *vertices[index];
}

inline const glm::vec3 &MeshFace::getNormal(int index) const
{
    return *normals[index];
}

inline const glm::vec2 &MeshFace::getTexcoord(int index) const
{
    return *texcoords[index];
}

inline bool MeshFace::useNormals() const
{
    return (normals != NULL);
}

inline bool MeshFace::useTexcoords() const
{
    return (texcoords != NULL);
}

inline bool MeshFace::noClusters() const
{
    return noclusters;
}

inline bool MeshFace::isSmoothBounce() const
{
    return smoothBounce;
}

inline bool MeshFace::isSpecial() const
{
    return (specialState != 0);
}

inline bool MeshFace::isBaseFace() const
{
    return (specialState & BaseFace) != 0;
}

inline bool MeshFace::isLinkToFace() const
{
    return (specialState & LinkToFace) != 0;
}

inline bool MeshFace::isLinkFromFace() const
{
    return (specialState & LinkFromFace) != 0;
}

inline const MeshFace* MeshFace::getLink() const
{
    return specialData->linkFace;
}

inline bool MeshFace::isZPlane() const
{
    return ((planeBits & ZPlane) != 0);
}

inline bool MeshFace::isUpPlane() const
{
    return ((planeBits & UpPlane) != 0);
}

inline bool MeshFace::isDownPlane() const
{
    return ((planeBits & DownPlane) != 0);
}


#endif // BZF_MESH_FACE_OBSTACLE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
