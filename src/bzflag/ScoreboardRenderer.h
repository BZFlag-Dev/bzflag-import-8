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

#ifndef __SCOREBOARDRENDERER_H__
#define __SCOREBOARDRENDERER_H__

#include "common.h"

/* system interface headers */
#include <string>
#include <vector>

/* common interface headers */
#include "bzfgl.h"

/* local interface headers */
#include "Player.h"


/**
 * ScoreboardRenderer:
 *  Encapsulates information about rendering the scoreboard display.
 */
class ScoreboardRenderer
{
public:
    ScoreboardRenderer();
    ~ScoreboardRenderer();

    void      setDim(bool);
    void    setWindowSize (float x, float y, float width, float height);
    void      render(bool forceDisplay);
    static Player* getLeader(std::string *label = NULL);

    static const int HUNT_NONE = 0;
    static const int HUNT_SELECTING = 1;
    static const int HUNT_ENABLED = 2;
    void    setHuntState(int _state);
    int         getHuntState() const;
    void      setHuntNextEvent ();    // invoked when 'down' button pressed
    void      setHuntPrevEvent ();    // invoked when 'up' button pressed
    void      setHuntSelectEvent ();      // invoked when 'fire' button pressed
    void    huntKeyEvent (bool isAdd);  // invoked when '7' or 'U' is pressed
    void    clearHuntedTanks ();
    void    huntReset ();        // invoked when joining a server

    static void    setAlwaysTeamScore (bool onoff);
    static bool    getAlwaysTeamScore ();

    static void    setSort (int _sortby);
    static int     getSort ();
    static const char **getSortLabels();
    static const int SORT_SCORE = 0;
    static const int SORT_NORMALIZED = 1;
    static const int SORT_REVERSE = 2;
    static const int SORT_CALLSIGN = 3;
    static const int SORT_TKS = 4;
    static const int SORT_TKRATIO = 5;
    static const int SORT_TEAM = 6;
    static const int SORT_MYRATIO = 7;
    static const int SORT_MAXNUM = SORT_MYRATIO;

    void setTeamScoreY ( float val )
    {
        teamScoreYVal = val;
    }
    void setRoaming ( bool val )
    {
        roaming = val;
    }

    // does not include observers
    static void getPlayerList(std::vector<Player*>& players);

protected:
    void hudColor3fv(const glm::vec3 &);
    void renderTeamScores (float y, float x, float dy);
    void renderScoreboard();
    void renderCtfFlags();
    void drawRoamTarget(float x0, float y0, float x1, float y1);
    void drawPlayerScore(const Player*,
                         float x1, float x2, float x3, float xs, float y,
                         int mottoLen, bool huntInd);
    static const char *sortLabels[SORT_MAXNUM+2];
    static int sortMode;
    static bool alwaysShowTeamScore;
    void   stringAppendNormalized (std::string *s, float n);

private:
    void setMinorFontSize(float height);
    void setLabelsFontSize(float height);
    static int teamScoreCompare(const void* _a, const void* _b);
    static int sortCompareCp(const void* _a, const void* _b);
    static int sortCompareI2(const void* _a, const void* _b);
    static Player** newSortedList(int sortType, bool obsLast, int *_numPlayers=NULL);
    void exitSelectState (void);

private:
    float winX;
    float winY;
    float winWidth;
    float winHeight;

    bool dim;
    int huntPosition;
    bool huntSelectEvent;
    int huntPositionEvent;
    int huntState;
    bool huntAddMode;    // valid only if state == SELECTING
    float teamScoreYVal;
    bool roaming;

    glm::vec3 messageColor;
    int minorFontFace;
    float minorFontSize;
    int labelsFontFace;
    float labelsFontSize;

    float scoreLabelWidth;
    float killsLabelWidth;
    float teamScoreLabelWidth;
    float teamCountLabelWidth;
    float huntArrowWidth;
    float huntPlusesWidth;
    float huntedArrowWidth;
    float tkWarnRatio;

    static std::string scoreSpacingLabel;
    static std::string scoreLabel;
    static std::string killSpacingLabel;
    static std::string killLabel;
    static std::string teamScoreSpacingLabel;
    static std::string playerLabel;
    static std::string teamCountSpacingLabel;
    int numHunted;
};


#endif /* __SCOREBOARDRENDERER_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
