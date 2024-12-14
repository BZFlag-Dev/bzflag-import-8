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
#include "ArcObstacle.h"

// System interfaces
#include <math.h>
#include <assert.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

//Common interfaces
#include "global.h"
#include "Pack.h"
#include "PhysicsDriver.h"
#include "MeshTransform.h"

#include "MeshUtils.h"


const char* ArcObstacle::typeName = "ArcObstacle";


ArcObstacle::ArcObstacle()
{
    return;
}


ArcObstacle::ArcObstacle(const MeshTransform& xform,
                         const glm::vec3 &_pos, const glm::vec3 &_size,
                         float _rotation, float _sweepAngle, float _ratio,
                         const float _texsize[4], bool _useNormals,
                         int _divisions, const BzMaterial* mats[MaterialCount],
                         int physics, bool bounce, bool drive, bool shoot,
                         bool rico) :
    Obstacle(_pos, _rotation, _size[0], _size[1], _size[2], drive, shoot, rico)
{
    // arc specific parameters
    transform = xform;
    divisions = _divisions;
    sweepAngle = _sweepAngle;
    ratio = _ratio;
    phydrv = physics;
    smoothBounce = bounce;
    useNormals = _useNormals;
    memcpy(texsize, _texsize, sizeof(texsize));
    memcpy(materials, mats, sizeof(materials));

    finalize();

    return;
}


ArcObstacle::~ArcObstacle()
{
    return;
}


Obstacle* ArcObstacle::copyWithTransform(const MeshTransform& xform) const
{
    MeshTransform tmpXform = transform;
    tmpXform.append(xform);

    ArcObstacle* copy =
        new ArcObstacle(tmpXform, pos, size, angle, sweepAngle, ratio,
                        texsize, useNormals, divisions,
                        const_cast<const BzMaterial**>(materials), phydrv,
                        smoothBounce, driveThrough, shootThrough, ricochet);
    return copy;
}


const char* ArcObstacle::getType() const
{
    return typeName;
}


const char* ArcObstacle::getClassName() // const
{
    return typeName;
}


bool ArcObstacle::isValid() const
{
    return true;
}


bool ArcObstacle::isFlatTop() const
{
    return true;
}


void ArcObstacle::finalize()
{
    return;
}


MeshObstacle* ArcObstacle::makeMesh()
{
    MeshObstacle* mesh;

    bool isPie = false;    // has no inside edge
    bool isCircle = false; // angle of 360 degrees
    const float minSize = 1.0e-6f; // cheezy / lazy

    // absolute the sizes
    float sz[3];
    sz[0] = fabsf(size[0]);
    sz[1] = fabsf(size[1]);
    sz[2] = fabsf(size[2]);

    // validity checking
    if ((sz[0] < minSize) || (sz[1] < minSize) || (sz[2] < minSize) ||
            (fabsf(texsize[0]) < minSize) || (fabsf(texsize[1]) < minSize) ||
            (fabsf(texsize[2]) < minSize) || (fabsf(texsize[3]) < minSize) ||
            (ratio < 0.0f) || (ratio > 1.0f))
        return NULL;

    // adjust the texture sizes   FIXME: finish texsz[2] & texsz[3]
    float texsz[4];
    memcpy (texsz, texsize, sizeof(float[4]));
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
        texsz[1] = -(sz[2] / texsz[1]);

    // setup the angles
    float r = getRotation();
    float a = sweepAngle;
    if (a > +360.0f)
        a = +360.0f;
    if (a < -360.0f)
        a = -360.0f;
    a = a * (float)(M_PI / 180.0); // convert to radians
    if (a < 0.0f)
    {
        r = r + a;
        a = -a;
    }

    // more validity checking
    if (divisions <= (int) ((a + minSize) / M_PI))
        return NULL;

    if (fabsf ((float)M_PI - fmodf (a + (float)M_PI, (float)M_PI * 2.0f)) < minSize)
        isCircle = true;

    // setup the radii
    float inrad = sz[0] * (1.0f - ratio);
    float outrad = sz[0];
    if (inrad > outrad)
    {
        const float tmp = inrad;
        inrad = outrad;
        outrad = tmp;
    }
    if ((outrad < minSize) || ((outrad - inrad) < minSize))
        return NULL;
    if (inrad < minSize)
        isPie = true;
    const float squish = sz[1] / sz[0];

    if (isPie)
        mesh = makePie(isCircle, a, r, sz[2], outrad, squish, texsz);
    else
        mesh = makeRing(isCircle, a, r, sz[2], inrad, outrad, squish, texsz);

    // wrap it up
    mesh->finalize();

    if (mesh->isValid())
        return mesh;
    else
    {
        delete mesh;
        return NULL;
    }
}


