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

/* OpenGLMaterial:
 *  Encapsulates an OpenGL material.
 *
 * All materials are for front and back faces.
 * OpenGLMaterial reference counts so copying or assigning a
 * material keeps the same display list until the object is
 * changed.
 *
 * operator==() returns true iff the two objects refer to the
 * same display list.  operator!=() returns true iff the two
 * objects do not refer to the same display list.  Materials
 * that don't refer to the same display list but have exactly
 * the same colors and shininess compare not-equal.
 *
 * <, <=, >, >= define an arbitrary ordering of materials.
 */

#ifndef BZF_OPENGL_MATERIAL_H
#define BZF_OPENGL_MATERIAL_H

// 1st
#include "common.h"

// System headers
#include <glm/vec3.hpp>

// Common headers
#include "bzfgl.h"

class OpenGLMaterial
{
public:
    OpenGLMaterial();
    OpenGLMaterial(const glm::vec3 &specularRGB,
                   const glm::vec3 &emissiveRGB,
                   GLfloat shininess = 0.0f);
    OpenGLMaterial(const OpenGLMaterial&);
    ~OpenGLMaterial();
    OpenGLMaterial& operator=(const OpenGLMaterial&);

    bool        operator==(const OpenGLMaterial&) const;
    bool        operator!=(const OpenGLMaterial&) const;
    bool        operator<(const OpenGLMaterial&) const;

    const glm::vec3 &getSpecularColor() const;
    const glm::vec3 &getEmissiveColor() const;
    GLfloat     getShininess() const;

    bool        isValid() const;
    void        execute() const;

private:
    class Rep
    {
    public:
        ~Rep();
        void        ref();
        void        unref();
        void        execute();
        static Rep* getRep(const glm::vec3 &specular,
                           const glm::vec3 &emissive,
                           GLfloat shininess);
    private:
        Rep(const glm::vec3 &specular,
            const glm::vec3 &emissive,
            GLfloat shininess);
        static void freeContext(void*);
        static void initContext(void*);
    public:
        int     refCount;
        Rep*        prev;
        Rep*        next;
        GLuint      list;
        glm::vec3   specular;
        glm::vec3   emissive;
        GLfloat     shininess;
        static Rep* head;
    };
    Rep*        rep;
};

//
// OpenGLMaterial
//

inline const glm::vec3 &OpenGLMaterial::getSpecularColor() const
{
    return rep->specular;
}

inline const glm::vec3 &OpenGLMaterial::getEmissiveColor() const
{
    return rep->emissive;
}

inline GLfloat      OpenGLMaterial::getShininess() const
{
    return rep->shininess;
}

#endif // BZF_OPENGL_MATERIAL_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
