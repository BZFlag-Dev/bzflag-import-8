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

/* the common header */
#include "common.h"

/* interface header */
#include "CustomZone.h"

/* system headers */
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sstream>
#include <math.h>

/* local implementation headers */
//#include "EntryZones.h"
#include "WorldInfo.h"
#include "Flag.h"
#include "Team.h"


CustomZone::CustomZone()
{
    pos[0] = pos[1] = pos[2] = 0.0f;
    rotation = 0.0f;
    size[0] = size[1] = size[2] = 1.0f;
}


// make a safety zone for all team flags
void CustomZone::addFlagSafety(float x, float y, WorldInfo* worldInfo)
{
    pos[0] = x;
    pos[1] = y;
    pos[2] = 0.0f;
    size[0] = 0.0f;
    size[1] = 0.0f;
    size[2] = 0.0f;
    rotation = 0.0f;

    // add the qualifiers
    for (int team = 0; team < CtfTeams; team++)
    {
        std::string qual = getFlagSafetyQualifier(team);
        if (qual.size() > 0)
            qualifiers.push_back(qual);
    }

    worldInfo->addZone(this);

    return;
}


void CustomZone::addZoneFlagCount(FlagType* flagType, int count)
{
    ZoneFlagMap::iterator it = zoneFlagMap.find(flagType);
    if (it != zoneFlagMap.end())
        count += it->second;
    if (count < 0)
        count = 0;
    zoneFlagMap[flagType] = count;
    return;
}


bool CustomZone::read(const char *cmd, std::istream& input)
{
    if (strcmp(cmd, "flag") == 0)
    {
        std::string args, flag;

        std::getline(input, args);
        std::istringstream parms(args);

        while (parms >> flag)
        {
            if (flag == "good")
            {
                FlagSet &fs = Flag::getGoodFlags();
                for (FlagSet::iterator it = fs.begin(); it != fs.end(); ++it)
                {
                    FlagType *f = *it;
                    if (f->endurance != FlagNormal)   // Null and Team flags
                    {
                        const std::string& qual = getFlagTypeQualifier(f);
                        if (qual.size() > 0)
                            qualifiers.push_back(qual);
                    }
                }
            }
            else if (flag == "bad")
            {
                FlagSet &fs = Flag::getBadFlags();
                for (FlagSet::iterator it = fs.begin(); it != fs.end(); ++it)
                {
                    FlagType *f = *it;
                    if (f->endurance != FlagNormal)   // Null and Team flags
                    {
                        const std::string& qual = getFlagTypeQualifier(f);
                        if (qual.size() > 0)
                            qualifiers.push_back(qual);
                    }
                }
            }
            else
            {
                FlagType* f = Flag::getDescFromAbbreviation(flag.c_str());
                if (f == Flags::Null)
                {
                    logDebugMessage(1,"WARNING: bad flag type: %s\n", flag.c_str());
                    input.putback('\n');
                    return false;
                }
                if (f->endurance == FlagNormal)
                {
                    logDebugMessage(1,"WARNING: you probably want a safety: %s\n", flag.c_str());
                    input.putback('\n');
                    return false;
                }
                const std::string& qual = getFlagTypeQualifier(f);
                if (qual.size() > 0)
                    qualifiers.push_back(qual);
            }
        }

        input.putback('\n');
        if (qualifiers.empty())
            return false;
    }
    else if (strcmp(cmd, "zoneflag") == 0)
    {
        std::string args, flag;
        std::getline(input, args);
        std::istringstream parms(args);
        int count;

        if (!(parms >> flag))
            return false;
        if (!(parms >> count))
            count = 1;

        if (flag == "good")
        {
            FlagSet &fs = Flag::getGoodFlags();
            for (FlagSet::iterator it = fs.begin(); it != fs.end(); ++it)
            {
                FlagType *f = *it;
                if (f->endurance != FlagNormal)   // Null and Team flags
                    addZoneFlagCount(f, count);
            }
        }
        else if (flag == "bad")
        {
            FlagSet &fs = Flag::getBadFlags();
            for (FlagSet::iterator it = fs.begin(); it != fs.end(); ++it)
            {
                FlagType *f = *it;
                if (f->endurance != FlagNormal)   // Null and Team flags
                    addZoneFlagCount(f, count);
            }
        }
        else
        {
            FlagType *f = Flag::getDescFromAbbreviation(flag.c_str());
            if (f != Flags::Null)
                addZoneFlagCount(f, count);
            else
            {
                logDebugMessage(1,"WARNING: bad zoneflag type: %s\n", flag.c_str());
                input.putback('\n');
                return false;
            }
        }
        input.putback('\n');
    }
    else if ((strcmp(cmd, "team") == 0) || (strcmp(cmd, "safety") == 0))
    {
        std::string args;
        std::getline(input, args);
        std::istringstream  parms(args);

        int color;
        const bool safety = (strcmp(cmd, "safety") == 0);

        while (parms >> color)
        {
            if ((color < 0) || (color >= CtfTeams))
            {
                input.putback('\n');
                return false;
            }
            std::string qual;
            if (safety)
                qual = getFlagSafetyQualifier(color);
            else
                qual = getPlayerTeamQualifier(color);
            if (qual.size() > 0)
                qualifiers.push_back(qual);
        }
        input.putback('\n');
        if (qualifiers.empty())
            return false;
    }
    else if (!WorldFileLocation::read(cmd, input))
        return false;

    return true;
}


