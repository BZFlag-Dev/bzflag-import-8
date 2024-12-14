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

#ifndef BZF_CONTROL_PANEL_H
#define BZF_CONTROL_PANEL_H

// common - 1st
#include "common.h"

// system headers
#include <string>
#include <vector>
#include <deque>
#include <glm/vec3.hpp>

//common headers
#include "bzfgl.h"

// local headers
#include "MainWindow.h"

class RadarRenderer;
class SceneRenderer;

struct ControlPanelMessage
{
public:
    ControlPanelMessage(const std::string&);
    std::string     getString() const;
    std::string     getString(bool withDateTime) const;
    std::string     formatTimestamp(int mode) const;
    void        breakLines(float maxLength, int fontFace, float fontSize);

    std::vector<std::string>    lines;
    int numlines;

private:
    std::string     string;

    // Message timestamp variables
    int year, month, day, hour, min, sec;
};

class ControlPanel
{
public:
    ControlPanel(MainWindow&, SceneRenderer&);
    ~ControlPanel();

    void setControlColor(const glm::vec3 &color = glm::vec3(0.0f));
    void        render(SceneRenderer&);
    void        resize();
    void        invalidate();

    void        setNumberOfFrameBuffers(int);

    void        addMessage(const std::string&, const int mode = 3);
    static void addMutexMessage(const std::string&);
    void        setMessagesOffset(int offset, int whence, bool paged);
    void        setMessagesMode(int _messageMode);
    int     getMessagesMode()
    {
        return messageMode;
    };
    void        setStatus(const char*);
    void        setRadarRenderer(RadarRenderer*);

    void        setDimming(float dimming);

    void        togglePaused();

    void        saveMessages(const std::string& filename,
                             bool stripAnsi, bool timestamp) const;

private:
    // no copying!
    ControlPanel(const ControlPanel&);
    ControlPanel&   operator=(const ControlPanel&);

    static void     resizeCallback(void*);
    static void     exposeCallback(void*);
    static void     bzdbCallback(const std::string& name, void* data);

    enum MessageModes
    {
        MessageAllTabs = -2,
        MessageCurrent = -1,
        MessageAll     = 0,
        MessageChat    = 1,
        MessageServer  = 2,
        MessageMisc    = 3,
        MessageModeCount
    };
    bool tabsOnRight;
    std::vector<const char *> *tabs;
    std::vector<float> tabTextWidth;
    long totalTabWidth;


    MainWindow&     window;
    bool        resized;
    int         numBuffers;
    int         changedMessage;
    RadarRenderer*  radarRenderer;
    SceneRenderer*  renderer;

    int         fontFace;
    float       fontSize;

    float       dimming;
    float       du, dv;
    int         radarAreaPixels[4];
    int         messageAreaPixels[4];
    std::deque<ControlPanelMessage> messages[MessageModeCount];
    int messageMode;
    glm::vec3   teamColor;
    static int      messagesOffset;
    static const int    maxScrollPages;
    int         maxLines;
    float       margin;
    float       lineHeight;
    bool        unRead[MessageModeCount];
    bool        paused;

};

inline void ControlPanel::setDimming(float newDimming)
{
    const float newDim = 1.0f - newDimming;
    dimming = (newDim > 1.0f) ? 1.0f : (newDim < 0.0f) ? 0.0f : newDim;
}

inline void ControlPanel::togglePaused()
{
    paused = !paused;
}


#endif // BZF_CONTROL_PANEL_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
