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

#ifndef __HUDRENDERER_H__
#define __HUDRENDERER_H__

#include "common.h"

/* system interface headers */
#include <vector>
#include <string>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

/* common interface headers */
#include "TimeKeeper.h"
#include "HUDuiTypeIn.h"
#include "Flag.h"

/* local interface headers */
#include "FlashClock.h"
#include "MainWindow.h"
#include "BzfDisplay.h"
#include "SceneRenderer.h"
#include "Player.h"
#include "ScoreboardRenderer.h"
#include "LocalPlayer.h"

#include "OpenGLUtils.h"

const int       MaxAlerts = 3;
const int       HUDNumCracks = 8;
const int       HUDCrackLevels = 4;


class HUDMarker
{
public:
    float     heading;
    glm::vec3 color;
};
typedef std::vector<HUDMarker> MarkerList;


class EnhancedHUDMarker
{
public:
    EnhancedHUDMarker()
        : pos(0.0f, 0.0f, 0.0f)
        , color(0.0f, 0.0f, 0.0f)
        , friendly(false)
    {}
    EnhancedHUDMarker(const glm::vec3 &p, const glm::vec3 &c)
        : pos(p)
        , color(c)
        , friendly(false)
    {}
    glm::vec3 pos;
    glm::vec3 color;
    std::string name;
    bool friendly;
};
typedef std::vector<EnhancedHUDMarker> EnhancedMarkerList;


/**
 * HUDRenderer:
 *  Encapsulates information about rendering the heads-up display.
 */
class HUDRenderer
{
public:
    HUDRenderer(const BzfDisplay*, const SceneRenderer&);
    ~HUDRenderer();

    void drawGeometry();

    int           getNoMotionSize() const;
    int           getMaxMotionSize() const;

    void      setColor(float r, float g, float b);
    void      setPlaying(bool playing);
    void      setRoaming(bool roaming);
    void      setPlayerHasHighScore(bool = true);
    void      setTeamHasHighScore(bool = true);
    void      setHeading(float angle);
    void      setAltitude(float altitude);
    void      setAltitudeTape(bool = true);
    void      setFPS(float fps);
    void      setDrawTime(float drawTimeInseconds);
    void      setFrameTriangleCount(int tpf);
    void      setFrameRadarTriangleCount(int rtpf);
    void      setAlert(int num, const char* string, float duration,
                       bool warning = false);
    void      setFlagHelp(FlagType* desc, float duration);
    void      initCracks();
    void      setCracks(bool showCracks);
    void      addMarker(float heading, const glm::vec3 &color);
    void      setRestartKeyLabel(const std::string&);
    void      setTimeLeft(uint32_t timeLeftInSeconds);

    void      AddEnhancedMarker(const glm::vec3 &pos, const glm::vec3 &color,
                                bool friendly = false, float zShift = 0.0f);
    void      AddEnhancedNamedMarker(const glm::vec3 &pos, const glm::vec3 &color, std::string name,
                                     bool friendly = false, float zShift = 0.0f);
    void      AddLockOnMarker(const glm::vec3 &pos, std::string name,
                              bool friendly = false, float zShift = 0.0f);

    void      saveMatrixes(const glm::mat4 &mm, const glm::mat4 &pm);
    void      setDim(bool);

    bool      getComposing() const;
    std::string   getComposeString() const;
    void      setComposeString(const std::string &message) const;
    void      setComposeString(const std::string &message, bool _allowEdit) const;

    void      setComposing(const std::string &prompt);
    void      setComposing(const std::string &prompt, bool _allowEdit);

    void      render(SceneRenderer&);
    ScoreboardRenderer *getScoreboard();

protected:
    void      hudColor3Afv(const glm::vec3 &, const float);
    void      renderAlerts(void);
    void      renderStatus(void);
    void      renderCracks();
    void      renderOptions(SceneRenderer&);
    void      renderCompose(SceneRenderer&);
    void      renderBox(SceneRenderer&);
    void      renderTankLabels(SceneRenderer&);
    void      renderPlaying(SceneRenderer&);
    void      renderNotPlaying(SceneRenderer&);
    void      renderRoaming(SceneRenderer&);
    void      renderTimes(void);
    void      renderShots(const Player*);

