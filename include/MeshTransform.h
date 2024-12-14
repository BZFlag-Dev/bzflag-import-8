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

#ifndef _TRANSFORM_H_
#define _TRANSFORM_H_

//1st
#include "common.h"

// System headers
#include <string>
#include <vector>
#include <iostream>
#include <glm/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

enum TransformType
{
    ShiftTransform = 0,
    ScaleTransform = 1,
    ShearTransform = 2,
    SpinTransform  = 3,
    IndexTransform = 4,
    LastTransform
};

typedef struct
{
    TransformType type;
    int index;
    float data[4];
} TransformData;

class MeshTransform
{
public:
    MeshTransform();
    MeshTransform(const MeshTransform&);
    ~MeshTransform();

    MeshTransform& operator=(const MeshTransform& transform);
    void append(const MeshTransform& transform);
    void prepend(const MeshTransform& transform);

    bool setName(const std::string& name);
    void addShift(const glm::vec3 &shift);
    void addScale(const glm::vec3 &scale);
    void addShear(const float shear[3]);
    void addSpin(const float degrees, const glm::vec3 &normal);
    void addReference(int transform);

    bool isEmpty() const
    {
        return (transforms.size() <= 0);
    }

    bool isValid();
    void finalize();

    const std::string& getName() const;

    int packSize() const;
    void* pack(void*) const;
    const void* unpack(const void*);

    void print(std::ostream& out, const std::string& indent) const;
    void printTransforms(std::ostream& out, const std::string& indent) const;

private:

    std::string name;
    std::vector<TransformData> transforms;

public:
    class Tool
    {
    public:
        Tool(const MeshTransform& transform);
        ~Tool();

        bool isInverted() const;
        bool isSkewed() const; // scaled or sheared
        void modifyVertex(glm::vec3 &vertex) const;
        void modifyNormal(glm::vec3 &normal) const;
        void modifyOldStyle(glm::vec3 &pos, glm::vec3 &size,
                            float& angle, bool& flipz) const;
        const glm::mat4 &getMatrix() const;

    private:
        void processTransforms(const std::vector<TransformData>& tforms);

        bool empty;
        bool inverted;
        bool skewed;
        glm::mat4 vertexMatrix;
        glm::mat3 normalMatrix;
    };

    friend class MeshTransform::Tool;
};

inline bool MeshTransform::Tool::isInverted() const
{
    return inverted;
}

inline bool MeshTransform::Tool::isSkewed() const
{
    return skewed;
}

inline const glm::mat4 &MeshTransform::Tool::getMatrix() const
{
    return vertexMatrix;
}


class MeshTransformManager
{
public:
    MeshTransformManager();
    ~MeshTransformManager();
    void update();
    void clear();
    int addTransform(MeshTransform* driver);
    int findTransform(const std::string& name) const;
    const MeshTransform* getTransform(int id) const;

    int packSize() const;
    void* pack(void*) const;
    const void* unpack(const void*);

    void print(std::ostream& out, const std::string& indent) const;

private:
    std::vector<MeshTransform*> transforms;
};

inline const MeshTransform* MeshTransformManager::getTransform(int id) const
{
    if ((id >= 0) && (id < (int)transforms.size()))
        return transforms[id];
    else
        return NULL;
}


extern MeshTransformManager TRANSFORMMGR;


#endif //_TRANSFORM_H_

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
