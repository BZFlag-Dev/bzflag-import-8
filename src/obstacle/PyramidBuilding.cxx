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

// Interface header
#include "PyramidBuilding.h"

// System headers
#include <math.h>
#include <glm/gtc/type_ptr.hpp>

// Common headers
#include "global.h"
#include "Pack.h"
#include "Intersect.h"
#include "MeshTransform.h"

const char*     PyramidBuilding::typeName = "PyramidBuilding";

PyramidBuilding::PyramidBuilding()
{
    // do nothing
}

PyramidBuilding::PyramidBuilding(const glm::vec3 &p, float a,
                                 float w, float b, float h, bool drive, bool shoot, bool rico) :
    Obstacle(p, a, w, b, h,drive,shoot,rico)
{
    finalize();
    return;
}

PyramidBuilding::~PyramidBuilding()
{
    // do nothing
}

void PyramidBuilding::finalize()
{
    Obstacle::setExtents();
    return;
}

Obstacle* PyramidBuilding::copyWithTransform(const MeshTransform& xform) const
{
    auto newPos = pos;
    float newAngle;
    auto newSize = size;
    newAngle = angle;

    MeshTransform::Tool tool(xform);
    bool flipped;
    tool.modifyOldStyle(newPos, newSize, newAngle, flipped);

    PyramidBuilding* copy =
        new PyramidBuilding(newPos, newAngle, newSize[0], newSize[1], newSize[2],
                            driveThrough, shootThrough, ricochet);

    copy->ZFlip = !(getZFlip() == flipped);

    return copy;
}

const char*     PyramidBuilding::getType() const
{
    return typeName;
}

const char*     PyramidBuilding::getClassName() // const
{
    return typeName;
}

float           PyramidBuilding::intersect(const Ray& r) const
{
    return timeRayHitsPyramids(r, getPosition(), getRotation(),
                               getWidth(), getBreadth(), getHeight(),
                               getZFlip());
}

void PyramidBuilding::getNormal(const glm::vec3 &p, glm::vec3 &n) const
{
    // get normal in z = const plane
    const float s = shrinkFactor(p[2]);
    getNormalRect(p, getPosition(), getRotation(),
                  s * getWidth(), s * getBreadth(), n);

    // make sure we are not way above or way below it
    // above is good so we can drive on it when it's fliped
    float top =  getPosition()[2] + getHeight();
    float bottom = getPosition()[2];

    if (s ==0)
    {
        if (this->getZFlip())
        {
            if (p[2] >= top)
            {
                n = glm::vec3(0.0f, 0.0f, 1.0f);
                return;
            }
        }
        else
        {
            if (p[2] <= bottom)
            {
                n = glm::vec3(0.0f, 0.0f, -1.0f);
                return;
            }
        }
    }

    // now angle it due to slope of wall
    // FIXME -- this assumes the pyramid has a square base!
    const float h = 1.0f / hypotf(getHeight(), getWidth());
    n[0] *= h * getHeight();
    n[1] *= h * getHeight();
    n[2] = h * getWidth();

    if (this->getZFlip())
        n[2] *= -1;
}

void PyramidBuilding::get3DNormal(const glm::vec3 &p,
                                  glm::vec3 &n) const
{
    const float epsilon = ZERO_TOLERANCE;

    // get normal in z = const plane
    const float s = shrinkFactor(p[2]);
    getNormalRect(p, getPosition(), getRotation(),
                  s * getWidth(), s * getBreadth(), n);

    // make sure we are not way above or way below it
    // above is good so we can drive on it when it's flipped
    float top =  getPosition()[2]+getHeight();
    float bottom = getPosition()[2];

    if (s == 0)
    {
        if (getZFlip())
        {
            if (p[2] >= top)
            {
                n = glm::vec3(0.0f, 0.0f, 1.0f);
                return;
            }
        }
        else
        {
            if (p[2] <= bottom)
            {
                n = glm::vec3(0.0f, 0.0f, -1.0f);
                return;
            }
        }
    }

    if (s >= 1.0f - epsilon)
    {
        if (getZFlip())
            n = glm::vec3(0.0f, 0.0f, 1.0f);
        else
            n = glm::vec3(0.0f, 0.0f, -1.0f);
        return;
    }

    // now angle it due to slope of wall
    // we figure out if it was an X or Y wall that was hit
    // by checking the normal returned from getNormalRect()
    // FIXME -- fugly beyond belief
    float baseLength = getWidth();
    const float normalAngle = atan2f(n[1], n[0]);
    const float rightAngle = fabsf(fmodf(normalAngle - getRotation() + (float)(M_PI/2.0), (float)M_PI));
    if ((rightAngle < 0.1) || (rightAngle > (M_PI - 0.1)))
        baseLength = getBreadth();
    const float h = 1.0f / hypotf(getHeight(), baseLength);
    n[0] *= h * getHeight();
    n[1] *= h * getHeight();
    n[2]  = h * baseLength;

    if (this->getZFlip())
        n[2] *= -1;
}

