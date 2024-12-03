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

#ifndef BZDBCACHE_H
#define BZDBCACHE_H

// implementation headers
#include "StateDatabase.h"

class BZDBCache
{
public:
    static void init();

    // prohibit external write access
    template <class T>
    class ReadOnly
    {
        friend class BZDBCache;
    public:
        inline operator T() const
        {
            return data;
        }
    private:
        ReadOnly() {}
        ReadOnly& operator=(const T& value)
        {
            data = value;
            return *this;
        }
    private:
        ReadOnly(const ReadOnly&);
        ReadOnly& operator=(const ReadOnly&);
    private:
        T data;
    };

    // our basics types
    typedef ReadOnly<int>   Int;
    typedef ReadOnly<bool>  Bool;
    typedef ReadOnly<float> Float;

    static Bool  displayMainFlags;
    static Bool  blend;
    static Bool  texture;
    static Bool  shadows;
    static Bool  stencilShadows;
    static Bool  zbuffer;
    static Bool  tessellation;
    static Bool  lighting;
    static Bool  smooth;
    static Bool  colorful;
    static Int   flagChunks;
    static Bool  animatedTreads;
    static Int   radarStyle;
    static Float radarTankPixels;
    static Int   leadingShotLine;
    static Int   radarForwardMarker;
    static Float linedRadarShots;
    static Float sizedRadarShots;
    static Int   radarPosition;
    static Float shotLength;
    static Float pulseRate;
    static Float pulseDepth;
    static Int   controlPanelTimestamp;
    static Bool  showCollisionGrid;
    static Bool  showCullingGrid;
    static Bool  useMeshForRadar;

    static Bool drawCelestial;
    static Bool drawClouds;
    static Bool drawGround;
    static Bool drawGroundLights;
    static Bool drawMountains;
    static Bool drawSky;

    static Float maxLOD;
    static Float worldSize;
    static Float radarLimit;
    static Float gravity;
    static Float tankWidth;
    static Float tankLength;
    static Float tankHeight;
    static Float tankSpeed;
    static Float tankRadius;
    static Float flagRadius;
    static Float flagPoleSize;
    static Float flagPoleWidth;

    static Float gmSize;

    static Float hudGUIBorderOpacityFactor;
    static Float shotBrightness;

public:
    /** public method to update cached variable
    has to be called at best opportunity
    (e.g. at beginning of main loop)
    */
    static void update();

private:
    static void clientCallback(const std::string &name, void *);
    static void serverCallback(const std::string &name, void *);
};

#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
