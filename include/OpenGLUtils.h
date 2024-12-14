/* bzflag
 * Copyright (c) 1993-2023 Tim Riker
 * Written By Jeffrey Myers
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef __OPENGLUTILS_H__
#define __OPENGLUTILS_H__

#include "common.h"

// system headers
#include <string>
#include <map>
#include <vector>
#include <glm/fwd.hpp>

// common headers
#include "bzfgl.h"


class BzMaterial;
class OpenGLGState;


#define INVALID_GL_ID 0xffffffff


extern void bzMat2gstate(const BzMaterial* bzmat, OpenGLGState& gstate,
                         glm::vec4 &color, const glm::vec4 *&colorPtr);

extern bool parseBlendFactors(const std::string& s, GLenum& src, GLenum& dst);

extern float getFloatColor(int val);
extern void setColor(float dstColor[3], int r, int g, int b);
extern void glSetColor(const float c[3], float alpha = 1.0f);
extern void glTranslatefv(const float v[3]);

typedef enum
{
    eCenter,
    eLowerLeft,
    eLowerRight,
    eUpperLeft,
    eUpperRight,
    eCenterLeft,
    eCenterRight,
    eCenterTop,
    eCenterBottom
} eAlignment;


#endif // __OPENGLUTILS_H__


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