MeshObstacle* ArcObstacle::makePie(bool isCircle, float a, float r,
                                   float h, float radius, float squish,
                                   float texsz[4])
{
    MeshObstacle* mesh;
    int i;

    // setup the coordinates
    std::vector<char> checkTypes;
    std::vector<glm::vec3> checkPoints;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    glm::vec3 v, n;
    glm::vec2 t;

    // add the checkpoint (one is sufficient)
    if (isCircle)
    {
        v[0] = pos[0];
        v[1] = pos[1];
    }
    else
    {
        const float dir = r + (0.5f * a);
        v[0] = pos[0] + (cosf(dir) * radius * 0.5f);
        v[1] = pos[1] + (sinf(dir) * radius * 0.5f * squish);
    }
    v[2] = pos[2] + (0.5f * fabsf(size[2]));
    checkPoints.push_back(v);
    checkTypes.push_back(MeshObstacle::CheckInside);

    // setup the texsize across the disc
    if (texsz[2] < 0.0f)
        texsz[2] = -((2.0f * radius) / texsz[2]);
    if (texsz[3] < 0.0f)
        texsz[3] = -((2.0f * radius * squish) / texsz[3]);

    const float astep = a / (float) divisions;

    for (i = 0; i < (divisions + 1); i++)
    {
        float ang = r + (astep * (float)i);
        float cos_val = cosf(ang);
        float sin_val = sinf(ang);

        // vertices and normals
        if (!isCircle || (i != divisions))
        {
            float delta[2];
            delta[0] = cos_val * radius;
            delta[1] = (sin_val * radius) * squish;
            // vertices
            v[0] = pos[0] + delta[0];
            v[1] = pos[1] + delta[1];
            v[2] = pos[2];
            vertices.push_back(v);
            v[2] = v[2] + h;
            vertices.push_back(v);
            // normal
            if (useNormals)
            {
                n[2] = 0.0f;
                n[0] = cos_val * squish;
                n[1] = sin_val;
                float len = 1.0f / sqrtf((n[0] * n[0]) + (n[1] * n[1]));
                n[0] = n[0] * len;
                n[1] = n[1] * len;
                normals.push_back(n);
            }
        }

        // texture coordinates (around the edge)
        t[0] = (float) i / (float) divisions;
        t[0] = texsz[0] * t[0];
        t[1] = 0.0f;
        texcoords.push_back(t);
        // outside texcoord
        t[1] = texsz[1] * 1.0f;
        texcoords.push_back(t);
    }

    // texture coordinates (around the disc)
    for (i = 0; i < (divisions + 1); i++)
    {
        float ang = astep * (float)i;
        float cos_val = cosf(ang);
        float sin_val = sinf(ang);
        t[0] = texsz[2] * (0.5f + (0.5f * cos_val));
        t[1] = texsz[3] * (0.5f + (0.5f * sin_val));
        texcoords.push_back(t);
    }

    // the central coordinates
    v[0] = pos[0];
    v[1] = pos[1];
    v[2] = pos[2];
    vertices.push_back(v); // bottom
    v[2] = pos[2] + h;
    vertices.push_back(v); // top
    t[0] = texsz[2] * 0.5f;
    t[1] = texsz[3] * 0.5f;
    texcoords.push_back(t);

    // setup the face count
    int fcount = (divisions * 3);
    if (!isCircle)
    {
        fcount = fcount + 2; // add the start and end faces
    }

    mesh = new MeshObstacle(transform, checkTypes, checkPoints,
                            vertices, normals, texcoords, fcount,
                            false, smoothBounce, driveThrough, shootThrough, ricochet);

    // now make the faces
    int vlen, nlen;
    if (isCircle)
    {
        vlen = divisions * 2;
        nlen = divisions;
    }
    else
    {
        vlen = (divisions + 1) * 2;
        nlen = (divisions + 1);
    }

    const int vtop = vlen + 1;
    const int vbot = vlen;
    const int tmid = ((divisions + 1) * 3);

    std::vector<int> vlist;
    std::vector<int> nlist;
    std::vector<int> tlist;

    for (i = 0; i < divisions; i++)
    {

// handy macros
#define PV(x) (((x) + (i * 2)) % vlen)
#define PN(x) (((x) + i) % nlen)
#define PTO(x) ((x) + (i * 2))           // outside edge
#define PTC(x) (((divisions + 1) * 2) + (x) + i)   // around the disc
#define PTCI(x) (((divisions + 1) * 3) - (x) - i - 1)

        // outside
        push4Ints(vlist, PV(0), PV(2), PV(3), PV(1));
        if (useNormals) push4Ints(nlist, PN(0), PN(1), PN(1), PN(0));
        push4Ints(tlist, PTO(0), PTO(2), PTO(3), PTO(1));
        addFace(mesh, vlist, nlist, tlist, materials[Outside_], phydrv);

        // top
        push3Ints(vlist, vtop, PV(1), PV(3));
        push3Ints(tlist, tmid, PTC(0), PTC(1));
        addFace(mesh, vlist, nlist, tlist, materials[Top], phydrv);

        // bottom
        push3Ints(vlist, vbot, PV(2), PV(0));
        push3Ints(tlist, tmid, PTCI(1), PTCI(0));
        addFace(mesh, vlist, nlist, tlist, materials[Bottom], phydrv);
    }


    if (!isCircle)
    {
        int tc = (divisions * 2);
        // start face
        push4Ints(vlist, vbot, 0, 1, vtop);
        push4Ints(tlist, 0, tc + 0, tc + 1, 1);
        addFace(mesh, vlist, nlist, tlist, materials[StartFace], phydrv);

        // end face
        int e = divisions * 2;
        push4Ints(vlist, e + 0, vbot, vtop, e + 1);
        push4Ints(tlist, 0, tc + 0, tc + 1, 1);
        addFace(mesh, vlist, nlist, tlist, materials[EndFace], phydrv);
    }

    return mesh;
}


