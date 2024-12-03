/* bzflag
 * Copyright (c) 1993-2021 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#pragma once

#include "common.h"

#include <string>
#include <vector>

#include "bzfsAPI.h"

extern void initPython (int argc, char **argv);
extern void destroyPython ();

extern void startPython (const char *fileName, const char *params);
extern void stopPython ();

extern float getPythonMinWaitTime();

extern void bzPythonEvent(bz_EventData *eventData);
extern bool bzPython_SlashCommand(int playerID, bz_ApiString, bz_ApiString,
                                  bz_APIStringList*);
extern bool bzPython_isPollActive(std::string command);
extern bool bzPython_PollOpen(std::string command, int playerId,
                              const char *target);
extern bool bzPython_PollClose(std::string command, const char *target,
                               bool success);
extern void bzPython_PollHelp(int playerID);

extern bool bzPython_CheckIfCustomMap(const std::string &customObject);
extern void bzPython_customZoneMapObject(const std::string &customObject,
        const std::vector<std::string> &customLines);

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
