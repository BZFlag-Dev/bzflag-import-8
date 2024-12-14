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

// interface header
#include "WeatherRenderer.h"

// System headers
#include <glm/gtc/random.hpp>

// common impl headers
#include "TextureManager.h"
#include "StateDatabase.h"
#include "BZDBCache.h"
#include "TimeKeeper.h"
#include "TextUtils.h"
#include "ParseColor.h"
#include "Intersect.h"
#include "Extents.h"
#include "OpenGLAPI.h"

// local impl headers
#include "RoofTops.h"

static void bzdbCallBack(const std::string& UNUSED(name), void *userData)
{
    static_cast<WeatherRenderer*>(userData)->set();
}


WeatherRenderer::WeatherRenderer()
{
    rainColor[0] = glm::vec4(0.75f);
    rainColor[1] = glm::vec4(0.0f);

    rainSize[0] = rainSize[1] = 1.0f;

    rainDensity = 1000;
    rainSpeed = -100.0f;
    rainSpeedMod = 10.0f;
    rainSpread = 500.0f;

    doPuddles = true;
    doLineRain = true;
    doBillBoards = false;
    spinRain = false;
    cullRoofTops = true;
    roofPuddles = false;

    rainStartZ = -1.0f;
    rainEndZ = 0.0f;

    lastRainTime = 0.0f;

    maxPuddleTime = 5.0f;
    puddleSpeed = 1.0f;

    puddleColor = glm::vec4(1.0f);

    dropList = puddleList = INVALID_GL_LIST_ID;

    gridSize = 200.0f;

    keyFactor = 1.0f / gridSize;

    rainCount = 0;
    cellCount = 0;

    // install callbacks
    BZDB.addCallback("_rainType", bzdbCallBack, this);
    BZDB.addCallback("_rainDensity", bzdbCallBack, this);
    BZDB.addCallback("_rainSpread", bzdbCallBack, this);
    BZDB.addCallback("_rainSpeedMod", bzdbCallBack, this);
    BZDB.addCallback("_rainStartZ", bzdbCallBack, this);
    BZDB.addCallback("_rainEndZ", bzdbCallBack, this);
    BZDB.addCallback("_rainBaseColor", bzdbCallBack, this);
    BZDB.addCallback("_useRainPuddles", bzdbCallBack, this);
    BZDB.addCallback("_useLineRain", bzdbCallBack, this);
    BZDB.addCallback("_useRainBillboards", bzdbCallBack, this);
    BZDB.addCallback("_rainPuddleSpeed", bzdbCallBack, this);
    BZDB.addCallback("_rainMaxPuddleTime", bzdbCallBack, this);
    BZDB.addCallback("_rainPuddleColor", bzdbCallBack, this);
    BZDB.addCallback("_rainPuddleTexture", bzdbCallBack, this);
    BZDB.addCallback("_rainTexture", bzdbCallBack, this);
    BZDB.addCallback("_rainSpins", bzdbCallBack, this);
    BZDB.addCallback("_rainRoofs", bzdbCallBack, this);
    BZDB.addCallback("userRainScale", bzdbCallBack, this);
}


WeatherRenderer::~WeatherRenderer()
{
    freeContext(); // free the display lists
    BZDB.removeCallback("_rainType", bzdbCallBack, this);
    BZDB.removeCallback("_rainDensity", bzdbCallBack, this);
    BZDB.removeCallback("_rainSpread", bzdbCallBack, this);
    BZDB.removeCallback("_rainSpeedMod", bzdbCallBack, this);
    BZDB.removeCallback("_rainStartZ", bzdbCallBack, this);
    BZDB.removeCallback("_rainEndZ", bzdbCallBack, this);
    BZDB.removeCallback("_rainBaseColor", bzdbCallBack, this);
    BZDB.removeCallback("_useRainPuddles", bzdbCallBack, this);
    BZDB.removeCallback("_useLineRain", bzdbCallBack, this);
    BZDB.removeCallback("_useRainBillboards", bzdbCallBack, this);
    BZDB.removeCallback("_rainPuddleSpeed", bzdbCallBack, this);
    BZDB.removeCallback("_rainMaxPuddleTime", bzdbCallBack, this);
    BZDB.removeCallback("_rainPuddleColor", bzdbCallBack, this);
    BZDB.removeCallback("_rainPuddleTexture", bzdbCallBack, this);
    BZDB.removeCallback("_rainTexture", bzdbCallBack, this);
    BZDB.removeCallback("_rainSpins", bzdbCallBack, this);
    BZDB.removeCallback("_rainRoofs", bzdbCallBack, this);
    BZDB.removeCallback("userRainScale", bzdbCallBack, this);
}


