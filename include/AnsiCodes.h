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

#ifndef _ANSI_CODES_H_
#define _ANSI_CODES_H_

// 1st
#include "common.h"

// System headers
#include <string>
#include <glm/vec3.hpp>

// Common headers
#include "TextUtils.h"

// Escape character to begin ANSI codes
#define ESC_CHAR    ((char) 0x1B)

// ANSI (ISO 6429) colors codes

#define ANSI_STR_RESET      "\033[0;1m" // reset & bright
#define ANSI_STR_RESET_FINAL    "\033[0m"   // only reset
#define ANSI_STR_BRIGHT     "\033[1m"
#define ANSI_STR_DIM        "\033[2m"
#define ANSI_STR_UNDERLINE  "\033[4m"
#define ANSI_STR_NO_UNDERLINE   "\033[24m"
#define ANSI_STR_PULSATING  "\033[5m"
#define ANSI_STR_NO_PULSATE "\033[25m"
#define ANSI_STR_REVERSE    "\033[7m"   // unimplemented
#define ANSI_STR_NO_REVERSE "\033[27m"  // unimplemented

#define ANSI_STR_FG_BLACK   "\033[30m"  // grey
#define ANSI_STR_FG_RED     "\033[31m"
#define ANSI_STR_FG_GREEN   "\033[32m"
#define ANSI_STR_FG_YELLOW  "\033[33m"
#define ANSI_STR_FG_BLUE    "\033[34m"
#define ANSI_STR_FG_MAGENTA "\033[35m"  // purple
#define ANSI_STR_FG_CYAN    "\033[36m"
#define ANSI_STR_FG_WHITE   "\033[37m"

#define ANSI_STR_FG_RGB     "\033[38;2" // 24-bit rgb (ISO-8613-3)

#define ANSI_STR_FG_ORANGE  "\033[130m" // orange (custom; not defined in ISO 6429)

// Color definitions
typedef enum
{
    // the first 8 codes line up with the TeamColor enum from global.h
    RogueColor        = 0,    // team (yellow)
    RedColor      = 1,    // team
    GreenColor        = 2,    // team
    BlueColor     = 3,    // team
    PurpleColor       = 4,    // team
    WhiteColor        = 5,    // observer
    GreyColor     = 6,    // rabbit
    OrangeColor       = 7,    // hunter
    CyanColor     = 8,

    LastColor     = 8,    // last of the actual colors, the rest are modifiers

    ResetColor        = 9,
    FinalResetColor   = 12,
    BrightColor       = 13,
    DimColor      = 14,
    PulsatingColor    = 10,
    NonPulsatingColor = 15,
    UnderlineColor    = 11,
    NonUnderlineColor = 16,

    LastCode      = 16,   // last of the codes

    // aliases
    YellowColor       = 0,    // aka RogueColor
    DefaultColor      = 6 // default to grey
} ColorCodes;

// These enum values have to line up with those above
static const std::string ColorStrings[17] =
{
    ANSI_STR_FG_YELLOW,   // 0  Rogue (yellow)
    ANSI_STR_FG_RED,  // 1  Red
    ANSI_STR_FG_GREEN,    // 2  Green
    ANSI_STR_FG_BLUE, // 3  Blue
    ANSI_STR_FG_MAGENTA,  // 4  Purple
    ANSI_STR_FG_WHITE,    // 5  White
    ANSI_STR_FG_BLACK,    // 6  Grey  (bright black is grey)
    ANSI_STR_FG_ORANGE,   // 7  Orange    (nonstandard)
    ANSI_STR_FG_CYAN, // 8  Cyan
    ANSI_STR_RESET,   // 9  Reset
    ANSI_STR_PULSATING,   // 10 Pulsating
    ANSI_STR_UNDERLINE,   // 11 Underline
    ANSI_STR_RESET_FINAL, // 12 Really reset (no brightness added)
    ANSI_STR_BRIGHT,  // 13 Bright mode
    ANSI_STR_DIM,     // 14 Dim mode
    ANSI_STR_NO_PULSATE,  // 15 No Pulsating
    ANSI_STR_NO_UNDERLINE // 16 No Underlining
};

inline std::string rgbToAnsi(const glm::vec3 &color)
{
    return TextUtils::format("%s;%u;%u;%um",
                             ANSI_STR_FG_RGB,
                             int(color[0] * 255),
                             int(color[1] * 255),
                             int(color[2] * 255)
                            );
}

// strip ANSI codes from a string
inline std::string stripAnsiCodes(const std::string &text)
{
    std::string str = "";

    int length = (int)text.size();
    for (int i = 0; i < length; i++)
    {
        if (text[i] == ESC_CHAR)
        {
            i++;
            if ((i < length) && (text[i] == '['))
            {
                i++;
                while ((i < length) && ((text[i] == ';') ||
                                        ((text[i] >= '0') && (text[i] <= '9'))))
                    i++;
            }
        }
        else
            str += text[i];
    }

    return str;
}

#endif //_ANSI_CODES_H_

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
