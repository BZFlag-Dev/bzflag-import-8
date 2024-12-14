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

/*
 * Team:
 *   Encapsulates information about a team
 */

#ifndef BZF_TEAM_H
#define BZF_TEAM_H

// 1st
#include "common.h"

// System headers
#include <string>
#include <glm/fwd.hpp>

// Common headers
#include "global.h"

const int       TeamPLen = 10;

struct Team
{
public:
    Team();

    void*       pack(void*) const;
    const void*     unpack(const void*);

    static const std::string    getImagePrefix(TeamColor); // const
    static const char*      getName(TeamColor); // const
    static TeamColor        getTeam(const std::string &name); // const
    static const glm::vec3 &getTankColor(TeamColor); // const
    static const glm::vec3 &getRadarColor(TeamColor team); // const
    static const glm::vec3 &getShotColor(TeamColor team); // const
    static const std::string    getAnsiCode(TeamColor team); // const
    static bool         isColorTeam(TeamColor); // const

    static void     setColors(TeamColor,
                              const glm::vec3 &tank,
                              const glm::vec3 &radar);
    static void     updateShotColors();

public:
    unsigned short  size;           // num players on team

    short       getWins() const
    {
        return won;
    }
    short       getLosses() const
    {
        return lost;
    }
    void        setWins(short v)
    {
        won = v;
    }
    void        setLosses(short v)
    {
        lost = v;
    }

    static glm::vec3 tankColor[NumTeams];
    static glm::vec3 radarColor[NumTeams];
    static glm::vec3 shotColor[NumTeams];

private:
    unsigned short  won;            // wins by team members
    unsigned short  lost;           // losses by team members

    static float    addBrightness(const float color); // const

};


#endif // BZF_TEAM_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
