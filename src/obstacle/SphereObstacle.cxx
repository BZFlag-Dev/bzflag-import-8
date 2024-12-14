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

// Interface
#include "SphereObstacle.h"

// System headers
#include <math.h>
#include <assert.h>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>

// Common headers
#include "global.h"
#include "Pack.h"
#include "PhysicsDriver.h"
#include "MeshTransform.h"

#include "MeshUtils.h"

const char* SphereObstacle::typeName = "SphereObstacle";


SphereObstacle::SphereObstacle()
{
    return;
}


SphereObstacle::SphereObstacle(const MeshTransform& xform,
                               const glm::vec3 &_pos, const glm::vec3 &_size,
                               float _rotation, const float _texsize[2],
                               bool _useNormals, bool _hemisphere,
                               int _divisions,
                               const BzMaterial* mats[MaterialCount],
                               int physics, bool bounce, bool drive,
                               bool shoot, bool rico) :
    Obstacle(_pos, _rotation, _size[0], _size[1], _size[2], drive, shoot,
             rico)
{
    // arc specific parameters
    transform = xform;
    divisions = _divisions;
    hemisphere = _hemisphere;
    phydrv = physics;
    smoothBounce = bounce;
    useNormals = _useNormals;
    memcpy(texsize, _texsize, sizeof(texsize));
    memcpy(materials, mats, sizeof(materials));

    finalize();

    return;
}


SphereObstacle::~SphereObstacle()
{
    return;
}


Obstacle* SphereObstacle::copyWithTransform(const MeshTransform& xform) const
{
    MeshTransform tmpXform = transform;
    tmpXform.append(xform);

    SphereObstacle* copy =
        new SphereObstacle(tmpXform, pos, size, angle, texsize, useNormals,
                           hemisphere, divisions, const_cast<const BzMaterial**>(materials),
                           phydrv, smoothBounce, driveThrough, shootThrough, ricochet);
    return copy;
}


const char* SphereObstacle::getType() const
{
    return typeName;
}


const char* SphereObstacle::getClassName() // const
{
    return typeName;
}


bool SphereObstacle::isValid() const
{
    return true;
}


void SphereObstacle::finalize()
{
    return;
}


