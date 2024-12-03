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

/* interface header */
#include "GUIOptionsMenu.h"

/* common implementation headers */
#include "BZDBCache.h"
#include "TextUtils.h"
#include "FontManager.h"

/* local implementation headers */
#include "MainMenu.h"
#include "HUDDialogStack.h"
#include "HUDuiList.h"
#include "HUDuiLabel.h"
#include "ScoreboardRenderer.h"
#include "playing.h"

GUIOptionsMenu::GUIOptionsMenu()
{
    // add controls
    std::vector<HUDuiControl*>& listHUD = getControls();

    // cache font face ID
    int fontFace = MainMenu::getFontFace();

    HUDuiLabel* label = new HUDuiLabel;
    label->setFontFace(fontFace);
    label->setString("GUI Settings");
    listHUD.push_back(label);

    HUDuiList* option;

    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Scoreboard Font Size:");
    option->setCallback(callback, "S");
    std::vector<std::string>* options = &option->getList();
    options->push_back(std::string("Auto"));
    option->createSlider(4);
    option->update();
    listHUD.push_back(option);

    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Scoreboard Sort:");
    option->setCallback(callback, "p");
    options = &option->getList();
    const char **sortLabels = ScoreboardRenderer::getSortLabels();
    while (*sortLabels != NULL)
        options->push_back(std::string(*sortLabels++));
    option->update();
    listHUD.push_back(option);

    // set motto display length
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Motto Display Length:");
    option->setCallback(callback, "E");
    option->createSlider(32 + 1);
    option->update();
    listHUD.push_back(option);


    // set Radar Translucency
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Radar Opacity:");
    option->setCallback(callback, "Y");
    option->createSlider(11);
    option->update();
    listHUD.push_back(option);

    // set radar size
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Radar Size:");
    option->setCallback(callback, "R");
    option->createSlider(maxRadarSize + 1);
    option->update();
    listHUD.push_back(option);

    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Radar Style:");
    option->setCallback(callback, "e");
    options = &option->getList();
    options->push_back(std::string("Normal"));
    options->push_back(std::string("Fast"));
    options->push_back(std::string("Fast Sorted"));
    options->push_back(std::string("Enhanced"));
    option->update();
    listHUD.push_back(option);

    // set radar shot size
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Radar Shot Size:");
    option->setCallback(callback, "s");
    option->createSlider(11);
    option->update();
    listHUD.push_back(option);

    // set radar shot length
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Radar Shot Length:");
    option->setCallback(callback, "l");
    option->createSlider(11);
    option->update();
    listHUD.push_back(option);

    // radar shot leading line
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Radar Shot Line:");
    option->setCallback(callback, "F");
    options = &option->getList();
    options->push_back(std::string("Lagging"));
    options->push_back(std::string("Leading"));
    options->push_back(std::string("Leading & Lagging"));
    option->update();
    listHUD.push_back(option);

    // radar marker for forward direction
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Radar Forward Marker:");
    option->setCallback(callback, "m");
    options = &option->getList();
    options->push_back(std::string("Tick"));
    options->push_back(std::string("Line"));
    option->update();
    listHUD.push_back(option);

    // set radar position
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Radar Position:");
    option->setCallback(callback, "P");
    options = &option->getList();
    options->push_back(std::string("Left"));
    options->push_back(std::string("Right"));
    option->update();
    listHUD.push_back(option);

    // toggle coloring of shots on radar
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Colored shots on radar:");
    option->setCallback(callback, "z");
    options = &option->getList();
    options->push_back(std::string("Off"));
    options->push_back(std::string("On"));
    option->update();
    listHUD.push_back(option);



    // set Panel Translucency
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Panel Opacity:");
    option->setCallback(callback, "y");
    option->createSlider(11);
    option->update();
    listHUD.push_back(option);

    // set panel size
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Panel Height:");
    option->setCallback(callback, "a");
    option->createSlider(maxRadarSize + 1);
    option->update();
    listHUD.push_back(option);

    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("ControlPanel Font Size:");
    option->setCallback(callback, "C");
    options = &option->getList();
    options->push_back(std::string("Auto"));
    option->createSlider(4);
    option->update();
    listHUD.push_back(option);

    // Tabs
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Control panel tabs:");
    option->setCallback(callback, "t");
    options = &option->getList();
    options->push_back(std::string("Off"));
    options->push_back(std::string("Left"));
    options->push_back(std::string("Right"));
    option->update();
    listHUD.push_back(option);

    // Automatic pausing of the console
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Automatically pause the control panel:");
    option->setCallback(callback, "o");
    options = &option->getList();
    options->push_back(std::string("Off"));
    options->push_back(std::string("On"));
    option->update();
    listHUD.push_back(option);

    // Time/date display settings
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Timestamps in console:");
    option->setCallback(callback, "Z");
    options = &option->getList();
    options->push_back(std::string("Off"));
    options->push_back(std::string("Time"));
    options->push_back(std::string("Date and Time"));
    option->update();
    listHUD.push_back(option);

    // GUI coloring
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Control panel coloring:");
    option->setCallback(callback, "c");
    options = &option->getList();
    options->push_back(std::string("Off"));
    options->push_back(std::string("On"));
    option->update();
    listHUD.push_back(option);

    // Underline color
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Underline color:");
    option->setCallback(callback, "u");
    options = &option->getList();
    options->push_back(std::string("Cyan"));
    options->push_back(std::string("Grey"));
    options->push_back(std::string("Text"));
    option->update();
    listHUD.push_back(option);

    // Killer Highlight
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Killer Highlight:");
    option->setCallback(callback, "k");
    options = &option->getList();
    options->push_back(std::string("None"));
    options->push_back(std::string("Pulsating"));
    options->push_back(std::string("Underline"));
    option->update();
    listHUD.push_back(option);

    // Pulsate Rate
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Pulsation Rate:");
    option->setCallback(callback, "r");
    option->createSlider(9);
    option->update();
    listHUD.push_back(option);

    // Pulsate Depth
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Pulsation Depth:");
    option->setCallback(callback, "d");
    option->createSlider(9);
    option->update();
    listHUD.push_back(option);



    // Time/date display settings
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Time / Date Display:");
    option->setCallback(callback, "h");
    options = &option->getList();
    options->push_back(std::string("Time"));
    options->push_back(std::string("Date"));
    options->push_back(std::string("Date and Time"));
    option->update();
    listHUD.push_back(option);

    // HUD Reload timer
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Reload timer on HUD:");
    option->setCallback(callback, "T");
    options = &option->getList();
    options->push_back(std::string("Off"));
    options->push_back(std::string("On"));
    option->update();
    listHUD.push_back(option);

    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Always Show Team Scores:");
    option->setCallback(callback, "q");
    options = &option->getList();
    options->push_back(std::string("Off"));
    options->push_back(std::string("On"));
    option->update();
    listHUD.push_back(option);

    // set observer info
    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Extended Observer Info:");
    option->setCallback(callback, "O");
    options = &option->getList();
    options->push_back(std::string("Off"));
    options->push_back(std::string("On"));
    options->push_back(std::string("On With Apparent Speeds"));
    options->push_back(std::string("Full"));
    option->update();
    listHUD.push_back(option);

    option = new HUDuiList;
    option->setFontFace(fontFace);
    option->setLabel("Show Coordinates:");
    option->setCallback(callback, "D");
    options = &option->getList();
    options->push_back(std::string("Off"));
    options->push_back(std::string("On"));
    option->update();
    listHUD.push_back(option);

    initNavigation(listHUD, 1, listHUD.size()-1);
}

