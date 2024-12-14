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

#ifndef __FLAGINFO_H__
#define __FLAGINFO_H__

/* bzflag common header */
#include "common.h"

/* system headers */
#include <stdlib.h>

/* bzflag common headers */
#include "Flag.h"
#include "TimeKeeper.h"

/** FlagInfo describes a flag as it pertains to the world.
 */
class FlagInfo
{
public:

    FlagInfo();

    void setRequiredFlag(FlagType *desc);
    void addFlag();
    void *pack(void *buf, bool hide = false);
    void dropFlag(const glm::vec3 &pos, glm::vec3 &landingPos, bool vanish);
    void resetFlag(const glm::vec3 &position, bool teamIsEmpty);
    void grab(int playerIndex);
    TeamColor teamIndex() const;
    int  getIndex() const;
    bool landing(const TimeKeeper &tm);
    void getTextualInfo(char *message);
    bool exist();

    static FlagInfo *get(int index);
    static void setSize(int _numFlags);
    static void setAllowed(std::vector<FlagType*> allowed);
    static void setExtra(int extra);
    static int  lookupFirstTeamFlag(int teamindex);
    static float getNextDrop(TimeKeeper &tm);
    static void  setNoFlagInAir();

    // flag info
    Flag flag;
    // player index who has flag
    int player;
    // how many grabs before removed
    int grabs;
    // true if flag must be in game
    bool required;
    // number of shots on this flag
    int numShots;

private:
    int flagIndex;
    static std::vector<FlagType*> allowedFlags;
    static int            numExtraFlags;
    static int            numFlags;
    static int            numFlagsInAir;
    static FlagInfo         *flagList;

    // time flag will land
    TimeKeeper            dropDone;
};
#else
class FlagInfo;
#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
