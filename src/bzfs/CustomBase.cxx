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

// interface header
#include "CustomBase.h"

// system includes
#include <string.h>

// common implementation headers
#include "global.h" // for CtfTeams
#include "BaseBuilding.h"
#include "ObstacleMgr.h"


CustomBase::CustomBase()
{
    pos[0] = pos[1] = pos[2] = 0.0f;
    rotation = 0.0f;
    size[0] = size[1] = BZDB.eval(StateDatabase::BZDB_BASESIZE);
    color = 0;

    triggerWorldWep = false;
    worldWepType = "SW";
}


bool CustomBase::read(const char *cmd, std::istream& input)
{
    if (strcmp(cmd, "color") == 0)
    {
        input >> color;
        if ((color <= 0) || (color >= CtfTeams))
            return false;
    }
    else if (strcmp(cmd, "oncap") == 0)
    {
        triggerWorldWep = true;
        input >> worldWepType;
    }
    else
    {
        if (!WorldFileObstacle::read(cmd, input))
            return false;
    }
    return true;
}


void CustomBase::writeToGroupDef(GroupDefinition *groupdef)
{
    auto absSize = glm::abs(size);
    BaseBuilding* base = new BaseBuilding(pos, rotation, absSize, color, ricochet);
    groupdef->addObstacle(base);

    if (triggerWorldWep)
        worldEventManager.addEvent(bz_eCaptureEvent,
                                   new WorldWeaponGlobalEventHandler(Flag::getDescFromAbbreviation(worldWepType.c_str()), pos, rotation, 0,
                                           (TeamColor)color));
}

// Local variables: ***
// mode: C++ ***
// tab-width: 4***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
