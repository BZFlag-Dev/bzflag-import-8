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

#ifndef __TEAMBASES_H__
#define __TEAMBASES_H__

// 1st
#include "common.h"

/* system interface headers */
#include <vector>
#include <map>
#include <glm/vec3.hpp>

/* common interface headers */
#include "global.h"


class TeamBase
{
    // This class represents one base
public:
    TeamBase(): position(), size(), rotation(0.0) {}
    TeamBase(const glm::vec3 &pos, const glm::vec3 &siz, float rot);
    void getRandomPosition( float &x, float &y, float &z ) const;
    glm::vec3 position;
    glm::vec3 size;
    float rotation;
};


class TeamBases
{
    // This class represents all the bases for one team
public:

    TeamBases();
    TeamBases(TeamColor team, bool initDefault = false);
    void addBase(const glm::vec3 &position,
                 const glm::vec3 &size,
                 float rotation);
    int size() const;
    TeamColor getTeam() const;
    const glm::vec3 &getBasePosition(int base) const;
    float findBaseZ( float x, float y, float z ) const;
    const TeamBase& getRandomBase( int id );

private:
    typedef std::vector<TeamBase> TeamBaseList;

    TeamBaseList teamBases;
    TeamColor    color;
};

typedef std::map<int, TeamBases> BasesList;


#endif /* __TEAMBASES_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
