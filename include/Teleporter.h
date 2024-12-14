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

/* Teleporter:
 *  Encapsulates a box in the game environment.
 */

#ifndef BZF_TELEPORTER_H
#define BZF_TELEPORTER_H

#include "common.h"
#include <string>
#include "Obstacle.h"
#include "MeshFace.h"

class Teleporter : public Obstacle
{
public:
    Teleporter();
    Teleporter(const glm::vec3 &pos, float rotation,
               float width, float breadth, float height,
               float borderSize = 1.0f, bool horizontal = false,
               bool drive = false, bool shoot = false, bool ricochet = false);
    ~Teleporter();

    Obstacle*       copyWithTransform(const MeshTransform&) const override;

    void        setName(const std::string& name);
    const std::string&  getName() const;

    const char*     getType() const override;
    static const char*  getClassName(); // const

    float       getBorder() const;
    bool        isHorizontal() const;
    bool        isValid() const override;

    float       intersect(const Ray&) const override;
    void        getNormal(const glm::vec3 &p, glm::vec3 &n) const override;

    bool        inCylinder(const glm::vec3 &p, float radius, float height) const override;
    bool        inBox(const glm::vec3 &p, float angle,
                      float halfWidth, float halfBreadth, float height) const override;
    bool        inMovingBox(const glm::vec3 &oldP, float oldAngle,
                            const glm::vec3 &newP, float newAngle,
                            float halfWidth, float halfBreadth, float height) const override;
    bool        isCrossing(const glm::vec3 &p, float angle,
                           float halfWidth, float halfBreadth, float height,
                           glm::vec4 *plane) const override;

    bool        getHitNormal(
        const glm::vec3 &pos1, float azimuth1,
        const glm::vec3 &pos2, float azimuth2,
        float halfWidth, float halfBreadth,
        float height,
        glm::vec3 &normal) const override;

    float       isTeleported(const Ray&, int& face) const;
    float       getProximity(const glm::vec3 &p, float radius) const;
    bool        hasCrossed(const glm::vec3 &p1, const glm::vec3 &p2,
                           int& face) const;
    void        getPointWRT(const Teleporter& t2, int face1, int face2,
                            glm::vec3 &p, glm::vec3 *d, float *a) const;

    void        makeLinks();
    const MeshFace* getBackLink() const;
    const MeshFace* getFrontLink() const;

    int         packSize() const override;
    void*       pack(void*) const override;
    const void*     unpack(const void*) override;

    void        print(std::ostream& out, const std::string& indent) const override;
    void        printOBJ(std::ostream& out, const std::string& indent) const override;

    std::string     userTextures[1];

private:
    void        finalize();

private:
    static const char*  typeName;

    std::string     name;

    float       border;
    bool        horizontal;
    glm::vec3   origSize;

    MeshFace*       backLink;
    MeshFace*       frontLink;
    glm::vec3   fvertices[4]; // front vertices
    glm::vec3   bvertices[4]; // back vertices
    glm::vec2   texcoords[4]; // shared texture coordinates
};

//
// Teleporter
//

inline float Teleporter::getBorder() const
{
    return border;
}

inline bool Teleporter::isHorizontal() const
{
    return horizontal;
}

inline const MeshFace* Teleporter::getBackLink() const
{
    return backLink;
}

inline const MeshFace* Teleporter::getFrontLink() const
{
    return frontLink;
}

inline const std::string& Teleporter::getName() const
{
    return name;
}

inline void Teleporter::setName(const std::string& _name)
{
    name = _name;
    return;
}


#endif // BZF_TELEPORTER_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
