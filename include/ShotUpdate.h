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
 * ShotUpdate:
 *  Encapsulates info needed to update a shot on remote
 *  hosts. Can be packed for transmission on the net.
 *
 * FiringInfo:
 *  Encapsulates info needed to create RemoteShotPath.
 *  Can be packed for transmission on the net.
 */

#ifndef BZF_SHOT_UPDATE_H
#define BZF_SHOT_UPDATE_H

// 1st
#include "common.h"

// system headers
#include <math.h>
#include <glm/vec3.hpp>

// local implementation headers
#include "Address.h"
#include "Flag.h"

const int       ShotUpdatePLen = PlayerIdPLen + 32;
const int       FiringInfoPLen = ShotUpdatePLen + 10;

class BaseLocalPlayer;

struct ShotUpdate
{
public:
    void*       pack(void*) const;
    const void*     unpack(const void*);

public:
    PlayerId        player;         // who's shot
    uint16_t        id;         // shot id unique to player
    glm::vec3   pos;         // shot position
    glm::vec3   vel;         // shot velocity
    float       dt;         // time shot has existed
    TeamColor       team;
};

struct FiringInfo
{
public:
    FiringInfo();
    FiringInfo(const BaseLocalPlayer&, int id);

    void*       pack(void*) const;
    const void*     unpack(const void*);

public:
    float          timeSent;
    ShotUpdate      shot;
    FlagType*       flagType;           // flag when fired
    float       lifetime;       // lifetime of shot (s)
};

#endif // BZF_SHOT_UPDATE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
