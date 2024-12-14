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

/* no header other than FlagInfo.h should be included here */

/* interface header */
#include "FlagInfo.h"

/* system headers */
#include <iostream>

// implementation-specific bzflag headers
#include "BZDBCache.h"

/* private */

/* protected */

/* public */

// flags list
FlagInfo          *FlagInfo::flagList      = NULL;
std::vector<FlagType*> FlagInfo::allowedFlags;
int         FlagInfo::numExtraFlags = 0;
int         FlagInfo::numFlags      = 0;
int         FlagInfo::numFlagsInAir;

FlagInfo::FlagInfo(): numShots(0), flagIndex(0)
{
    // prep flag
    flag.type       = Flags::Null;
    flag.status         = FlagNoExist;
    flag.endurance      = FlagNormal;
    flag.owner          = NoPlayer;
    flag.position[0]    = 0.0f;
    flag.position[1]    = 0.0f;
    flag.position[2]    = 0.0f;
    flag.launchPosition[0]  = 0.0f;
    flag.launchPosition[1]  = 0.0f;
    flag.launchPosition[2]  = 0.0f;
    flag.landingPosition[0] = 0.0f;
    flag.landingPosition[1] = 0.0f;
    flag.landingPosition[2] = 0.0f;
    flag.flightTime     = 0.0f;
    flag.flightEnd      = 0.0f;
    flag.initialVelocity    = 0.0f;
    player          = -1;
    grabs           = 0;
    required        = false;
}

void FlagInfo::setSize(int _numFlags)
{
    // sanity check
    if (_numFlags > 100000)
    {
        std::cerr << "WARNING: FlagInfo::setSize was given an insane flag count of " << _numFlags << std::endl;
        std::cerr << "clamping to 100000 flags just for kicks" << std::endl;
        _numFlags = 100000;
    }

    numFlags = _numFlags;
    delete[] flagList;
    flagList = NULL;
    if (numFlags)
        flagList = new FlagInfo[numFlags];
    for (int i = 0; i < numFlags; i++)
        flagList[i].flagIndex = i;
}

void FlagInfo::setAllowed(std::vector<FlagType*> allowed)
{
    allowedFlags = allowed;
}

void FlagInfo::setExtra(int extra)
{
    numExtraFlags = extra;
}

int FlagInfo::lookupFirstTeamFlag(int teamindex)
{
    for (int i = 0; i < numFlags; i++)
    {
        if (flagList[i].flag.type->flagTeam == teamindex)
            return i;
    }
    return -1;
}

void FlagInfo::setRequiredFlag(FlagType *desc)
{
    required = true;
    flag.type = desc;
}

void FlagInfo::addFlag()
{
    const float flagAltitude = BZDB.eval(StateDatabase::BZDB_FLAGALTITUDE);
    const float gravity      = BZDBCache::gravity;

    // flag is now entering game
    numFlagsInAir++;
    flag.status     = FlagComing;

    // compute drop time
    const float flightTime = 2.0f * sqrtf(-2.0f * flagAltitude / gravity);
    flag.flightTime   = 0.0f;
    flag.flightEnd     = flightTime;
    flag.initialVelocity   = -0.5f * gravity * flightTime;
    dropDone         = TimeKeeper::getCurrent();
    dropDone        += flightTime;

    if (flag.type == Flags::Null)
        // pick a random flag
        flag.type = allowedFlags[(int)(allowedFlags.size() * (float)bzfrand())];

    // decide how sticky the flag will be
    if (flag.type->flagQuality == FlagBad)
        flag.endurance = FlagSticky;
    else
        flag.endurance = FlagUnstable;

    // how times will it stick around
    if ((flag.endurance == FlagSticky) || (flag.type == Flags::Thief))
        grabs = 1;
    else
        grabs = BZDB.evalInt(StateDatabase::BZDB_MAXFLAGGRABS);
}

void *FlagInfo::pack(void *buf, bool hide)
{
    if (FlagInfo::flagList[flagIndex].flag.type->flagTeam != ::NoTeam)
        hide = false;
    if (FlagInfo::flagList[flagIndex].player != -1)
        hide = false;
    buf = nboPackUShort(buf, flagIndex);
    if (hide)
        buf = FlagInfo::flagList[flagIndex].flag.fakePack(buf);
    else
        buf = FlagInfo::flagList[flagIndex].flag.pack(buf);
    return buf;
}

