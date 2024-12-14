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
#include "HUDuiList.h"

// common implementation headers
#include "BundleMgr.h"
#include "Bundle.h"
#include "FontManager.h"
#include "OpenGLAPI.h"

// local implementation headers
#include "HUDui.h"

//
// HUDuiList
//

HUDuiList::HUDuiList() : HUDuiControl(), index(-1)
{
    // do nothing
}

HUDuiList::~HUDuiList()
{
    // do nothing
}

int         HUDuiList::getIndex() const
{
    return index;
}

void            HUDuiList::setIndex(int _index)
{
    if (_index < 0) index = 0;
    else if (_index >= (int)list.size()) index = list.size() - 1;
    else index = _index;
}

std::vector<std::string>&       HUDuiList::getList()
{
    return list;
}

void            HUDuiList::update()
{
    setIndex(index);
}

void            HUDuiList::createSlider(const int numValues)
{
    // create a slider with numValues options
    /* createSlider(4) does the equivalent of
       options->push_back(std::string("[O---]"));
       options->push_back(std::string("[-O--]"));
       options->push_back(std::string("[--O-]"));
       options->push_back(std::string("[---O]"));
    */
    std::vector<std::string> &options = getList();

    std::string line(numValues + 2, '-');
    line[0] = '[';
    line[numValues + 1] = ']';

    for (int i = 0; i < numValues; i++)
    {
        if (i > 0) line[i] = '-';
        line[i + 1] = 'O';
        options.push_back(line);
    }
}

bool            HUDuiList::doKeyPress(const BzfKeyEvent& key)
{
    if (key.ascii == '\t')
    {
        HUDui::setFocus(getNext());
        return true;
    }

    if (key.ascii == 0)
        switch (key.button)
        {
        case BzfKeyEvent::Up:
            HUDui::setFocus(getPrev());
            break;

        case BzfKeyEvent::Down:
            HUDui::setFocus(getNext());
            break;

        case BzfKeyEvent::Left:
            if (index != -1)
            {
                if (--index < 0) index = list.size() - 1;
                doCallback();
            }
            break;

        case BzfKeyEvent::Right:
            if (index != -1)
            {
                if (++index >= (int)list.size()) index = 0;
                doCallback();
            }
            break;

        case BzfKeyEvent::Home:
            if (index != -1)
            {
                index = 0;
                doCallback();
            }
            break;

        case BzfKeyEvent::End:
            if (index != -1)
            {
                index = list.size() - 1;
                doCallback();
            }
            break;

        default:
            return false;
        }

    switch (key.ascii)
    {
    case 13:
    case 27:
        return false;
    }

    return true;
}

bool            HUDuiList::doKeyRelease(const BzfKeyEvent&)
{
    // ignore key releases
    return false;
}

void            HUDuiList::doRender()
{
    Bundle *bdl = BundleMgr::getCurrentBundle();
    if (index != -1 && getFontFace() >= 0)
    {
        glColor(hasFocus() ? textColor : dimTextColor);
        FontManager &fm = FontManager::instance();
        fm.drawString(getX(), getY(), 0, getFontFace(), getFontSize(), bdl->getLocalString(list[index]));
    }
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
