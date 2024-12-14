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

#ifndef _MESH_DRAW_INFO_H_
#define _MESH_DRAW_INFO_H_

// Top Dog, King of the Hill
#include "common.h"

// System headers
#include <map>
#include <string>
#include <vector>
#include <iostream>

// Common headers
#include "Extents.h"
#include "BzMaterial.h"
#include "MeshTransform.h"

class MeshObstacle;
class MeshDrawMgr;

class Corner;
class DrawCmd;
class DrawSet;
class DrawLod;
class AnimationInfo;

typedef std::map<unsigned int, unsigned int> UintMap;


class MeshDrawInfo
{
public:
    // server side generation
    MeshDrawInfo(const std::vector<std::string>& options);

    // client side unpacking
    MeshDrawInfo();

    // client side copies
    // - the vertex data belongs to the source
    // - the BzMaterials are regenerated from the map
    MeshDrawInfo(const MeshDrawInfo* drawInfo,
                 const MeshTransform& xform,
                 const std::map<const BzMaterial*, const BzMaterial*>&);

    ~MeshDrawInfo();

    bool parse(std::istream& in);

    bool isValid() const;

    bool clientSetup(const MeshObstacle* mesh);
    bool serverSetup(const MeshObstacle* mesh);

    bool isServerSide() const;

    bool isCopy() const;
    const MeshDrawInfo* getSource() const;

    bool isInvisible() const;

    void getMaterials(MaterialSet& matSet) const;

    MeshDrawMgr* getDrawMgr() const;
    void setDrawMgr(MeshDrawMgr*);

    void setName(const std::string&);
    const std::string& getName() const;

    const glm::vec3 &getPosition() const;
    float            getRadius2() const;

    const Extents& getExtents() const;

    int getLodCount() const;
    const DrawLod* getDrawLods() const;

    const glm::vec3 *getVertices() const;
    const glm::vec3 *getNormals() const;
    const glm::vec2 *getTexcoords() const;

    int getRadarCount() const;
    const DrawLod* getRadarLods() const;

    int getLineCount() const;

    const MeshTransform::Tool* getTransformTool() const;
    const MaterialMap* getMaterialMap() const;

    void updateAnimation(double time);
    const AnimationInfo* getAnimationInfo() const;

    int packSize() const;
    void *pack(void*) const;
    const void *unpack(const void*);

    void print(std::ostream& out, const std::string& indent) const;

private:
    void init();
    void clear();
    bool validate(const MeshObstacle* mesh) const;

private:
    const MeshDrawInfo* source; // copy source, or NULL
    int lines;

    bool valid;
    bool serverSide;

    std::string name;

    std::vector<std::string> lodOptions;

    MeshDrawMgr* drawMgr;

    Extents extents;
    glm::vec3 position;
    float     radius2;

    MaterialMap* matMap;
    MeshTransform::Tool* xformTool;

    // elements
    int cornerCount;
    Corner* corners;
    glm::vec3 *vertices;
    glm::vec3 *normals;
    glm::vec2 *texcoords;

    int rawVertCount;
    glm::vec3 *rawVerts;
    int rawNormCount;
    glm::vec3 *rawNorms;
    int rawTxcdCount;
    glm::vec2 *rawTxcds;

    int lodCount;
    DrawLod* lods;

    int radarCount;
    DrawLod* radarLods;

    AnimationInfo* animInfo;
};

inline bool MeshDrawInfo::isValid() const
{
    return valid;
}
inline bool MeshDrawInfo::isServerSide() const
{
    return serverSide;
}

inline bool MeshDrawInfo::isCopy() const
{
    return (source != NULL);
}
inline void MeshDrawInfo::setDrawMgr(MeshDrawMgr* mgr)
{
    drawMgr = mgr;
}
inline const MeshDrawInfo* MeshDrawInfo::getSource() const
{
    return source;
}
inline const std::string& MeshDrawInfo::getName() const
{
    return name;
}

inline const glm::vec3 &MeshDrawInfo::getPosition() const
{
    return position;
}

inline float MeshDrawInfo::getRadius2() const
{
    return radius2;
}

inline const Extents& MeshDrawInfo::getExtents() const
{
    return extents;
}
inline int MeshDrawInfo::getLodCount() const
{
    return lodCount;
}
inline const DrawLod* MeshDrawInfo::getDrawLods() const
{
    return lods;
}
inline const glm::vec3 *MeshDrawInfo::getVertices() const
{
    return vertices;
}
inline const glm::vec3 *MeshDrawInfo::getNormals() const
{
    return normals;
}
inline const glm::vec2 *MeshDrawInfo::getTexcoords() const
{
    return texcoords;
}
inline int MeshDrawInfo::getRadarCount() const
{
    return radarCount;
}
inline const DrawLod* MeshDrawInfo::getRadarLods() const
{
    return radarLods;
}
inline int MeshDrawInfo::getLineCount() const
{
    return lines;
}
inline const MeshTransform::Tool*  MeshDrawInfo::getTransformTool() const
{
    return xformTool;
}
inline const MaterialMap* MeshDrawInfo::getMaterialMap() const
{
    return matMap;
}
inline const AnimationInfo* MeshDrawInfo::getAnimationInfo() const
{
    return animInfo;
}


class Corner
{
public:
    Corner();
    ~Corner();
    int packSize() const;
    void *pack(void*) const;
    const void *unpack(const void*);
public:
    int vertex;
    int normal;
    int texcoord;
};


class DrawCmd
{
public:
    DrawCmd();

    void clear();

    void finalize();

    int packSize() const;
    void *pack(void*) const;
    const void *unpack(const void*);

public:
    enum DrawModes            // OpenGL
    {
        DrawPoints    = 0x0000, // 0x0000
        DrawLines     = 0x0001, // 0x0001
        DrawLineLoop  = 0x0002, // 0x0002
        DrawLineStrip = 0x0003, // 0x0003
        DrawTriangles = 0x0004, // 0x0004
        DrawTriangleStrip = 0x0005, // 0x0005
        DrawTriangleFan   = 0x0006, // 0x0006
        DrawQuads     = 0x0007, // 0x0007
        DrawQuadStrip = 0x0008, // 0x0008
        DrawPolygon   = 0x0009, // 0x0009
        DrawModeCount
    };
    enum DrawIndexType
    {
        DrawIndexUShort   = 0x1403, // 0x1403
        DrawIndexUInt = 0x1405, // 0x1405
        DrawIndexTypeCount
    };

public:
    unsigned int drawMode;
    int count;
    void* indices;
    unsigned int indexType;
    unsigned int minIndex;
    unsigned int maxIndex;
};


class DrawSet
{
public:
    DrawSet();

    void clear();

    int packSize() const;
    void *pack(void*) const;
    const void *unpack(const void*);

public:
    int count;
    DrawCmd* cmds;
    const BzMaterial* material;
    bool wantList;
    glm::vec3 position;
    float radius2;
    int triangleCount;
};


class DrawLod
{
public:
    DrawLod();

    void clear();

    int packSize() const;
    void *pack(void*) const;
    const void *unpack(const void*);

public:
    int count;
    DrawSet* sets;
    float lengthPerPixel;
    int triangleCount;
};


class AnimationInfo
{
public:
    AnimationInfo();
    int packSize() const;
    void *pack(void*) const;
    const void *unpack(const void*);
public:
    float angvel;
    std::string dummy;
    float angle;
    float cos_val;
    float sin_val;
};


#endif // _MESH_DRAW_INFO_H_

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
