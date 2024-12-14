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

#ifndef _PARSE_COLOR_H_
#define _PARSE_COLOR_H_

// 1st
#include "common.h"

// System interfaces
#include <string>
#include <iostream>
#include <glm/fwd.hpp>

extern bool parseColorCString(const char* str, float color[4]);
extern bool parseColorString(const std::string& str, float color[4]);
extern bool parseColorString(const std::string& str, glm::vec4 &color);
extern bool parseColorStream(std::istream& input, float color[4]);
extern bool parseColorStream(std::istream& input, glm::vec4 &color);


#endif // _PARSE_COLOR_H_


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