void WeatherRenderer::init(void)
{
    OpenGLGStateBuilder gstate;
    TextureManager& tm = TextureManager::instance();

    gstate.reset();
    gstate.setShading();
    gstate.setBlending((GLenum) GL_SRC_ALPHA,
                       (GLenum) GL_ONE_MINUS_SRC_ALPHA);
    gstate.setAlphaFunc();
    rainGState = gstate.getState();

    // just set up some default textures
    gstate.setTexture(tm.getTextureID("snowflake"));
    texturedRainState = gstate.getState();

    gstate.setTexture(tm.getTextureID("puddle"));
    puddleState = gstate.getState();

    buildPuddleList();
}


void WeatherRenderer::set(void)
{
    // check the bzdb and see if we need to change any rain stuff
    rainDensity = 0;

    if (dbItemSet("_rainType") || dbItemSet("_rainDensity"))
    {
        // default rain desnity
        rainDensity = (int)(1000.0f * BZDB.eval("userRainScale"));

        // some defaults
        doLineRain = false;
        rainSize[0] = 1.0f;
        rainSize[1] = 1.0f;
        rainSpeed = -100.0f;
        rainSpeedMod = 50.0f;
        doPuddles = true;
        rainColor[0] = glm::vec4(0.75f, 0.75f, 0.85f, 0.75f);
        rainColor[1] = glm::vec4(0.0f);
        rainSize[0] = rainSize[1] = 1.0f;
        maxPuddleTime = 1.5f;
        puddleSpeed = 1.0f;
        puddleColor = glm::vec4(1.0f);
        spinRain = true;
        cullRoofTops = true;
        roofPuddles = false;

        rainSpread = 500.0f;

        TextureManager& tm = TextureManager::instance();

        OpenGLGStateBuilder gstate;
        gstate.reset();
        gstate.setShading();
        gstate.setBlending((GLenum) GL_SRC_ALPHA,
                           (GLenum) GL_ONE_MINUS_SRC_ALPHA);
        gstate.setAlphaFunc();

        OpenGLGStateBuilder puddleGStateBuilder(puddleState);

        puddleGStateBuilder.setTexture(tm.getTextureID("puddle"));

        if (dbItemSet("_rainType"))
        {
            std::string rainType = TextUtils::tolower(BZDB.get("_rainType"));

            if (rainType == "snow")
            {
                doBillBoards = false;
                gstate.setTexture (tm.getTextureID("snowflake"));
                rainSpeed = -20.0f;
                rainSpeedMod = 5.0f;
                doPuddles = false;
            }
            else if (rainType == "fatrain")
            {
                doBillBoards = false;
                rainSize[0] = 0.5f;
                rainSize[1] = 0.75f;
                gstate.setTexture(tm.getTextureID("raindrop"));
                rainSpeed = -50.0f;
                rainSpeedMod = 25.0f;
                spinRain = false;
            }
            else if (rainType == "frog")
            {
                gstate.setTexture(tm.getTextureID ("frog"));
                rainSpeed = -100.0f;
                rainSpeedMod = 5.0f;
                doPuddles = true;
                doBillBoards = true;
                // ewww
                puddleSpeed = 3.0f;
                puddleColor[0] = 0.75f;
                puddleColor[1] = puddleColor[2] = 0.0f;
                rainSize[0] = 2.0f;
                rainSize[1] = 2.0f;
            }
            else if (rainType == "particle")
            {
                rainDensity = (int)(500.0f * BZDB.eval("userRainScale"));
                gstate.setTexture(tm.getTextureID ("red_super_bolt"));
                rainSpeed = -20.0f;
                rainSpeedMod = 5.0f;
                doPuddles = true;
                doBillBoards = true;
                puddleSpeed = 10.0f;
                // ewww
                puddleColor[0] = 1.0f;
                puddleColor[1] = puddleColor[2] = 0.0f;
                rainSize[0] = 1.0f;
                rainSize[1] = 1.0f;
            }
            else if (rainType == "bubble")
            {
                gstate.setTexture(tm.getTextureID("bubble"));
                rainSpeed = 20.0f;
                rainSpeedMod = 1.0f;
                doBillBoards = true;
                doPuddles = false;
            }
            else if (rainType == "rain")
            {
                puddleColor[0] = 1.0f;
                puddleColor[1] = puddleColor[2] = 1.0f;
                doLineRain = true;
                rainSize[0] = 0.0f;
                rainSize[1] = 0.75f;
                puddleGStateBuilder.setTexture(tm.getTextureID("puddle"));
            }
        }

        if (dbItemSet("_rainPuddleTexture"))
        {
            puddleGStateBuilder.setTexture(
                tm.getTextureID(BZDB.get("_rainPuddleTexture").c_str()));
        }

        // see if the texture is specificly overiden
        if (dbItemSet("_rainTexture"))
        {
            gstate.setTexture (
                tm.getTextureID(BZDB.get("_rainTexture").c_str()));
        }

        texturedRainState = gstate.getState();

        // if there is a specific overides for stuff
        if (dbItemSet("_rainSpread"))
            rainSpread = BZDB.eval("_rainSpread");

        if (dbItemSet("_rainDensity"))
            rainDensity = (int)(BZDB.eval("_rainDensity") * BZDB.eval("userRainScale"));

        if (dbItemSet("_rainSpeed"))
            rainSpeed = BZDB.eval("_rainSpeed");

        if (dbItemSet("_rainSpeedMod"))
            rainSpeedMod = BZDB.eval("_rainSpeedMod");

        if (dbItemSet("_rainSize"))
            BZDB.evalPair("_rainSize", rainSize);

        if (dbItemSet("_rainStartZ"))
            rainStartZ = BZDB.eval("_rainStartZ");

        if (dbItemSet("_rainEndZ"))
            rainEndZ = BZDB.eval("_rainEndZ");

        if (dbItemSet("_rainBaseColor"))
            parseColorString(BZDB.get("_rainBaseColor"), rainColor[0]);

        if (dbItemSet("_rainTopColor"))
            parseColorString(BZDB.get("_rainTopColor"), rainColor[1]);

        if (dbItemSet("_useRainPuddles"))
            doPuddles = BZDB.isTrue("_useRainPuddles");

        if (dbItemSet("_useLineRain"))
            doLineRain = BZDB.isTrue("_useLineRain");

        if (dbItemSet("_useRainBillboards"))
            doBillBoards = BZDB.isTrue("_useRainBillboards");

        if (dbItemSet("_rainPuddleColor"))
            parseColorString(BZDB.get("_rainPuddleColor"), puddleColor);

        if (dbItemSet("_rainPuddleSpeed"))
            puddleSpeed = BZDB.eval("rainPuddleSpeed");

        if (dbItemSet("_rainMaxPuddleTime"))
            maxPuddleTime = BZDB.eval("_rainMaxPuddleTime");

        if (dbItemSet("_rainSpins"))
            spinRain = BZDB.isTrue("_rainSpins");

        if (dbItemSet("_rainRoofs"))
        {
            cullRoofTops = (BZDB.evalInt("_rainRoofs") >= 1);
            roofPuddles = (BZDB.evalInt("_rainRoofs") >= 2);
        }

        // update the actual puddle material
        puddleState = puddleGStateBuilder.getState();


        // make sure we know where to start and stop the rain
        // we want to compute the heights for us
        if (rainStartZ == -1 && rainEndZ == 0)
        {
            // check the dir
            if (rainSpeed < 0)   // rain going down
            {
                rainStartZ = 120.0f * BZDBCache::tankHeight; // same as the clouds
                rainEndZ = 0;
            }
            else     // rain going up (tiny bubbles)
            {
                if (rainSpeed == 0)
                    rainSpeed = 0.1f;

                rainEndZ = 120.0f * BZDBCache::tankHeight;  // same as the clouds
                rainStartZ = 0;
            }
        }
        else
        {
            // the specified rain start and end values,
            // make sure they make sense with the directon
            if (rainSpeed < 0)   // rain going down
            {
                if (rainEndZ > rainStartZ)
                {
                    float temp = rainStartZ;
                    rainStartZ = rainEndZ;
                    rainEndZ = temp;
                }
            }
            else     // rain going up
            {
                if (rainEndZ < rainStartZ)
                {
                    float temp = rainStartZ;
                    rainStartZ = rainEndZ;
                    rainEndZ = temp;
                }
            }
        }

        float rainHeightDelta = rainEndZ - rainStartZ;

        int totalRain = raindrops.size();

        if (totalRain < rainDensity)
        {
            auto minPos = glm::vec3(-rainSpread, -rainSpread, rainStartZ);
            auto maxPos = glm::vec3(
                              rainSpread,
                              rainSpread,
                              rainStartZ + rainHeightDelta);
            for (int drops = totalRain; drops < rainDensity; drops++)
            {
                rain drop;
                drop.speed = rainSpeed
                             + glm::linearRand(-rainSpeedMod, rainSpeedMod);
                drop.pos = glm::linearRand(minPos, maxPos);
                if (cullRoofTops)
                {
                    drop.roofTop =
                        RoofTops::getTopHeight(drop.pos[0], drop.pos[1], drop.pos[2]);
                }
                else
                    drop.roofTop = 0.0f;

                {
                    raindrops.push_back (drop);
                }
            }
            lastRainTime = float(TimeKeeper::getCurrent().getSeconds());
        }
        // recompute the drops based on the posible new size
        buildDropList();
    }
    else
    {
        raindrops.clear();
        chunkMap.clear();
        rainCount = 0;
    }
}


