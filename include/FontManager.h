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

#ifndef _FONT_MANAGER_H_
#define _FONT_MANAGER_H_

// Inherits from
#include "Singleton.h"

// System interfaces
#include <map>
#include <string>
#include <vector>

// Common interfaces
#include "bzfgl.h"
#include "AnsiCodes.h"

class ImageFont;

typedef std::map<int, ImageFont*> FontSizeMap;
typedef std::vector<FontSizeMap>    FontFaceList;
typedef std::map<std::string, int>  FontFaceMap;

class FontManager : public Singleton<FontManager>
{
public:
    FontManager();
    ~FontManager();

    void loadAll(std::string dir);

    void clear();

    void rebuild(void);

    int getFaceID(std::string faceName);

    int getNumFaces(void);
    const char* getFaceName(int faceID);

    void drawString(float x, float y, float z, int faceID, float size,
                    const std::string &text,
                    const glm::vec3 resetColor = glm::vec3(-1.0f));

    float getStrLength(int faceID, float size, const std::string &text,
                       bool alreadyStripped = false);
    float getStrLength(const std::string &face, float size,
                       const std::string &text, bool alreadyStripped = false);

    float getStrHeight(int faceID, float size, const std::string &text);
    float getStrHeight(std::string face, float size, const std::string &text);

    void setDimFactor(float newDimFactor);
    void setOpacity(float newOpacity);
    void setDarkness(float newDimFactor);

    void unloadAll(void);

protected:
    friend class Singleton<FontManager>;

private:
    float         getPulseFactor() const;
    ImageFont*    getClosestSize(int faceID, float size, bool bigger);
    ImageFont*    getClosestRealSize(int faceID, float desiredSize, float &actualSize);
    FontFaceMap   faceNames;
    FontFaceList  fontFaces;

    std::string   fontDirectory;

    float     opacity;
    float     dimFactor; // ANSI code dimming
    float     darkness;  // darkening of all colors

    static void   callback(const std::string& name, void *);
    static void   freeContext(void *data);
    static void   initContext(void *data);
    static glm::vec3 underlineColor;
};

inline void FontManager::setDimFactor(float newDimFactor)
{
    dimFactor = newDimFactor;
}

inline void FontManager::setOpacity(float newOpacity)
{
    opacity = newOpacity;
}

inline void FontManager::setDarkness(float newDarkness)
{
    darkness = newDarkness;
}

#endif //_FONT_MANAGER_H_

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