MeshObstacle* ArcObstacle::makeRing(bool isCircle, float a, float r,
                                    float h, float inrad, float outrad,
                                    float squish, float texsz[4])
{
    MeshObstacle* mesh;

    // setup the coordinates
    std::vector<char> checkTypes;
    std::vector<glm::vec3> checkPoints;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    glm::vec3 v, n;
    glm::vec2 t;

    // add the checkpoints (very wasteful)
    v[0] = pos[0];
    v[1] = pos[1];
    const float height = fabsf(size[2]);
    if (pos[2] > 0.0f)
    {
        // down
        v[2] = pos[2] - (1.0f * height);
        checkPoints.push_back(v);
        checkTypes.push_back(MeshObstacle::CheckOutside);
        // up
        v[2] = pos[2] + (2.0f * height);
        checkPoints.push_back(v);
        checkTypes.push_back(MeshObstacle::CheckOutside);
    }
    else
    {
        // up
        v[2] = pos[2] + (2.0f * height);
        checkPoints.push_back(v);
        checkTypes.push_back(MeshObstacle::CheckOutside);
        // down
        v[2] = pos[2] - (1.0f * height);
        checkPoints.push_back(v);
        checkTypes.push_back(MeshObstacle::CheckOutside);
    }
    // east
    v[2] = pos[2] + (0.5f * height);
    v[0] = pos[0] + (outrad * 2.0f);
    checkPoints.push_back(v);
    checkTypes.push_back(MeshObstacle::CheckOutside);
    // west
    v[0] = pos[0] - (outrad * 2.0f);
    checkPoints.push_back(v);
    checkTypes.push_back(MeshObstacle::CheckOutside);
    // north
    v[0] = pos[0];
    v[1] = pos[1] + (outrad * squish * 2.0f);
    checkPoints.push_back(v);
    checkTypes.push_back(MeshObstacle::CheckOutside);
    // south
    v[1] = pos[1] - (outrad * squish * 2.0f);
    checkPoints.push_back(v);
    checkTypes.push_back(MeshObstacle::CheckOutside);

    int i;
    const float astep = a / (float) divisions;

    for (i = 0; i < (divisions + 1); i++)
    {
        float ang = r + (astep * (float)i);
        float cos_val = cosf(ang);
        float sin_val = sinf(ang);

        // vertices and normals
        if (!isCircle || (i != divisions))
        {
            // inside points
            v[0] = pos[0] + (cos_val * inrad);
            v[1] = pos[1] + (squish * (sin_val * inrad));
            v[2] = pos[2];
            vertices.push_back(v);
            v[2] = v[2] + h;
            vertices.push_back(v);
            // outside points
            v[0] = pos[0] + (cos_val * outrad);
            v[1] = pos[1] + (squish * (sin_val * outrad));
            v[2] = pos[2];
            vertices.push_back(v);
            v[2] = v[2] + h;
            vertices.push_back(v);
            // inside normal
            if (useNormals)
            {
                n[2] = 0.0f;
                n[0] = -cos_val * squish;
                n[1] = -sin_val;
                float len = 1.0f / sqrtf((n[0] * n[0]) + (n[1] * n[1]));
                n[0] = n[0] * len;
                n[1] = n[1] * len;
                normals.push_back(n);
                // outside normal
                n[0] = -n[0];
                n[1] = -n[1];
                normals.push_back(n);
            }
        }

        // texture coordinates
        // inside texcoord
        t[0] = (float) i / (float) divisions;
        t[0] = texsz[0] * t[0];
        t[1] = 0.0f;
        texcoords.push_back(t);
        // outside texcoord
        t[1] = texsz[1] * 1.0f;
        texcoords.push_back(t);
    }

    // setup the face count
    int fcount = (divisions * 4);
    if (!isCircle)
    {
        fcount = fcount + 2; // add the start and end faces
    }

    mesh = new MeshObstacle(transform, checkTypes, checkPoints,
                            vertices, normals, texcoords, fcount,
                            false, smoothBounce, driveThrough, shootThrough, ricochet);

    // now make the faces
    int vlen, nlen;
    if (isCircle)
    {
        vlen = divisions * 4;
        nlen = divisions * 2;
    }
    else
    {
        vlen = (divisions + 1) * 4;
        nlen = (divisions + 1) * 2;
    }

    std::vector<int> vlist;
    std::vector<int> nlist;
    std::vector<int> tlist;

    for (i = 0; i < divisions; i++)
    {

// handy macros
#define RV(x) (((x) + (i * 4)) % vlen)
#define RN(x) (((x) + (i * 2)) % nlen)
#define RT(x) ((x) + (i * 2))
#define RIT(x) ((divisions + ((x)%2))*2 - ((x) + (i * 2)))

        // inside
        push4Ints(vlist, RV(4), RV(0), RV(1), RV(5));
        if (useNormals) push4Ints(nlist, RN(2), RN(0), RN(0), RN(2));
        push4Ints(tlist, RIT(2), RIT(0), RIT(1), RIT(3));
        addFace(mesh, vlist, nlist, tlist, materials[Inside], phydrv);

        // outside
        push4Ints(vlist, RV(2), RV(6), RV(7), RV(3));
        if (useNormals) push4Ints(nlist, RN(1), RN(3), RN(3), RN(1));
        push4Ints(tlist, RT(0), RT(2), RT(3), RT(1));
        addFace(mesh, vlist, nlist, tlist, materials[Outside_], phydrv);

        // top
        push4Ints(vlist, RV(3), RV(7), RV(5), RV(1));
        push4Ints(tlist, RT(0), RT(2), RT(3), RT(1));
        addFace(mesh, vlist, nlist, tlist, materials[Top], phydrv);

        // bottom
        push4Ints(vlist, RV(0), RV(4), RV(6), RV(2));
        push4Ints(tlist, RT(0), RT(2), RT(3), RT(1));
        addFace(mesh, vlist, nlist, tlist, materials[Bottom], phydrv);
    }

    if (!isCircle)
    {
        int tc = (divisions * 2);
        // start face
        push4Ints(vlist, 0, 2, 3, 1);
        push4Ints(tlist, 0, tc + 0, tc + 1, 1);
        addFace(mesh, vlist, nlist, tlist, materials[StartFace], phydrv);

        // end face
        int e = divisions * 4;
        push4Ints(vlist, e + 2, e + 0, e + 1, e + 3);
        push4Ints(tlist, 0, tc + 0, tc + 1, 1);
        addFace(mesh, vlist, nlist, tlist, materials[EndFace], phydrv);
    }

    return mesh;
}


