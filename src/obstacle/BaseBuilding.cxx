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
#include "BaseBuilding.h"

// System headers
#include <math.h>
#include <glm/gtc/type_ptr.hpp>

// Common headers
#include "global.h"
#include "Pack.h"
#include "Intersect.h"
#include "MeshTransform.h"


const char*     BaseBuilding::typeName = "BaseBuilding";

BaseBuilding::BaseBuilding()
{
}

BaseBuilding::BaseBuilding(const glm::vec3 &p, float rotation,
                           const glm::vec3 &_size, int _team, bool rico) :
    BoxBuilding(p, rotation, _size[0], _size[1], _size[2], false, false, rico),
    team(_team)
{
}

BaseBuilding::~BaseBuilding()
{
    // do nothing
}

Obstacle* BaseBuilding::copyWithTransform(const MeshTransform& xform) const
{
    auto newPos = pos;
    float newAngle;
    auto newSize = size;
    newAngle = angle;

    MeshTransform::Tool tool(xform);
    bool flipped;
    tool.modifyOldStyle(newPos, newSize, newAngle, flipped);

    BaseBuilding* copy = new BaseBuilding(newPos, newAngle, newSize, team, ricochet);

    return copy;
}

const char*     BaseBuilding::getType() const
{
    return typeName;
}

const char*     BaseBuilding::getClassName()
{
    return typeName;
}

bool            BaseBuilding::inCylinder(const glm::vec3 &p, float radius, float height) const
{
    return (p[2] < (getPosition()[2] + getHeight()))
           &&     ((p[2]+height) > getPosition()[2])
           &&     testRectCircle(getPosition(), getRotation(), getWidth(), getBreadth(), p, radius);
}

bool BaseBuilding::inMovingBox(const glm::vec3 &oldP, float,
                               const glm::vec3 &p, float _angle,
                               float dx, float dy, float height) const
{
    // if a base is just the ground (z == 0 && height == 0) no collision
    // ground is already handled
    if (!getPosition()[2] && !getHeight())
        return false;

    return BoxBuilding::inMovingBox(oldP, 0.0f, p, _angle, dx, dy, height);
}

bool BaseBuilding::isCrossing(const glm::vec3 &p, float _angle,
                              float dx, float dy, float height,
                              glm::vec4 *plane) const
{
    // if not inside or contained, then not crossing
    if (!inBox(p, _angle, dx, dy, height) ||
            testRectInRect(getPosition(), getRotation(),
                           getWidth(), getBreadth(), p, _angle, dx, dy))
        return false;
    if (!plane) return true;

    // it's crossing -- choose which wall is being crossed (this
    // is a guestimate, should really do a careful test). Just
    // see which wall the point is closest to
    const auto &p2 = getPosition();
    const float a2  = getRotation();
    const float c   = cosf(-a2), s = sinf(-a2);
    const float x   = c * (p[0] - p2[0]) - s * (p[1] - p2[1]);
    const float y   = c * (p[1] - p2[1]) - s * (p[0] - p2[0]);
    auto pw = p2;
    if (fabsf(fabsf(x) - getWidth()) < fabsf(fabsf(y) - getBreadth()))
    {
        plane->x = ((x < 0.0) ? -cosf(a2) : cosf(a2));
        plane->y = ((x < 0.0) ? -sinf(a2) : sinf(a2));
        pw[0] += getWidth() * plane->x;
        pw[1] += getWidth() * plane->y;
    }
    else
    {
        plane->x = ((y < 0.0) ? sinf(a2) : -sinf(a2));
        plane->y = ((y < 0.0) ? cosf(a2) : -cosf(a2));
        pw[0] += getBreadth() * plane->x;
        pw[1] += getBreadth() * plane->y;
    }

    // now finish off plane equation
    plane->z = 0.0;
    plane->w = -(plane->x * pw[0] + plane->y * pw[1]);
    return true;
}

int BaseBuilding::getTeam() const
{
    return team;
}

void* BaseBuilding::pack(void* buf) const
{
    buf = nboPackUShort(buf, (uint16_t) team);

    buf = BoxBuilding::pack(buf);

    return buf;
}


const void* BaseBuilding::unpack(const void* buf)
{
    uint16_t shortTeam;
    buf = nboUnpackUShort(buf, shortTeam);
    team = (int)shortTeam;

    buf = BoxBuilding::unpack(buf);

    return buf;
}


int BaseBuilding::packSize() const
{
    int fullSize = 0;
    fullSize += sizeof(uint16_t); // team
    fullSize += BoxBuilding::packSize();
    return fullSize;
}


void BaseBuilding::print(std::ostream& out, const std::string& indent) const
{
    out << indent << "base" << std::endl;
    const auto &myPos = getPosition();
    out << indent << "  position " << myPos[0] << " " << myPos[1] << " "
        << myPos[2] << std::endl;
    out << indent << "  size " << getWidth() << " " << getBreadth()
        << " " << getHeight() << std::endl;
    out << indent << "  rotation " << ((getRotation() * 180.0) / M_PI)
        << std::endl;
    out << indent << "  color " << getTeam() << std::endl;
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

void BaseBuilding::printOBJ(std::ostream& out, const std::string& UNUSED(indent)) const
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
    const glm::vec3 zAxis = glm::vec3(0.0f, 0.0f, +1.0f);
    xform.addScale(getSize());
    xform.addSpin(degrees, zAxis);
    xform.addShift(getPosition());
    xform.finalize();
    MeshTransform::Tool xtool(xform);
    for (i = 0; i < 8; i++)
        xtool.modifyVertex(verts[i]);
    for (i = 0; i < 6; i++)
        xtool.modifyNormal(norms[i]);

    out << "# OBJ - start base" << std::endl;
    out << "o bzbase_team" << team << "_" << getObjCounter() << std::endl;

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
    out << "usemtl basetop_team" << team << std::endl;
    out << "f -5/-4/-2 -6/-3/-2 -7/-2/-2 -8/-1/-2" << std::endl;
    out << "f -4/-4/-1 -3/-3/-1 -2/-2/-1 -1/-1/-1" << std::endl;
    out << "usemtl basewall_team" << team << std::endl;
    out << "f -8/-4/-6 -7/-3/-6 -3/-2/-6 -4/-1/-6" << std::endl;
    out << "f -7/-4/-5 -6/-3/-5 -2/-2/-5 -3/-1/-5" << std::endl;
    out << "f -6/-4/-4 -5/-3/-4 -1/-2/-4 -2/-1/-4" << std::endl;
    out << "f -5/-4/-3 -8/-3/-3 -4/-2/-3 -1/-1/-3" << std::endl;

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