void WeatherRenderer::update(void)
{
    // update the time
    float frameTime = float(TimeKeeper::getCurrent().getSeconds() - lastRainTime);
    lastRainTime = float(TimeKeeper::getCurrent().getSeconds());

    // clamp the update time
    // its not an important sim so just keep it smooth
    if (frameTime > 0.06f)
        frameTime = 0.06f;

    {
        // update all the drops in the world
        std::vector<rain>::iterator itr = raindrops.begin();
        while (itr != raindrops.end())
        {
            if (updateDrop(itr, frameTime))
                itr++;
        }
    }

    // update all the puddles
    std::vector<puddle>::iterator puddleItr = puddles.begin();

    while (puddleItr != puddles.end())
    {
        if (updatePuddle(puddleItr, frameTime))
            puddleItr++;
    }
}


void WeatherRenderer::draw(const SceneRenderer& sr)
{
    {
        if (raindrops.empty())
            return;
    }

    glDisable(GL_CULL_FACE);
    glMatrixMode(GL_MODELVIEW);
    glColor4f(1, 1, 1, 1.0f);
    glDepthMask(GL_FALSE);

    if (doLineRain)   // we are doing line rain
    {
        rainGState.setState();
        glBegin(GL_LINES);
        std::vector<rain>::iterator itr = raindrops.begin();
        while (itr != raindrops.end())
        {
            drawLineDrop(*itr);
            itr++;
        }
        glEnd();
    }
    else
    {
        texturedRainState.setState();
        std::vector<rain>::iterator itr = raindrops.begin();
        while (itr != raindrops.end())
        {
            drawDrop(*itr, sr);
            itr++;
        }
    }

    if (doPuddles)
    {
        std::vector<puddle>::iterator puddleItr = puddles.begin();
        puddleState.setState();
        while (puddleItr != puddles.end())
        {
            drawPuddle(*puddleItr);
            puddleItr++;
        }
    }

    glEnable(GL_CULL_FACE);
    glColor4f(1, 1, 1, 1);
    glDepthMask(GL_TRUE);
}