float ArcObstacle::intersect(const Ray&) const
{
    assert(false);
    return -1.0f;
}


void ArcObstacle::get3DNormal(const glm::vec3 &, glm::vec3 &) const
{
    assert(false);
    return;
}


void ArcObstacle::getNormal(const glm::vec3 &, glm::vec3 &) const
{
    assert(false);
    return;
}


bool ArcObstacle::inCylinder(const glm::vec3 &,float, float) const
{
    assert(false);
    return false;
}


void* ArcObstacle::pack(void* buf) const
{
    buf = transform.pack(buf);
    buf = nboPackVector(buf, pos);
    buf = nboPackVector(buf, size);
    buf = nboPackFloat(buf, angle);
    buf = nboPackFloat(buf, sweepAngle);
    buf = nboPackFloat(buf, ratio);
    buf = nboPackInt(buf, divisions);
    buf = nboPackInt(buf, phydrv);

    int i;
    for (i = 0; i < 4; i++)
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
    stateByte |= canRicochet()    ? (1 << 4) : 0;
    buf = nboPackUByte(buf, stateByte);

    return buf;
}


const void* ArcObstacle::unpack(const void* buf)
{
    int32_t inTmp;
    buf = transform.unpack(buf);
    buf = nboUnpackVector(buf, pos);
    buf = nboUnpackVector(buf, size);
    buf = nboUnpackFloat(buf, angle);
    buf = nboUnpackFloat(buf, sweepAngle);
    buf = nboUnpackFloat(buf, ratio);
    buf = nboUnpackInt(buf, inTmp);
    divisions = int(inTmp);
    buf = nboUnpackInt(buf, inTmp);
    phydrv = int(inTmp);

    int i;
    for (i = 0; i < 4; i++)
        buf = nboUnpackFloat(buf, texsize[i]);
    for (i = 0; i < MaterialCount; i++)
    {
        int matindex;
        buf = nboUnpackInt(buf, inTmp);
        matindex = int(inTmp);
        materials[i] = MATERIALMGR.getMaterial(matindex);
    }

    // unpack the state byte
    unsigned char stateByte;
    buf = nboUnpackUByte(buf, stateByte);
    driveThrough = (stateByte & (1 << 0)) != 0;
    shootThrough = (stateByte & (1 << 1)) != 0;
    smoothBounce = (stateByte & (1 << 2)) != 0;
    useNormals   = (stateByte & (1 << 3)) != 0;
    ricochet     = (stateByte & (1 << 4)) != 0;

    finalize();

    return buf;
}


