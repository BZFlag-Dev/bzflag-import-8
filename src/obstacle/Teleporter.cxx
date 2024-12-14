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
#include "Teleporter.h"

// System headers
#include <math.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

// Common headers
#include "global.h"
#include "Pack.h"
#include "Intersect.h"
#include "MeshTransform.h"


const char* Teleporter::typeName = "Teleporter";


Teleporter::Teleporter()
{
    backLink = NULL;
    frontLink = NULL;
    return;
}


Teleporter::Teleporter(const glm::vec3 &p, float a, float w,
                       float b, float h, float _border, bool _horizontal,
                       bool drive, bool shoot, bool rico) :
    Obstacle(p, a, w, b, h, drive, shoot, rico),
    border(_border), horizontal(_horizontal)
{
    finalize();
    return;
}


Teleporter::~Teleporter()
{
    delete backLink;
    delete frontLink;
    return;
}


Obstacle* Teleporter::copyWithTransform(const MeshTransform& xform) const
{
    auto newPos = pos;
    auto newSize = origSize;
    float newAngle = angle;

    MeshTransform::Tool tool(xform);
    bool flipped;
    tool.modifyOldStyle(newPos, newSize, newAngle, flipped);

    Teleporter* copy =
        new Teleporter(newPos, newAngle, newSize[0], newSize[1], newSize[2],
                       border, horizontal, driveThrough, shootThrough, ricochet);

    copy->setName(name);

    return copy;
}

void Teleporter::finalize()
{
    origSize[0] = size[0];
    origSize[1] = size[1];
    origSize[2] = size[2];

    if (!horizontal)
    {
        size[1] = origSize[1] + (border * 2.0f);
        size[2] = origSize[2] + border;
    }

    // the same as the default Obstacle::getExtents(), except
    // that we use the larger of the border half-width and size[0].
    float sizeX = border * 0.5f;
    if (size[0] > sizeX)
        sizeX = size[0];
    float xspan = (fabsf(cosf(angle)) * sizeX) + (fabsf(sinf(angle)) * size[1]);
    float yspan = (fabsf(cosf(angle)) * size[1]) + (fabsf(sinf(angle)) * sizeX);
    extents.mins[0] = pos[0] - xspan;
    extents.maxs[0] = pos[0] + xspan;
    extents.mins[1] = pos[1] - yspan;
    extents.maxs[1] = pos[1] + yspan;
    extents.mins[2] = pos[2];
    extents.maxs[2] = pos[2] + size[2];

    makeLinks();

    return;
}