bool            PyramidBuilding::inCylinder(const glm::vec3 &p,
        float radius, float height) const
{
    // really rough -- doesn't decrease size with height
    return (p[2] < (getPosition()[2] + getHeight()))
           &&     ((p[2]+height) >= getPosition()[2])
           &&     testRectCircle(getPosition(), getRotation(), getWidth(), getBreadth(), p, radius);
}

bool            PyramidBuilding::inBox(const glm::vec3 &p, float a,
                                       float dx, float dy, float height) const
{
    // Tank is below pyramid ?
    if (p[2] + height < getPosition()[2])
        return false;
    // Tank is above pyramid ?
    if (p[2] >= getPosition()[2] + getHeight())
        return false;
    // Could be inside. Then check collision with the rectangle at object height
    // This is a rectangle reduced by shrinking but pass the height that we are
    // not so sure where collision can be
    const float s = shrinkFactor(p[2], height);
    return testRectRect(getPosition(), getRotation(),
                        s * getWidth(), s * getBreadth(), p, a, dx, dy);
}

bool PyramidBuilding::inMovingBox(const glm::vec3 &, float,
                                  const glm::vec3 &p, float _angle,
                                  float dx, float dy, float dz) const
{
    return inBox (p, _angle, dx, dy, dz);
}

bool PyramidBuilding::isCrossing(const glm::vec3 &p, float a,
                                 float dx, float dy, float height,
                                 glm::vec4 *plane) const
{
    // if not inside or contained then not crossing
    if (!inBox(p, a, dx, dy, height) ||
            testRectInRect(getPosition(), getRotation(),
                           getWidth(), getBreadth(), p, a, dx, dy))
        return false;
    if (!plane) return true;

    // it's crossing -- choose which wall is being crossed (this
    // is a guestimate, should really do a careful test).  just
    // see which wall the point is closest to.
    const auto &p2 = getPosition();
    const float a2 = getRotation();
    const float c = cosf(-a2), s = sinf(-a2);
    const float x = c * (p[0] - p2[0]) - s * (p[1] - p2[1]);
    const float y = c * (p[1] - p2[1]) + s * (p[0] - p2[0]);
    float pw[2];
    if (fabsf(fabsf(x) - getWidth()) < fabsf(fabsf(y) - getBreadth()))
    {
        plane->x = ((x < 0.0) ? -cosf(a2) : cosf(a2));
        plane->y = ((x < 0.0) ? -sinf(a2) : sinf(a2));
        pw[0] = p2[0] + getWidth() * plane->x;
        pw[1] = p2[1] + getWidth() * plane->y;
    }
    else
    {
        plane->x = ((y < 0.0) ? sinf(a2) : -sinf(a2));
        plane->y = ((y < 0.0) ? -cosf(a2) : cosf(a2));
        pw[0] = p2[0] + getBreadth() * plane->x;
        pw[1] = p2[1] + getBreadth() * plane->y;
    }

    // now finish off plane equation (FIXME -- assumes a square base)
    const float h = 1.0f / hypotf(getHeight(), getWidth());
    plane->x *= h * getHeight();
    plane->y *= h * getHeight();
    plane->z = h * getWidth();
    plane->w = -(plane->x * pw[0] + plane->y * pw[1]);
    return true;
}

bool            PyramidBuilding::getHitNormal(
    const glm::vec3 &pos1, float,
    const glm::vec3 &pos2, float,
    float, float, float height,
    glm::vec3 &normal) const
{
    // pyramids height and flipping
    // normalize height sign and report that in flip
    float oHeight = getHeight();
    bool  flip    = getZFlip();
    if (oHeight < 0)
    {
        flip    = !flip;
        oHeight = -oHeight;
    }

    // get Bottom and Top of building
    float oBottom = getPosition()[2];
    float oTop    = oBottom + oHeight;

    // get higher and lower point of base of colliding object
    float objHigh = pos1[2];
    float objLow  = pos2[2];
    if (objHigh < objLow)
    {
        float temp = objHigh;
        objHigh    = objLow;
        objLow     = temp;
    }

    normal[0] = normal[1] = 0;
    if (flip && objHigh >= oTop)
    {
        // base of higher object is over the plateau
        normal = glm::vec3(0.0f, 0.0f, 1.0f);
        return true;
    }
    else if (!flip && objLow + height < oBottom)
    {
        // top of lower object is below the base
        normal = glm::vec3(0.0f, 0.0f, -1.0f);
        return true;
    }

    // get normal in z = const plane
    const float s = shrinkFactor(pos1[2], height);

    getNormalRect(pos1, getPosition(), getRotation(),
                  s * getWidth(), s * getBreadth(), normal);

    // now angle it due to slope of wall
    // FIXME -- this assumes the pyramid has a square base!
    const float h = 1.0f / hypotf(oHeight, getWidth());
    normal[0] *= h * oHeight;
    normal[1] *= h * oHeight;
    normal[2]  = h * getWidth();

    if (flip)
        normal[2] = -normal[2];
    return true;
}