void FlagInfo::dropFlag(const glm::vec3 &pos, glm::vec3 &landingPos, bool vanish)
{
    numFlagsInAir++;
    flag.status        = vanish ? FlagGoing : FlagInAir;

    flag.landingPosition[0] = landingPos[0];
    flag.landingPosition[1] = landingPos[1];
    flag.landingPosition[2] = landingPos[2];

    flag.position[0]       = landingPos[0];
    flag.position[1]       = landingPos[1];
    flag.position[2]       = landingPos[2];
    flag.launchPosition[0] = pos[0];
    flag.launchPosition[1] = pos[1];
    flag.launchPosition[2] = pos[2] + BZDBCache::tankHeight;

    // compute flight info -- flight time depends depends on start and end
    // altitudes and desired height above start altitude
    const float gravity   = BZDBCache::gravity;
    const float flagAltitude   = BZDB.eval(StateDatabase::BZDB_FLAGALTITUDE);
    const float thrownAltitude = (flag.type == Flags::Shield) ?
                                 BZDB.eval(StateDatabase::BZDB_SHIELDFLIGHT) * flagAltitude : flagAltitude;
    const float maxAltitude    = pos[2] + thrownAltitude;
    const float upTime     = sqrtf(-2.0f * thrownAltitude / gravity);
    const float downTime       = sqrtf(-2.0f * (maxAltitude - pos[2]) / gravity);
    const float flightTime     = upTime + downTime;

    dropDone       = TimeKeeper::getCurrent();
    dropDone      += flightTime;
    flag.flightTime      = 0.0f;
    flag.flightEnd       = flightTime;
    flag.initialVelocity = -gravity * upTime;
}

void FlagInfo::resetFlag(const glm::vec3 &position, bool teamIsEmpty)
{
    // reset a flag's info
    player      = -1;
    // if it's a random flag, reset flag id
    if (flagIndex >= numFlags - numExtraFlags)
        flag.type = Flags::Null;

    flag.position = position;

    // required flags mustn't just disappear
    if (required)
    {
        if (flag.type->flagTeam == ::NoTeam)
            // flag in now entering game
            addFlag();
        else if (teamIsEmpty)
            flag.status = FlagNoExist;
        else
            flag.status = FlagOnGround;
    }
    else
        flag.status = FlagNoExist;
}

void FlagInfo::grab(int playerIndex)
{
    flag.status = FlagOnTank;
    flag.owner  = playerIndex;
    player      = playerIndex;
    numShots    = 0;
}

TeamColor FlagInfo::teamIndex() const
{
    return flag.type->flagTeam;
}

int FlagInfo::getIndex() const
{
    return flagIndex;
}

float FlagInfo::getNextDrop(TimeKeeper &tm)
{
    // find timeout when next flag would hit ground
    float waitTime = 3.0f;
    if (numFlagsInAir > 0)
    {
        for (int i = 0; i < numFlags; i++)
        {
            FlagInfo &flag = flagList[i];
            if (flag.flag.status != FlagNoExist &&
                    flag.flag.status != FlagOnTank &&
                    flag.flag.status != FlagOnGround &&
                    flag.dropDone - tm < waitTime)
                waitTime = float(flag.dropDone - tm);
        }
    }
    return waitTime;
}

bool FlagInfo::landing(const TimeKeeper &tm)
{
    if (numFlagsInAir <= 0)
        return false;

    bool land = false;
    if (flag.status == FlagInAir || flag.status == FlagComing)
    {
        if (dropDone - tm <= 0.0f)
        {
            flag.status = FlagOnGround;
            numFlagsInAir--;
            land  = true;
        }
    }
    else if (flag.status == FlagGoing)
    {
        if (dropDone - tm <= 0.0f)
        {
            flag.status = FlagNoExist;
            numFlagsInAir--;
            land  = true;
        }
    }
    return land;
}

void FlagInfo::setNoFlagInAir()
{
    numFlagsInAir = 0;
}

void FlagInfo::getTextualInfo(char *message)
{
    sprintf(message, "#%-3d i:%-3s p:%-3d r:%-2d g:%-2d s:%-2d "
            "p:{%.1f, %.1f, %.1f}",
            flagIndex, flag.type->flagAbbv.c_str(), player,
            required ? 1 : 0, grabs, flag.status,
            flag.position[0], flag.position[1], flag.position[2]);
}

bool FlagInfo::exist()
{
    return flag.status != FlagNoExist;
}

FlagInfo *FlagInfo::get(int index)
{
    if (index < 0)
        return NULL;
    if (index >= numFlags)
        return NULL;
    return &flagList[index];
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
