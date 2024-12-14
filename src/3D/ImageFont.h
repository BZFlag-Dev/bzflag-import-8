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

#ifndef _IMAGE_FONT_H_
#define _IMAGE_FONT_H_

/* common header */
#include "common.h"

/* system headers */
#include <string>
#include <glm/fwd.hpp>

/* interface headers */
#include "bzfgl.h"
#include "OSFile.h"

#define MAX_TEXTURE_FONT_CHARS  (128)

class ImageFont
{
public:
    ImageFont();
    virtual ~ImageFont();

    int getSize() const;
    const char* getFaceName() const;

    bool load(OSFile &file);

    virtual void build() = 0;
    virtual bool isBuilt() const = 0;

    virtual void filter(bool dofilter) = 0;
    virtual void drawString(float scale,
                            const glm::vec3 &color, float alpha,
                            const char *str, int len) = 0;

    float getStrLength(float scale, const char *str, int len) const;
    float getStrLength(float scale, const std::string &str) const;

    virtual void free() = 0;

protected:
    struct FontMetrics
    {
        int initialDist;
        int charWidth;
        int whiteSpaceDist;
        int fullWidth; // initialDist + charWidth + whiteSpaceDist
        int startX;
        int endX;
        int startY;
        int endY;
    };
    FontMetrics   fontMetrics[MAX_TEXTURE_FONT_CHARS];

    std::string faceName;
    std::string texture;
    long int    size;
    int         textureXSize;
    int         textureYSize;
    int         textureZStep;
    int         numberOfCharacters;

private:
    // don't copy me
    ImageFont(const ImageFont&);
    ImageFont &operator=(const ImageFont&);
};

#endif //_IMAGE_FONT_H_

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