void Teleporter::makeLinks()
{
    int i;

    // make the new pointers to floats,
    // the MeshFace will delete[] them
    auto fvrts = new glm::vec3*[4];
    auto bvrts = new glm::vec3*[4];
    auto ftxcds = new glm::vec2*[4];
    auto btxcds = new glm::vec2*[4];
    for (i = 0; i < 4; i++)
    {
        fvrts[i] = &fvertices[i];
        bvrts[i] = &bvertices[i];
        ftxcds[i] = &texcoords[i];
        btxcds[i] = &texcoords[i];
    }

    // get the basics
    const auto &p = getPosition();
    const float a = getRotation();
    const float w = getWidth();
    const float b = getBreadth();
    const float br = getBorder();
    const float h = getHeight();

    // setup the texcoord coordinates
    const float xtxcd = 1.0f;
    float ytxcd;
    if ((b - br) > 0.0f)
        ytxcd = h / (2.0f * (b - br));
    else
        ytxcd = 1.0f;
    texcoords[0][0] = 0.0f;
    texcoords[0][1] = 0.0f;
    texcoords[1][0] = xtxcd;
    texcoords[1][1] = 0.0f;
    texcoords[2][0] = xtxcd;
    texcoords[2][1] = ytxcd;
    texcoords[3][0] = 0.0f;
    texcoords[3][1] = ytxcd;

    const float cos_val = cosf(a);
    const float sin_val = sinf(a);

    if (!horizontal)
    {
        const float params[4][2] =
        {{-1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
        float wlen[2] = { (cos_val * w), (sin_val * w) };
        float blen[2] = { (-sin_val * (b - br)), (cos_val * (b - br)) };

        for (i = 0; i < 4 ; i++)
        {
            bvrts[i]->x = p[0] + (wlen[0] + (blen[0] * params[i][0]));
            bvrts[i]->y = p[1] + (wlen[1] + (blen[1] * params[i][0]));
            bvrts[i]->z = p[2] + ((h - br) * params[i][1]);
        }
        backLink = new MeshFace(NULL, 4, bvrts, NULL, btxcds,
                                NULL, -1, false, false, true, true, false);

        for (i = 0; i < 4 ; i++)
        {
            *fvrts[i] = p;
            fvrts[i]->x -= wlen[0] + (blen[0] * params[i][0]);
            fvrts[i]->y -= wlen[1] + (blen[1] * params[i][0]);
            fvrts[i]->z += (h - br) * params[i][1];
        }
        frontLink = new MeshFace(NULL, 4, fvrts, NULL, ftxcds,
                                 NULL, -1, false, false, true, true, false);
    }
    else
    {
        float xlen = w - br;
        float ylen = b - br;
        for (i = 0; i < 4 ; i++)
        {
            *bvrts[i]    = p;
            bvrts[i]->z += h - br;
        }
        bvrts[0]->x += (cos_val * xlen) - (sin_val * ylen);
        bvrts[0]->y += (cos_val * ylen) + (sin_val * xlen);
        bvrts[1]->x += (cos_val * xlen) - (sin_val * -ylen);
        bvrts[1]->y += (cos_val * -ylen) + (sin_val * xlen);
        bvrts[2]->x += (cos_val * -xlen) - (sin_val * -ylen);
        bvrts[2]->y += (cos_val * -ylen) + (sin_val * -xlen);
        bvrts[3]->x += (cos_val * -xlen) - (sin_val * ylen);
        bvrts[3]->y += (cos_val * ylen) + (sin_val * -xlen);
        backLink = new MeshFace(NULL, 4, bvrts, NULL, btxcds,
                                NULL, -1, false, false, true, true, false);

        for (i = 0; i < 4; i++)
        {
            *fvrts[i] = *bvrts[3 - i]; // reverse order
            fvrts[i]->z = p[2] + h; // change the height
        }
        frontLink = new MeshFace(NULL, 4, fvrts, NULL, ftxcds,
                                 NULL, -1, false, false, true, true, false);
    }

    return;
}


const char* Teleporter::getType() const
{
    return typeName;
}


const char* Teleporter::getClassName() // const
{
    return typeName;
}


bool Teleporter::isValid() const
{
    if (!backLink->isValid() || !frontLink->isValid())
        return false;
    return Obstacle::isValid();
}


float Teleporter::intersect(const Ray& r) const
{
    // expand to include border
    return timeRayHitsBlock(r, getPosition(), getRotation(),
                            getWidth(), getBreadth(), getHeight());
}


void Teleporter::getNormal(const glm::vec3 &p1, glm::vec3 &n) const
{
    // get normal to closest border column (assume column is circular)
    const auto &p2 = getPosition();
    const float c = cosf(-getRotation());
    const float s = sinf(-getRotation());
    const float b = 0.5f * getBorder();
    const float d = getBreadth() - b;
    const float j = (c * (p1[1] - p2[1]) + s * (p1[0] - p2[0]) > 0.0f) ? d : -d;
    glm::vec2 cc;
    cc[0] = p2[0] + (s * j);
    cc[1] = p2[1] + (c * j);
    getNormalRect(p1, cc, getRotation(), b, b, n);
}


bool Teleporter::inCylinder(const glm::vec3 &p, float radius, float height) const
{
    return (p[2]+height) >= getPosition()[2] &&
           p[2] <= getPosition()[2] + getHeight() &&
           testRectCircle(getPosition(), getRotation(),
                          getWidth(), getBreadth(), p, radius);
}


bool Teleporter::inBox(const glm::vec3 &p, float a,
                       float dx, float dy, float dz) const
{
    const float tankTop = p[2] + dz;
    const float teleTop = getExtents().maxs[2];
    const float crossbarBottom = teleTop - getBorder();

    if ((p[2] < crossbarBottom) && (tankTop >= getPosition()[2]))
    {
        // test individual border columns
        const float c = cosf(getRotation());
        const float s = sinf(getRotation());
        const float d = getBreadth() - (0.5f * getBorder());
        const float r = 0.5f * getBorder();
        glm::vec2 o;
        o[0] = getPosition()[0] - (s * d);
        o[1] = getPosition()[1] + (c * d);
        if (testRectRect(p, a, dx, dy, o, getRotation(), r, r))
            return true;
        o[0] = getPosition()[0] + (s * d);
        o[1] = getPosition()[1] - (c * d);
        if (testRectRect(p, a, dx, dy, o, getRotation(), r, r))
            return true;
    }

    if ((p[2] < teleTop) && (tankTop >= crossbarBottom))
    {
        // test crossbar
        if (testRectRect(p, a, dx, dy, getPosition(),
                         getRotation(), getWidth(), getBreadth()))
            return true;
    }

    return false;
}


bool Teleporter::inMovingBox(const glm::vec3 &oldP, float UNUSED(oldAngle),
                             const glm::vec3 &p, float a,
                             float dx, float dy, float dz) const
{
    auto minPos = p;
    if (oldP[2] < p[2])
        minPos[2] = oldP[2];
    dz += fabsf(oldP[2] - p[2]);
    return inBox(minPos, a, dx, dy, dz);
}


bool Teleporter::isCrossing(const glm::vec3 &p, float a,
                            float dx, float dy, float,
                            glm::vec4 *plane) const
{
    // if not inside or contained then not crossing
    const auto &p2 = getPosition();
    if (!testRectRect(p, a, dx, dy,
                      p2, getRotation(), getWidth(), getBreadth() - getBorder()) ||
            p[2] < p2[2] || p[2] > p2[2] + getHeight() - getBorder())
        return false;
    if (!plane) return true;

    // it's crossing -- choose which wall is being crossed (this
    // is a guestimate, should really do a careful test).  just
    // see which wall the point is closest to.
    const float a2 = getRotation();
    const float c = cosf(-a2);
    const float s = sinf(-a2);
    const float x = c * (p[0] - p2[0]) - s * (p[1] - p2[1]);
    float pw[2];
    plane->x = ((x < 0.0f) ? -cosf(a2) : cosf(a2));
    plane->y = ((x < 0.0f) ? -sinf(a2) : sinf(a2));
    pw[0] = p2[0] + getWidth() * plane->x;
    pw[1] = p2[1] + getWidth() * plane->y;

    // now finish off plane equation
    plane->z = 0.0f;
    plane->w = -(plane->x * pw[0] + plane->y * pw[1]);
    return true;
}


// return true if ray goes through teleporting part
float Teleporter::isTeleported(const Ray& r, int& face) const
{
    // get t's for teleporter with and without border
    const float tb = intersect(r);
    const float t = timeRayHitsBlock(r, getPosition(),
                                     getRotation(), getWidth(),
                                     getBreadth() - getBorder(), getHeight() - getBorder());

    // if intersection with border is before one without then doesn't teleport
    // (cos it hit the border first).  also no teleport if no intersection.
    if ((tb >= 0.0f && t - tb > 1e-6) || t < 0.0f)
        return -1.0f;

    // get teleport position.  if above or below teleporter then no teleportation.
    glm::vec3 p;
    r.getPoint(t, p);
    p[2] -= getPosition()[2];
    if (p[2] < 0.0f || p[2] > getHeight() - getBorder())
        return -1.0f;

    // figure out which face:  rotate intersection into teleporter space,
    //    if to east of teleporter then face 0 else face 1.
    const float x = cosf(-getRotation()) * (p[0] - getPosition()[0]) -
                    sinf(-getRotation()) * (p[1] - getPosition()[1]);
    face = (x > 0.0f) ? 0 : 1;
    return t;
}


float Teleporter::getProximity(const glm::vec3 &p, float radius) const
{
    // make sure tank is sufficiently close
    if (!testRectCircle(getPosition(), getRotation(),
                        getWidth(), getBreadth() - getBorder(),
                        p, 1.2f * radius))
        return 0.0f;

    // transform point to teleporter space
    // translate origin
    float pa[3];
    pa[0] = p[0] - getPosition()[0];
    pa[1] = p[1] - getPosition()[1];
    pa[2] = p[2] - getPosition()[2];

    // make sure not too far above or below teleporter
    if (pa[2] < -1.2f * radius ||
            pa[2] > getHeight() - getBorder() + 1.2f * radius)
        return 0.0f;

    // rotate and reflect into first quadrant
    const float c = cosf(-getRotation()), s = sinf(-getRotation());
    const float x = fabsf(c * pa[0] - s * pa[1]);
    const float y = fabsf(c * pa[1] + s * pa[0]);

    // get proximity to face
    float t = 1.2f - x / radius;

    // if along side then trail off as point moves away from faces
    if (y > getBreadth() - getBorder())
    {
        float f = (float)(2.0 / M_PI) * atan2f(x, y - getBreadth() + getBorder());
        t *= f * f;
    }
    else if (pa[2] < 0.0f)
    {
        float f = 1.0f + pa[2] / (1.2f * radius);
        if (f >= 0.0f && f <= 1.0f) t *= f * f;
    }
    else if (pa[2] > getHeight() - getBorder())
    {
        float f = 1.0f - (pa[2] - getHeight() + getBorder()) / (1.2f * radius);
        if (f >= 0.0f && f <= 1.0f) t *= f * f;
    }

    return t > 0.0f ? (t > 1.0f ? 1.0f : t) : 0.0f;
}


bool Teleporter::hasCrossed(const glm::vec3 &p1, const glm::vec3 &p2, int& f) const
{
    // check above/below teleporter
    const auto &p = getPosition();
    if ((p1[2] < p[2] && p2[2] < p[2]) ||
            (p1[2] > p[2] + getHeight() - getBorder() &&
             p2[2] > p[2] + getHeight() - getBorder()))
        return false;

    const float c = cosf(-getRotation()), s = sinf(-getRotation());
    const float x1 = c * (p1[0] - p[0]) - s * (p1[1] - p[1]);
    const float x2 = c * (p2[0] - p[0]) - s * (p2[1] - p[1]);
    const float y2 = c * (p2[1] - p[1]) + s * (p2[0] - p[0]);
    if (x1 * x2 < 0.0f && fabsf(y2) <= getBreadth() - getBorder())
    {
        f = (x1 > 0.0f) ? 0 : 1;
        return true;
    }
    return false;
}


void Teleporter::getPointWRT(const Teleporter& t2, int face1, int face2,
                             glm::vec3 &p, glm::vec3 *d, float *a) const
{
    // transforming pIn to pOut, going from tele1(this) to tele2
    const Teleporter& tele1 = *this;
    const Teleporter& tele2 = t2;

    const auto &p1 = tele1.getPosition();
    const auto &p2 = tele2.getPosition();
    const auto &s1 = tele1.getSize();
    const auto &s2 = tele2.getSize();

    const auto pos1  = p1;
    const auto size1 = s1;
    const auto pos2  = p2;
    const auto size2 = s2;

    const float bord1 = tele1.getBorder();
    const float bord2 = tele2.getBorder();

    // y & z axis scaling factors  (relative active areas)
    const auto dims1 = glm::vec2(size1.y - bord1, size1.z - bord1);
    const auto dims2 = glm::vec2(size2.y - bord2, size2.z - bord2);
    const auto dimsScale = (dims2 / dims1);

    // adjust the angles for the faces
    // NOTE: if (face1 == face2), there's an extra 180 degrees spin
    const float pi = float(M_PI);
    const float radians1 = tele1.getRotation() + ((face1 == 0) ? 0.0f : pi);
    const float radians2 = tele2.getRotation() + ((face2 == 1) ? 0.0f : pi);

    // translate to origin, and revert rotation
    p -= pos1;
    p = glm::rotateZ(p, -radians1);

    // fixed x offset, and scale y & z coordinates
    p.x = -size2.x;
    p.y *= dimsScale.x;
    p.z *= dimsScale.y;

    // apply rotation, translate to new position
    p = glm::rotateZ(p, +radians2);
    p += pos2;

    // fill in output angle and direction variables, if requested
    const float a_ = radians2 - radians1;
    if (a)
        *a += a_;
    if (d)
    {
        const float c = cosf(a_);
        const float s = sinf(a_);
        const float dx = d->x;
        const float dy = d->y;
        d->x = c * dx - s * dy;
        d->y = s * dx + c * dy;
    }
}


bool Teleporter::getHitNormal(const glm::vec3 &pos1, float azimuth1,
                              const glm::vec3 &pos2, float azimuth2,
                              float width, float breadth, float,
                              glm::vec3 &normal) const
{
    return Obstacle::getHitNormal(pos1, azimuth1,
                                  pos2, azimuth2, width, breadth,
                                  getPosition(), getRotation(), getWidth(), getBreadth(),
                                  getHeight(), normal) >= 0.0f;
}



void* Teleporter::pack(void* buf) const
{
    buf = nboPackStdString(buf, name);

    buf = nboPackVector(buf, pos);
    buf = nboPackFloat(buf, angle);
    buf = nboPackVector(buf, origSize);
    buf = nboPackFloat(buf, border);

    unsigned char horizontalByte = horizontal ? 1 : 0;
    buf = nboPackUByte(buf, horizontalByte);

    unsigned char stateByte = 0;
    stateByte |= isDriveThrough() ? _DRIVE_THRU : 0;
    stateByte |= isShootThrough() ? _SHOOT_THRU : 0;
    stateByte |= canRicochet()    ? _RICOCHET   : 0;
    buf = nboPackUByte(buf, stateByte);

    return buf;
}


const void* Teleporter::unpack(const void* buf)
{
    buf = nboUnpackStdString(buf, name);

    buf = nboUnpackVector(buf, pos);
    buf = nboUnpackFloat(buf, angle);
    buf = nboUnpackVector(buf, size);
    buf = nboUnpackFloat(buf, border);

    unsigned char horizontalByte;
    buf = nboUnpackUByte(buf, horizontalByte);
    horizontal = (horizontalByte == 0) ? false : true;

    unsigned char stateByte;
    buf = nboUnpackUByte(buf, stateByte);
    driveThrough = (stateByte & _DRIVE_THRU) != 0;
    shootThrough = (stateByte & _SHOOT_THRU) != 0;
    ricochet     = (stateByte & _RICOCHET)   != 0;

    finalize();

    return buf;
}


int Teleporter::packSize() const
{
    int fullSize = 0;
    fullSize += nboStdStringPackSize(name);
    fullSize += sizeof(float[3]); // pos
    fullSize += sizeof(float);    // rotation
    fullSize += sizeof(float[3]); // size
    fullSize += sizeof(float);    // border
    fullSize += sizeof(uint8_t);  // horizontal
    fullSize += sizeof(uint8_t);  // state bits
    return fullSize;
}


void Teleporter::print(std::ostream& out, const std::string& indent) const
{
    out << indent << "teleporter";
    if (name.size() > 0)
        out << " " << name;
    out << std::endl;
    const auto &_pos = getPosition();
    out << indent << "  position " << _pos[0] << " " << _pos[1] << " "
        << _pos[2] << std::endl;
    out << indent << "  size " << origSize[0] << " " << origSize[1] << " "
        << origSize[2] << std::endl;
    out << indent << "  rotation " << ((getRotation() * 180.0) / M_PI)
        << std::endl;
    out << indent << "  border " << getBorder() << std::endl;
    if (horizontal)
        out << indent << "  horizontal" << std::endl;
    if (ricochet)
        out << indent << "  ricochet" << std::endl;
    out << indent << "end" << std::endl;
    return;
}


static void outputFloat(std::ostream& out, float value)
{
    char buffer[32];
    snprintf(buffer, 30, " %.8f", value);
    out << buffer;
    return;
}

void Teleporter::printOBJ(std::ostream& out, const std::string& UNUSED(indent)) const
{
    int i;
    glm::vec3 verts[8] =
    {
        {-1.0f, -1.0f, 0.0f},
        {+1.0f, -1.0f, 0.0f},
        {+1.0f, +1.0f, 0.0f},
        {-1.0f, +1.0f, 0.0f},
        {-1.0f, -1.0f, 1.0f},
        {+1.0f, -1.0f, 1.0f},
        {+1.0f, +1.0f, 1.0f},
        {-1.0f, +1.0f, 1.0f}
    };
    glm::vec3 norms[6] =
    {
        {0.0f, -1.0f, 0.0f}, {+1.0f, 0.0f, 0.0f},
        {0.0f, +1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, +1.0f}
    };
    float txcds[4][2] =
    {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };
    MeshTransform xform;
    const float degrees = getRotation() * (float)(180.0 / M_PI);
    const auto zAxis = glm::vec3(0.0f, 0.0f, +1.0f);
    xform.addScale(getSize());
    xform.addSpin(degrees, zAxis);
    xform.addShift(getPosition());
    xform.finalize();
    MeshTransform::Tool xtool(xform);
    for (i = 0; i < 8; i++)
        xtool.modifyVertex(verts[i]);
    for (i = 0; i < 6; i++)
        xtool.modifyNormal(norms[i]);

    out << "# OBJ - start tele" << std::endl;
    out << "o bztele_" << name << std::endl;

    for (i = 0; i < 8; i++)
    {
        out << "v";
        outputFloat(out, verts[i][0]);
        outputFloat(out, verts[i][1]);
        outputFloat(out, verts[i][2]);
        out << std::endl;
    }
    for (i = 0; i < 4; i++)
    {
        out << "vt";
        outputFloat(out, txcds[i][0]);
        outputFloat(out, txcds[i][1]);
        out << std::endl;
    }
    for (i = 0; i < 6; i++)
    {
        out << "vn";
        outputFloat(out, norms[i][0]);
        outputFloat(out, norms[i][1]);
        outputFloat(out, norms[i][2]);
        out << std::endl;
    }
    out << "usemtl telefront" << std::endl;
    out << "f -7/-4/-5 -6/-3/-5 -2/-2/-5 -3/-1/-5" << std::endl;
    out << "usemtl teleback" << std::endl;
    out << "f -5/-4/-3 -8/-3/-3 -4/-2/-3 -1/-1/-3" << std::endl;
    out << "usemtl telerim" << std::endl;
    out << "f -5/-4/-2 -6/-3/-2 -7/-2/-2 -8/-1/-2" << std::endl;
    out << "f -4/-4/-1 -3/-3/-1 -2/-2/-1 -1/-1/-1" << std::endl;
    out << "f -8/-4/-6 -7/-3/-6 -3/-2/-6 -4/-1/-6" << std::endl;
    out << "f -6/-4/-4 -5/-3/-4 -1/-2/-4 -2/-1/-4" << std::endl;

    out << std::endl;

    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
