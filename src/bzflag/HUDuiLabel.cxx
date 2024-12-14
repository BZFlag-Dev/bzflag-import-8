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

// interface headers
#include "HUDuiLabel.h"

// common implementation headers
#include "BundleMgr.h"
#include "Bundle.h"
#include "FontManager.h"

// local implementation headers
#include "HUDui.h"

//
// HUDuiLabel
//

HUDuiLabel::HUDuiLabel() : HUDuiControl()
{
    darker = false;
    params = NULL;
    color = textColor;
}

HUDuiLabel::~HUDuiLabel()
{
    if (params)
        delete params;
}

std::string     HUDuiLabel::getString() const
{
    std::string theString;
    Bundle *bdl = BundleMgr::getCurrentBundle();
    if (params)
        theString = bdl->formatMessage(string, params);
    else
        theString = bdl->getLocalString(string);

    return theString;
}

void            HUDuiLabel::setString(const std::string& _string, const std::vector<std::string> *_params)
{
    string = _string;
    if (_params)
    {
        if (params != NULL)
            delete params;

        params = new std::vector<std::string>(*_params);
    }
    onSetFont();
}

void            HUDuiLabel::onSetFont()
{
    HUDuiControl::onSetFont();
}

bool            HUDuiLabel::doKeyPress(const BzfKeyEvent& key)
{
    if (key.ascii == 0) switch (key.button)
        {
        case BzfKeyEvent::Up:
            HUDui::setFocus(getPrev());
            break;

        case BzfKeyEvent::Down:
            HUDui::setFocus(getNext());
            break;

        default:
            return false;
        }

    if (key.ascii == '\t')
    {
        HUDui::setFocus(getNext());
        return true;
    }

    switch (key.ascii)
    {
    case 13:
    case 27:
        return false;
    }
    return true;
}

bool            HUDuiLabel::doKeyRelease(const BzfKeyEvent&)
{
    return false;
}

void            HUDuiLabel::setDarker(bool d)
{
    darker = d;
}

void            HUDuiLabel::setColor(GLfloat r, GLfloat g, GLfloat b)
{
    color = glm::vec3(r, g, b);
}

void            HUDuiLabel::doRender()
{
    if (getFontFace() < 0)
        return;
    // render string
    FontManager &fm = FontManager::instance();
    float darkness;
    if (hasFocus())
        darkness = 1.0f;
    else if (!darker)
        darkness = 0.7f;
    else
        darkness = 0.4f;
    fm.setDarkness(darkness);
    fm.drawString(getX(), getY(), 0,
                  getFontFace(), getFontSize(),
                  getString(), color);
    fm.setDarkness(1.0f);
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
