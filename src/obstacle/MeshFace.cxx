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
#include "MeshFace.h"

// System headers
#include <math.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/mixed_product.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/geometric.hpp>

// Common headers
#include "global.h"
#include "Pack.h"
#include "MeshObstacle.h"
#include "PhysicsDriver.h"
#include "Intersect.h"


const char* MeshFace::typeName = "MeshFace";


MeshFace::MeshFace(MeshObstacle* _mesh)
{
    mesh = _mesh;
    vertexCount = 0;
    vertices = NULL;
    normals = NULL;
    texcoords = NULL;
    noclusters = false;
    smoothBounce = false;
    driveThrough = false;
    shootThrough = false;
    edges = NULL;
    edgePlanes = NULL;
    specialData = NULL;
    specialState = 0;
    phydrv = -1;

    return;
}


MeshFace::MeshFace(
    MeshObstacle* _mesh, int _vertexCount,
    glm::vec3 *_vertices[], glm::vec3 **_normals, glm::vec2 **_texcoords,
    const BzMaterial* _bzMaterial, int physics,
    bool _noclusters, bool bounce, bool drive, bool shoot, bool rico)
{
    mesh = _mesh;
    vertexCount = _vertexCount;
    vertices = _vertices;
    normals = _normals;
    texcoords = _texcoords;
    bzMaterial = _bzMaterial;
    phydrv = physics;
    noclusters = _noclusters;
    smoothBounce = bounce;
    driveThrough = drive;
    shootThrough = shoot;
    ricochet = rico;
    edges = NULL;
    edgePlanes = NULL;
    specialData = NULL;
    specialState = 0;

    finalize();

    return;
}


void MeshFace::finalize()
{
    float maxCrossSqr = 0.0f;
    auto bestCross = glm::vec3(0.0f);
    int bestSet[3] = { -1, -1, -1 };

    // find the best vertices for making the plane
    int i, j, k;
    for (i = 0; i < (vertexCount - 2); i++)
    {
        auto &verti = *vertices[i];
        for (j = i; j < (vertexCount - 1); j++)
        {
            auto &vertj = *vertices[j];
            auto edge2 = verti - vertj;
            for (k = j; k < (vertexCount - 0); k++)
            {
                auto edge1 = *vertices[k] - vertj;
                auto cross = glm::cross(edge1, edge2);
                const float lenSqr = glm::length2(cross);
                if (lenSqr > maxCrossSqr)
                {
                    maxCrossSqr = lenSqr;
                    bestSet[0] = i;
                    bestSet[1] = j;
                    bestSet[2] = k;
                    bestCross = cross;
                }
            }
        }
    }

    if (maxCrossSqr < +1.0e-20f)
    {

        logDebugMessage(1,"invalid mesh face (%f)", maxCrossSqr);
        if ((debugLevel >= 3) && (mesh != NULL))
        {
            logDebugMessage(0,":");
            for (i = 0; i < vertexCount; i++)
                logDebugMessage(0," %i", vertices[i] - mesh->getVertices());
            print(std::cerr, "");
        }
        logDebugMessage(1,"\n");

        vertexCount = 0;
        return;
    }

    // make the plane
    float scale = 1.0f / sqrtf (maxCrossSqr);
    const auto &vert = *vertices[bestSet[1]];
    bestCross *= scale;
    plane = glm::vec4(bestCross, -glm::dot(bestCross, vert));

    // see if the whole face is convex
    int v;
    auto v0 = *vertices[0];
    auto v1 = *vertices[1];
    for (v = 0; v < vertexCount; v++)
    {
        auto &v2 = *vertices[(v + 2) % vertexCount];
        const float d = glm::mixedProduct(v1 - v0, v2 - v1, bestCross);
        if (d <= 0.0f)
        {

            logDebugMessage(1,"non-convex mesh face (%f)", d);
            if ((debugLevel >= 3) && (mesh != NULL))
            {
                logDebugMessage(0,":");
                for (i = 0; i < vertexCount; i++)
                    logDebugMessage(0," %i", vertices[i] - mesh->getVertices());
                print(std::cerr, "");
            }
            logDebugMessage(1,"\n");

            vertexCount = 0;
            return;
        }
        v0 = v1;
        v1 = v2;
    }

    // see if the vertices are coplanar
    for (v = 0; v < vertexCount; v++)
    {
        const float cross = glm::dot(*vertices[v], bestCross);
        if (fabsf(cross + plane[3]) > 1.0e-3)
        {
            logDebugMessage(1,"non-planar mesh face (%f)", cross + plane[3]);
            if ((debugLevel >= 3) && (mesh != NULL))
            {
                logDebugMessage(0,":");
                for (i = 0; i < vertexCount; i++)
                    logDebugMessage(0," %i", vertices[i] - mesh->getVertices());
                print(std::cerr, "");
            }
            logDebugMessage(1,"\n");

            vertexCount = 0;
            return;
        }
    }

    // setup extents
    for (v = 0; v < vertexCount; v++)
        extents.expandToPoint(*vertices[v]);

    // setup fake obstacle parameters
    pos[0] = (extents.maxs[0] + extents.mins[0]) / 2.0f;
    pos[1] = (extents.maxs[1] + extents.mins[1]) / 2.0f;
    pos[2] = extents.mins[2];
    size[0] = (extents.maxs[0] - extents.mins[0]) / 2.0f;
    size[1] = (extents.maxs[1] - extents.mins[1]) / 2.0f;
    size[2] = (extents.maxs[2] - extents.mins[2]);
    angle = 0.0f;
    ZFlip = false;

    // make the edge planes
    edgePlanes = new glm::vec4[vertexCount];
    v1 = *vertices[0];
    for (v = 0; v < vertexCount; v++)
    {
        const int next = (v + 1) % vertexCount;
        auto &v2 = *vertices[next];
        auto edgePlane = glm::normalize(glm::cross(v2 - v1, bestCross));
        edgePlanes[v] = glm::vec4(edgePlane, -glm::dot(v1, edgePlane));
        v1 = v2;
    }

    // set the plane type
    planeBits = 0;
    const float fudge = 1.0e-5f;
    if ((fabsf(plane[2]) + fudge) >= 1.0f)
    {
        planeBits |= ZPlane;
        if (plane[2] > 0.0f)
        {
            planeBits |= UpPlane;
            //FIXME
            plane[2] = 1.0f;
            plane[3] = -pos[2];
        }
        else
        {
            planeBits |= DownPlane;
            //FIXME
            plane[2] = -1.0f;
            plane[3] = +pos[2];
        }
        //FIXME
        plane[0] = 0.0f;
        plane[1] = 0.0f;
    }
    else if ((fabsf(plane[0]) + fudge) >= 1.0f)
        planeBits |= XPlane;
    else if ((fabsf(plane[1]) + fudge) >= 1.0f)
        planeBits |= YPlane;

    if (fabsf(plane[2]) < fudge)
        planeBits |= WallPlane;

    return;
}