void WeatherRenderer::freeContext(void)
{
    if (dropList != INVALID_GL_LIST_ID)
    {
        glDeleteLists(dropList, 1);
        dropList = INVALID_GL_LIST_ID;
    }
    if (puddleList != INVALID_GL_LIST_ID)
    {
        glDeleteLists(puddleList, 1);
        puddleList = INVALID_GL_LIST_ID;
    }
    return;
}


void WeatherRenderer::rebuildContext(void)
{
    buildDropList();
    buildPuddleList();
    return;
}


void WeatherRenderer::buildDropList(bool _draw)
{
    if (!_draw)
    {
        if (dropList != INVALID_GL_LIST_ID)
        {
            glDeleteLists(dropList, 1);
            dropList = INVALID_GL_LIST_ID;
        }
        dropList = glGenLists(1);
        glNewList(dropList, GL_COMPILE);
    }

    if (doBillBoards)
    {
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0);
        glVertex3f(-rainSize[0], -rainSize[1], 0);

        glTexCoord2f(1, 0);
        glVertex3f(rainSize[0], -rainSize[1], 0);

        glTexCoord2f(0, 1);
        glVertex3f(-rainSize[0], rainSize[1], 0);

        glTexCoord2f(1, 1);
        glVertex3f(rainSize[0], rainSize[1], 0);
        glEnd();
    }
    else
    {
        glPushMatrix();
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0);
        glVertex3f(-rainSize[0], 0, -rainSize[1]);

        glTexCoord2f(1, 0);
        glVertex3f(rainSize[0], 0, -rainSize[1]);

        glTexCoord2f(0, 1);
        glVertex3f(-rainSize[0], 0, rainSize[1]);

        glTexCoord2f(1, 1);
        glVertex3f(rainSize[0], 0, rainSize[1]);
        glEnd();

        glRotatef(120, 0, 0, 1);

        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0);
        glVertex3f(-rainSize[0], 0, -rainSize[1]);

        glTexCoord2f (1, 0);
        glVertex3f(rainSize[0], 0, -rainSize[1]);

        glTexCoord2f (0, 1);
        glVertex3f(-rainSize[0], 0, rainSize[1]);

        glTexCoord2f (1, 1);
        glVertex3f(rainSize[0], 0, rainSize[1]);
        glEnd();

        glRotatef(120, 0, 0, 1);

        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0);
        glVertex3f(-rainSize[0], 0, -rainSize[1]);

        glTexCoord2f(1, 0);
        glVertex3f(rainSize[0], 0, -rainSize[1]);

        glTexCoord2f(0, 1);
        glVertex3f(-rainSize[0], 0, rainSize[1]);

        glTexCoord2f(1, 1);
        glVertex3f(rainSize[0], 0, rainSize[1]);
        glEnd();
        glPopMatrix();
    }

    if (!_draw)
        glEndList();
}