void            PyramidBuilding::getCorner(int index,
        glm::vec3 &_pos) const
{
    const auto &base = getPosition();
    const float c = cosf(getRotation());
    const float s = sinf(getRotation());
    const float w = getWidth();
    const float h = getBreadth();
    const float top  = getHeight() + base[2];
    switch (index)
    {
    case 0:
        _pos[0] = base[0] + c * w - s * h;
        _pos[1] = base[1] + s * w + c * h;
        if (getZFlip())
            _pos[2] = top;
        else
            _pos[2] = base[2];
        break;
    case 1:
        _pos[0] = base[0] - c * w - s * h;
        _pos[1] = base[1] - s * w + c * h;
        if (getZFlip())
            _pos[2] = top;
        else
            _pos[2] = base[2];
        break;
    case 2:
        _pos[0] = base[0] - c * w + s * h;
        _pos[1] = base[1] - s * w - c * h;
        if (getZFlip())
            _pos[2] = top;
        else
            _pos[2] = base[2];
        break;
    case 3:
        _pos[0] = base[0] + c * w + s * h;
        _pos[1] = base[1] + s * w - c * h;
        if (getZFlip())
            _pos[2] = top;
        else
            _pos[2] = base[2];
        break;
    case 4:
        _pos[0] = base[0];
        _pos[1] = base[1];
        if (getZFlip())
            _pos[2] = base[2];
        else
            _pos[2] = top;
        break;
    }
}

float           PyramidBuilding::shrinkFactor(float z,
        float height) const
{
    float shrink;

    // Normalize Height and flip to have height > 0
    float oHeight = getHeight();
    bool  flip    = getZFlip();
    if (oHeight < 0)
    {
        flip    = !flip;
        oHeight = - oHeight;
    }

// Remove heights bias
    const auto &_pos = getPosition();
    z -= _pos[2];
    if (oHeight <= ZERO_TOLERANCE)
        shrink = 1.0f;
    else
    {
        // Normalize heights
        z /= oHeight;

        // if flipped the bigger intersection is at top of the object
        if (flip)
        {
            // Normalize the object height, we have not done yet
            z += height / oHeight;
        }

        // shrink is that
        if (flip)
            shrink = z;
        else
            shrink = 1.0f - z;
    }

    // clamp in 0 .. 1
    if (shrink < 0.0)
        shrink = 0.0;
    else if (shrink > 1.0)
        shrink = 1.0;

    return shrink;
}

bool            PyramidBuilding::isFlatTop() const
{
    return getZFlip();
}



void* PyramidBuilding::pack(void* buf) const
{
    buf = nboPackVector(buf, pos);
    buf = nboPackFloat(buf, angle);
    buf = nboPackVector(buf, size);

    unsigned char stateByte = 0;
    stateByte |= isDriveThrough() ? _DRIVE_THRU : 0;
    stateByte |= isShootThrough() ? _SHOOT_THRU : 0;
    stateByte |= getZFlip() ? _FLIP_Z : 0;
    stateByte |= canRicochet() ? _RICOCHET : 0;
    buf = nboPackUByte(buf, stateByte);

    return buf;
}


const void* PyramidBuilding::unpack(const void* buf)
{
    buf = nboUnpackVector(buf, pos);
    buf = nboUnpackFloat(buf, angle);
    buf = nboUnpackVector(buf, size);

    unsigned char stateByte;
    buf = nboUnpackUByte(buf, stateByte);
    driveThrough = (stateByte & _DRIVE_THRU) != 0;
    shootThrough = (stateByte & _SHOOT_THRU) != 0;
    ricochet     = (stateByte & _RICOCHET)   != 0;
    ZFlip = (stateByte & _FLIP_Z) != 0;

    finalize();

    return buf;
}