MeshFace::~MeshFace()
{
    delete[] vertices;
    delete[] normals;
    delete[] texcoords;
    delete[] edges;
    delete[] edgePlanes;
    delete specialData;
    return;
}


const char* MeshFace::getType() const
{
    return typeName;
}


const char* MeshFace::getClassName() // const
{
    return typeName;
}


bool MeshFace::isValid() const
{
    // this is used as a tag in finalize()
    if (vertexCount == 0)
        return false;
    else
        return true;
}


bool MeshFace::isFlatTop() const
{
    return isUpPlane();
}


float MeshFace::intersect(const Ray& ray) const
{
    // NOTE: i'd use a quick test here first, but the
    //       plan is to use an octree for the collision
    //       manager which should get us close enough
    //       that a quick test might actually eat up time.
    //
    // find where the ray crosses each plane, and then
    // check the dot-product of the three bounding planes
    // to see if the intersection point is contained within
    // the face.
    //
    //  L - line unit vector    Lo - line origin
    //  N - plane normal unit vector  d  - plane offset
    //  P - point in question  t - time
    //
    //  (N dot P) + d = 0           { plane equation }
    //  P = (t * L) + Lo             { line equation }
    //  t (N dot L) + (N dot Lo) + d = 0
    //
    //  t = - (d + (N dot Lo)) / (N dot L)     { time of impact }
    //
    const auto &dir    = ray.getDirection();
    const auto &origin = ray.getOrigin();
    const auto normal  = glm::vec3(plane);

    // get the time until the shot would hit each plane
    const float linedot = -glm::dot(normal, dir);
    if (linedot <= 0.001f)
    {
        // shot is either parallel, or going through backwards
        return -1.0f;
    }
    const float origindot = glm::dot(normal, origin);
    // linedot should be safe to divide with now
    float hitTime = plane[3] + origindot;
    if (hitTime < 0.0f)
        return -1.0f;

    hitTime /= linedot;

    // get the contact location
    auto point = glm::vec4(dir * hitTime + origin, 1.0f);

    // now test against the edge planes
    for (int q = 0; q < vertexCount; q++)
    {
        float d = glm::dot(edgePlanes[q], point);
        if (d > 0.001f)
            return -1.0f;
    }

    return hitTime;
}