    void      drawLockonMarker(const EnhancedHUDMarker &marker,
                               const glm::vec2         &viewPos);
    void      drawWaypointMarker(const EnhancedHUDMarker &marker,
                                 const glm::vec2       &viewPos);
    void      drawMarkersInView(int centerX, int centerY, const LocalPlayer* myTank);

    void      makeCrack(glm::vec3 crackpattern[HUDNumCracks][(1 << HUDCrackLevels) + 1], int n, int l, float a);
    std::string   makeHelpString(const char* help) const;

private:
    void      setBigFontSize(int width, int height);
    void      setAlertFontSize(int width, int height);
    void      setMajorFontSize(int width, int height);
    void      setMinorFontSize(int width, int height);
    void      setHeadingFontSize(int width, int height);
    void      setComposeFontSize(int width, int height);
    void      setLabelsFontSize(int width, int height);
    void      setOneToOnePrj();
    void      coverWhenBurrowed(const LocalPlayer &myTank);
    glm::vec2 getMarkerCoordinate(const glm::vec3 &pos,
                                  const glm::vec2 &viewPos);

    void      resize(bool firstTime);
    static void   resizeCallback(void*);

    const BzfDisplay* display;
    ScoreboardRenderer* scoreboard;
    MainWindow&       window;
    bool          firstRender;
    int           noMotionSize;
    int           maxMotionSize;
    float         headingOffset;
    glm::vec3     hudColor;
    glm::vec3     messageColor;
    glm::vec3     warningColor;

    int       bigFontFace;
    float     bigFontSize;
    int       alertFontFace;
    float     alertFontSize;
    int       majorFontFace;
    float     majorFontSize;
    int       minorFontFace;
    float     minorFontSize;
    int       headingFontFace;
    float     headingFontSize;
    int       composeFontFace;
    float     composeFontSize;
    int       labelsFontFace;
    float     labelsFontSize;
    float   majorFontHeight;
    float   alertFontHeight;

    bool      playing;
    bool      roaming;
    bool      dim;
    int       numPlayers;
    uint32_t  timeLeft;
    TimeKeeper    timeSet;
    bool      playerHasHighScore;
    bool      teamHasHighScore;
    float     heading;
    float     altitude;
    bool      altitudeTape;
    float     fps;
    float     minDrawTime;
    float     drawTime;
    float     drawTimeTmp;
    float     maxDrawTime;
    int       drawTimeCnt;
    float     headingMarkSpacing;
    float     headingLabelWidth[36];
    float     altitudeMarkSpacing;
    float     altitudeLabelMaxWidth;
    float     restartLabelWidth;
    float     resumeLabelWidth;
    float     autoPilotWidth;
    float     cancelDestructLabelWidth;
    float     gameOverLabelWidth;
    float     huntArrowWidth;
    float     huntedArrowWidth;
    float     tkWarnRatio;
    std::string   restartLabel;

    FlashClock        globalClock;
    FlashClock        scoreClock;

    FlashClock        alertClock[MaxAlerts];
    std::string       alertLabel[MaxAlerts];
    float     alertLabelWidth[MaxAlerts];
    glm::vec3 alertColor[MaxAlerts];

    float     flagHelpY;
    FlashClock        flagHelpClock;
    int           flagHelpLines;
    std::string       flagHelpText;

    bool      showOptions;
    bool      showCompose;

    glm::vec3 cracks[HUDNumCracks][(1 << HUDCrackLevels) + 1];
    TimeKeeper        crackStartTime;
    bool      showCracks;

    HUDuiTypeIn*  composeTypeIn;

    glm::mat4  modelMatrix;
    glm::mat4  projMatrix;
    glm::ivec4 viewport;

    MarkerList        markers;
    EnhancedMarkerList    enhancedMarkers;
    EnhancedMarkerList    lockOnMarkers;

    static const float    altitudeOffset;
    static std::string    headingLabel[36];
    static std::string    restartLabelFormat;
    static std::string    resumeLabel;
    static std::string    cancelDestructLabel;
    static std::string    gameOverLabel;
    static std::string    autoPilotLabel;
    bool          dater;
    unsigned int      lastTimeChange;
    int           triangleCount;
    int           radarTriangleCount;
};


#endif /* __HUDRENDERER_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