int ArcObstacle::packSize() const
{
    int fullSize = transform.packSize();
    fullSize += sizeof(float[3]);
    fullSize += sizeof(float[3]);
    fullSize += sizeof(float);
    fullSize += sizeof(float);
    fullSize += sizeof(float);
    fullSize += sizeof(int32_t);
    fullSize += sizeof(int32_t);
    fullSize += sizeof(float[4]);
    fullSize += sizeof(int32_t[MaterialCount]);
    fullSize += sizeof(unsigned char);
    return fullSize;
}


void ArcObstacle::print(std::ostream& out, const std::string& indent) const
{
    int i;

    out << indent << "arc" << std::endl;

    out << indent << "  position " << pos[0] << " "
        << pos[1] << " " << pos[2] << std::endl;
    out << indent << "  size " << size[0] << " "
        << size[1] << " " << size[2] << std::endl;
    out << indent << "  rotation " << ((angle * 180.0) / M_PI) << std::endl;
    out << indent << "  angle " << sweepAngle << std::endl;
    out << indent << "  ratio " << ratio << std::endl;
    out << indent << "  divisions " << divisions << std::endl;

    transform.printTransforms(out, indent);

    out << indent << "  texsize " << texsize[0] << " " << texsize[1] << " "
        << texsize[2] << " " << texsize[3]
        << std::endl;

    const char* sideNames[MaterialCount] =
    { "top", "bottom", "inside", "outside", "startside", "endside" };
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
    if (ricochet)
        out << indent << "  ricochet" << std::endl;
    if (!useNormals)
        out << indent << "  flatshading" << std::endl;

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