MeshObstacle* SphereObstacle::makeMesh()
{
    MeshObstacle* mesh;
    int i, j, q;
    glm::vec3 sz;
    float texsz[2];
    const float minSize = 1.0e-6f; // cheezy / lazy
    int factor = 2;

    // setup the multiplying factor
    if (hemisphere)
        factor = 1;

    // absolute the sizes
    sz = glm::abs(size);

    // adjust the texture sizes
    memcpy (texsz, texsize, sizeof(float[2]));
    if (texsz[0] < 0.0f)
    {
        // unless you want to do elliptic integrals, here's
        // the Ramanujan approximation for the circumference
        // of an ellipse  (it will be rounded anyways)
        const float circ =
            (float)M_PI * ((3.0f * (sz[0] + sz[1])) -
                           sqrtf ((sz[0] + (3.0f * sz[1])) * (sz[1] + (3.0f * sz[0]))));
        // make sure it's an integral number so that the edges line up
        texsz[0] = -floorf(circ / texsz[0]);
    }
    if (texsz[1] < 0.0f)
        texsz[1] = -((2.0f * sz[2]) / texsz[1]);


    // validity checking
    if ((divisions < 1) || (texsz[0] < minSize) || (texsz[1] < minSize) ||
            (sz[0] < minSize) || (sz[1] < minSize) || (sz[2] < minSize))
        return NULL;

    // setup the coordinates
    std::vector<char> checkTypes;
    std::vector<glm::vec3> checkPoints;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    glm::vec3 v, n;
    glm::vec2 t;

    // add the checkpoint (one is sufficient)
    v = pos;
    if (hemisphere)
        v[2] = v[2] + (0.5f * fabsf(size[2]));
    checkPoints.push_back(v);
    checkTypes.push_back(MeshObstacle::CheckInside);

    // the center vertices
    v     = pos;
    v[2] += sz[2];
    vertices.push_back(v);
    if (!hemisphere)
    {
        v[2] = pos[2] - sz[2];
        vertices.push_back(v);
    }
    if (useNormals)
    {
        n[0] = 0.0f;
        n[1] = 0.0f;
        n[2] = 1.0f;
        normals.push_back(n);
        if (!hemisphere)
        {
            n[2] = -1.0f;
            normals.push_back(n);
        }
    }
    t[0] = 0.5f; // weirdness
    t[1] = 1.0f;
    texcoords.push_back(t);
    if (!hemisphere)
    {
        t[1] = 0.0f;
        texcoords.push_back(t);
    }

    // the rest of the vertices
    for (i = 0; i < divisions; i++)
    {
        for (j = 0; j < (4 * (i + 1)); j++)
        {
            float h_angle = (float)((M_PI * 2.0) * j / (4 * (i + 1)));
            h_angle = h_angle + getRotation();
            float v_angle = (float)((M_PI / 2.0) *
                                    (divisions - i - 1) / (divisions));
            glm::vec3 unit;
            unit[0] = cosf(h_angle) * cosf(v_angle);
            unit[1] = sinf(h_angle) * cosf(v_angle);
            unit[2] = sinf(v_angle);
            // vertex
            v = pos + sz * unit;
            vertices.push_back(v);
            // normal
            if (useNormals)
            {
                n = glm::normalize(unit / sz);
                normals.push_back(n);
            }
            // texcoord
            t[0] = (float)j / (float)(4 * (i + 1));
            t[0] = t[0] * texsz[0];
            t[1] = (float)(divisions - i - 1) / (float)divisions;
            if (!hemisphere)
                t[1] = 0.5f + (0.5f * t[1]);
            t[1] = t[1] * texsz[1];
            texcoords.push_back(t);

            // the bottom hemisphere
            if (!hemisphere)
            {
                if (i != (divisions - 1))
                {
                    // vertex
                    v[2] = (2 * pos[2]) - v[2];
                    vertices.push_back(v);
                    // normal
                    if (useNormals)
                    {
                        n[2] = -n[2];
                        normals.push_back(n);
                    }
                    // texcoord
                    t[1] = texsz[1] - t[1];
                    texcoords.push_back(t);
                }
            }
        }
    }

    // the closing strip of texture coordinates
    const int texStripOffset = texcoords.size();
    t[0] = texsz[0] * 0.5f; // weirdness
    t[1] = texsz[1] * 1.0f;
    texcoords.push_back(t);
    if (!hemisphere)
    {
        t[1] = 0.0f;
        texcoords.push_back(t);
    }
    for (i = 0; i < divisions; i++)
    {
        t[0] = texsz[0] * 1.0f;
        t[1] = (float)(divisions - i - 1) / (float)divisions;
        if (!hemisphere)
            t[1] = 0.5f + (0.5f * t[1]);
        t[1] = texsz[1] * t[1];
        texcoords.push_back(t);
        // the bottom hemisphere
        if (!hemisphere)
        {
            if (i != (divisions - 1))
            {
                t[1] = texsz[1] - t[1];
                texcoords.push_back(t);
            }
        }
    }

    // the bottom texcoords for hemispheres
    const int bottomTexOffset = texcoords.size();
    if (hemisphere)
    {
        const float astep = (float)((M_PI * 2.0) / (float) (divisions * 4));
        for (i = 0; i < (divisions * 4); i++)
        {
            float ang = astep * (float)i;
            t[0] = texsz[0] * (0.5f + (0.5f * cosf(ang)));
            t[1] = texsz[1] * (0.5f + (0.5f * sinf(ang)));
            texcoords.push_back(t);
        }
    }

    // make the mesh
    int faceCount = (divisions * divisions) * 8;
    mesh = new MeshObstacle(transform, checkTypes, checkPoints,
                            vertices, normals, texcoords, faceCount,
                            false, smoothBounce, driveThrough, shootThrough, ricochet);

    // add the faces to the mesh
    std::vector<int> vlist;
    std::vector<int> nlist;
    std::vector<int> tlist;

    int k = (divisions - 1);
    int ringOffset;
    if (!hemisphere)
        ringOffset = 1 + (((k*k)+k)*2);
    else
        ringOffset = 0;

    for (q = 0; q < 4; q++)
    {
        for (i = 0; i < divisions; i++)
        {
            for (j = 0; j < (i + 1); j++)
            {
                int a, b, c, d, ta, tc;
                // a, b, c form the upwards pointing triangle
                // b, c, d form the downwards pointing triangle
                // ta and tc are the texcoords for a and c
                const bool lastStrip = ((q == 3) && (j == i));
                const bool lastCircle = (i == (divisions - 1));

                // setup 'a'
                if (i > 0)
                {
                    if (lastStrip)
                    {
                        k = (i - 1);
                        a = 1 + (((k*k)+k)*2);
                    }
                    else
                    {
                        k = (i - 1);
                        a = 1 + (((k*k)+k)*2) + (q*(k+1)) + j;
                    }
                }
                else
                    a = 0;

                // setup 'b'
                b = 1 + (((i*i)+i)*2) + (q*(i+1)) + j;

                // setup 'c'
                if (lastStrip)
                    c = 1 + (((i*i)+i)*2);
                else
                    c = b + 1;

                // setup 'd' for the down-pointing triangle
                k = (i + 1);
                d = 1 + (((k*k)+k)*2) + (q*(k+1)) + (j + 1);


                // top hemisphere
                a = a * factor;
                if (!lastCircle)
                {
                    b = b * factor;
                    c = c * factor;
                }
                else
                {
                    b = b + ringOffset;
                    c = c + ringOffset;
                }
                if (i != (divisions - 2))
                    d = d * factor;
                else
                    d = d + ringOffset;

                // deal with the last strip of texture coordinates
                if (!lastStrip)
                {
                    ta = a;
                    tc = c;
                }
                else
                {
                    ta = texStripOffset + (i * factor);
                    tc = texStripOffset + ((i + 1) * factor);
                }

                push3Ints(vlist, a, b, c);
                if (useNormals) push3Ints(nlist, a, b, c);
                push3Ints(tlist, ta, b, tc);
                addFace(mesh, vlist, nlist, tlist, materials[Edge], phydrv);
                if (!lastCircle)
                {
                    push3Ints(vlist, b, d, c);
                    if (useNormals) push3Ints(nlist, b, d, c);
                    push3Ints(tlist, b, d, tc);
                    addFace(mesh, vlist, nlist, tlist, materials[Edge], phydrv);
                }

                // bottom hemisphere
                if (!hemisphere)
                {
                    a = a + 1;
                    ta = ta + 1;
                    if (!lastCircle)
                    {
                        b = b + 1;
                        c = c + 1;
                        tc = tc + 1;
                    }
                    if (i != (divisions - 2))
                        d = d + 1;
                    push3Ints(vlist, a, c, b);
                    if (useNormals) push3Ints(nlist, a, c, b);
                    push3Ints(tlist, ta, tc, b);
                    addFace(mesh, vlist, nlist, tlist, materials[Edge], phydrv);
                    if (!lastCircle)
                    {
                        push3Ints(vlist, b, c, d);
                        if (useNormals) push3Ints(nlist, b, c, d);
                        push3Ints(tlist, b, tc, d);
                        addFace(mesh, vlist, nlist, tlist, materials[Edge], phydrv);
                    }
                }
            }
        }
    }

    // add the bottom disc
    if (hemisphere)
    {
        k = (divisions - 1);
        const int offset = 1 + (((k*k)+k)*2);
        for (i = 0; i < (divisions * 4); i++)
        {
            const int vv = (divisions * 4) - i - 1;
            vlist.push_back(vv + offset);
            tlist.push_back(i + bottomTexOffset);
        }
        addFace(mesh, vlist, nlist, tlist, materials[Bottom], phydrv);
    }

    // wrap it up
    mesh->finalize();

    if (!mesh->isValid())
    {
        delete mesh;
        mesh = NULL;
    }
    return mesh;
}