void WeatherRenderer::buildPuddleList(bool _draw)
{
    float scale = 1;
    if (!_draw)
    {
        if (puddleList != INVALID_GL_LIST_ID)
        {
            glDeleteLists(puddleList, 1);
            puddleList = INVALID_GL_LIST_ID;
        }
        puddleList = glGenLists(1);
        glNewList(puddleList, GL_COMPILE);
    }

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0, 0);
    glVertex3f(-scale, -scale, 0);

    glTexCoord2f(1, 0);
    glVertex3f(scale, -scale, 0);

    glTexCoord2f(0, 1);
    glVertex3f(-scale, scale, 0);

    glTexCoord2f(1, 1);
    glVertex3f(scale, scale, 0);
    glEnd();

    if (!_draw)
        glEndList();
}


bool WeatherRenderer::updateDrop(std::vector<rain>::iterator& drop,
                                 float frameTime)
{
    drop->pos[2] += drop->speed * frameTime;

    bool killDrop = false;

    if (drop->speed < 0)
    {
        if (cullRoofTops)
            killDrop = (drop->pos[2] < drop->roofTop);
        else
            killDrop = (drop->pos[2] < rainEndZ);
    }
    else
        killDrop = (drop->pos[2] > rainEndZ);

    if (killDrop)
    {
        if (doPuddles && (roofPuddles ||
                          !(cullRoofTops && (drop->speed < 0.0f) && (drop->roofTop != 0.0f))))
        {
            puddle thePuddle;
            thePuddle.pos[0] = drop->pos[0];
            thePuddle.pos[1] = drop->pos[1];
            if (!cullRoofTops)
                thePuddle.pos[2] = rainEndZ;
            else
                thePuddle.pos[2] = drop->roofTop + 0.05f;
            thePuddle.time = 0.001f;
            puddles.push_back (thePuddle);
        }

        {
            if ((int)(raindrops.size()) <= rainDensity)
            {
                // reset this drop
                drop->pos[2] = rainStartZ;
                drop->speed =
                    rainSpeed + ((float) (bzfrand() * 2.0f - 1.0f) * rainSpeedMod);
                drop->pos[0] = (((float) bzfrand() * 2.0f - 1.0f) * rainSpread);
                drop->pos[1] = (((float) bzfrand() * 2.0f - 1.0f) * rainSpread);
                if (cullRoofTops)
                {
                    drop->roofTop =
                        RoofTops::getTopHeight (drop->pos[0], drop->pos[1], drop->pos[2]);
                    // clamp the rain to the valid rain range.
                    if ( rainSpeed > 0)
                    {
                        if ( drop->roofTop < rainEndZ )
                            drop->roofTop = rainEndZ;
                        else if ( drop->roofTop > rainEndZ )
                            drop->roofTop = rainEndZ;
                    }
                }
                else
                    drop->roofTop = rainEndZ;

                return true;
            }
            else     // we need less rain, so don't do this one
            {
                drop = raindrops.erase(drop);
                return false;
            }
        }
    }
    return true;
}