void MeshFace::get3DNormal(const glm::vec3 &p, glm::vec3 &n) const
{
    if (!smoothBounce || !useNormals())
    {
        // just use the plain normal
        n = plane;
    }
    else
    {
        // FIXME: this isn't quite right
        // normal smoothing to fake curved surfaces
        int i;
        // calculate the triangle ares
        float* areas = new float[vertexCount];
        auto v1 = *vertices[0];
        auto vp = p;
        for (i = 0; i < vertexCount; i++)
        {
            int next = (i + 1) % vertexCount;
            auto &v2 = *vertices[next];
            areas[i] = glm::length(glm::cross(vp - v1, v2 - v1));
            v1 = v2;
        }
        float smallestArea = MAXFLOAT;
        float* twinAreas = new float[vertexCount];
        for (i = 0; i < vertexCount; i++)
        {
            int next = (i + 1) % vertexCount;
            twinAreas[i] = areas[i] + areas[next];
            if (twinAreas[i] < 1.0e-10f)
            {
                n = *normals[next];
                delete[] areas;
                delete[] twinAreas;
                return;
            }
            if (twinAreas[i] < smallestArea)
                smallestArea = twinAreas[i];
        }
        delete[] areas;
        auto normal = glm::vec3(0.0f);
        for (i = 0; i < vertexCount; i++)
        {
            int next = (i + 1) % vertexCount;
            float factor = smallestArea / twinAreas[i];
            normal += *normals[next] * factor;
        }
        delete[] twinAreas;
        if (normal.x || normal.y || normal.z)
            n = glm::normalize(normal);
        else
            n = plane;
        return;
    }

    return;
}


void MeshFace::getNormal(const glm::vec3 &, glm::vec3 &n) const
{
    n = plane;
    return;
}


///////////////////////////////////////////////////////////////
//  FIXME - all geometry after this point is currently JUNK! //
///////////////////////////////////////////////////////////////


bool MeshFace::getHitNormal(const glm::vec3 &UNUSED(oldPos), float UNUSED(oldAngle),
                            const glm::vec3 &UNUSED(newPos), float UNUSED(newAngle),
                            float UNUSED(dx), float UNUSED(dy), float UNUSED(height),
                            glm::vec3 &normal) const
{
    normal = plane;
    return true;
}


bool MeshFace::inCylinder(const glm::vec3 &p,float radius, float height) const
{
    return inBox(p, 0.0f, radius, radius, height);
}


bool MeshFace::inBox(const glm::vec3 &p, float _angle,
                     float dx, float dy, float height) const
{
    int i;

    // Z axis separation test
    if ((extents.mins[2] > (p[2] + height)) || (extents.maxs[2] < p[2]))
        return false;

    // translate the face so that the box is an origin box
    // centered at 0,0,0  (this assumes that it is cheaper
    // to move the polygon then the box, tris and quads will
    // probably be the dominant polygon types).

    glm::vec4 pln; // translated plane
    glm::vec3 *v = new glm::vec3[vertexCount]; // translated vertices
    const float cos_val = cosf(-_angle);
    const float sin_val = sinf(-_angle);
    for (i = 0; i < vertexCount; i++)
    {
        float h[2];
        h[0] = vertices[i]->x - p[0];
        h[1] = vertices[i]->y - p[1];
        v[i][0] = (cos_val * h[0]) - (sin_val * h[1]);
        v[i][1] = (cos_val * h[1]) + (sin_val * h[0]);
        v[i][2] = vertices[i]->z - p[2];
    }
    pln[0] = (cos_val * plane[0]) - (sin_val * plane[1]);
    pln[1] = (cos_val * plane[1]) + (sin_val * plane[0]);
    pln[2] = plane[2];
    pln[3] = plane[3] + glm::dot(glm::vec3(plane), p);

    // testPolygonInAxisBox() expects us to have already done all of the
    // separation tests with respect to the box planes. we could not do
    // the X and Y axis tests until we'd found the translated vertices,
    // so we will do them now.

    // X axis test
    float min, max;
    min = +MAXFLOAT;
    max = -MAXFLOAT;
    for (i = 0; i < vertexCount; i++)
    {
        if (v[i][0] < min)
            min = v[i][0];
        if (v[i][0] > max)
            max = v[i][0];
    }
    if ((min > dx) || (max < -dx))
    {
        delete[] v;
        return false;
    }

    // Y axis test
    min = +MAXFLOAT;
    max = -MAXFLOAT;
    for (i = 0; i < vertexCount; i++)
    {
        if (v[i][1] < min)
            min = v[i][1];
        if (v[i][1] > max)
            max = v[i][1];
    }
    if ((min > dy) || (max < -dy))
    {
        delete[] v;
        return false;
    }

    // FIXME: do not use testPolygonInAxisBox()
    Extents box;
    // mins
    box.mins[0] = -dx;
    box.mins[1] = -dy;
    box.mins[2] = 0.0f;
    // maxs
    box.maxs[0] = +dx;
    box.maxs[1] = +dy;
    box.maxs[2] = height;

    bool hit = testPolygonInAxisBox(vertexCount, v, pln, box);

    delete[] v;

    return hit;
}