float SphereObstacle::intersect(const Ray&) const
{
    assert(false);
    return -1.0f;
}

void SphereObstacle::get3DNormal(const glm::vec3 &, glm::vec3 &) const
{
    assert(false);
    return;
}

void SphereObstacle::getNormal(const glm::vec3 &, glm::vec3 &) const
{
    assert(false);
    return;
}

bool SphereObstacle::inCylinder(const glm::vec3 &, float, float) const
{
    assert(false);
    return false;
}

void *SphereObstacle::pack(void *buf) const
{
    buf = transform.pack(buf);
    buf = nboPackVector(buf, pos);
    buf = nboPackVector(buf, size);
    buf = nboPackFloat(buf, angle);
    buf = nboPackInt(buf, divisions);
    buf = nboPackInt(buf, phydrv);

    int i;
    for (i = 0; i < 2; i++)
        buf = nboPackFloat(buf, texsize[i]);
    for (i = 0; i < MaterialCount; i++)
    {
        int matindex = MATERIALMGR.getIndex(materials[i]);
        buf = nboPackInt(buf, matindex);
    }

    // pack the state byte
    unsigned char stateByte = 0;
    stateByte |= isDriveThrough() ? (1 << 0) : 0;
    stateByte |= isShootThrough() ? (1 << 1) : 0;
    stateByte |= smoothBounce     ? (1 << 2) : 0;
    stateByte |= useNormals       ? (1 << 3) : 0;
    stateByte |= hemisphere       ? (1 << 4) : 0;
    stateByte |= canRicochet()    ? (1 << 5) : 0;
    buf = nboPackUByte(buf, stateByte);

    return buf;
}


