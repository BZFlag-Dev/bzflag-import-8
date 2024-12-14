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
#include "HUDuiControl.h"

// system headers
#include <iostream>

// common implementation headers
#include "BundleMgr.h"
#include "Bundle.h"
#include "TextureManager.h"
#include "FontManager.h"
#include "OpenGLAPI.h"

// local implementation headers
#include "HUDui.h"


//
// HUDuiControl
//

// init static members
const glm::vec3     HUDuiControl::dimTextColor = { 0.7f, 0.7f, 0.7f };
const glm::vec3     HUDuiControl::textColor    = { 1.0f, 1.0f, 1.0f };
OpenGLGState*       HUDuiControl::gstate = NULL;
int     HUDuiControl::arrow = -1;
int         HUDuiControl::arrowFrame = 0;
TimeKeeper      HUDuiControl::lastTime;
int         HUDuiControl::totalCount = 0;

HUDuiControl::HUDuiControl() : showingFocus(true),
    fontFace(-1), fontSize(11),
    x(0.0f), y(0.0f),
    width(1.0f), height(1.0f),
    fontHeight(11.0f),
    desiredLabelWidth(0.0f),
    trueLabelWidth(0.0f),
    prev(this), next(this),
    cb(NULL), userData(NULL)
{
    if (totalCount == 0)
    {
        // load arrow texture
        TextureManager &tm = TextureManager::instance();
        arrow = tm.getTextureID( "menu_arrow" );

        // make gstate for focus arrow
        gstate = new OpenGLGState;
        OpenGLGStateBuilder builder(*gstate);
        builder.setTexture(arrow);
        builder.setBlending();
//    builder.setSmoothing();
        //builder.setTextureEnvMode(GL_REPLACE);
        *gstate = builder.getState();

        // get start time for animation
        lastTime = TimeKeeper::getCurrent();
    }

    totalCount++;
}

HUDuiControl::~HUDuiControl()
{
    if (--totalCount == 0)
    {
        delete gstate;
        arrow = -1;
        gstate = NULL;
    }
}

float           HUDuiControl::getLabelWidth() const
{
    return desiredLabelWidth;
}

std::string     HUDuiControl::getLabel() const
{
    return BundleMgr::getCurrentBundle()->getLocalString(label);
}

int         HUDuiControl::getFontFace() const
{
    return fontFace;
}

float           HUDuiControl::getFontSize() const
{
    return fontSize;
}

HUDuiControl*       HUDuiControl::getPrev() const
{
    return prev;
}

HUDuiControl*       HUDuiControl::getNext() const
{
    return next;
}

HUDuiCallback       HUDuiControl::getCallback() const
{
    return cb;
}

const void*     HUDuiControl::getUserData() const
{
    return userData;
}

void            HUDuiControl::setPosition(float _x, float _y)
{
    x = _x;
    y = _y;
}

void            HUDuiControl::setSize(float _width, float _height)
{
    width = _width;
    height = _height;
}

void            HUDuiControl::setLabelWidth(float labelWidth)
{
    desiredLabelWidth = labelWidth;
}

void            HUDuiControl::setLabel(const std::string& _label)
{

    label = _label;
    if (fontFace >= 0)
    {
        FontManager &fm = FontManager::instance();
        trueLabelWidth = fm.getStrLength(fontFace, fontSize, getLabel() + "99");
    }
}

void            HUDuiControl::setFontFace(int _fontFace)
{
    fontFace = _fontFace;
    onSetFont();
}

void            HUDuiControl::setFontSize(float size)
{
    fontSize = size;
    onSetFont();
}

void            HUDuiControl::setPrev(HUDuiControl* _prev)
{
    if (!_prev) prev = this;
    else prev = _prev;
}

void            HUDuiControl::setNext(HUDuiControl* _next)
{
    if (!_next) next = this;
    else next = _next;
}

void            HUDuiControl::setCallback(HUDuiCallback _cb, const void* _ud)
{
    cb = _cb;
    userData = _ud;
}