int PyramidBuilding::packSize() const
{
    int fullSize = 0;
    fullSize += sizeof(float[3]); // pos
    fullSize += sizeof(float[3]); // size
    fullSize += sizeof(float);    // rotation
    fullSize += sizeof(uint8_t);  // state bits
    return fullSize;
}


void PyramidBuilding::print(std::ostream& out, const std::string& indent) const
{
    out << indent << "pyramid" << std::endl;
    const auto &_pos = getPosition();
    out << indent << "  position " << _pos[0] << " " << _pos[1] << " "
        << _pos[2] << std::endl;
    out << indent << "  size " << getWidth() << " " << getBreadth()
        << " " << getHeight() << std::endl;
    out << indent << "  rotation " << ((getRotation() * 180.0) / M_PI)
        << std::endl;
    if (getZFlip())
        out << indent << "  flipz" << std::endl;

    if (isPassable())
        out << indent << "  passable" << std::endl;
    else
    {
        if (isDriveThrough())
            out << indent << "  drivethrough" << std::endl;
        if (isShootThrough())
            out << indent << "  shootthrough" << std::endl;
    }
    if (canRicochet())
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

void PyramidBuilding::printOBJ(std::ostream& out, const std::string& UNUSED(indent)) const
{
    int i;
    glm::vec3 verts[5] =
    {
        {-1.0f, -1.0f, 0.0f},
        {+1.0f, -1.0f, 0.0f},
        {+1.0f, +1.0f, 0.0f},
        {-1.0f, +1.0f, 0.0f},
        { 0.0f,  0.0f, 1.0f}
    };
    const float sqrt1_2 = (float)M_SQRT1_2;
    glm::vec3 norms[5] =
    {
        {0.0f, -sqrt1_2, +sqrt1_2}, {+sqrt1_2, 0.0f, +sqrt1_2},
        {0.0f, +sqrt1_2, +sqrt1_2}, {-sqrt1_2, 0.0f, +sqrt1_2},
        {0.0f, 0.0f, -1.0f}
    };
    const float k = 1.0f / 8.0f;
    const auto  s = k * getSize();
    float txcds[7][2] =
    {
        {0.0f, 0.0f}, {s[0], 0.0f}, {s[0], s[1]}, {0.0f, s[1]},
        {0.5f*s[0], sqrtf(s[0]*s[0]+s[2]*s[2])},
        {s[1], 0.0f}, {0.5f*s[1], sqrtf(s[1]*s[1]+s[2]*s[2])}
    };
    MeshTransform xform;
    const float degrees = getRotation() * (float)(180.0 / M_PI);
    const auto zAxis = glm::vec3(0.0f, 0.0f, +1.0f);
    if (getZFlip())
    {
        const auto xAxis = glm::vec3(1.0f, 0.0f, 0.0f);
        xform.addSpin(180.0f, xAxis);
        xform.addShift(zAxis);
    }
    xform.addScale(getSize());
    xform.addSpin(degrees, zAxis);
    xform.addShift(getPosition());
    xform.finalize();
    MeshTransform::Tool xtool(xform);
    for (i = 0; i < 5; i++)
        xtool.modifyVertex(verts[i]);
    for (i = 0; i < 5; i++)
        xtool.modifyNormal(norms[i]);

    out << "# OBJ - start pyramid" << std::endl;
    out << "o bzpyr_" << getObjCounter() << std::endl;

    for (i = 0; i < 5; i++)
    {
        out << "v";
        outputFloat(out, verts[i][0]);
        outputFloat(out, verts[i][1]);
        outputFloat(out, verts[i][2]);
        out << std::endl;
    }
    for (i = 0; i < 7; i++)
    {
        out << "vt";
        outputFloat(out, txcds[i][0]);
        outputFloat(out, txcds[i][1]);
        out << std::endl;
    }
    for (i = 0; i < 5; i++)
    {
        out << "vn";
        outputFloat(out, norms[i][0]);
        outputFloat(out, norms[i][1]);
        outputFloat(out, norms[i][2]);
        out << std::endl;
    }
    out << "usemtl pyrwall" << std::endl;
    out << "f -1/-1/-5 -5/-7/-5 -4/-6/-5" << std::endl;
    out << "f -1/-3/-4 -4/-7/-4 -3/-2/-4" << std::endl;
    out << "f -1/-1/-3 -3/-7/-3 -2/-6/-3" << std::endl;
    out << "f -1/-3/-2 -2/-7/-2 -5/-2/-2" << std::endl;
    out << "f -2/-7/-1 -3/-6/-1 -4/-5/-1 -5/-4/-1" << std::endl;

    out << std::endl;

    incObjCounter();

    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