const void *SphereObstacle::unpack(const void *buf)
{
    int32_t inTmp;
    buf = transform.unpack(buf);
    buf = nboUnpackVector(buf, pos);
    buf = nboUnpackVector(buf, size);
    buf = nboUnpackFloat(buf, angle);
    buf = nboUnpackInt(buf, inTmp);
    divisions = int(inTmp);
    buf = nboUnpackInt(buf, inTmp);
    phydrv = int(inTmp);

    int i;
    for (i = 0; i < 2; i++)
        buf = nboUnpackFloat(buf, texsize[i]);
    for (i = 0; i < MaterialCount; i++)
    {
        int32_t matindex;
        buf = nboUnpackInt(buf, matindex);
        materials[i] = MATERIALMGR.getMaterial(matindex);
    }

    // unpack the state byte
    unsigned char stateByte;
    buf = nboUnpackUByte(buf, stateByte);
    driveThrough = (stateByte & (1 << 0)) != 0;
    shootThrough = (stateByte & (1 << 1)) != 0;
    smoothBounce = (stateByte & (1 << 2)) != 0;
    useNormals   = (stateByte & (1 << 3)) != 0;
    hemisphere   = (stateByte & (1 << 4)) != 0;
    ricochet     = (stateByte & (1 << 5)) != 0;
    finalize();

    return buf;
}


int SphereObstacle::packSize() const
{
    int fullSize = transform.packSize();
    fullSize += sizeof(float[3]);
    fullSize += sizeof(float[3]);
    fullSize += sizeof(float);
    fullSize += sizeof(int32_t);
    fullSize += sizeof(int32_t);
    fullSize += sizeof(float[2]);
    fullSize += sizeof(int32_t[MaterialCount]);
    fullSize += sizeof(unsigned char);
    return fullSize;
}


void SphereObstacle::print(std::ostream& out, const std::string& indent) const
{
    int i;

    out << indent << "sphere" << std::endl;

    out << indent << "  position " << pos[0] << " " << pos[1] << " "
        << pos[2] << std::endl;
    out << indent << "  size " << size[0] << " " << size[1] << " "
        << size[2] << std::endl;
    out << indent << "  rotation " << ((angle * 180.0) / M_PI) << std::endl;
    out << indent << "  divisions " << divisions << std::endl;
    if (hemisphere)
        out << indent << "  hemisphere" << std::endl;

    transform.printTransforms(out, indent);

    out << indent << "  texsize " << texsize[0] << " "
        << texsize[1] << std::endl;

    const char* sideNames[MaterialCount] =
    { "edge", "bottom" };
    for (i = 0; i < MaterialCount; i++)
    {
        out << indent << "  " << sideNames[i] << " matref ";
        MATERIALMGR.printReference(out, materials[i]);
        out << std::endl;
    }

    const PhysicsDriver* driver = PHYDRVMGR.getDriver(phydrv);
    if (driver != NULL)
    {
        out << indent << "  phydrv ";
        if (driver->getName().size() > 0)
            out << driver->getName();
        else
            out << phydrv;
        out << std::endl;
    }

    if (smoothBounce)
        out << indent << "  smoothBounce" << std::endl;
    if (driveThrough)
        out << indent << "  driveThrough" << std::endl;
    if (shootThrough)
        out << indent << "  shootThrough" << std::endl;
    if (!useNormals)
        out << indent << "  flatshading" << std::endl;
    if (ricochet)
        out << indent << "  ricochet" << std::endl;

    out << indent << "end" << std::endl;

    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
