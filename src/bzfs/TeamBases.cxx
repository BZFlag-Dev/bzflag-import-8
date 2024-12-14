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
#include "TeamBases.h"

// System headers
#include <string.h>
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>

// Common headers
#include "Protocol.h"
#include "Pack.h"
#include "BZDBCache.h"

TeamBases::TeamBases()
{
    color = RedTeam;
}

TeamBases::TeamBases(TeamColor team, bool initDefault)
{
    color = team;

    if (!initDefault)
        return;

    float worldSize = BZDBCache::worldSize;
    float baseSize = BZDB.eval(StateDatabase::BZDB_BASESIZE);

    teamBases.resize(1);
    TeamBase &teamBase = teamBases[0];
    switch (team)
    {
    case RedTeam:
        teamBase.position[0] = (-worldSize + baseSize) / 2.0f;
        teamBase.position[1] = 0.0f;
        break;

    case GreenTeam:
        teamBase.position[0] = (worldSize - baseSize) / 2.0f;
        teamBase.position[1] = 0.0f;
        break;

    case BlueTeam:
        teamBase.position[0] = 0.0f;
        teamBase.position[1] = (-worldSize + baseSize) / 2.0f;
        break;

    case PurpleTeam:
        teamBase.position[0] = 0.0f;
        teamBase.position[1] = (worldSize - baseSize) / 2.0f;
        break;

    default:
        // no valid team, should throw here if we could
        break;
    }

    teamBase.position[2] = 0.0f;
    teamBase.rotation = 0.0f;
    teamBase.size[0] = baseSize / 2.0f;
    teamBase.size[1] = baseSize / 2.0f;
    teamBase.size[2] = 0.0f;
}

void TeamBases::addBase(const glm::vec3 &position, const glm::vec3 &_size,
                        float rotation )
{
    TeamBase base(position, _size, rotation);
    teamBases.push_back(base);
}

int TeamBases::size() const
{
    return teamBases.size();
}

TeamColor TeamBases::getTeam() const
{
    return color;
}

const glm::vec3 &TeamBases::getBasePosition(int base) const
{
    if ((base < 0) || (base >= (int)teamBases.size()))
        base = 0;

    return teamBases[base].position;
}

float TeamBases::findBaseZ( float x, float y, float z ) const
{
    for (auto &teamBase : teamBases)
    {
        const auto &pos = teamBase.position;
        if (pos[2] > z)
            continue;

        const auto &_size = teamBase.size;
        float rotation    = teamBase.rotation;
        const auto n      = glm::vec2(x, y) - glm::vec2(pos);
        const float an    = atan2f(n.y, n.x) - rotation;
        const float di    = glm::length(n);
        const auto r      = glm::abs(glm::vec2(cosf(an), sinf(an))) * di;

        if (r.x < _size[0] && r.y < _size[1])
            return pos[2];
    }

    return -1.0f;
}

const TeamBase &TeamBases::getRandomBase( int id )
{
    return teamBases[id % teamBases.size()];
}

TeamBase::TeamBase(const glm::vec3 &pos, const glm::vec3 &siz, float rot)
{
    position = pos;
    size     = siz;
    rotation = rot;
}

void TeamBase::getRandomPosition( float &x, float &y, float &z ) const
{
    float deltaX = (size[0] - 2.0f * BZDBCache::tankRadius) * ((float)bzfrand() - 0.5f);
    float deltaY = (size[1] - 2.0f * BZDBCache::tankRadius) * ((float)bzfrand() - 0.5f);
    x = position[0] + deltaX * cosf(rotation) - deltaY * sinf(rotation);
    y = position[1] + deltaX * sinf(rotation) + deltaY * cosf(rotation);
    z = position[2] + size[2];
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