GUIOptionsMenu::~GUIOptionsMenu()
{
}

void            GUIOptionsMenu::execute()
{
}

void            GUIOptionsMenu::resize(int _width, int _height)
{
    HUDDialog::resize(_width, _height);

    // use a big font for title, smaller font for the rest
    const float titleFontSize = (float)_height / 15.0f;
    const float fontSize = (float)_height / 70.0f;
    FontManager &fm = FontManager::instance();

    // reposition title
    std::vector<HUDuiControl*>& listHUD = getControls();
    HUDuiLabel* title = (HUDuiLabel*)listHUD[0];
    title->setFontSize(titleFontSize);
    const float titleWidth = fm.getStrLength(MainMenu::getFontFace(), titleFontSize, title->getString());
    const float titleHeight = fm.getStrHeight(MainMenu::getFontFace(), titleFontSize, " ");
    float x = 0.5f * ((float)_width - titleWidth);
    float y = (float)_height - titleHeight;
    title->setPosition(x, y);

    // reposition options
    x = 0.54f * (float)_width;
    y -= 0.6f * titleHeight;
    const float h = fm.getStrHeight(MainMenu::getFontFace(), fontSize, " ");
    const int count = listHUD.size();
    for (int i = 1; i < count; i++)
    {
        listHUD[i]->setFontSize(fontSize);
        listHUD[i]->setPosition(x, y);
        // Add extra space after Motto Display Length, Colored shots on radar, and Pulsation Depth
        if (i == 3 || i == 11 || i == 22)
            y -= 1.75f * h;
        else
            y -= 1.0f * h;
    }

    // load current settings
    SceneRenderer* renderer = getSceneRenderer();
    if (renderer)
    {
        int i = 1;
        // Scoreboard Font Size
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("scorefontsize")));
        // Scoreboard Sort
        ((HUDuiList*)listHUD[i++])->setIndex(ScoreboardRenderer::getSort());
        // Motto Display Length
        if (BZDB.isTrue("hideMottos"))
            ((HUDuiList*)listHUD[i++])->setIndex(0);
        else
            ((HUDuiList*)listHUD[i++])->setIndex(BZDB.evalInt("mottoDispLen") / 4);

        // Radar Opacity
        ((HUDuiList*)listHUD[i++])->setIndex((int)(10.0f * renderer->getRadarOpacity() + 0.5));
        // Radar Size
        ((HUDuiList*)listHUD[i++])->setIndex(renderer->getRadarSize());
        // Radar Style
        ((HUDuiList*)listHUD[i++])->setIndex(BZDBCache::radarStyle);
        // Radar Shot Size
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("sizedradarshots")));
        // Radar Shot Length
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("linedradarshots")));
        // Radar Shot Line
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("leadingShotLine")));
        // Radar Forward Marker
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("radarForwardMarker")));
        // Radar Position
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("radarPosition")));
        // Colored shots on radar
        ((HUDuiList*)listHUD[i++])->setIndex(BZDB.isTrue("coloredradarshots") ? 1 : 0);


        // Panel Opacity
        ((HUDuiList*)listHUD[i++])->setIndex((int)(10.0f * renderer->getPanelOpacity() + 0.5));
        // Panel Height
        ((HUDuiList*)listHUD[i++])->setIndex(renderer->getPanelHeight());
        // ControlPanel Font Size
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("cpanelfontsize")));
        // Control Panel Tabs
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("showtabs")));
        // Automatically pause the computer panel
        ((HUDuiList*)listHUD[i++])->setIndex(BZDB.isTrue("pauseConsole") ? 1 : 0);
        // Timestamps in console
        ((HUDuiList*)listHUD[i++])->setIndex(BZDB.evalInt("controlPanelTimestamp"));
        // Control panel coloring
        ((HUDuiList*)listHUD[i++])->setIndex(BZDB.isTrue("colorful") ? 1 : 0);
        // Underline color - find index of mode string in options
        const std::vector<std::string> &opts = ((HUDuiList*)listHUD[i])->getList();
        std::string uColor = BZDB.get("underlineColor");
        ((HUDuiList*)listHUD[i++])->setIndex(std::find(opts.begin(), opts.end(), uColor) - opts.begin());
        // Killer Highlight
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("killerhighlight")));
        // Pulsation Rate
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("pulseRate") * 5) - 1);
        // Pulsating Depth
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("pulseDepth") * 10) - 1);


        // Time / Date Display
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("timedate")));
        // Reload timer on HUD
        ((HUDuiList*)listHUD[i++])->setIndex(BZDB.isTrue("displayReloadTimer") ? 1 : 0);
        // Always Show Team Scores
        ((HUDuiList*)listHUD[i++])->setIndex(ScoreboardRenderer::getAlwaysTeamScore());
        // Extended Observer Info
        ((HUDuiList*)listHUD[i++])->setIndex(static_cast<int>(BZDB.eval("showVelocities")));
        // Show coordinates
        ((HUDuiList*)listHUD[i++])->setIndex(BZDB.isTrue("showCoordinates") ? 1 : 0);
    }
}