bool MeshFace::inMovingBox(const glm::vec3 &oldPos, float UNUSED(oldAngle),
                           const glm::vec3 &newPos, float newAngle,
                           float dx, float dy, float height) const
{
    // expand the box with respect to Z axis motion
    glm::vec3 _pos;
    _pos[0] = newPos[0];
    _pos[1] = newPos[1];
    if (oldPos[2] < newPos[2])
        _pos[2] = oldPos[2];
    else
        _pos[2] = newPos[2];
    height = height + fabsf(oldPos[2] - newPos[2]);

    return inBox(_pos, newAngle, dx, dy, height);
}


bool MeshFace::isCrossing(const glm::vec3 &, float, float, float, float,
                          glm::vec4 *_plane) const
{
    if (_plane != NULL)
        *_plane = plane;
    return true;
}


void *MeshFace::pack(void *buf) const
{
    // state byte
    unsigned char stateByte = 0;
    stateByte |= useNormals()     ? (1 << 0) : 0;
    stateByte |= useTexcoords()   ? (1 << 1) : 0;
    stateByte |= isDriveThrough() ? (1 << 2) : 0;
    stateByte |= isShootThrough() ? (1 << 3) : 0;
    stateByte |= smoothBounce     ? (1 << 4) : 0;
    stateByte |= noclusters       ? (1 << 5) : 0;
    stateByte |= canRicochet()    ? (1 << 6) : 0;
    buf = nboPackUByte(buf, stateByte);

    // vertices
    buf = nboPackInt(buf, vertexCount);
    for (int i = 0; i < vertexCount; i++)
    {
        int32_t index = vertices[i] - mesh->getVertices();
        buf = nboPackInt(buf, index);
    }

    // normals
    if (useNormals())
    {
        for (int i = 0; i < vertexCount; i++)
        {
            int32_t index = normals[i] - mesh->getNormals();
            buf = nboPackInt(buf, index);
        }
    }

    // texcoords
    if (useTexcoords())
    {
        for (int i = 0; i < vertexCount; i++)
        {
            int32_t index = texcoords[i] - mesh->getTexcoords();
            buf = nboPackInt(buf, index);
        }
    }

    // material
    int matindex = MATERIALMGR.getIndex(bzMaterial);
    buf = nboPackInt(buf, matindex);

    // physics driver
    buf = nboPackInt(buf, phydrv);

    return buf;
}


