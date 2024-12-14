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

// Interface header
#include "OpenGLAPI.h"

// System headers
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// common headers
#include "bzfgl.h"

void glColor(const glm::vec3 &c, float alpha)
{
    glColor4f(c.r, c.g, c.b, alpha);
}

void glColor(const glm::vec4 &c)
{
    glColor4f(c.r, c.g, c.b, c.a);
}

void glVertex(const glm::vec2 &p)
{
    glVertex2f(p.x, p.y);
}

void glVertex(const glm::vec2 &p, float z)
{
    glVertex3f(p.x, p.y, z);
}

void glVertex(const glm::vec3 &p)
{
    glVertex3f(p.x, p.y, p.z);
}

void glNormal(const glm::vec2 &n)
{
    glNormal3f(n.x, n.y, 0.0f);
}

void glNormal(const glm::vec3 &n)
{
    glNormal3f(n.x, n.y, n.z);
}

void glTexCoord(const glm::vec2 &t)
{
    glTexCoord2f(t.s, t.t);
}

void glLoadMatrix(const glm::mat4 &m)
{
    glLoadMatrixf(glm::value_ptr(m));
}

void glMultMatrix(const glm::mat4 &m)
{
    glMultMatrixf(glm::value_ptr(m));
}

void glTranslate(const glm::vec3 &v)
{
    glTranslatef(v.x, v.y, v.z);
}

void glVertexPointer(const glm::vec3 vertices[])
{
    glVertexPointer(3, GL_FLOAT, 0, glm::value_ptr(vertices[0]));
}

void glTexCoordPointer(const glm::vec2 texCoords[])
{
    glTexCoordPointer(2, GL_FLOAT, 0, glm::value_ptr(texCoords[0]));
}

glm::vec4 glGetFogColor()
{
    glm::vec4 fogColor;
    glGetFloatv(GL_FOG_COLOR, glm::value_ptr(fogColor));
    return fogColor;
}

void glSetFogColor(const glm::vec4 fogColor)
{
    glFogfv(GL_FOG_COLOR, glm::value_ptr(fogColor));
}

glm::ivec4 glGetViewport()
{
    glm::ivec4 viewport;
    glGetIntegerv(GL_VIEWPORT, glm::value_ptr(viewport));
    return viewport;
}

void glClipPlane(int id, const glm::dvec4 &plane)
{
    glClipPlane(id, glm::value_ptr(plane));
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