void            HUDuiControl::onSetFont()
{
    if (fontFace >= 0)
    {
        FontManager &fm = FontManager::instance();
        fontHeight = fm.getStrHeight(fontFace, fontSize, getLabel());
        trueLabelWidth = fm.getStrLength(fontFace, fontSize, getLabel() + "99");
    }
    else
    {
        fontHeight = 11.0f;
        trueLabelWidth = 0.0f;
    }
}

bool            HUDuiControl::hasFocus() const
{
    return this == HUDui::getFocus();
}

void            HUDuiControl::setFocus()
{
    HUDui::setFocus(this);
}

void            HUDuiControl::showFocus(bool _showingFocus)
{
    showingFocus = _showingFocus;
}

void            HUDuiControl::doCallback()
{
    if (cb) (*cb)(this, userData);
}

void            HUDuiControl::renderFocus()
{
    float fh2;

    TextureManager &tm = TextureManager::instance();
    const ImageInfo &info = tm.getInfo(arrow);

    if (gstate->isTextured())   // assumes there are w/h frames of animation h x h in each image
    {
        float imageSize = (float)info.y;
        int uFrames = 1;
        if (imageSize != 0)
            uFrames = int(info.x/imageSize); // 4;
        int vFrames = 1; // 4;
        float du = 1.0f / (float)uFrames;
        float dv = 1.0f / (float)vFrames;

        float u = (float)(arrowFrame % uFrames) / (float)uFrames;
        float v = (float)(arrowFrame / uFrames) / (float)vFrames;
        fh2 = floorf(1.5f * fontHeight) - 1.0f; // this really should not scale the image based on the font,
        gstate->setState();             // best would be to load an image for each size
        glColor3f(1.0f, 1.0f, 1.0f);
        float imageXShift = 0.5f;
        float imageYShift = -fh2 * 0.2f;
        float outputSize = fh2;
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(u, v);
        glVertex2f(x + imageXShift - outputSize, y + imageYShift);
        glTexCoord2f(u + du, v);
        glVertex2f(x + imageXShift, y + imageYShift);
        glTexCoord2f(u, v + dv);
        glVertex2f(x + imageXShift - outputSize, y + outputSize + imageYShift);
        glTexCoord2f(u + du, v + dv);
        glVertex2f(x + imageXShift, y + outputSize + imageYShift);
        glEnd();

        TimeKeeper nowTime = TimeKeeper::getCurrent();
        if (nowTime - lastTime > 0.07f)
        {
            lastTime = nowTime;
            if (++arrowFrame == uFrames * vFrames) arrowFrame = 0;
        }
    }
    else
    {
        fh2 = floorf(0.5f * fontHeight);
        gstate->setState();
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_TRIANGLES);
        glVertex2f(x - fh2 - fontHeight, y + fontHeight - 1.0f);
        glVertex2f(x - fh2 - fontHeight, y);
        glVertex2f(x - fh2 - 1.0f, y + 0.5f * (fontHeight - 1.0f));
        glEnd();

        glColor3f(0.0f, 0.0f, 0.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x - fh2 - fontHeight, y + fontHeight - 1.0f);
        glVertex2f(x - fh2 - fontHeight, y);
        glVertex2f(x - fh2 - 1.0f, y + 0.5f * (fontHeight - 1.0f));
        glEnd();
    }
}

void            HUDuiControl::renderLabel()
{
    std::string theLabel = getLabel();
    if (theLabel.length() > 0 && fontFace >= 0)
    {
        FontManager &fm = FontManager::instance();
        const float dx = (desiredLabelWidth > trueLabelWidth)
                         ? desiredLabelWidth : trueLabelWidth;
        fm.drawString(x - dx, y, 0, fontFace, fontSize, theLabel);
    }
}

void            HUDuiControl::render()
{
    if (hasFocus() && showingFocus) renderFocus();
    glColor(hasFocus() ? textColor : dimTextColor);
    renderLabel();
    doRender();
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
