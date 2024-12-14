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

/*
 * main game loop stuff
 */

#ifndef BZF_PLAYING_H
#define BZF_PLAYING_H

#include "common.h"

// system includes
#include <string>
#include <vector>

/* common headers */
#include "StartupInfo.h"
#include "AutoCompleter.h"

/* local headers */
#include "MainWindow.h"
#include "ShotStats.h"
#include "ControlPanel.h"
#include "HUDRenderer.h"

#include "Protocol.h"

#define MAX_MESSAGE_HISTORY (20)

typedef void        (*JoinGameCallback)(bool success, void* data);
typedef void        (*ConnectStatusCallback)(std::string& str);
typedef void        (*PlayingCallback)(void*);
struct PlayingCallbackItem
{
public:
    PlayingCallback cb;
    void*       data;
};

class BzfDisplay;
class MainWindow;
class SceneRenderer;
class Player;
class ServerLink;

BzfDisplay*     getDisplay();
MainWindow*     getMainWindow();
ShotStats*      getShotStats();
SceneRenderer*      getSceneRenderer();
void            setSceneDatabase();
StartupInfo*        getStartupInfo();
void            notifyBzfKeyMapChanged();
bool            setVideoFormat(int, bool test = false);
void            startPlaying(BzfDisplay* display,
                             SceneRenderer&);

bool            addExplosion(const glm::vec3 &pos,
                             float size, float duration);
void            addTankExplosion(const glm::vec3 &pos);
void            addShotExplosion(const glm::vec3 &pos);
void            addShotPuff(const glm::vec3 &pos, float azimuth, float elevation);
void            warnAboutMainFlags();
void            warnAboutRadarFlags();
void            warnAboutRadar();
void            warnAboutConsole();
void            addPlayingCallback(PlayingCallback, void* data);
void            removePlayingCallback(PlayingCallback, void* data);

void            joinGame(JoinGameCallback, void* userData);
void            leaveGame();
std::vector<std::string>& getSilenceList();
void            updateEvents();
void            addMessage(const Player* player,
                           const std::string& msg,
                           int mode = 3,
                           bool highlight = false,
                           const char* oldColor = NULL);

int         curlProgressFunc(void* clientp,
                             double dltotal, double dlnow,
                             double ultotal, double ulnow);

void selectNextRecipient (bool forward, bool robotIn);
void handleFlagDropped(Player* tank);
void setTarget();
bool shouldGrabMouse();
void setRoamingLabel();
void drawFrame(const float dt);

void initGlobalAres();
void killGlobalAres();

extern void joinGame();

extern void applyJSModifiers(float& jsx, float& jsy);

extern HUDRenderer  *hud;
extern char     messageMessage[PlayerIdPLen + MessageLen];
extern ServerLink*  serverLink;
extern int      numFlags;
extern StartupInfo  startupInfo;
extern DefaultCompleter completer;
extern bool      gameOver;
extern ControlPanel    *controlPanel;
extern bool      fireButton;
extern float        destructCountdown;
extern bool      pausedByUnmap;
extern int        savedVolume;
extern MainWindow      *mainWindow;
extern float        pauseCountdown;
extern float        clockAdjust;
extern float        roamDZoom;
extern bool      roamButton;

#define MAX_DT_LIMIT 0.1f
#define MIN_DT_LIMIT 0.001f

enum BlowedUpReason
{
    GotKilledMsg,
    GotShot,
    GotRunOver,
    GotCaptured,
    GenocideEffect,
    SelfDestruct,
    WaterDeath,
    LastReason,
    DeathTouch = PhysicsDriverDeath
};

#endif // BZF_PLAYING_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
