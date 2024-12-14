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
#include "callbacks.h"

/* local headers */
#include "LocalPlayer.h"
#include "HUDRenderer.h"
#include "ParseColor.h"
#include "Team.h"
#include "playing.h"

static void     setTeamColor(TeamColor team, const std::string& str)
{
    glm::vec4 color;
    parseColorString(str, color);
    // don't worry about alpha, Team::setColors() doesn't use it
    Team::setColors(team, color, Team::getRadarColor(team));
}

static void     setRadarColor(TeamColor team, const std::string& str)
{
    glm::vec4 color;
    parseColorString(str, color);
    // don't worry about alpha, Team::setColors() doesn't use it
    Team::setColors(team, Team::getTankColor(team), color);
}

void setFlagHelp(const std::string& name, void*)
{
    if (LocalPlayer::getMyTank() == NULL)
        return;
    static const float FlagHelpDuration = 60.0f;
    if (BZDB.isTrue(name))
        hud->setFlagHelp(LocalPlayer::getMyTank()->getFlag(), FlagHelpDuration);
    else
        hud->setFlagHelp(Flags::Null, 0.0);
}

void setColor(const std::string& name, void*)
{
    if (name == "roguecolor")
        setTeamColor(RogueTeam, BZDB.get(name));
    else if (name == "redcolor")
        setTeamColor(RedTeam, BZDB.get(name));
    else if (name == "greencolor")
        setTeamColor(GreenTeam, BZDB.get(name));
    else if (name == "bluecolor")
        setTeamColor(BlueTeam, BZDB.get(name));
    else if (name == "purplecolor")
        setTeamColor(PurpleTeam, BZDB.get(name));
    else if (name == "observercolor")
        setTeamColor(ObserverTeam, BZDB.get(name));
    else if (name == "rabbitcolor")
        setTeamColor(RabbitTeam, BZDB.get(name));
    else if (name == "huntercolor")
        setTeamColor(HunterTeam, BZDB.get(name));
    else if (name == "rogueradar")
        setRadarColor(RogueTeam, BZDB.get(name));
    else if (name == "redradar")
        setRadarColor(RedTeam, BZDB.get(name));
    else if (name == "greenradar")
        setRadarColor(GreenTeam, BZDB.get(name));
    else if (name == "blueradar")
        setRadarColor(BlueTeam, BZDB.get(name));
    else if (name == "purpleradar")
        setRadarColor(PurpleTeam, BZDB.get(name));
    else if (name == "observerradar")
        setRadarColor(ObserverTeam, BZDB.get(name));
    else if (name == "rabbitradar")
        setRadarColor(RabbitTeam, BZDB.get(name));
    else if (name == "hunterradar")
        setRadarColor(HunterTeam, BZDB.get(name));
    else
        Team::updateShotColors();
}

void setDepthBuffer(const std::string& name, void*)
{
    /* if zbuffer was set and not available, unset it */
    if (BZDB.isTrue(name))
    {
        GLint value;
        glGetIntegerv(GL_DEPTH_BITS, &value);
        if (value == 0)
        {
            // temporarily remove ourself
            BZDB.removeCallback(name, setDepthBuffer, NULL);
            BZDB.set(name, "0");
            // add it again
            BZDB.addCallback(name, setDepthBuffer, NULL);
        }
    }
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