void CustomZone::writeToWorld(WorldInfo* worldInfo) const
{
    worldInfo->addZone( this );
}

void CustomZone::getRandomPoint(glm::vec3 &pt) const
{
    float x = (float)((bzfrand() * (2.0f * size[0])) - size[0]);
    float y = (float)((bzfrand() * (2.0f * size[1])) - size[1]);

    const float cos_val = cosf(rotation);
    const float sin_val = sinf(rotation);
    pt[0] = (x * cos_val) - (y * sin_val);
    pt[1] = (x * sin_val) + (y * cos_val);

    pt[0] += pos[0];
    pt[1] += pos[1];
    pt[2] = pos[2];
}

float CustomZone::getDistToPoint (const glm::vec3 &_pos) const
{
    // FIXME - should use proper minimum distance from
    // the zone edge, and maybe -1.0f if its inside the zone
    const float dist = glm::distance(_pos, pos);

    return dist;
}


static std::string makeIntQualifier(char prefix, int value)
{
    static std::string qual;
    char buffer[16];
    snprintf(buffer, 16, "%c%i", prefix, value);
    qual = buffer;
    return qual;
}

static int checkIntQualifier(char prefix, const std::string& qual)
{
    if (qual[0] == prefix)
    {
        const char* start = qual.c_str() + 1;
        char* end;
        int id = strtol(start, &end, 10);
        if (end != start)
            return id;
    }
    return -1;
}


const std::string& CustomZone::getFlagIdQualifier(int flagId)
{
    static std::string qual;
    qual = makeIntQualifier('#', flagId);
    return qual;
}

int CustomZone::getFlagIdFromQualifier(const std::string& qual)
{
    return checkIntQualifier('#', qual);
}


const std::string& CustomZone::getFlagTypeQualifier(FlagType* flagType)
{
    static std::string qual;
    if (flagType != NULL)
    {
        qual = "f";
        qual += flagType->flagAbbv;
    }
    else
        qual = "";
    return qual;
}

FlagType* CustomZone::getFlagTypeFromQualifier(const std::string& qual)
{
    if (qual[0] == 'f')
        return Flag::getDescFromAbbreviation(qual.c_str() + 1);
    else
        return Flags::Null;
}


const std::string& CustomZone::getFlagSafetyQualifier(int team)
{
    static std::string qual;
    if ((team > 0) && (team < CtfTeams))
        qual = makeIntQualifier('$', team);
    else
        qual = "";
    return qual;
}

int CustomZone::getFlagSafetyFromQualifier(const std::string& qual)
{
    return checkIntQualifier('$', qual);
}


const std::string& CustomZone::getPlayerTeamQualifier(int team)
{
    static std::string qual;
    if ((team >= 0) && (team < CtfTeams))
        qual = makeIntQualifier('t', team);
    else
        qual = "";
    return qual;
}

int CustomZone::getPlayerTeamFromQualifier(const std::string& qual)
{
    return checkIntQualifier('t', qual);
}


// Local variables: ***
// mode: C++ ***
// tab-width: 4***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
