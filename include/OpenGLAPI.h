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

#ifndef _OPENGLAPI_H
#define _OPENGLAPI_H

// 1st
#include "common.h"

// System headers
#include <glm/fwd.hpp>

extern void glColor(const glm::vec3 &c, float alpha = 1.0f);
extern void glColor(const glm::vec4 &c);
extern void glVertex(const glm::vec2 &p);
extern void glVertex(const glm::vec2 &p, float z);
extern void glVertex(const glm::vec3 &p);
extern void glNormal(const glm::vec2 &n);
extern void glNormal(const glm::vec3 &n);
extern void glTexCoord(const glm::vec2 &t);
extern void glLoadMatrix(const glm::mat4 &m);
extern void glMultMatrix(const glm::mat4 &m);
extern void glTranslate(const glm::vec3 &v);
extern glm::vec4 glGetFogColor();
extern void glSetFogColor(const glm::vec4 fogColor);
extern glm::ivec4 glGetViewport();
extern void glVertexPointer(const glm::vec3 vertices[]);
extern void glTexCoordPointer(const glm::vec2 texCoords[]);
extern void glClipPlane(int id, const glm::dvec4 &plane);

#endif // _OPENGLAPI_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