bool WeatherRenderer::updatePuddle(std::vector<puddle>::iterator& splash,
                                   float frameTime)
{
    if (splash->time > maxPuddleTime)
    {
        splash = puddles.erase(splash);
        return false;
    }
    splash->time += frameTime;
    return true;
}


void WeatherRenderer::drawLineDrop(rain& drop)
{
    {
        float alphaMod = 0;

        if (drop.pos[2] < 5.0f)
            alphaMod = 1.0f - (5.0f / drop.pos[2]);

        float alphaVal = rainColor[0][3] - alphaMod;
        if (alphaVal < 0)
            alphaVal = 0;

        glColor4f(rainColor[0][0], rainColor[0][1], rainColor[0][2], alphaVal);
        glVertex(drop.pos);

        alphaVal = rainColor[1][3] - alphaMod;
        if (alphaVal < 0)
            alphaVal = 0;

        glColor4f(rainColor[1][0], rainColor[1][1], rainColor[1][2], alphaVal);
        glVertex3f(drop.pos[0], drop.pos[1],
                   drop.pos[2] + (rainSize[1] - (drop.speed * 0.15f)));
    }
}


void WeatherRenderer::drawDrop(rain& drop, const SceneRenderer& sr)
{
    {
        float alphaMod = 0;

        if (drop.pos[2] < 2.0f)
            alphaMod = (2.0f - drop.pos[2]) * 0.5f;

        glColor4f(1, 1, 1, 1.0f - alphaMod);
        glPushMatrix();
        glTranslatef(drop.pos[0], drop.pos[1], drop.pos[2]);
        if (doBillBoards)
            sr.getViewFrustum().executeBillboard();
        else if (spinRain)
            glRotatef(lastRainTime * 10.0f * rainSpeed * 0.85f, 0, 1, 0);

        if (spinRain)
            glRotatef(lastRainTime * 10.0f * rainSpeed, 0, 0, 1);

        if (1)
            glCallList(dropList);
        else
            buildDropList(true);
        glPopMatrix();
    }
}


void WeatherRenderer::drawPuddle(puddle& splash)
{
    glPushMatrix();
    glTranslatef(splash.pos[0], splash.pos[1], splash.pos[2]);

    float scale = fabsf(splash.time * rainSpeed * 0.035f * puddleSpeed);
    float lifeTime = splash.time / maxPuddleTime;

    glColor4f(puddleColor[0], puddleColor[1], puddleColor[2], 1.0f - lifeTime);

    glScalef(scale, scale, scale);
    if (1)
        glCallList(puddleList);
    else
        buildPuddleList(true);

    glPopMatrix();
}


void WeatherRenderer::addDrop(rain& drop)
{
    int key = keyFromPos(drop.pos[0], drop.pos[1]);

    std::map<int, visibleChunk>::iterator itr = chunkMap.find(key);
    rainCount++;

    if (itr != chunkMap.end())
    {
        itr->second.drops.push_back(drop);
        return;
    }

    visibleChunk chunk;

    setChunkFromDrop(chunk, drop);

    chunk.drops.push_back(drop);
    chunkMap[key] = chunk;
    cellCount++;
}


int WeatherRenderer::keyFromPos(float x, float y) const
{
    union
    {
        short pos[2];
        int   key;
    } temp;
    temp.pos[0] = (short)(x * keyFactor);
    temp.pos[1] = (short)(y * keyFactor);
    return temp.key;
}


void WeatherRenderer::setChunkFromDrop(visibleChunk& chunk, rain& drop)
{
    int keyX = (int)(drop.pos[0] * keyFactor);
    int keyY = (int)(drop.pos[1] * keyFactor);

    chunk.bbox.mins[0] = keyX * gridSize;
    chunk.bbox.mins[1] = keyY * gridSize;
    chunk.bbox.mins[2] = rainStartZ > rainEndZ ? rainEndZ : rainStartZ;
    chunk.bbox.maxs[0] = keyX * gridSize + gridSize;
    chunk.bbox.maxs[1] = keyY * gridSize + gridSize;
    chunk.bbox.maxs[2] = rainStartZ > rainEndZ ? rainStartZ : rainEndZ;
}


bool WeatherRenderer::dbItemSet(const char *name)
{
    if (!BZDB.isSet(name))
        return false;

    if (TextUtils::tolower(BZDB.get(name)) == "none")
        return false;

    return true;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