void            GUIOptionsMenu::callback(HUDuiControl* w, const void* data)
{
    HUDuiList* list = (HUDuiList*)w;

    SceneRenderer* sceneRenderer = getSceneRenderer();
    switch (((const char*)data)[0])
    {
    case 'e':
        BZDB.setInt("radarStyle", list->getIndex());
        break;

    case 'C':
    {
        BZDB.setInt("cpanelfontsize", list->getIndex());
        getMainWindow()->getWindow()->callResizeCallbacks();
        break;
    }

    case 'h':
    {
        BZDB.setInt("timedate", list->getIndex());
        break;
    }

    case 'Z':
    {
        BZDB.setInt("controlPanelTimestamp", list->getIndex());
        break;
    }

    case 'S':
    {
        BZDB.setInt("scorefontsize", list->getIndex());
        getMainWindow()->getWindow()->callResizeCallbacks();
        break;
    }

    case 'O':
    {
        BZDB.setInt("showVelocities", list->getIndex());
        getMainWindow()->getWindow()->callResizeCallbacks();
        break;
    }

    case 'y':
    {
        const float newOpacity = (float)list->getIndex() / 10.0f;
        if (newOpacity == 1.0f || sceneRenderer->getRadarOpacity() == 1.0f)
            sceneRenderer->setRadarOpacity(newOpacity);
        sceneRenderer->setPanelOpacity(newOpacity);
        break;
    }

    case 'Y':
    {
        const float newOpacity = (float)list->getIndex() / 10.0f;
        sceneRenderer->setRadarOpacity(newOpacity);
        if (newOpacity == 1.0f || sceneRenderer->getPanelOpacity() == 1.0f)
            sceneRenderer->setPanelOpacity(newOpacity);
        break;
    }

    case 'z':
        BZDB.set("coloredradarshots", list->getIndex() ? "1" : "0");
        break;

    case 'l':
        BZDB.set("linedradarshots", TextUtils::format("%d", list->getIndex()));
        break;

    case 's':
        BZDB.set("sizedradarshots", TextUtils::format("%d", list->getIndex()));
        break;

    case 'F':
        BZDB.setInt("leadingShotLine", list->getIndex());
        break;

    case 'm':
        BZDB.setInt("radarForwardMarker", list->getIndex());
        break;

    case 'P':
        BZDB.setInt("radarPosition", list->getIndex());
        controlPanel->resize();
        break;

    case 'R':
    {
        BZDB.setInt("radarsize", list->getIndex());
        break;
    }

    case 'a':
    {
        BZDB.setInt("panelheight", list->getIndex());
        break;
    }

    case 'o':
    {
        BZDB.set("pauseConsole", list->getIndex() ? "1" : "0");
        break;
    }

    case 'c':
    {
        BZDB.set("colorful", list->getIndex() ? "1" : "0");
        break;
    }

    case 't':
    {
        BZDB.set("showtabs", TextUtils::format("%d", list->getIndex()));
        break;
    }

    case 'u':
    {
        std::vector<std::string>* options = &list->getList();
        std::string color = (*options)[list->getIndex()];
        BZDB.set("underlineColor", color);
        break;
    }

    case 'k':
    {
        BZDB.set("killerhighlight", TextUtils::format("%d", list->getIndex()));
        break;
    }

    case 'r':
    {
        BZDB.set("pulseRate", TextUtils::format("%f", (float)(list->getIndex() + 1) / 5.0f));
        break;
    }

    case 'd':
    {
        BZDB.set("pulseDepth", TextUtils::format("%f", (float)(list->getIndex() + 1) / 10.0f));
        break;
    }

    case 'T':
    {
        BZDB.set("displayReloadTimer", list->getIndex() ? "1" : "0");
        break;
    }

    case 'p':
        ScoreboardRenderer::setSort(list->getIndex());
        break;

    case 'q':
        ScoreboardRenderer::setAlwaysTeamScore(list->getIndex() ? true : false);
        break;

    case 'E':
        BZDB.set("mottoDispLen",  TextUtils::format("%d", list->getIndex() * 4));
        BZDB.set("hideMottos", list->getIndex() ? "0" : "1");
        break;
    case 'D':
    {
        BZDB.setBool("showCoordinates", list->getIndex() == 1);
    }


    }
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