const void *MeshFace::unpack(const void *buf)
{
    int32_t inTmp;
    driveThrough = shootThrough = smoothBounce = false;
    // state byte
    bool tmpNormals, tmpTexcoords;
    unsigned char stateByte = 0;
    buf = nboUnpackUByte(buf, stateByte);
    tmpNormals   = (stateByte & (1 << 0)) != 0;
    tmpTexcoords = (stateByte & (1 << 1)) != 0;
    driveThrough = (stateByte & (1 << 2)) != 0;
    shootThrough = (stateByte & (1 << 3)) != 0;
    smoothBounce = (stateByte & (1 << 4)) != 0;
    noclusters   = (stateByte & (1 << 5)) != 0;
    ricochet     = (stateByte & (1 << 6)) != 0;

    // vertices
    buf = nboUnpackInt(buf, inTmp);
    vertexCount = int(inTmp);
    vertices = new glm::vec3*[vertexCount];
    for (int i = 0; i < vertexCount; i++)
    {
        int32_t index;
        buf = nboUnpackInt(buf, index);
        vertices[i] = &mesh->getVertices()[index];
    }

    // normals
    if (tmpNormals)
    {
        normals = new glm::vec3*[vertexCount];
        for (int i = 0; i < vertexCount; i++)
        {
            int32_t index;
            buf = nboUnpackInt(buf, index);
            normals[i] = &mesh->getNormals()[index];
        }
    }

    // texcoords
    if (tmpTexcoords)
    {
        texcoords = new glm::vec2*[vertexCount];
        for (int i = 0; i < vertexCount; i++)
        {
            int32_t index;
            buf = nboUnpackInt(buf, index);
            texcoords[i] = &mesh->getTexcoords()[index];
        }
    }

    // material
    int32_t matindex;
    buf = nboUnpackInt(buf, matindex);
    bzMaterial = MATERIALMGR.getMaterial(matindex);

    // physics driver
    buf = nboUnpackInt(buf, inTmp);
    phydrv = int(inTmp);

    finalize();

    return buf;
}


int MeshFace::packSize() const
{
    int fullSize = sizeof(unsigned char);
    fullSize += sizeof(int32_t);
    fullSize += sizeof(int32_t) * vertexCount;
    if (useNormals())
        fullSize += sizeof(int32_t) * vertexCount;
    if (useTexcoords())
        fullSize += sizeof(int32_t) * vertexCount;
    fullSize += sizeof(int32_t); // material
    fullSize += sizeof(int32_t); // physics driver

    return fullSize;
}


void MeshFace::print(std::ostream& out, const std::string& indent) const
{
    if (mesh == NULL)
        return;

    int i;
    out << indent << "  face" << std::endl;

    if (debugLevel >= 3)
    {
        out << indent << "  # plane normal = " << plane[0] << " " << plane[1] << " "
            << plane[2] << " " << plane[3] << std::endl;
    }

    out << indent << "    vertices";
    for (i = 0; i < vertexCount; i++)
    {
        int index = vertices[i] - mesh->getVertices();
        out << " " << index;
    }
    if (debugLevel >= 3)
    {
        out << indent << " #";
        for (i = 0; i < vertexCount; i++)
            out << " " << vertices[i]->x << " " << vertices[i]->y << " " << vertices[i]->z;
    }
    out << std::endl;

    if (normals != NULL)
    {
        out << indent << "    normals";
        for (i = 0; i < vertexCount; i++)
        {
            int index = normals[i] - mesh->getNormals();
            out << " " << index;
        }
        if (debugLevel >= 3)
        {
            out << " #";
            for (i = 0; i < vertexCount; i++)
                out << " " << normals[i]->x <<  " " << normals[i]->y << " " << normals[i]->z;
        }
        out << std::endl;
    }

    if (texcoords != NULL)
    {
        out << indent << "    texcoords";
        for (i = 0; i < vertexCount; i++)
        {
            int index = texcoords[i] - mesh->getTexcoords();
            out << " " << index;
        }
        if (debugLevel >= 3)
        {
            out << " #";
            for (i = 0; i < vertexCount; i++)
                out << " " << texcoords[i]->s <<  " " << texcoords[i]->t;
        }
        out << std::endl;
    }

    out << indent << "    matref ";
    MATERIALMGR.printReference(out, bzMaterial);
    out << std::endl;

    const PhysicsDriver* driver = PHYDRVMGR.getDriver(phydrv);
    if (driver != NULL)
    {
        out << indent << "    phydrv ";
        if (driver->getName().size() > 0)
            out << driver->getName();
        else
            out << phydrv;
        out << std::endl;
    }

    if (noclusters && !mesh->noClusters())
        out << indent << "    noclusters" << std::endl;
    if (smoothBounce && !mesh->useSmoothBounce())
        out << indent << "    smoothBounce" << std::endl;
    if ((driveThrough && shootThrough) &&
            !(mesh->isDriveThrough() && mesh->isShootThrough()))
        out << indent << "    passable" << std::endl;
    else
    {
        if (driveThrough && !mesh->isDriveThrough())
            out << indent << "    driveThrough" << std::endl;
        if (shootThrough && !mesh->isShootThrough())
            out << indent << "    shootThrough" << std::endl;
    }
    if (ricochet &&  !mesh->canRicochet())
        out << indent << "  ricochet" << std::endl;

    out << indent << "  endface" << std::endl;

    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
