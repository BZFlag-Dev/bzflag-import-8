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
#include "playing.h"

// system includes
#ifdef _WIN32
#include <shlobj.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#else
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <utime.h>
#endif
#include <cmath>

// common headers
#include "AccessList.h"
#include "AnsiCodes.h"
#include "AresHandler.h"
#include "BackgroundRenderer.h"
#include "BaseBuilding.h"
#include "BillboardSceneNode.h"
#include "BZDBCache.h"
#include "BzfMedia.h"
#include "bzsignal.h"
#include "CommandsStandard.h"
#include "DirectoryNames.h"
#include "ErrorHandler.h"
#include "FileManager.h"
#include "FlagSceneNode.h"
#include "GameTime.h"
#include "KeyManager.h"
#include "md5.h"
#include "ObstacleList.h"
#include "ObstacleMgr.h"
#include "OpenGLFramebuffer.h"
#include "PhysicsDriver.h"
#include "PlatformFactory.h"
#include "QuadWallSceneNode.h"
#include "ServerAuth.h"
#include "SphereSceneNode.h"
#include "TankGeometryMgr.h"
#include "Team.h"
#include "TextureManager.h"
#include "TextUtils.h"
#include "version.h"
#include "WordFilter.h"
#include "ZSceneDatabase.h"

// local implementation headers
#include "AutoPilot.h"
#include "bzflag.h"
#include "commands.h"
#include "daylight.h"
#include "Downloads.h"
#include "effectsRenderer.h"
#include "FlashClock.h"
#include "ForceFeedback.h"
#include "LocalPlayer.h"
#include "HUDDialogStack.h"
#include "HUDRenderer.h"
#include "MainMenu.h"
#include "motd.h"
#include "RadarRenderer.h"
#include "Roaming.h"
#include "RobotPlayer.h"
#include "Roster.h"
#include "SceneBuilder.h"
#include "ScoreboardRenderer.h"
#include "sound.h"
#include "ShotStats.h"
#include "TrackMarks.h"
#include "World.h"
#include "WorldBuilder.h"
#include "HUDui.h"

#include "CollisionManager.h"

#include <sstream>

//#include "messages.h"

static const float  FlagHelpDuration = 60.0f;
StartupInfo     startupInfo;
static MainMenu     *mainMenu;
ServerLink      *serverLink = NULL;
static World        *world = NULL;
static LocalPlayer  *myTank = NULL;
static BzfDisplay   *display = NULL;
MainWindow      *mainWindow = NULL;
static SceneRenderer    *sceneRenderer = NULL;
ControlPanel        *controlPanel = NULL;
static RadarRenderer    *radar = NULL;
HUDRenderer     *hud = NULL;
static ScoreboardRenderer *scoreboard = NULL;
static ShotStats    *shotStats = NULL;
static SceneDatabaseBuilder* sceneBuilder = NULL;
static Team     *teams = NULL;
int         numFlags = 0;
static bool     joinRequested = false;
static bool     waitingDNS    = false;
static bool     serverError = false;
static bool     serverDied = false;
bool            fireButton = false;
bool            roamButton = false;
static bool     firstLife = false;
static bool     showFPS = false;
static bool     showDrawTime = false;
bool            pausedByUnmap = false;
static bool     unmapped = false;
static int      preUnmapFormat = -1;
static double       epochOffset;
static double       lastEpochOffset;
float           clockAdjust = 0.0f;
float           pauseCountdown = 0.0f;
float           destructCountdown = 0.0f;
static float        testVideoFormatTimer = 0.0f;
static int      testVideoPrevFormat = -1;
static std::vector<PlayingCallbackItem> playingCallbacks;
bool            gameOver = false;
static std::vector<BillboardSceneNode*> explosions;
static std::vector<BillboardSceneNode*> prototypeExplosions;
int         savedVolume = -1;
static bool     grabMouseAlways = false;
static FlashClock   pulse;
static bool     wasRabbit = false;
static bool     justJoined = false;

int         sentForbidIdentify = 0;

float           roamDZoom = 0.0f;

static MessageOfTheDay  *motd = NULL;
DefaultCompleter    completer;

char            messageMessage[PlayerIdPLen + MessageLen];

double          lastObserverUpdateTime = -1;

static void     setHuntTarget();
static void     setTankFlags();
static const void   *handleMsgSetVars(const void *msg);
static void     handlePlayerMessage(uint16_t, uint16_t, const void*);
static void     handleFlagTransferred(Player* fromTank, Player* toTank, int flagIndex);
static void     enteringServer(const void *buf);
static void     joinInternetGame2();
static void     cleanWorldCache();
static void     markOld(std::string &fileName);
#ifdef ROBOT
static void     setRobotTarget(RobotPlayer* robot);
#endif

static ResourceGetter   *resourceDownloader = NULL;

// Far and Near Frustum clipping planes
static const float FarPlaneScale = 1.5f; // gets multiplied by BZDB_WORLDSIZE
static const float FarPlaneDefault = FarPlaneScale * 800.0f;
static const float FarDeepPlaneScale = 10.0f;
static const float FarDeepPlaneDefault = FarPlaneDefault * FarDeepPlaneScale;
static const float NearPlaneNormal = 1.0f;
static const float NearPlaneClose = 0.25f; // for drawing in the cockpit
static bool FarPlaneCull = false;
static float FarPlane = FarPlaneDefault;
static float FarDeepPlane = FarDeepPlaneDefault;
static float NearPlane = NearPlaneNormal;

static bool leftMouseButton   = false;
static bool rightMouseButton  = false;
static bool middleMouseButton = false;

static const char*  blowedUpMessage[] =
{
    NULL,
    "Got shot by ",
    "Got flattened by ",
    "Team flag was captured by ",
    "Teammate hit with Genocide by ",
    "Tank Self Destructed",
    "Tank Rusted"
};
static bool     gotBlowedUp(BaseLocalPlayer* tank,
                            BlowedUpReason reason,
                            PlayerId killer,
                            const ShotPath *hit = NULL,
                            int physicsDriver = -1);

#ifdef ROBOT
static void     handleMyTankKilled(int reason);
static ServerLink   *robotServer[MAX_ROBOTS];
#endif

static double       userTimeEpochOffset;

static bool     entered = false;
static bool     joiningGame = false;
static WorldBuilder *worldBuilder = NULL;
static std::string  worldUrl;
static std::string  worldCachePath;
static std::string  md5Digest;
static uint32_t     worldPtr = 0;
static char     *worldDatabase = NULL;
static bool     isCacheTemp;
static std::ostream *cacheOut = NULL;
static bool     downloadingInitialTexture = false;

static AresHandler* ares = NULL;
void initGlobalAres()
{
    ares = new AresHandler(0);
}
void killGlobalAres()
{
    delete ares;
    ares = NULL;
}
static Address serverNetworkAddress = Address();

OpenGLFramebuffer glFramebuffer;

static AccessList   ServerAccessList("ServerAccess.txt", NULL);

// access silencePlayers from bzflag.cxx
std::vector<std::string>& getSilenceList()
{
    return silencePlayers;
}

// try to select the next recipient in the specified direction
// eventually avoiding robots
void selectNextRecipient (bool forward, bool robotIn)
{
    LocalPlayer *my = LocalPlayer::getMyTank();
    const Player *recipient = my->getRecipient();
    int rindex;
    if (!recipient)
    {
        rindex = - 1;
        forward = true;
    }
    else
    {
        const PlayerId id = recipient->getId();
        rindex = lookupPlayerIndex(id);
    }
    int i = rindex;
    while (true)
    {
        if (forward)
        {
            i++;
            if (i == curMaxPlayers)
            {
                // if no old rec id we have just ended our search
                if (recipient == NULL)
                    break;
                else
                    // wrap around
                    i = 0;
            }
        }
        else
        {
            if (i == 0)
                // wrap around
                i = curMaxPlayers;
            i--;
        }
        if (i == rindex)
            break;
        if (remotePlayers[i] && (robotIn || remotePlayers[i]->getPlayerType() == TankPlayer))
        {
            my->setRecipient(remotePlayers[i]);
            break;
        }
    }
}

//
// should we grab the mouse?
//

static void     setGrabMouse(bool grab)
{
    grabMouseAlways = grab;
}

bool        shouldGrabMouse()
{
#if defined(_WIN32) && defined(DEBUG)
    return false;
#endif
    return grabMouseAlways && !unmapped &&
           (myTank == NULL || !myTank->isPaused() || myTank->isAutoPilot());
}

//
// some simple global functions
//

void warnAboutMainFlags()
{
    // warning message for hidden flags
    if (!BZDBCache::displayMainFlags)
    {
        std::string showFlagsMsg = ColorStrings[YellowColor];
        showFlagsMsg += "Flags on field hidden, to show them ";
        std::vector<std::string> keys = KEYMGR.getKeysFromCommand("toggleFlags main", true);

        if (!keys.empty())
        {
            showFlagsMsg += "hit \"" + ColorStrings[WhiteColor];
            showFlagsMsg += tolower(keys[0][0]);
            showFlagsMsg += ColorStrings[YellowColor] + "\"";
        }
        else
            showFlagsMsg += " bind a key to Toggle Flags on Field";
        addMessage(NULL, showFlagsMsg);
    }
}

void warnAboutRadarFlags()
{
    if (!BZDB.isTrue("displayRadarFlags"))
    {
        std::string showFlagsMsg = ColorStrings[YellowColor];
        showFlagsMsg += "Flags on radar hidden, to show them ";
        std::vector<std::string> keys = KEYMGR.getKeysFromCommand("toggleFlags radar", true);

        if (keys.empty())
            showFlagsMsg += " bind a key to Toggle Flags on Radar";
        else
        {
            showFlagsMsg += "hit \"" + ColorStrings[WhiteColor];
            showFlagsMsg += tolower(keys[0][0]);
            showFlagsMsg += ColorStrings[YellowColor] + "\"";
        }
        addMessage(NULL, showFlagsMsg);
    }
}

void warnAboutRadar()
{
    if (!BZDB.isTrue("displayRadar"))
    {
        std::string message = ColorStrings[YellowColor];
        message += "To toggle the radar ";
        std::vector<std::string> keys = KEYMGR.getKeysFromCommand("toggleRadar", true);

        if (keys.empty())
            message += " bind a key to Toggle Radar";
        else
        {
            message += "hit \"" + ColorStrings[WhiteColor];
            message += keys[0];
            message += ColorStrings[YellowColor] + "\"";
        }

        addMessage(NULL, message);
    }
}

void warnAboutConsole()
{
    if (!BZDB.isTrue("displayConsole"))
    {
        std::string message = ColorStrings[YellowColor];
        message += "To toggle the console ";
        std::vector<std::string> keys = KEYMGR.getKeysFromCommand("toggleConsole", true);

        if (keys.empty())
            message += " bind a key to Toggle Console";
        else
        {
            message += "hit \"" + ColorStrings[WhiteColor];
            message += keys[0];
            message += ColorStrings[YellowColor] + "\"";
        }

        // can't use a console message for this one
        hud->setAlert(3, message.c_str(), 2.0f, true);
    }
}


inline bool isViewTank(Player* tank)
{
    return (((tank != NULL) &&
             (tank == LocalPlayer::getMyTank())) ||
            (ROAM.isRoaming()
             && (ROAM.getMode() == Roaming::roamViewFP)
             && (ROAM.getTargetTank() == tank)));
}


BzfDisplay*     getDisplay()
{
    return display;
}

MainWindow*     getMainWindow()
{
    return mainWindow;
}

ShotStats*      getShotStats()
{
    return shotStats;
}

SceneRenderer*      getSceneRenderer()
{
    return sceneRenderer;
}

void            setSceneDatabase()
{
    SceneDatabase *scene; // FIXME - test the zbuffer here

    // delete the old database
    sceneRenderer->setSceneDatabase(NULL);

    // make the scene, and record the processing time
    TimeKeeper startTime = TimeKeeper::getCurrent();
    scene = sceneBuilder->make(world);
    float elapsed = float(TimeKeeper::getCurrent() - startTime);

    // print debugging info
    if (BZDBCache::zbuffer)
        logDebugMessage(2,"ZSceneDatabase processed in %.3f seconds.\n", elapsed);
    else
        logDebugMessage(2,"BSPSceneDatabase processed in %.3f seconds.\n", elapsed);

    // set the scene
    sceneRenderer->setSceneDatabase(scene);
}

StartupInfo*        getStartupInfo()
{
    return &startupInfo;
}

bool            setVideoFormat(int index, bool test)
{
#if defined(_WIN32)
    // give windows extra time to test format (context reloading takes a while)
    static const float testDuration = 10.0f;
#else
    static const float testDuration = 5.0f;
#endif

    // ignore bad formats or when the format test timer is running
    if (testVideoFormatTimer != 0.0f || !display->isValidResolution(index))
        return false;

    // ignore if no change
    if (display->getResolution() == index) return true;

    // change it
    testVideoPrevFormat = display->getResolution();
    if (!display->setResolution(index)) return false;

    // handle resize
    mainWindow->setFullscreen();
    mainWindow->getWindow()->callResizeCallbacks();
    mainWindow->warpMouse();
    if (test) testVideoFormatTimer = testDuration;
    else if (shouldGrabMouse()) mainWindow->grabMouse();
    return true;
}

void            addPlayingCallback(PlayingCallback cb, void* data)
{
    PlayingCallbackItem item;
    item.cb = cb;
    item.data = data;
    playingCallbacks.push_back(item);
}

void            removePlayingCallback(PlayingCallback _cb, void* data)
{
    std::vector<PlayingCallbackItem>::iterator it = playingCallbacks.begin();
    while (it != playingCallbacks.end())
    {
        if (it->cb == _cb && it->data == data)
        {
            playingCallbacks.erase(it);
            break;
        }
        ++it;
    }
}

static void     callPlayingCallbacks()
{
    const int count = playingCallbacks.size();
    for (int i = 0; i < count; i++)
    {
        const PlayingCallbackItem& cb = playingCallbacks[i];
        (*cb.cb)(cb.data);
    }
}

void            joinGame()
{
    if (joiningGame)
    {
        if (worldBuilder)
        {
            delete worldBuilder;
            worldBuilder = NULL;
        }
        if (worldDatabase)
        {
            free(worldDatabase);
            worldDatabase = NULL;
        }
        HUDDialogStack::get()->setFailedMessage("Download stopped by user action");
        joiningGame = false;
    }
    joinRequested = true;
}

//
// handle signals that should kill me quickly
//

static void     dying(int sig)
{
    bzSignal(sig, SIG_DFL);
    display->setDefaultResolution();
    raise(sig);
}

//
// handle signals that should kill me nicely
//

static void     suicide(int sig)
{
    bzSignal(sig, SIG_PF(suicide));
    CommandsStandard::quit();
}

//
// handle signals that should disconnect me from the server
//

static void     hangup(int sig)
{
    bzSignal(sig, SIG_PF(hangup));
    serverDied = true;
    serverError = true;
}

static ServerLink*  lookupServer(const Player *_player)
{
    PlayerId id = _player->getId();
    if (myTank->getId() == id) return serverLink;
#ifdef ROBOT
    for (int i = 0; i < numRobots; i++)
        if (robots[i] && robots[i]->getId() == id)
            return robotServer[i];
#endif
    return NULL;
}

//
// user input handling
//

#if defined(DEBUG)
#define FREEZING
#endif
#if defined(FREEZING)
static bool     motionFreeze = false;
#endif

static enum { None, Left, Right, Up, Down } keyboardMovement;
static int shiftKeyStatus;


static bool doKeyCommon(const BzfKeyEvent& key, bool pressed)
{
    keyboardMovement = None;
    shiftKeyStatus   = key.shift;
    const std::string cmd = KEYMGR.get(key, pressed);
    if (key.ascii == 27)
    {
        if (pressed)
        {
            mainMenu->createControls();
            HUDDialogStack::get()->push(mainMenu);
        }
        return true;
    }
    else if (scoreboard->getHuntState() == ScoreboardRenderer::HUNT_SELECTING)
    {
        if (key.button == BzfKeyEvent::Down || KEYMGR.get(key, true) == "identify")
        {
            if (pressed)
                scoreboard->setHuntNextEvent();
            return true;
        }
        else if (key.button == BzfKeyEvent::Up || KEYMGR.get(key, true) == "drop")
        {
            if (pressed)
                scoreboard->setHuntPrevEvent();
            return true;
        }
        else if (KEYMGR.get(key, true) == "fire")
        {
            if (pressed)
                scoreboard->setHuntSelectEvent();
            return true;
        }
    }

    std::string cmdDrive = cmd;
    if (cmdDrive.empty())
    {
        // Check for driving keys
        BzfKeyEvent cleanKey = key;
        cleanKey.shift = 0;
        cmdDrive = KEYMGR.get(cleanKey, pressed);
    }
    if (cmdDrive == "turn left")
        keyboardMovement = Left;
    else if (cmdDrive == "turn right")
        keyboardMovement = Right;
    else if (cmdDrive == "drive forward")
        keyboardMovement = Up;
    else if (cmdDrive == "drive reverse")
        keyboardMovement = Down;

    if (myTank)
    {
        switch (keyboardMovement)
        {
        case Left:
            myTank->setKey(BzfKeyEvent::Left, pressed);
            break;
        case Right:
            myTank->setKey(BzfKeyEvent::Right, pressed);
            break;
        case Up:
            myTank->setKey(BzfKeyEvent::Up, pressed);
            break;
        case Down:
            myTank->setKey(BzfKeyEvent::Down, pressed);
            break;
        case None:
            break;
        }
    }

    if (!cmd.empty())
    {
        if (cmd == "fire")
            fireButton = pressed;
        roamButton = pressed;
        if (keyboardMovement == None)
        {
            std::string result = CMDMGR.run(cmd);
            if (!result.empty())
                controlPanel->addMessage(result);
        }
        return true;
    }

    // if we don't have a tank, the following key commands don't apply
    if (!myTank)
        return false;

    {
        // built-in unchangeable keys.  only perform if not masked.
        switch (key.ascii)
        {
        case 'T':
        case 't':
            // toggle frames-per-second display
            if (pressed)
            {
                showFPS = !showFPS;
                if (!showFPS)
                    hud->setFPS(-1.0);
            }
            return true;

        case 'Y':
        case 'y':
            // toggle milliseconds for drawing
            if (pressed)
            {
                showDrawTime = !showDrawTime;
                if (!showDrawTime)
                    hud->setDrawTime(-1.0);
            }
            return true;

            // for testing forced recreation of OpenGL context
#if defined(DEBUG_RENDERING)
        case 'X':
            if (pressed && ((shiftKeyStatus & BzfKeyEvent::AltKey) != 0))
            {
                // destroy OpenGL context
                getMainWindow()->getWindow()->freeContext();

                // recreate OpenGL context
                getMainWindow()->getWindow()->makeContext();

                // force a redraw (mainly for control panel)
                getMainWindow()->getWindow()->callExposeCallbacks();

                // cause sun/moon to be repositioned immediately
                lastEpochOffset = epochOffset - 5.0;

                // reload display lists and textures and initialize other state
                OpenGLGState::initContext();
            }
            return true;
#endif // DEBUG_RENDERING

        case ']':
        case '}':
            // plus 30 seconds
            if (pressed) clockAdjust += 30.0f;
            return true;

        case '[':
        case '{':
            // minus 30 seconds
            if (pressed) clockAdjust -= 30.0f;
            return true;

        default:
            break;
        } // end switch on key
        // Shot/Accuracy Statistics display
        if (key.button == BzfKeyEvent::Home && pressed)
        {
            if (!shotStats)
                shotStats = new ShotStats;

            HUDDialogStack::get()->push(shotStats);
            return true;
        }
    } // end key handle
    return false;
}

static void doKeyNotPlaying(const BzfKeyEvent&, bool, bool)
{
}

static void doKeyPlaying(const BzfKeyEvent& key, bool pressed, bool haveBinding)
{
#if defined(FREEZING)
    if (key.ascii == '`' && pressed && !haveBinding && key.shift)
    {
        // toggle motion freeze
        motionFreeze = !motionFreeze;
        if (motionFreeze)
            addMessage(NULL, "The tank's motion is now frozen! ... Press Shift+` to unfreeze");
        return;
    }
#endif

    if (key.ascii == 0 &&
            key.button >= BzfKeyEvent::F1 &&
            key.button <= BzfKeyEvent::F10 &&
            (key.shift & (BzfKeyEvent::ControlKey +
                          BzfKeyEvent::AltKey)) != 0 && !haveBinding)
    {
        // [Ctrl]-[Fx] is message to team
        // [Alt]-[Fx] is message to all
        if (pressed)
        {
            char name[32];
            int msgno = (key.button - BzfKeyEvent::F1) + 1;
            void* buf = messageMessage;
            if (key.shift == BzfKeyEvent::ControlKey && world->allowTeams())
            {
                sprintf(name, "quickTeamMessage%d", msgno);
                buf = nboPackUByte(buf, TeamToPlayerId(myTank->getTeam()));
            }
            else
            {
                sprintf(name, "quickMessage%d", msgno);
                buf = nboPackUByte(buf, AllPlayers);
            }
            if (BZDB.isSet(name))
            {
                char messageBuffer[MessageLen];
                strncpy(messageBuffer,
                        BZDB.get(name).c_str(),
                        MessageLen - 1);
                messageBuffer[MessageLen - 1] = '\0';
                nboPackString(buf, messageBuffer, MessageLen);
                serverLink->send(MsgMessage, sizeof(messageMessage), messageMessage);
            }
        }
    }
    else if (myTank->isAlive())
    {
        // Might be a direction key. Save it for later.
        if ((myTank->getInputMethod() != LocalPlayer::Keyboard) && pressed)
        {
            if (keyboardMovement != None)
                if (BZDB.isTrue("allowInputChange"))
                    myTank->setInputMethod(LocalPlayer::Keyboard);
        }
    }
}


static bool roamMouseWheel(const BzfKeyEvent& key, bool pressed)
{
    if ((key.button != BzfKeyEvent::WheelUp) &&
            (key.button != BzfKeyEvent::WheelDown))
        return false;
    if (middleMouseButton || (leftMouseButton == rightMouseButton))
        return false;
    if (!ROAM.isRoaming() || !myTank || (myTank->getTeam() != ObserverTeam))
        return false;

    if (pressed)
    {
        const bool roamMouseWheelSwap = BZDB.isTrue("roamMouseWheelSwap");
        const bool up = (key.button == BzfKeyEvent::WheelUp);
        if (leftMouseButton != roamMouseWheelSwap)
            ROAM.changeTarget(up ? Roaming::next : Roaming::previous);
        else if (rightMouseButton != roamMouseWheelSwap)
        {
            const int newMode = ROAM.getMode() + (up ? +1 : -1);
            if ((newMode < int(Roaming::roamViewCount)) &&
                    (newMode > int(Roaming::roamViewDisabled)))
                ROAM.setMode(Roaming::RoamingView(newMode));
        }
    }

    return true;
}


static void doKey(const BzfKeyEvent& key, bool pressed)
{
    switch (key.button)
    {
    case BzfKeyEvent::LeftMouse:
    {
        leftMouseButton   = pressed;
        break;
    }
    case BzfKeyEvent::RightMouse:
    {
        rightMouseButton  = pressed;
        break;
    }
    case BzfKeyEvent::MiddleMouse:
    {
        middleMouseButton = pressed;
        break;
    }
    }

    if (roamMouseWheel(key, pressed))
        return;

    if (myTank)
    {
        const std::string cmd = KEYMGR.get(key, pressed);
        if (cmd == "jump")
            myTank->setJumpPressed(pressed);
    }

    if (HUDui::getFocus())
    {
        if ((pressed && HUDui::keyPress(key)) ||
                (!pressed && HUDui::keyRelease(key)))
            return;
    }

    bool haveBinding = doKeyCommon(key, pressed);

    if (!myTank)
        doKeyNotPlaying(key, pressed, haveBinding);
    else
        doKeyPlaying(key, pressed, haveBinding);
}


void     applyJSModifiers(float& jsx, float& jsy)
{
    // enforce range setting limits (min 0% to 20%, max 25% to 100%) so the maximum can't be less than the minimum
    const auto jsRangeMax = std::max(std::min(float(BZDB.evalInt("jsRangeMax")) / 100.0f, 1.0f), 0.25f);
    const auto jsRangeMin = std::max(std::min(float(BZDB.evalInt("jsRangeMin")) / 100.0f, 0.2f), 0.0f);

    // joystick axes inversion values
    // 0: no inversion
    // 1: invert X
    // 2: invert Y
    // 3: invert both
    jsx *= BZDB.evalInt("jsInvertAxes") % 2 == 1 ? -1.0f : 1.0f;
    jsy *= BZDB.evalInt("jsInvertAxes") > 1 ? -1.0f : 1.0f;

    // scaled radial dead zone and cap
    const auto jsMagnitude = std::sqrt(jsx * jsx + jsy * jsy);
    const auto jsRangeMultiplier = jsRangeMax * (jsMagnitude - jsRangeMin) / (jsRangeMax - jsRangeMin) / jsMagnitude;

    // exponential ramp
    const auto jsRampType = BZDB.get("jsRampType");
    auto jsRampFactor = jsRangeMultiplier;
    if(jsRampType == "squared")
        jsRampFactor = std::pow(jsRangeMultiplier, 2.0f);
    else if(jsRampType == "cubed")
        jsRampFactor = std::pow(jsRangeMultiplier, 3.0f);

    // apply both factors
    if(jsMagnitude < jsRangeMin || isnan(jsRangeMultiplier))
        jsx = jsy = 0.0f;
    else
    {
        jsx *= jsRampFactor;
        jsy *= jsRampFactor;
    }

    // stretch corners
    if(BZDB.isTrue("jsStretchCorners"))
    {
        const auto stretchFactor = (1.0f - float(std::abs(std::abs(atan(jsy / jsx) / M_PI) - 0.25f)) * 4.0f) * jsMagnitude;
        const auto stretchValue = std::sqrt(2.0f);

        if(! isnan(stretchFactor))
        {
            jsx *= (1.0f - stretchFactor) + stretchValue * stretchFactor; // mix based on stretchFactor from 0.0 to 1.0
            jsy *= (1.0f - stretchFactor) + stretchValue * stretchFactor;
        }
    }

    // proportionally scale the coordinates back to the range limit
    const auto jsxAbs = std::abs(jsx);
    const auto jsyAbs = std::abs(jsy);
    if(jsxAbs > jsRangeMax || jsyAbs > jsRangeMax)
    {
        const auto jsRangeExcess = (jsxAbs > jsyAbs ? jsxAbs : jsyAbs) / jsRangeMax;
        jsx /= jsRangeExcess;
        jsy /= jsRangeExcess;
    }
}


static void     doMotion()
{
    float rotation = 0.0f, speed = 1.0f;
    const int noMotionSize = hud->getNoMotionSize();
    const int maxMotionSize = hud->getMaxMotionSize();

    int keyboardRotation = myTank->getRotation();
    int keyboardSpeed    = myTank->getSpeed();
    /* see if controls are reversed */
    if (myTank->getFlag() == Flags::ReverseControls)
    {
        keyboardRotation = -keyboardRotation;
        keyboardSpeed    = -keyboardSpeed;
    }

    // mouse is default steering method; query mouse pos always, not doing so
    // can lead to stuttering movement with X and software rendering (uncertain why)
    int mx = 0, my = 0;
    mainWindow->getMousePosition(mx, my);

    // determine if joystick motion should be used instead of mouse motion
    // when the player bumps the mouse, LocalPlayer::getInputMethod return Mouse;
    // make it return Joystick when the user bumps the joystick
    auto jsx = 0.0f, jsy = 0.0f;
    if (mainWindow->haveJoystick())
    {
        if (myTank->getInputMethod() == LocalPlayer::Joystick)
        {
            // if we're using the joystick right now, replace mouse coords with joystick coords
            mainWindow->getJoyPosition(jsx, jsy);
        }
        else
        {
            // if the joystick is not active, and we're not forced to some other input method,
            // see if it's moved and autoswitch
            if (BZDB.isTrue("allowInputChange"))
            {
                mainWindow->getJoyPosition(jsx, jsy);

                // if we aren't using the joystick, but it's moving, start using it
                if(std::sqrt(jsx * jsx + jsy * jsy) > std::min(std::max(float(BZDB.evalInt("jsRangeMin")) / 100.0f, 0.0f), 0.2f))
                    myTank->setInputMethod(LocalPlayer::Joystick); // joystick motion
            } // allowInputChange
        } // getInputMethod == Joystick
    } // mainWindow->Joystick

    /* see if controls are reversed */
    if (myTank->getFlag() == Flags::ReverseControls)
    {
        mx = -mx;
        my = -my;
    }

#if defined(FREEZING)
    if (motionFreeze) return;
#endif

    if (myTank->isAutoPilot())
        doAutoPilot(rotation, speed);
    else if (myTank->getInputMethod() == LocalPlayer::Keyboard)
    {

        rotation = (float)keyboardRotation;
        speed    = (float)keyboardSpeed;
        if (speed < 0.0f)
            speed /= 2.0;

        rotation *= BZDB.eval("displayFOV") / 60.0f;
        if (BZDB.isTrue("slowKeyboard"))
        {
            rotation *= 0.5f;
            speed *= 0.5f;
        }
    }
    else if (myTank->getInputMethod() == LocalPlayer::Joystick)
    {
        applyJSModifiers(jsx, jsy);

        // calculate desired rotation
        if (keyboardRotation && !devDriving)
        {
            rotation = float(keyboardRotation);
            rotation *= BZDB.eval("displayFOV") / 60.0f;
            if (BZDB.isTrue("slowKeyboard"))
                rotation *= 0.5f;
        }
        else
        {
            rotation = -jsx;

            if (rotation > 1.0f)
                rotation = 1.0f;
            if (rotation < -1.0f)
                rotation = -1.0f;
        }

        // calculate desired speed
        if (keyboardSpeed && !devDriving)
        {
            speed = float(keyboardSpeed);
            if (speed < 0.0f)
                speed *= 0.5f;
            if (BZDB.isTrue("slowKeyboard"))
                speed *= 0.5f;
        }
        else
        {
            speed = -jsy;
            if (speed > 1.0f)
                speed = 1.0f;
            if (speed < -0.5f)
                speed = -0.5f;
        }
    }
    else // mouse
    {
        // calculate desired rotation
        if (keyboardRotation && !devDriving)
        {
            rotation = float(keyboardRotation);
            rotation *= BZDB.eval("displayFOV") / 60.0f;
            if (BZDB.isTrue("slowKeyboard"))
                rotation *= 0.5f;
        }
        else if (mx < -noMotionSize)
        {
            rotation = float(-mx - noMotionSize) / float(maxMotionSize - noMotionSize);
            if (rotation > 1.0f)
                rotation = 1.0f;
        }
        else if (mx > noMotionSize)
        {
            rotation = -float(mx - noMotionSize) / float(maxMotionSize - noMotionSize);
            if (rotation < -1.0f)
                rotation = -1.0f;
        }

        // calculate desired speed
        if (keyboardSpeed && !devDriving)
        {
            speed = float(keyboardSpeed);
            if (speed < 0.0f)
                speed *= 0.5f;
            if (BZDB.isTrue("slowKeyboard"))
                speed *= 0.5f;
        }
        else if (my < -noMotionSize)
        {
            speed = float(-my - noMotionSize) / float(maxMotionSize - noMotionSize);
            if (speed > 1.0f)
                speed = 1.0f;
        }
        else if (my > noMotionSize)
        {
            speed = -float(my - noMotionSize) / float(maxMotionSize - noMotionSize);
            if (speed < -0.5f)
                speed = -0.5f;
        }
        else
            speed = 0.0f;
    }

    myTank->setDesiredAngVel(rotation);
    myTank->setDesiredSpeed(speed);
}


static void mouseClamp()
{
    // only clamp when it might be useful
    if (HUDDialogStack::get()->isActive() || (myTank == NULL) || !myTank->isAlive() ||
            myTank->isPaused() || (myTank->getTeam() == ObserverTeam))
    {
        mainWindow->disableConfineToMotionbox();
        return;
    }

    // do not clamp if CTRL is being held down
    bool alt, ctrl, shift;
    display->getModState(alt, ctrl, shift);
    if (ctrl)
    {
        mainWindow->disableConfineToMotionbox();
        return;
    }

    // calculate the max motion size in pixels
    const int clampFudge = 2;
    const int w  = mainWindow->getWidth();
    const int vh = mainWindow->getViewHeight();
    const float xScale = (float)w  / (float) MinX;
    const float yScale = (float)vh / (float) MinY;
    const float scale = (xScale < yScale) ? xScale : yScale;
    const float effScale =  scale * (0.7f + RENDERER.getMaxMotionFactor() / 16.667f);
    const int maxMotionSize = (int)((float)MaxMotionSize * effScale);
    const int pixels = maxMotionSize + clampFudge;

    // calculate the clamp extents
    const int xc = (w / 2);
    const int xn = xc - pixels;
    const int xp = xc + pixels;
    const int yc = (vh / 2);
    const int yn = yc - pixels;
    const int yp = yc + pixels;

    // clamp, as required
    mainWindow->confineToMotionbox(xn, yn, xp, yp);
}


static void     doEvent(BzfDisplay *disply)
{
    BzfEvent event;
    if (!disply->getEvent(event)) return;

    switch (event.type)
    {
    case BzfEvent::Quit:
        CommandsStandard::quit();
        break;

    case BzfEvent::Redraw:
        mainWindow->getWindow()->callExposeCallbacks();
        sceneRenderer->setExposed();
        break;

    case BzfEvent::Resize:
        if (mainWindow->getWidth() != event.resize.width || mainWindow->getHeight() != event.resize.height)
        {
            mainWindow->getWindow()->setSize(event.resize.width, event.resize.height);
            mainWindow->getWindow()->callResizeCallbacks();
        }
        break;

    case BzfEvent::Map:
        // window has been mapped.  this normally occurs when the game
        // is uniconified.  if the player was paused because of an unmap
        // then resume.
        if (pausedByUnmap)
        {
            pausedByUnmap = false;
            pauseCountdown = 0.0f;
            if (myTank && myTank->isAlive() && myTank->isPaused())
            {
                myTank->setPause(false);
                controlPanel->addMessage("Resumed");
            }
        }

        // restore the resolution we want if full screen
        if (mainWindow->getFullscreen())
        {
            if (preUnmapFormat != -1)
            {
                disply->setResolution(preUnmapFormat);
                mainWindow->warpMouse();
            }
        }

        // restore the sound
        if (savedVolume != -1)
        {
            setSoundVolume(savedVolume);
            savedVolume = -1;
        }

        unmapped = false;
        if (shouldGrabMouse())
            mainWindow->grabMouse();
        break;

    case BzfEvent::Unmap:
        // begin pause countdown when unmapped if:  we're not already
        // paused because of an unmap (shouldn't happen), we're not
        // already counting down to pausing, we're alive, and we're not
        // already paused.
        if (!pausedByUnmap && (pauseCountdown == 0.0f) &&
                myTank && myTank->isAlive() &&
                !myTank->isPaused() && !myTank->isAutoPilot() &&
                !BZDB.isTrue("noUnmapPause"))   // handy for testing
        {
            // get ready to pause (no cheating through instantaneous pausing)
            pauseCountdown = 5.0f;

            // set this even though we haven't really paused yet
            pausedByUnmap = true;
        }

        // ungrab the mouse if we're running full screen
        if (mainWindow->getFullscreen() && !unmapped) // skip if already unmapped to avoid losing previous resolution
        {
            preUnmapFormat = -1;
            if (disply->getNumResolutions() > 1)
            {
                preUnmapFormat = disply->getResolution();
                disply->setDefaultResolution();
            }
        }

        // clear the mouse button states
        leftMouseButton = rightMouseButton = middleMouseButton = false;

        // turn off the sound
        if (savedVolume == -1)
        {
            savedVolume = getSoundVolume();
            setSoundVolume(0);
        }

        unmapped = true;
        mainWindow->ungrabMouse();
        break;

    case BzfEvent::KeyUp:
        doKey(event.keyUp, false);
        break;

    case BzfEvent::KeyDown:
        doKey(event.keyDown, true);
        break;

    case BzfEvent::MouseMove:
        if (myTank && myTank->isAlive() && (myTank->getInputMethod() != LocalPlayer::Mouse)
                && (BZDB.isTrue("allowInputChange")))
            myTank->setInputMethod(LocalPlayer::Mouse);
        if (BZDB.isTrue("mouseClamp"))
            mouseClamp();
        break;

    default:
        /* unset */
        break;
    }
}

void        addMessage(const Player *_player, const std::string& msg,
                       int mode, bool highlight, const char* oldColor)
{
    std::string fullMessage;

    if (BZDB.isTrue("colorful"))
    {
        if (_player)
        {
            if (highlight)
            {
                if (BZDB.get("killerhighlight") == "1")
                    fullMessage += ColorStrings[PulsatingColor];
                else if (BZDB.get("killerhighlight") == "2")
                    fullMessage += ColorStrings[UnderlineColor];
            }
            const PlayerId pid = _player->getId();
            if (pid < 200)
            {
                TeamColor color = _player->getTeam();
                fullMessage += Team::getAnsiCode(color);
            }
            else if (pid == ServerPlayer)
                fullMessage += ColorStrings[YellowColor];
            else
            {
                fullMessage += ColorStrings[CyanColor]; //replay observers
            }
            fullMessage += _player->getCallSign();

            if (highlight)
                fullMessage += ColorStrings[ResetColor];
#ifdef BWSUPPORT
            fullMessage += " (";
            fullMessage += Team::getName(_player->getTeam());
            fullMessage += ")";
#endif
            fullMessage += ColorStrings[DefaultColor] + ": ";
        }
        fullMessage += msg;
    }
    else
    {
        if (oldColor != NULL)
            fullMessage = oldColor;

        if (_player)
        {
            fullMessage += _player->getCallSign();

#ifdef BWSUPPORT
            fullMessage += " (";
            fullMessage += Team::getName(_player->getTeam());
            fullMessage += ")";
#endif
            fullMessage += ": ";
        }
        fullMessage += stripAnsiCodes(msg);
    }
    controlPanel->addMessage(fullMessage, mode);
}

static void     updateHighScores()
{
    /* check scores to see if my team and/or have the high score.  change
     * `>= bestScore' to `> bestScore' if you want to share the number
     * one spot. */
    bool anyPlayers = false;
    int i;
    for (i = 0; i < curMaxPlayers; i++)
        if (remotePlayers[i])
        {
            anyPlayers = true;
            break;
        }
#ifdef ROBOT
    if (!anyPlayers)
    {
        for (i = 0; i < numRobots; i++)
            if (robots[i])
            {
                anyPlayers = true;
                break;
            }
    }
#endif
    if (!anyPlayers)
    {
        hud->setPlayerHasHighScore(false);
        hud->setTeamHasHighScore(false);
        return;
    }

    bool haveBest = true;
    int bestScore = myTank ? myTank->getScore() : 0;
    for (i = 0; i < curMaxPlayers; i++)
        if (remotePlayers[i] && remotePlayers[i]->getScore() >= bestScore)
        {
            haveBest = false;
            break;
        }
#ifdef ROBOT
    if (haveBest)
    {
        for (i = 0; i < numRobots; i++)
            if (robots[i] && robots[i]->getScore() >= bestScore)
            {
                haveBest = false;
                break;
            }
    }
#endif
    hud->setPlayerHasHighScore(haveBest);

    if (myTank && Team::isColorTeam(myTank->getTeam()))
    {
        const Team& myTeam = World::getWorld()->getTeam(int(myTank->getTeam()));
        bestScore = myTeam.getWins() - myTeam.getLosses();
        haveBest = true;
        for (i = 0; i < NumTeams; i++)
        {
            if (i == int(myTank->getTeam())) continue;
            const Team& team = World::getWorld()->getTeam(i);
            if (team.size > 0 && team.getWins() - team.getLosses() >= bestScore)
            {
                haveBest = false;
                break;
            }
        }
        hud->setTeamHasHighScore(haveBest);
    }
    else
        hud->setTeamHasHighScore(false);
}

static void     updateFlag(FlagType* flag)
{
    if (flag == Flags::Null)
    {
        hud->setColor(1.0f, 0.625f, 0.125f);
        hud->setAlert(2, NULL, 0.0f);
    }
    else
    {
        const float* color = flag->getColor();
        hud->setColor(color[0], color[1], color[2]);
        hud->setAlert(2, flag->flagName.c_str(), 3.0f, flag->endurance == FlagSticky);
    }

    if (BZDB.isTrue("displayFlagHelp"))
        hud->setFlagHelp(flag, FlagHelpDuration);

    if ((!radar && !myTank) || !World::getWorld())
        return;

    radar->setJammed(flag == Flags::Jamming);
    hud->setAltitudeTape(flag == Flags::Jumping || World::getWorld()->allowJumping());
}


void            notifyBzfKeyMapChanged()
{
    std::string restartLabel = "Right Mouse";
    std::vector<std::string> keys = KEYMGR.getKeysFromCommand("restart", false);

    if (keys.empty())
    {
        // found nothing on down binding, so try up
        keys = KEYMGR.getKeysFromCommand("identify", true);
        if (keys.empty())
            std::cerr << "There does not appear to be any key bound to enter the game" << std::endl;
    }

    if (keys.size() >= 1)
    {
        // display single letter keys as a quoted lowercase letter
        if (keys[0].size() == 1)
        {
            restartLabel = '\"';
            restartLabel += tolower(keys[0][0]);
            restartLabel += '\"';
        }
        else
            restartLabel = keys[0];
    }

    // only show the first 2 keys found to keep things simple
    if (keys.size() > 1)
    {
        restartLabel.append(" or ");
        // display single letter keys as quoted lowercase letter
        if (keys[1].size() == 1)
        {
            restartLabel += '\"';
            restartLabel += tolower(keys[1][0]);
            restartLabel += '\"';
        }
        else
            restartLabel.append(keys[1]);
    }

    hud->setRestartKeyLabel(restartLabel);
}


//
// server message handling
//
static Player*      addPlayer(PlayerId id, const void* msg, int showMessage)
{
    uint16_t team, type, wins, losses, tks;
    char callsign[CallSignLen];
    char motto[MottoLen];
    msg = nboUnpackUShort (msg, type);
    msg = nboUnpackUShort (msg, team);
    msg = nboUnpackUShort (msg, wins);
    msg = nboUnpackUShort (msg, losses);
    msg = nboUnpackUShort (msg, tks);
    msg = nboUnpackString (msg, callsign, CallSignLen);
    msg = nboUnpackString (msg, motto, MottoLen);

    // Strip any ANSI color codes
    strncpy (callsign, stripAnsiCodes (std::string (callsign)).c_str (), 31);

    // id is slot, check if it's empty
    const int i = id;

    // sanity check
    if (i < 0)
    {
        printError (TextUtils::format ("Invalid player identification (%d)", i));
        std::
        cerr <<
             "WARNING: invalid player identification when adding player with id "
             << i << std::endl;
        return NULL;
    }

    if (remotePlayers[i])
    {
        // we're not in synch with server -> help! not a good sign, but not fatal.
        printError ("Server error when adding player, player already added");
        std::cerr << "WARNING: player already exists at location with id "
                  << i << std::endl;
        return NULL;
    }

    if (i >= curMaxPlayers)
    {
        curMaxPlayers = i + 1;
        World::getWorld ()->setCurMaxPlayers (curMaxPlayers);
    }

    // add player
    if (PlayerType (type) == TankPlayer || PlayerType (type) == ComputerPlayer)
    {
        remotePlayers[i] = new RemotePlayer (id, TeamColor (team), callsign, motto,
                                             PlayerType (type));
        remotePlayers[i]->changeScore (short (wins), short (losses), short (tks));
    }

#ifdef ROBOT
    if (PlayerType (type) == ComputerPlayer)
        for (int j = 0; j < numRobots; j++)
            if (robots[j] && !strncmp (robots[j]->getCallSign (), callsign,
                                       CallSignLen))
            {
                robots[j]->setTeam (TeamColor (team));
                break;
            }
#endif

    // show the message if we don't have the playerlist
    // permission.  if * we do, MsgAdminInfo should arrive
    // with more info.
    if (showMessage && !myTank->hasPlayerList ())
    {
        std::string message ("joining as ");
        if (team == ObserverTeam)
            message += "an observer";
        else
        {
            switch (PlayerType (type))
            {
            case TankPlayer:
                message += "a tank";
                break;
            case ComputerPlayer:
                message += "a robot tank";
                break;
            default:
                message += "an unknown type";
                break;
            }
        }
        if (!remotePlayers[i])
        {
            std::string name (callsign);
            name += ": " + message;
            message = name;
        }
        addMessage (remotePlayers[i], message);
    }
    completer.registerWord(callsign, true /* quote spaces */);

    if (shotStats)
        shotStats->refresh();

    return remotePlayers[i];
}


static void printIpInfo (const Player *_player, const Address& addr,
                         const std::string &note)
{
    if (_player == NULL)
        return;
    std::string colorStr;
    if (_player->getId() < 200)
    {
        TeamColor color = _player->getTeam();
        colorStr = Team::getAnsiCode(color);
    }
    else
    {
        colorStr = ColorStrings[CyanColor]; // replay observers
    }
    const std::string addrStr = addr.getDotNotation();
    std::string message = ColorStrings[CyanColor]; // default color
    message += "IPINFO: ";
    if (BZDBCache::colorful) message += colorStr;
    message += _player->getCallSign();
    if (BZDBCache::colorful) message += ColorStrings[CyanColor];
    message += "\t from: ";
    if (BZDBCache::colorful) message += colorStr;
    message += addrStr;

    message += ColorStrings[WhiteColor];
    for (int i = 0; i < (17 - (int)addrStr.size()); i++)
        message += " ";
    message += note;

    // print into the Server Menu
    controlPanel->addMessage(message, 2);

    return;
}


static bool removePlayer (PlayerId id)
{
    int playerIndex = lookupPlayerIndex(id);

    if (playerIndex < 0)
        return false;

    Player* p = remotePlayers[playerIndex];

    Address addr;
    std::string msg = "signing off";
    if (!p->getIpAddress(addr))
        addMessage(p, "signing off");
    else
    {
        msg += " from ";
        msg += addr.getDotNotation();
        p->setIpAddress(addr);
        addMessage(p, msg);
        if (BZDB.evalInt("showips") > 1)
            printIpInfo (p, addr, "(leave)");
    }

    if (myTank->getRecipient() == p)
        myTank->setRecipient(0);
    if (myTank->getNemesis() == p)
        myTank->setNemesis(0);
    if (myTank->getTarget() == p)
        myTank->setTarget(NULL);

    completer.unregisterWord(p->getCallSign());

    delete remotePlayers[playerIndex];
    remotePlayers[playerIndex] = NULL;

    while ((playerIndex >= 0)
            &&     (playerIndex+1 == curMaxPlayers)
            &&     (remotePlayers[playerIndex] == NULL))
    {
        playerIndex--;
        curMaxPlayers--;
    }
    World::getWorld()->setCurMaxPlayers(curMaxPlayers);

    if (shotStats)
        shotStats->refresh();

    return true;
}


static bool isCached(char *hexDigest)
{
    std::istream *cachedWorld;
    bool cached    = false;
    worldCachePath = getCacheDirName();
    worldCachePath += hexDigest;
    worldCachePath += ".bwc";
    if ((cachedWorld = FILEMGR.createDataInStream(worldCachePath, true)))
    {
        cached = true;
        delete cachedWorld;
    }
    return cached;
}


int curlProgressFunc(void* UNUSED(clientp),
                     curl_off_t dltotal, curl_off_t dlnow,
                     curl_off_t UNUSED(ultotal), curl_off_t UNUSED(ulnow))
{
    // FIXME: beaucoup cheeze here in the aborting style
    //    we should be using async dns and multi-curl

    // abort the download?
    BzfEvent event;
    if (display->isEventPending())
    {
        if (display->peekEvent(event))
        {
            switch (event.type)
            {
            case BzfEvent::Quit:
                return 1;         // terminate the curl call
            case BzfEvent::KeyDown:
                display->getEvent(event); // flush the event
                if (event.keyDown.ascii == 27)
                {
                    return 1;           // terminate the curl call
                }
                break;
            case BzfEvent::KeyUp:
                display->getEvent(event); // flush the event
                break;
            case BzfEvent::MouseMove:
                display->getEvent(event); // flush the event
                break;
            case BzfEvent::Unset:
            case BzfEvent::Map:
            case BzfEvent::Unmap:
            case BzfEvent::Redraw:
            case BzfEvent::Resize:
                // leave the event, it might be important
                break;
            }
        }
    }

    // update the status
    double percentage = 0.0;
    if ((int)dltotal > 0)
        percentage = 100.0 * dlnow / dltotal;
    char buffer[128];
    sprintf (buffer, "%2.1f%% (%i/%i)", percentage, (int)dlnow, (int)dltotal);
    HUDDialogStack::get()->setFailedMessage(buffer);

    return 0;
}

static void loadCachedWorld()
{
    // can't get a cache from nothing
    if (worldCachePath == std::string(""))
    {
        joiningGame = false;
        return;
    }

    // lookup the cached world
    std::istream *cachedWorld = FILEMGR.createDataInStream(worldCachePath, true);
    if (!cachedWorld)
    {
        HUDDialogStack::get()->setFailedMessage("World cache files disappeared.  Join canceled");
        drawFrame(0.0f);
        remove(worldCachePath.c_str());
        joiningGame = false;
        return;
    }

    // status update
    HUDDialogStack::get()->setFailedMessage("Loading world into memory...");
    drawFrame(0.0f);

    // get the world size
    cachedWorld->seekg(0, std::ios::end);
    std::streampos size = cachedWorld->tellg();
    unsigned long charSize = (unsigned long)std::streamoff(size);

    // load the cached world
    cachedWorld->seekg(0);
    char *localWorldDatabase = new char[charSize];
    if (!localWorldDatabase)
    {
        HUDDialogStack::get()->setFailedMessage("Error loading cached world.  Join canceled");
        drawFrame(0.0f);
        remove(worldCachePath.c_str());
        joiningGame = false;
        return;
    }
    cachedWorld->read(localWorldDatabase, charSize);
    delete cachedWorld;

    // verify
    HUDDialogStack::get()->setFailedMessage("Verifying world integrity...");
    drawFrame(0.0f);
    MD5 md5;
    md5.update((unsigned char *)localWorldDatabase, charSize);
    md5.finalize();
    std::string digest = md5.hexdigest();
    if (digest != md5Digest)
    {
        if (worldBuilder)
            delete worldBuilder;
        worldBuilder = NULL;
        delete[] localWorldDatabase;
        HUDDialogStack::get()->setFailedMessage("Error on md5. Removing offending file.");
        remove(worldCachePath.c_str());
        joiningGame = false;
        return;
    }

    // make world
    HUDDialogStack::get()->setFailedMessage("Preparing world...");
    drawFrame(0.0f);
    if (world)
    {
        delete world;
        world = NULL;
        World::setWorld(world);
    }
    if (!worldBuilder->unpack(localWorldDatabase))
    {
        // world didn't make for some reason
        if (worldBuilder)
            delete worldBuilder;
        worldBuilder = NULL;
        delete[] localWorldDatabase;
        HUDDialogStack::get()->setFailedMessage("Error unpacking world database. Join canceled.");
        remove(worldCachePath.c_str());
        joiningGame = false;
        return;
    }
    delete[] localWorldDatabase;

    // return world
    world = worldBuilder->getWorld();
    if (worldBuilder)
        delete worldBuilder;
    worldBuilder = NULL;

    HUDDialogStack::get()->setFailedMessage("Downloading files...");

    const bool doDownloads =  BZDB.isTrue("doDownloads");
    const bool updateDownloads =  BZDB.isTrue("updateDownloads");
    Downloads::startDownloads(doDownloads, updateDownloads, false);
    downloadingInitialTexture  = true;
}

class WorldDownLoader : cURLManager
{
public:
    void start(char * hexDigest);
private:
    void askToBZFS();
    virtual void finalization(char *data, unsigned int length, bool good);
};

void WorldDownLoader::start(char * hexDigest)
{
    if (isCached(hexDigest))
        loadCachedWorld();
    else if (worldUrl.size())
    {
        HUDDialogStack::get()->setFailedMessage
        (("Loading world from " + worldUrl).c_str());
        setProgressFunction(curlProgressFunc, worldUrl.c_str());
        setURL(worldUrl);
        addHandle();
        worldUrl = ""; // clear the state
    }
    else
        askToBZFS();
}

void WorldDownLoader::finalization(char *data, unsigned int length, bool good)
{
    if (good)
    {
        worldDatabase = data;
        theData       = NULL;
        MD5 md5;
        md5.update((unsigned char *)worldDatabase, length);
        md5.finalize();
        std::string digest = md5.hexdigest();
        if (digest != md5Digest)
        {
            HUDDialogStack::get()->setFailedMessage("Download from URL failed");
            askToBZFS();
        }
        else
        {
            std::ostream* cache =
                FILEMGR.createDataOutStream(worldCachePath, true, true);
            if (cache != NULL)
            {
                cache->write(worldDatabase, length);
                delete cache;
                loadCachedWorld();
            }
            else
            {
                HUDDialogStack::get()->setFailedMessage("Problem writing cache");
                askToBZFS();
            }
        }
    }
    else
        askToBZFS();
}

void WorldDownLoader::askToBZFS()
{
    HUDDialogStack::get()->setFailedMessage("Downloading World...");
    char message[MaxPacketLen];
    // ask for world
    nboPackUInt(message, 0);
    serverLink->send(MsgGetWorld, sizeof(uint32_t), message);
    worldPtr = 0;
    if (cacheOut)
        delete cacheOut;
    cacheOut = FILEMGR.createDataOutStream(worldCachePath, true, true);
}

static WorldDownLoader *worldDownLoader;

static void dumpMissingFlag(const char *buf, uint16_t len)
{
    int i;
    int nFlags = len/2;
    std::string flags;

    for (i = 0; i < nFlags; i++)
    {
        /* We can't use FlagType::unpack() here, since it counts on the
         * flags existing in our flag database.
         */
        if (i)
            flags += ", ";
        flags += buf[0];
        if (buf[1])
            flags += buf[1];
        buf += 2;
    }

    std::vector<std::string> args;
    args.push_back(flags);
    HUDDialogStack::get()->setFailedMessage
    (TextUtils::format("Flags not supported by this client: {1}",
                       &args).c_str());
}

static bool processWorldChunk(const void *buf, uint16_t len, int bytesLeft)
{
    int totalSize = worldPtr + len + bytesLeft;
    int doneSize  = worldPtr + len;
    if (cacheOut)
        cacheOut->write((const char *)buf, len);
    HUDDialogStack::get()->setFailedMessage
    (TextUtils::format
     ("Downloading World (%2d%% complete/%d kb remaining)...",
      (100 * doneSize / totalSize), bytesLeft / 1024).c_str());
    return bytesLeft == 0;
}

static void sendMeaCulpa(PlayerId victim)
{
    char meaculpa[MessageLen];
    char *buf;

    if (!myTank->isAutoPilot())
        return;

    strncpy(meaculpa, "sorry, i'm just a silly machine", MessageLen);
    buf = messageMessage;
    buf = (char*)nboPackUByte(buf, victim);
    nboPackString(buf, meaculpa, MessageLen-1);
    serverLink->send(MsgMessage, sizeof(messageMessage), messageMessage);
}

static void handleNearFlag (const void *msg, uint16_t)
{
    float pos[3];
    std::string flagName;
    msg = nboUnpackVector(msg, pos);
    msg = nboUnpackStdString(msg, flagName);

    std::string fullMessage = "Closest Flag: " + flagName;
    addMessage(NULL, ColorStrings[YellowColor]+fullMessage+ColorStrings[DefaultColor], 2, false, NULL);

    if (myTank)
        hud->setAlert(0, fullMessage.c_str(), 5.0f, false);
}

static void     handleServerMessage(bool human, uint16_t code,
                                    uint16_t len, const void* msg)
{
    bool checkScores = false;
    static WordFilter *wordfilter = (WordFilter *)BZDB.getPointer("filter");
    const World *_world = World::getWorld();

    switch (code)
    {

    case MsgNearFlag:
        // MsgNearFlag may arrive up to 1 lag period after dropping ID,
        // so process this only when carrying the ID flag
        if (myTank && myTank->getFlag() == Flags::Identify)
            handleNearFlag(msg,len);
        break;

    case MsgFetchResources:
        if (BZDB.isSet ("_noRemoteFiles") && BZDB.isTrue ("_noRemoteFiles"))
            break;
        else
        {
            uint16_t numItems;
            const void *buf;

            buf = nboUnpackUShort (msg, numItems);    // the type

            for (int i = 0; i < numItems; i++)
            {
                uint16_t itemType;
                char buffer[MessageLen];
                uint16_t stringLen;
                trResourceItem item;

                buf = nboUnpackUShort (buf, itemType);
                item.resType = (teResourceType) itemType;

                // URL
                buf = nboUnpackUShort (buf, stringLen);
                buf = nboUnpackString (buf, buffer, stringLen);

                buffer[stringLen] = '\0';
                item.URL = buffer;

                item.filePath = PlatformFactory::getMedia ()->getMediaDirectory() + DirectorySeparator;
                std::vector < std::string > temp =
                    TextUtils::tokenize (item.URL, std::string ("/"));

                item.fileName = temp[temp.size () - 1];
                item.filePath += item.fileName;

                std::string hostname;
                parseHostname (item.URL, hostname);
                if (authorizedServer (hostname))
                {
                    if (!resourceDownloader)
                        resourceDownloader = new ResourceGetter;
                    resourceDownloader->addResource (item);
                }
            }
        }
        break;

    case MsgCustomSound:
        // bail out if we don't want to do remote sounds
        if (BZDB.isSet ("_noRemoteSounds") && BZDB.isTrue ("_noRemoteSounds"))
            break;
        else
        {
            const void *buf;
            char buffer[MessageLen];
            uint16_t soundType;
            uint16_t stringLen;
            std::string soundName;

            buf = nboUnpackUShort (msg, soundType);   // the type
            buf = nboUnpackUShort (buf, stringLen);   // how long our str is
            buf = nboUnpackString (buf, buffer, stringLen);

            buffer[stringLen] = '\0';
            soundName = buffer;

            if (soundType == LocalCustomSound)
                playLocalSound (soundName);
        }
        break;

    case MsgUDPLinkEstablished:
        // server got our initial UDP packet
        serverLink->enableOutboundUDP();
        break;

    case MsgUDPLinkRequest:
        // we got server's initial UDP packet
        serverLink->confirmIncomingUDP();
        break;

    case MsgSuperKill:
        printError("Server forced a disconnect");
        serverError = true;
        break;

    case MsgAccept:
        break;

    case MsgReject:
    {
        const void *buf;
        char buffer[MessageLen];
        uint16_t rejcode;
        std::string reason;
        buf = nboUnpackUShort (msg, rejcode); // filler for now
        buf = nboUnpackString (buf, buffer, MessageLen);
        buffer[MessageLen - 1] = '\0';
        reason = buffer;
        printError(reason);
        serverError = true;
        break;
    }

    case MsgNegotiateFlags:
    {
        if (len > 0)
        {
            dumpMissingFlag((const char *)msg, len);
            break;
        }
        serverLink->send(MsgWantSettings, 0, NULL);
        break;
    }

    case MsgGameSettings:
    {
        if (worldBuilder)
            delete worldBuilder;
        worldBuilder = new WorldBuilder;
        worldBuilder->unpackGameSettings(msg);
        serverLink->send(MsgWantWHash, 0, NULL);
        HUDDialogStack::get()->setFailedMessage("Requesting World Hash...");
        break;
    }

    case MsgCacheURL:
    {
        char *cacheURL = new char[len];
        nboUnpackString(msg, cacheURL, len);
        worldUrl = cacheURL;
        delete [] cacheURL;
        break;
    }

    case MsgWantWHash:
    {
        char *hexDigest = new char[len];
        nboUnpackString(msg, hexDigest, len);
        isCacheTemp = hexDigest[0] == 't';
        md5Digest = &hexDigest[1];

        worldDownLoader->start(hexDigest);
        delete [] hexDigest;
        break;
    }

    case MsgGetWorld:
    {
        // create world
        uint32_t bytesLeft;
        const void *buf = nboUnpackUInt(msg, bytesLeft);
        bool last = processWorldChunk(buf, len - 4, bytesLeft);
        if (!last)
        {
            char message[MaxPacketLen];
            // ask for next chunk
            worldPtr += len - 4;
            nboPackUInt(message, worldPtr);
            serverLink->send(MsgGetWorld, sizeof(uint32_t), message);
            break;
        }
        if (cacheOut)
            delete cacheOut;
        cacheOut = NULL;
        loadCachedWorld();
        if (isCacheTemp)
            markOld(worldCachePath);
        break;
    }

    case MsgGameTime:
    {
        GameTime::unpack(msg);
        GameTime::update();
        break;
    }

    case MsgTimeUpdate:
    {
        int32_t timeLeft;
        msg = nboUnpackInt(msg, timeLeft);
        hud->setTimeLeft(timeLeft);
        if (timeLeft == 0)
        {
            if (myTank->getTeam() != ObserverTeam)
                gameOver = true;
            myTank->explodeTank();
            controlPanel->addMessage("Time Expired");
            hud->setAlert(0, "Time Expired", 10.0f, true);
            controlPanel->addMessage("GAME OVER");
            hud->setAlert(1, "GAME OVER", 10.0f, true);
#ifdef ROBOT
            for (int i = 0; i < numRobots; i++)
                if (robots[i])
                    robots[i]->explodeTank();
#endif
        }
        else if (timeLeft < 0)
        {
            controlPanel->addMessage("Game Paused");
            hud->setAlert(0, "Game Paused", 10.0f, true);
        }
        break;
    }

    case MsgScoreOver:
    {
        // unpack packet
        PlayerId id;
        uint16_t team;
        msg = nboUnpackUByte(msg, id);
        msg = nboUnpackUShort(msg, team);
        Player* _player = lookupPlayer(id);

        // make a message
        std::string msg2;
        if (team == (uint16_t)NoTeam)
        {
            // a player won
            if (_player)
            {
                msg2 = _player->getCallSign();
                msg2 += " (";
                msg2 += Team::getName(_player->getTeam());
                msg2 += ")";
            }
            else
                msg2 = "[unknown player]";
        }
        else
        {
            // a team won
            msg2 = Team::getName(TeamColor(team));
        }
        msg2 += " won the game";

        if (myTank->getTeam() != ObserverTeam)
            gameOver = true;
        hud->setTimeLeft((uint32_t)~0);
        myTank->explodeTank();
        controlPanel->addMessage(msg2);
        hud->setAlert(0, msg2.c_str(), 10.0f, true);
#ifdef ROBOT
        for (int i = 0; i < numRobots; i++)
            if (robots[i])
                robots[i]->explodeTank();
#endif
        break;
    }

    case MsgAddPlayer:
    {
        PlayerId id;
        msg = nboUnpackUByte(msg, id);
#if defined(FIXME) && defined(ROBOT)
        saveRobotInfo(id, msg);
#endif
        if (id == myTank->getId())
        {
            // it's me!  should be the end of updates
            enteringServer(msg);
        }
        else
        {
            addPlayer(id, msg, entered);
            checkScores = true;

            // update the tank flags when in replay mode.
            if (myTank->getId() >= 200)
                setTankFlags();
        }
        break;
    }

    case MsgRemovePlayer:
    {
        PlayerId id;
        msg = nboUnpackUByte(msg, id);
        if (removePlayer (id))
            checkScores = true;
        break;
    }

    case MsgFlagUpdate:
    {
        uint16_t count;
        uint16_t flagIndex;
        uint32_t offset = 0;
        msg = nboUnpackUShort(msg, count);
        offset += 2;
        for (int i = 0; i < count; i++)
        {
            if (offset >= len)
                break;

            msg = nboUnpackUShort(msg, flagIndex);
            msg = world->getFlag(int(flagIndex)).unpack(msg);
            offset += FlagPLen;

            world->initFlag(int(flagIndex));
        }
        break;
    }

    case MsgTeamUpdate:
    {
        uint8_t  numTeams;
        uint16_t team;

        msg = nboUnpackUByte(msg,numTeams);
        for (int i = 0; i < numTeams; i++)
        {
            msg = nboUnpackUShort(msg, team);
            Team uTeam;
            msg = uTeam.unpack(msg);
            if (_world)
                teams[int(team)] = uTeam;
        }
        checkScores = true;
        break;
    }

    case MsgAlive:
    {
        PlayerId id;
        float pos[3], forward;
        msg = nboUnpackUByte(msg, id);
        msg = nboUnpackVector(msg, pos);
        msg = nboUnpackFloat(msg, forward);
        int playerIndex = lookupPlayerIndex(id);

        if ((playerIndex >= 0) || (playerIndex == -2))
        {
            static const float zero[3] = { 0.0f, 0.0f, 0.0f };
            Player* tank = getPlayerByIndex(playerIndex);
            if (tank == myTank)
            {
                wasRabbit = tank->getTeam() == RabbitTeam;
                myTank->restart(pos, forward);
                firstLife = false;
                justJoined = false;
                if (!myTank->isAutoPilot())
                    mainWindow->warpMouse();
                hud->setAltitudeTape(World::getWorld()->allowJumping());
#ifdef ROBOT
            }
            else if (tank->getPlayerType() == ComputerPlayer)
            {
                for (int r = 0; r < numRobots; r++)
                {
                    if (robots[r] && robots[r]->getId() == playerIndex)
                    {
                        robots[r]->restart(pos,forward);
                        setRobotTarget(robots[r]);
                        break;
                    }
                }
#endif
            }

            tank->setDeathEffect(NULL);
            if (SceneRenderer::instance().useQuality() >= 2)
            {
                if (((tank != myTank)
                        && ((ROAM.getMode() != Roaming::roamViewFP)
                            || (tank != ROAM.getTargetTank())))
                        || BZDB.isTrue("enableLocalSpawnEffect"))
                {
                    if (myTank->getFlag() == Flags::Colorblindness)
                    {
                        static float cbColor[4] = {1,1,1,1};
                        EFFECTS.addSpawnEffect(cbColor, pos);
                    }
                    else
                        EFFECTS.addSpawnEffect(tank->getColor(), pos);
                }
            }
            tank->setStatus(PlayerState::Alive);
            tank->move(pos, forward);
            tank->setVelocity(zero);
            tank->setAngularVelocity(0.0f);
            tank->setDeadReckoning();
            tank->spawnEffect();
            if (tank == myTank)
                myTank->setSpawning(false);
            playSound(SFX_POP, pos, true, isViewTank(tank));
        }

        break;
    }

    case MsgAutoPilot:
    {
        PlayerId id;
        msg = nboUnpackUByte(msg, id);
        uint8_t autopilot;
        nboUnpackUByte(msg, autopilot);
        Player* tank = lookupPlayer(id);
        if (!tank) break;
        tank->setAutoPilot(autopilot != 0);
        addMessage(tank, autopilot ? "Roger taking controls" : "Roger releasing controls");
        break;
    }

    case MsgPause:
    {
        PlayerId id;
        msg = nboUnpackUByte(msg, id);
        uint8_t Pause;
        nboUnpackUByte(msg, Pause);
        Player* tank = lookupPlayer(id);
        if (!tank)
            break;

        tank->setPausedMessageState(Pause == 0);
        addMessage(tank, Pause ? "has paused" : "has unpaused" );
        break;
    }

    case MsgKilled:
    {
        PlayerId victim, killer;
        FlagType* flagType;
        int16_t shotId, reason;
        int phydrv = -1;
        msg = nboUnpackUByte(msg, victim);
        msg = nboUnpackUByte(msg, killer);
        msg = nboUnpackShort(msg, reason);
        msg = nboUnpackShort(msg, shotId);
        msg = FlagType::unpack(msg, flagType);
        if (reason == (int16_t)PhysicsDriverDeath)
        {
            int32_t inPhyDrv;
            msg = nboUnpackInt(msg, inPhyDrv);
            phydrv = int(inPhyDrv);
        }
        BaseLocalPlayer* victimLocal = getLocalPlayer(victim);
        BaseLocalPlayer* killerLocal = getLocalPlayer(killer);
        Player* victimPlayer = lookupPlayer(victim);
        Player* killerPlayer = lookupPlayer(killer);

        if (victimPlayer)
            victimPlayer->reportedHits++;
#ifdef ROBOT
        if (victimPlayer == myTank)
        {
            // uh oh, i'm dead
            if (myTank->isAlive())
            {
                serverLink->sendDropFlag(myTank->getPosition());
                handleMyTankKilled(reason);
            }
        }
#endif
        if (victimLocal)
        {
            // uh oh, local player is dead
            if (victimLocal->isAlive())
                gotBlowedUp(victimLocal, GotKilledMsg, killer);
        }
        else if (victimPlayer)
        {
            victimPlayer->setExplode(TimeKeeper::getTick());
            const float* pos = victimPlayer->getPosition();
            const bool localView = isViewTank(victimPlayer);
            if (reason == GotRunOver)
                playSound(SFX_RUNOVER, pos, killerLocal == myTank, localView);
            else
                playSound(SFX_EXPLOSION, pos, killerLocal == myTank, localView);
            float explodePos[3];
            explodePos[0] = pos[0];
            explodePos[1] = pos[1];
            explodePos[2] = pos[2] + victimPlayer->getMuzzleHeight();

            // TODO hook this back up for 2.4.4 or later
            TankDeathOverride* death = NULL;
            EFFECTS.addDeathEffect(victimPlayer->getColor(), pos, victimPlayer->getAngle(),reason,victimPlayer,flagType);

            victimPlayer->setDeathEffect(death);

            if (!death || death->ShowExplosion())
                addTankExplosion(explodePos);
        }

        if (killerLocal)
        {
            // local player did it
            if (shotId >= 0)
            {
                // terminate the shot
                killerLocal->endShot(shotId, true);
            }
            if (victimPlayer && killerLocal != victimPlayer)
            {
                if ((victimPlayer->getTeam() == killerLocal->getTeam()) &&
                        (killerLocal->getTeam() != RogueTeam) && !(killerPlayer == myTank && wasRabbit)
                        && World::getWorld()->allowTeams())
                {
                    // teamkill
                    if (killerPlayer == myTank)
                    {
                        hud->setAlert(1, "Don't kill teammates!!!", 3.0f, true);
                        playLocalSound(SFX_KILL_TEAM);
                        sendMeaCulpa(victimPlayer->getId());
                    }
                }
                else
                {
                    // enemy
                    if (myTank->isAutoPilot())
                    {
                        if (killerPlayer)
                        {
                            const ShotPath* shot = killerPlayer->getShot(int(shotId));
                            if (shot != NULL)
                                teachAutoPilot(shot->getFlag(), 1);
                        }
                    }
                }
            }
        }
        else if (killerPlayer)
        {
            const ShotPath* shot = killerPlayer->getShot(int(shotId));
            if (shot && !shot->isStoppedByHit())
                killerPlayer->addHitToStats(shot->getFlag());
        }

        // handle my personal score against other players
        if ((killerPlayer == myTank || victimPlayer == myTank) &&
                !(killerPlayer == myTank && victimPlayer == myTank))
        {
            if (killerLocal == myTank)
            {
                if (victimPlayer)
                    victimPlayer->changeLocalScore(1, 0, 0);
                myTank->setNemesis(victimPlayer);
            }
            else
            {
                if (killerPlayer)
                    killerPlayer->changeLocalScore(0, 1, killerPlayer->getTeam() == victimPlayer->getTeam() ? 1 : 0);
                myTank->setNemesis(killerPlayer);
            }
        }

        // handle self-destructions
        if (killerPlayer == victimPlayer && killerPlayer)
            killerPlayer->changeSelfKills(1);

        // add message
        if (human && victimPlayer)
        {
            std::string message(ColorStrings[WhiteColor]);
            if (killerPlayer == victimPlayer)
            {
                message += "blew myself up";
                addMessage(victimPlayer, message);
            }
            else if (killer >= LastRealPlayer)
            {
                message += "destroyed by the server";
                addMessage(victimPlayer, message);
            }
            else if (!killerPlayer)
            {
                message += "destroyed by a (GHOST)";
                addMessage(victimPlayer, message);
            }
            else if (reason == WaterDeath)
            {
                message += "fell in the water";
                addMessage(victimPlayer, message);
            }
            else if (reason == PhysicsDriverDeath)
            {
                const PhysicsDriver* driver = PHYDRVMGR.getDriver(phydrv);
                if (driver == NULL)
                    message += "Unknown Deadly Obstacle";
                else
                    message += driver->getDeathMsg();
                addMessage(victimPlayer, message);
            }
            else
            {
                std::string playerStr;
                if (World::getWorld()->allowTeams() && (killerPlayer->getTeam() == victimPlayer->getTeam()) &&
                        (killerPlayer->getTeam() != RogueTeam) &&
                        (killerPlayer->getTeam() != ObserverTeam))
                    playerStr += "teammate ";
                if (victimPlayer == myTank)
                {
                    if (BZDB.get("killerhighlight") == "1")
                        playerStr += ColorStrings[PulsatingColor];
                    else if (BZDB.get("killerhighlight") == "2")
                        playerStr += ColorStrings[UnderlineColor];
                }
                TeamColor color = killerPlayer->getTeam();
                playerStr += Team::getAnsiCode(color);
                playerStr += killerPlayer->getCallSign();

                if (victimPlayer == myTank)
                    playerStr += ColorStrings[ResetColor];
                playerStr += ColorStrings[WhiteColor];

                // Give more informative kill messages
                if (flagType == Flags::Laser)
                    message += "was fried by " + playerStr + "'s laser";
                else if (flagType == Flags::GuidedMissile)
                    message += "was destroyed by " + playerStr + "'s guided missile";
                else if (flagType == Flags::ShockWave)
                    message += "felt the effects of " + playerStr + "'s shockwave";
                else if (flagType == Flags::InvisibleBullet)
                    message += "didn't see " + playerStr + "'s bullet";
                else if (flagType == Flags::MachineGun)
                    message += "was turned into swiss cheese by " + playerStr + "'s machine gun";
                else if (flagType == Flags::SuperBullet)
                    message += "got skewered by " + playerStr + "'s super bullet";
                else
                    message += "killed by " + playerStr;
                addMessage(victimPlayer, message, 3, killerPlayer==myTank);
            }
        }

        if (World::getWorld()->allowTeams())  // geno only works in team games :)
        {
            // blow up if killer has genocide flag and i'm on same team as victim
            // (and we're not rogues, unless in rabbit mode)
            if (human && killerPlayer && victimPlayer && victimPlayer != myTank &&
                    (victimPlayer->getTeam() == myTank->getTeam()) &&
                    (myTank->getTeam() != RogueTeam) && shotId >= 0)
            {
                // now see if shot was fired with a GenocideFlag
                const ShotPath* shot = killerPlayer->getShot(int(shotId));
                if (shot && shot->getFlag() == Flags::Genocide)
                    gotBlowedUp(myTank, GenocideEffect, killerPlayer->getId());
            }

#ifdef ROBOT
            // blow up robots on victim's team if shot was genocide
            if (killerPlayer && victimPlayer && shotId >= 0)
            {
                const ShotPath* shot = killerPlayer->getShot(int(shotId));
                if (shot && shot->getFlag() == Flags::Genocide)
                    for (int i = 0; i < numRobots; i++)
                        if (robots[i] && victimPlayer != robots[i] &&
                                victimPlayer->getTeam() == robots[i]->getTeam() &&
                                robots[i]->getTeam() != RogueTeam)
                            gotBlowedUp(robots[i], GenocideEffect, killerPlayer->getId());
            }
#endif
        }

        checkScores = true;
        break;
    }

    case MsgGrabFlag:
    {
        // ROBOT -- FIXME -- robots don't grab flag at the moment
        PlayerId id;
        uint16_t flagIndex;
        msg = nboUnpackUByte(msg, id);
        msg = nboUnpackUShort(msg, flagIndex);
        msg = world->getFlag(int(flagIndex)).unpack(msg);
        Player* tank = lookupPlayer(id);
        if (!tank) break;

        // player now has flag
        tank->setFlag(world->getFlag(flagIndex).type);
        if (tank == myTank)
        {
            // grabbed flag
            playLocalSound(myTank->getFlag()->endurance != FlagSticky ?
                           SFX_GRAB_FLAG : SFX_GRAB_BAD);
            updateFlag(myTank->getFlag());
        }
        else if (isViewTank(tank))
        {
            playLocalSound(tank->getFlag()->endurance != FlagSticky ?
                           SFX_GRAB_FLAG : SFX_GRAB_BAD);
        }
        else if (myTank->getTeam() != RabbitTeam && tank &&
                 tank->getTeam() != myTank->getTeam() &&
                 world->getFlag(flagIndex).type->flagTeam == myTank->getTeam())
        {
            hud->setAlert(1, "Flag Alert!!!", 3.0f, true);
            playLocalSound(SFX_ALERT);
        }
        else
        {
            FlagType* fd = world->getFlag(flagIndex).type;
            if ( fd->flagTeam != NoTeam
                    && fd->flagTeam != tank->getTeam()
                    && ((tank && (tank->getTeam() == myTank->getTeam())))
                    && (Team::isColorTeam(myTank->getTeam())))
            {
                hud->setAlert(1, "Team Grab!!!", 3.0f, false);
                const float* pos = tank->getPosition();
                playWorldSound(SFX_TEAMGRAB, pos, false);
            }
        }
        std::string message("grabbed ");
        message += tank->getFlag()->flagName;
        message += " flag";
        addMessage(tank, message);
        break;
    }

    case MsgDropFlag:
    {
        PlayerId id;
        uint16_t flagIndex;
        msg = nboUnpackUByte(msg, id);
        msg = nboUnpackUShort(msg, flagIndex);
        msg = world->getFlag(int(flagIndex)).unpack(msg);
        Player* tank = lookupPlayer(id);
        if (!tank) break;
        handleFlagDropped(tank);
        break;
    }

    case MsgCaptureFlag:
    {
        PlayerId id;
        uint16_t flagIndex, team;
        msg = nboUnpackUByte(msg, id);
        msg = nboUnpackUShort(msg, flagIndex);
        msg = nboUnpackUShort(msg, team);
        Player* capturer = lookupPlayer(id);
        if (flagIndex >= world->getMaxFlags())
            break;
        Flag capturedFlag = world->getFlag(int(flagIndex));
        if (capturedFlag.type == Flags::Null)
            break;
        int capturedTeam = world->getFlag(int(flagIndex)).type->flagTeam;

        // player no longer has flag
        if (capturer)
        {
            capturer->setFlag(Flags::Null);
            if (capturer == myTank)
                updateFlag(Flags::Null);

            // add message
            if (int(capturer->getTeam()) == capturedTeam)
            {
                std::string message("took my flag into ");
                message += Team::getName(TeamColor(team));
                message += " territory";
                addMessage(capturer, message);
                if (capturer == myTank)
                {
                    hud->setAlert(1, "Don't capture your own flag!!!", 3.0f, true);
                    playLocalSound( SFX_KILL_TEAM );
                    sendMeaCulpa(TeamToPlayerId(myTank->getTeam()));
                }
            }
            else
            {
                std::string message("captured ");
                message += Team::getName(TeamColor(capturedTeam));
                message += "'s flag";
                addMessage(capturer, message);
            }
        }

        // play sound -- if my team is same as captured flag then my team lost,
        // but if I'm on the same team as the capturer then my team won.
        if (capturedTeam == int(myTank->getTeam()))
            playLocalSound(SFX_LOSE);
        else if (capturer && capturer->getTeam() == myTank->getTeam())
            playLocalSound(SFX_CAPTURE);


        // blow up if my team flag captured
        if (capturedTeam == int(myTank->getTeam()))
            gotBlowedUp(myTank, GotCaptured, id);
#ifdef ROBOT
        //kill all my robots if they are on the captured team
        for (int r = 0; r < numRobots; r++)
        {
            if (robots[r] && robots[r]->getTeam() == capturedTeam)
                gotBlowedUp(robots[r], GotCaptured, robots[r]->getId());
        }
#endif

        // everybody who's alive on capture team will be blowing up
        // but we're not going to get an individual notification for
        // each of them, so add an explosion for each now.  don't
        // include me, though;  I already blew myself up.
        for (int i = 0; i < curMaxPlayers; i++)
        {
            if (remotePlayers[i] &&
                    remotePlayers[i]->isAlive() &&
                    remotePlayers[i]->getTeam() == capturedTeam)
            {
                const float* pos = remotePlayers[i]->getPosition();
                playWorldSound(SFX_EXPLOSION, pos, false);
                float explodePos[3];
                explodePos[0] = pos[0];
                explodePos[1] = pos[1];
                explodePos[2] = pos[2] + remotePlayers[i]->getMuzzleHeight();

                // todo hook this back up for 2.4.4. or later
                TankDeathOverride *death = NULL;
                EFFECTS.addDeathEffect(remotePlayers[i]->getColor(), pos, remotePlayers[i]->getAngle(),GotCaptured,remotePlayers[i],
                                       NULL);

                remotePlayers[i]->setDeathEffect(death);

                if (!death || death->ShowExplosion())
                    addTankExplosion(explodePos);
            }
        }

        checkScores = true;
        break;
    }

    case MsgNewRabbit:
    {
        PlayerId id;
        msg = nboUnpackUByte(msg, id);
        Player *rabbit = lookupPlayer(id);

        for (int i = 0; i < curMaxPlayers; i++)
        {
            if (remotePlayers[i])
                remotePlayers[i]->setHunted(false);
            if (i != id && remotePlayers[i] && remotePlayers[i]->getTeam() != HunterTeam
                    && remotePlayers[i]->getTeam() != ObserverTeam)
                remotePlayers[i]->changeTeam(HunterTeam);
        }

        if (rabbit != NULL)
        {
            rabbit->changeTeam(RabbitTeam);
            if (rabbit == myTank)
            {
                wasRabbit = true;
                if (myTank->isPaused())
                    serverLink->sendNewRabbit();
                else
                {
                    hud->setAlert(0, "You are now the rabbit.", 10.0f, false);
                    playLocalSound(SFX_HUNT_SELECT);
                }
                scoreboard->setHuntState(ScoreboardRenderer::HUNT_NONE);
            }
            else if (myTank->getTeam() != ObserverTeam)
            {
                myTank->changeTeam(HunterTeam);
                if (myTank->isPaused() || myTank->isAlive())
                    wasRabbit = false;
                rabbit->setHunted(true);
                scoreboard->setHuntState(ScoreboardRenderer::HUNT_ENABLED);
            }

            addMessage(rabbit, "is now the rabbit", 3, true);
        }

#ifdef ROBOT
        for (int r = 0; r < numRobots; r++)
            if (robots[r])
            {
                if (robots[r]->getId() == id)
                    robots[r]->changeTeam(RabbitTeam);
                else
                    robots[r]->changeTeam(HunterTeam);
            }
#endif
        break;
    }

    case MsgShotBegin:
    {
        FiringInfo firingInfo;
        msg = firingInfo.unpack(msg);

        const int shooterid = firingInfo.shot.player;
        RemotePlayer* shooter = shooterid < maxRemotePlayers
                                ? remotePlayers[shooterid]
                                : NULL;

        if (shooterid != ServerPlayer)
        {
            if (shooter && shooter->getId() == shooterid)
            {
                shooter->addShot(firingInfo);

                if (SceneRenderer::instance().useQuality() >= 2)
                {
                    float shotPos[3];
                    shooter->getMuzzle(shotPos);

                    // if you are driving with a tank in observer mode
                    // and do not want local shot effects,
                    // disable shot effects for that specific tank
                    if ((ROAM.getMode() != Roaming::roamViewFP)
                            || (!ROAM.getTargetTank())
                            || (shooterid != ROAM.getTargetTank()->getId())
                            || BZDB.isTrue("enableLocalShotEffect"))
                    {
                        EFFECTS.addShotEffect(shooter->getColor(), shotPos,
                                              shooter->getAngle(),
                                              shooter->getVelocity());
                    }
                }
            }
            else
                break;
        }
        else
            World::getWorld()->getWorldWeapons()->addShot(firingInfo);

        if (human)
        {
            const float* pos = firingInfo.shot.pos;
            const bool importance = false;
            const bool localSound = isViewTank(shooter);
            if (firingInfo.flagType == Flags::ShockWave)
                playSound(SFX_SHOCK, pos, importance, localSound);
            else if (firingInfo.flagType == Flags::Laser)
                playSound(SFX_LASER, pos, importance, localSound);
            else if (firingInfo.flagType == Flags::GuidedMissile)
                playSound(SFX_MISSILE, pos, importance, localSound);
            else if (firingInfo.flagType == Flags::Thief)
                playSound(SFX_THIEF, pos, importance, localSound);
            else
                playSound(SFX_FIRE, pos, importance, localSound);
        }
        break;
    }

    case MsgShotEnd:
    {
        PlayerId id;
        int16_t shotId;
        uint16_t reason;
        msg = nboUnpackUByte(msg, id);
        msg = nboUnpackShort(msg, shotId);
        msg = nboUnpackUShort(msg, reason);
        BaseLocalPlayer* localPlayer = getLocalPlayer(id);

        if (localPlayer)
            localPlayer->endShot(int(shotId), false, reason == 0);
        else
        {
            Player *pl = lookupPlayer(id);
            if (pl)
                pl->endShot(int(shotId), false, reason == 0);
        }
        break;
    }

    case MsgHandicap:
    {
        PlayerId id;
        uint8_t numHandicaps;
        int16_t handicap;
        msg = nboUnpackUByte(msg, numHandicaps);
        for (uint8_t s = 0; s < numHandicaps; s++)
        {
            msg = nboUnpackUByte(msg, id);
            msg = nboUnpackShort(msg, handicap);

            Player *sPlayer = getLocalPlayer(id);
            if (!sPlayer)
            {
                int i = lookupPlayerIndex(id);
                if (i >= 0)
                    sPlayer = remotePlayers[i];
                else
                    logDebugMessage(1, "Received handicap update for unknown player!\n");
            }
            if (sPlayer)
            {
                // a relative score of -_handicapScoreDiff points will provide maximum handicap
                float normalizedHandicap = float(handicap)
                                           / std::max(1.0f, BZDB.eval(StateDatabase::BZDB_HANDICAPSCOREDIFF));

                /* limit how much of a handicap is afforded, and only provide
                 * handicap advantages instead of disadvantages.
                 */
                if (normalizedHandicap > 1.0f)
                    // advantage
                    normalizedHandicap  = 1.0f;
                else if (normalizedHandicap < 0.0f)
                    // disadvantage
                    normalizedHandicap  = 0.0f;

                sPlayer->setHandicap(normalizedHandicap);
            }
        }
        break;
    }

    case MsgScore:
    {
        uint8_t numScores;
        PlayerId id;
        uint16_t wins, losses, tks;
        msg = nboUnpackUByte(msg, numScores);

        for (uint8_t s = 0; s < numScores; s++)
        {
            msg = nboUnpackUByte(msg, id);
            msg = nboUnpackUShort(msg, wins);
            msg = nboUnpackUShort(msg, losses);
            msg = nboUnpackUShort(msg, tks);

            Player *sPlayer = NULL;
            if (id == myTank->getId())
                sPlayer = myTank;
            else
            {
                int i = lookupPlayerIndex(id);
                if (i >= 0)
                    sPlayer = remotePlayers[i];
                else
                    logDebugMessage(1,"Received score update for unknown player!\n");
            }
            if (sPlayer)
                sPlayer->changeScore(wins - sPlayer->getWins(),
                                     losses - sPlayer->getLosses(),
                                     tks - sPlayer->getTeamKills());
        }
        break;
    }

    case MsgSetVar:
    {
        msg = handleMsgSetVars(msg);
        break;
    }

    case MsgTeleport:
    {
        PlayerId id;
        uint16_t from, to;
        msg = nboUnpackUByte(msg, id);
        msg = nboUnpackUShort(msg, from);
        msg = nboUnpackUShort(msg, to);
        Player* tank = lookupPlayer(id);
        if (tank && tank != myTank)
        {
            int face;
            const Teleporter* teleporter = world->getTeleporter(int(to), face);
            if (teleporter)
            {
                const float* pos = teleporter->getPosition();
                tank->setTeleport(TimeKeeper::getTick(), short(from), short(to));
                playWorldSound(SFX_TELEPORT, pos);
            }
        }
        break;
    }

    case MsgTransferFlag:
    {
        PlayerId fromId, toId;
        unsigned short flagIndex;
        msg = nboUnpackUByte(msg, fromId);
        msg = nboUnpackUByte(msg, toId);
        msg = nboUnpackUShort(msg, flagIndex);
        msg = world->getFlag(int(flagIndex)).unpack(msg);
        Player* fromTank = lookupPlayer(fromId);
        Player* toTank = lookupPlayer(toId);
        handleFlagTransferred( fromTank, toTank, flagIndex);
        break;
    }


    case MsgMessage:
    {
        PlayerId src;
        PlayerId dst;
        uint8_t type;
        msg = nboUnpackUByte(msg, src);
        msg = nboUnpackUByte(msg, dst);
        msg = nboUnpackUByte(msg, type);

        // Only bother processing the message if we know how to handle it
        if (MessageType(type) != ChatMessage && MessageType(type) != ActionMessage)
            break;

        Player* srcPlayer = lookupPlayer(src);
        Player* dstPlayer = lookupPlayer(dst);
        TeamColor dstTeam = PlayerIdToTeam(dst);
        bool toAll = (dst == AllPlayers);
        bool fromServer = (src == ServerPlayer);
        bool toAdmin = (dst == AdminPlayers);
        std::string dstName;

        const std::string srcName = fromServer ? "SERVER" : (srcPlayer ? srcPlayer->getCallSign() : "(UNKNOWN)");
        if (dstPlayer)
            dstName = dstPlayer->getCallSign();
        else if (toAdmin)
            dstName = "Admin";
        else
            dstName = "(UNKNOWN)";
        std::string fullMsg;

        bool ignore = false;
        unsigned int i;
        if (!fromServer)
        {
            for (i = 0; i < silencePlayers.size(); i++)
            {
                const std::string &silenceCallSign = silencePlayers[i];
                if (srcName == silenceCallSign || "*" == silenceCallSign || ("-" == silenceCallSign && srcPlayer
                        && !srcPlayer->isRegistered()))
                {
                    ignore = true;
                    break;
                }
            }
        }

        if (ignore)
        {
#ifdef DEBUG
            // to verify working
            std::string msg2 = "Ignored Msg from " + srcName;
            if (silencePlayers[i] == "*")
                msg2 += " because all chat is ignored";
            else if (silencePlayers[i] == "-")
                msg2 += " because chat from unregistered players is ignored";
            addMessage(NULL,msg2);
#endif
            break;
        }

        std::string origText;
        // if filtering is turned on, filter away the goo
        if (wordfilter != NULL)
        {
            char filtered[MessageLen];
            strncpy(filtered, (const char *)msg, MessageLen - 1);
            filtered[MessageLen - 1] = '\0';
            if (wordfilter->filter(filtered))
                msg = filtered;
            origText = stripAnsiCodes(std::string((const char*)msg));   // use filtered[] while it is in scope
        }
        else
            origText = stripAnsiCodes(std::string((const char*)msg));

        std::string text = BundleMgr::getCurrentBundle()->getLocalString(origText);

        if (toAll || toAdmin || srcPlayer == myTank  || dstPlayer == myTank ||
                dstTeam == myTank->getTeam())
        {
            // message is for me
            std::string colorStr;
            if (srcPlayer == NULL)
                colorStr += ColorStrings[RogueTeam];
            else
            {
                const PlayerId pid = srcPlayer->getId();
                if (pid < 200)
                {
                    if (srcPlayer && srcPlayer->getTeam() != NoTeam)
                        colorStr += Team::getAnsiCode(srcPlayer->getTeam());
                    else
                        colorStr += ColorStrings[RogueTeam];
                }
                else if (pid == ServerPlayer)
                    colorStr += ColorStrings[YellowColor];
                else
                {
                    colorStr += ColorStrings[CyanColor]; // replay observers
                }
            }

            fullMsg += colorStr;

            // direct message to or from me
            if (dstPlayer)
            {
                //if (fromServer && (origText == "You are now an administrator!"
                //             || origText == "Password Accepted, welcome back."))
                //admin = true;

                // talking to myself? that's strange
                if (dstPlayer == myTank && srcPlayer == myTank)
                    fullMsg = text;
                else
                {
                    if (BZDB.get("killerhighlight") == "1")
                        fullMsg += ColorStrings[PulsatingColor];
                    else if (BZDB.get("killerhighlight") == "2")
                        fullMsg += ColorStrings[UnderlineColor];

                    if (srcPlayer == myTank)
                    {
                        if (MessageType(type) == ActionMessage)
                            fullMsg += "[->" + dstName + "][" + srcName + " " + text + "]";
                        else
                        {
                            fullMsg += "[->" + dstName + "]";
                            fullMsg += ColorStrings[ResetColor] + " ";
                            fullMsg += ColorStrings[CyanColor] + text;
                        }
                    }
                    else
                    {
                        if (MessageType(type) == ActionMessage)
                            fullMsg += "[" + srcName + " " + text + "]";
                        else
                        {
                            fullMsg += "[" + srcName + "->]";
                            fullMsg += ColorStrings[ResetColor] + " ";
                            fullMsg += ColorStrings[CyanColor] + text;
                        }

                        if (srcPlayer)
                            myTank->setRecipient(srcPlayer);

                        // play a sound on a private message not from self or server

                        bool playSound = !fromServer;
                        if (BZDB.isSet("beepOnServerMsg") && BZDB.isTrue("beepOnServerMsg"))
                            playSound = true;

                        if (playSound)
                        {
                            static TimeKeeper lastMsg = TimeKeeper::getSunGenesisTime();
                            if (TimeKeeper::getTick() - lastMsg > 2.0f)
                                playLocalSound( SFX_MESSAGE_PRIVATE );
                            lastMsg = TimeKeeper::getTick();
                        }
                    }
                }
            }
            else
            {
                // team / admin message
                if (toAdmin)
                {

                    // play a sound on a private message not from self or server
                    if (!fromServer)
                    {
                        static TimeKeeper lastMsg = TimeKeeper::getSunGenesisTime();
                        if (TimeKeeper::getTick() - lastMsg > 2.0f)
                            playLocalSound( SFX_MESSAGE_ADMIN );
                        lastMsg = TimeKeeper::getTick();
                    }

                    fullMsg += "[Admin] ";
                }

                if (dstTeam != NoTeam)
                {
#ifdef BWSUPPORT
                    fullMsg = "[to ";
                    fullMsg += Team::getName(TeamColor(dstTeam));
                    fullMsg += "] ";
#else
                    fullMsg += "[Team] ";
#endif

                    // play a sound if I didn't send the message
                    if (srcPlayer != myTank)
                    {
                        static TimeKeeper lastMsg = TimeKeeper::getSunGenesisTime();
                        if (TimeKeeper::getTick() - lastMsg > 2.0f)
                            playLocalSound(SFX_MESSAGE_TEAM);
                        lastMsg = TimeKeeper::getTick();
                    }
                }

                // display action messages differently
                if (MessageType(type) == ActionMessage)
                    fullMsg += srcName + " " + text;
                else
                    fullMsg += srcName + colorStr + ": " + ColorStrings[CyanColor] + text;
            }

            std::string oldcolor = "";
            if (!srcPlayer || srcPlayer->getTeam() == NoTeam)
                oldcolor = ColorStrings[RogueTeam];
            else if (srcPlayer->getTeam() == ObserverTeam)
                oldcolor = ColorStrings[CyanColor];
            else
                oldcolor = Team::getAnsiCode(srcPlayer->getTeam());
            if (fromServer)
                addMessage(NULL, fullMsg, 2, false, oldcolor.c_str());
            else
                addMessage(NULL, fullMsg, 1, false, oldcolor.c_str());

            if (!srcPlayer || srcPlayer!=myTank)
                hud->setAlert(0, fullMsg.c_str(), 3.0f, false);
        }
        break;
    }

    case MsgReplayReset:
    {
        int i;
        unsigned char lastPlayer;
        msg = nboUnpackUByte(msg, lastPlayer);

        // remove players up to 'lastPlayer'
        // any PlayerId above lastPlayer is a replay observers
        for (i=0 ; i<lastPlayer ; i++)
        {
            if (removePlayer (i))
                checkScores = true;
        }

        // remove all of the flags
        for (i=0 ; i<numFlags; i++)
        {
            Flag& flag = world->getFlag(i);
            flag.owner = (PlayerId) -1;
            flag.status = FlagNoExist;
            world->initFlag (i);
        }
        break;
    }

    case MsgAdminInfo:
    {
        uint8_t numIPs;
        msg = nboUnpackUByte(msg, numIPs);

        /* if we're getting this, we have playerlist perm */
        myTank->setPlayerList(true);

        // replacement for the normal MsgAddPlayer message
        if (numIPs == 1)
        {
            uint8_t ipsize;
            uint8_t index;
            Address ip;
            const void* tmpMsg = msg; // leave 'msg' pointing at the first entry

            tmpMsg = nboUnpackUByte(tmpMsg, ipsize);
            tmpMsg = nboUnpackUByte(tmpMsg, index);
            tmpMsg = ip.unpack(tmpMsg);
            int playerIndex = lookupPlayerIndex(index);
            Player* tank = getPlayerByIndex(playerIndex);
            if (!tank)
                break;

            std::string message("joining as ");
            if (tank->getTeam() == ObserverTeam)
                message += "an observer";
            else
            {
                switch (tank->getPlayerType())
                {
                case TankPlayer:
                    message += "a tank";
                    break;
                case ComputerPlayer:
                    message += "a robot tank";
                    break;
                default:
                    message += "an unknown type";
                    break;
                }
            }
            message += " from " + ip.getDotNotation();
            tank->setIpAddress(ip);
            addMessage(tank, message);
        }

        // print fancy version to be easily found
        if ((numIPs != 1) || (BZDB.evalInt("showips") > 0))
        {
            uint8_t playerId;
            uint8_t addrlen;
            Address addr;

            for (int i = 0; i < numIPs; i++)
            {
                msg = nboUnpackUByte(msg, addrlen);
                msg = nboUnpackUByte(msg, playerId);
                msg = addr.unpack(msg);

                int playerIndex = lookupPlayerIndex(playerId);
                Player *_player = getPlayerByIndex(playerIndex);
                if (!_player) continue;
                printIpInfo(_player, addr, "(join)");
                _player->setIpAddress(addr); // save for the signoff message
            } // end for loop
        }
        break;
    }

    case MsgFlagType:
    {
        FlagType* typ = NULL;
        FlagType::unpackCustom(msg, typ);
        logDebugMessage(1, "Got custom flag type from server: %s\n", typ->information().c_str());
        break;
    }

    case MsgPlayerInfo:
    {
        uint8_t numPlayers;
        int i;
        msg = nboUnpackUByte(msg, numPlayers);
        for (i = 0; i < numPlayers; ++i)
        {
            PlayerId id;
            msg = nboUnpackUByte(msg, id);
            Player *p = lookupPlayer(id);
            uint8_t info;
            // parse player info bitfield
            msg = nboUnpackUByte(msg, info);
            if (!p)
                continue;
            p->setAdmin((info & IsAdmin) != 0);
            p->setRegistered((info & IsRegistered) != 0);
            p->setVerified((info & IsVerified) != 0);
        }
        break;
    }

    // inter-player relayed message
    case MsgPlayerUpdate:
    case MsgPlayerUpdateSmall:
    case MsgGMUpdate:
    case MsgLagPing:
        handlePlayerMessage(code, 0, msg);
        break;
    }

    if (checkScores) updateHighScores();
}

//
// player message handling
//

static void     handlePlayerMessage(uint16_t code, uint16_t,
                                    const void* msg)
{
    switch (code)
    {
    case MsgPlayerUpdate:
    case MsgPlayerUpdateSmall:
    {
        float timestamp; // could be used to enhance deadreckoning, but isn't for now
        PlayerId id;
        int32_t order;
        const void *buf = msg;
        buf = nboUnpackFloat(buf, timestamp);
        buf = nboUnpackUByte(buf, id);
        Player* tank = lookupPlayer(id);
        if (!tank || tank == myTank) break;
        nboUnpackInt(buf, order); // peek! don't update the msg pointer
        if (order <= tank->getOrder()) break;
        short oldStatus = tank->getStatus();
        tank->unpack(msg, code);
        short newStatus = tank->getStatus();
        if ((oldStatus & short(PlayerState::Paused)) !=
                (newStatus & short(PlayerState::Paused)))
            addMessage(tank, (tank->getStatus() & PlayerState::Paused) ?
                       "Paused" : "Resumed");
        if ((oldStatus & short(PlayerState::Exploding)) == 0 &&
                (newStatus & short(PlayerState::Exploding)) != 0)
        {
            // player has started exploding and we haven't gotten killed
            // message yet -- set explosion now, play sound later (when we
            // get killed message).  status is already !Alive so make player
            // alive again, then call setExplode to kill him.
            tank->setStatus(newStatus | short(PlayerState::Alive));
            tank->setExplode(TimeKeeper::getTick());
            // ROBOT -- play explosion now
        }
        break;
    }

    case MsgGMUpdate:
    {
        ShotUpdate shot;
        msg = shot.unpack(msg);
        Player* tank = lookupPlayer(shot.player);
        if (!tank || tank == myTank) break;
        RemotePlayer* remoteTank = (RemotePlayer*)tank;
        RemoteShotPath* shotPath =
            (RemoteShotPath*)remoteTank->getShot(shot.id);
        if (shotPath) shotPath->update(shot, code, msg);
        PlayerId targetId;
        msg = nboUnpackUByte(msg, targetId);
        Player* targetTank = lookupPlayer(targetId);
        if (targetTank && (targetTank == myTank) && (myTank->isAlive()))
        {
            static TimeKeeper lastLockMsg;
            if (TimeKeeper::getTick() - lastLockMsg > 0.75)
            {
                playWorldSound(SFX_LOCK, shot.pos);
                lastLockMsg=TimeKeeper::getTick();
                addMessage(tank, "locked on me");
            }
        }
        break;
    }

    // just echo lag ping message
    case MsgLagPing:
        serverLink->send(MsgLagPing,2,msg);
        break;
    }
}

//
// message handling
//

static void     doMessages()
{
    char msg[MaxPacketLen];
    uint16_t code, len;
    int e = 0;
    // handle server messages
    if (serverLink)
    {

        while (!serverError && (e = serverLink->read(code, len, msg, 0)) == 1)
            handleServerMessage(true, code, len, msg);
        if (e == -2)
        {
            printError("Server communication error");
            serverError = true;
            return;
        }
    }

#ifdef ROBOT
    for (int i = 0; i < numRobots; i++)
    {
        while (robotServer[i] && robotServer[i]->read(code, len, msg, 0) == 1)
            ;
        if (code == MsgKilled || code == MsgShotBegin || code == MsgShotEnd)
            handleServerMessage(false, code, len, msg);
    }
#endif
}

static void     updateFlags(float dt)
{
    for (int i = 0; i < numFlags; i++)
    {
        Flag& flag = world->getFlag(i);
        if (flag.status == FlagOnTank)
        {
            // position flag on top of tank
            Player* tank = lookupPlayer(flag.owner);
            if (tank)
            {
                const float* pos = tank->getPosition();
                flag.position[0] = pos[0];
                flag.position[1] = pos[1];
                flag.position[2] = pos[2] + tank->getDimensions()[2];
            }
        }
        world->updateFlag(i, dt);
    }
    FlagSceneNode::waveFlag(dt);
}

bool            addExplosion(const float* _pos,
                             float size, float duration, bool grounded)
{
    // ignore if no prototypes available;
    if (prototypeExplosions.empty())
        return false;

    // don't show explosions if quality is low
    if (sceneRenderer->useQuality() < 1) return false;

    // don't add explosion if blending or texture mapping are off
    if (!BZDBCache::blend || !BZDBCache::texture)
        return false;

    // pick a random prototype explosion
    const int index = (int)(bzfrand() * (float)prototypeExplosions.size());

    // make a copy and initialize it
    BillboardSceneNode* newExplosion = prototypeExplosions[index]->copy();
    GLfloat pos[3];
    pos[0] = _pos[0];
    pos[1] = _pos[1];
    pos[2] = _pos[2];
    newExplosion->move(pos);
    newExplosion->setSize(size);
    newExplosion->setDuration(duration);
    newExplosion->setAngle((float)(2.0 * M_PI * bzfrand()));
    newExplosion->setLight();
    newExplosion->setLightColor(1.0f, 0.8f, 0.5f);
    newExplosion->setLightAttenuation(0.05f, 0.0f, 0.03f);
    newExplosion->setLightScaling(size / BZDBCache::tankLength);
    newExplosion->setLightFadeStartTime(0.7f * duration);
    if (grounded)
        newExplosion->setGroundLight(true);

    // add copy to list of current explosions
    explosions.push_back(newExplosion);

    // the rest of the stuff is for tank explosions
    if (size < (3.0f * BZDBCache::tankLength))
        return true;

    // bring on the noise, a tank blew up
    int boom = (int) (bzfrand() * 8.0) + 3;
    const float lightGain = (float)boom + 1.0f;

    // turn up the volume
    newExplosion->setLightColor(1.0f * lightGain,
                                0.8f * lightGain,
                                0.5f * lightGain);
    while (boom--)
    {
        // pick a random prototype explosion
        const int idx = (int)(bzfrand() * (float)prototypeExplosions.size());

        // make a copy and initialize it
        BillboardSceneNode* newExpl = prototypeExplosions[idx]->copy();
        GLfloat explPos[3];
        explPos[0] = _pos[0]+(float)(bzfrand()*12.0 - 6.0);
        explPos[1] = _pos[1]+(float)(bzfrand()*12.0 - 6.0);
        explPos[2] = _pos[2]+(float)(bzfrand()*10.0);
        newExpl->move(explPos);
        newExpl->setSize(size);
        newExpl->setDuration(duration);
        newExpl->setAngle((float)(2.0 * M_PI * bzfrand()));

        // add copy to list of current explosions
        explosions.push_back(newExpl);
    }

    return true;
}

void            addTankExplosion(const float* pos)
{
    addExplosion(pos, BZDB.eval(StateDatabase::BZDB_TANKEXPLOSIONSIZE), 1.2f, false);
}

void            addShotExplosion(const float* pos)
{
    // only play explosion sound if you see an explosion
    if (addExplosion(pos, 1.2f * BZDBCache::tankLength, 0.8f, false))
        playWorldSound(SFX_SHOT_BOOM, pos);
}

void            addShotPuff(const float* pos, float azimuth, float elevation)
{
    bool useClasicPuff  = false;

    if (BZDB.evalInt("gmPuffEffect") == 1)
        useClasicPuff = true;

    if (useClasicPuff)
    {
        addExplosion(pos, 0.3f * BZDBCache::tankLength, 0.8f, true);
        return;
    }

    float rots[2] = {azimuth,elevation};
    EFFECTS.addGMPuffEffect(pos, rots, NULL);
}

// process pending input events
void           processInputEvents(float maxProcessingTime)
{
    if (mainWindow && display)
    {
        TimeKeeper start = TimeKeeper::getCurrent();
        while (display->isEventPending() &&
                !CommandsStandard::isQuit() &&
                (TimeKeeper::getCurrent() - start < maxProcessingTime))
        {
            // process one event
            doEvent(display);
        }
    }
}

static void     updateExplosions(float dt)
{
    // update time of all explosions
    int i;
    const int count = explosions.size();
    for (i = 0; i < count; i++)
        explosions[i]->updateTime(dt);

    // reap expired explosions
    for (i = count - 1; i >= 0; i--)
    {
        if (explosions[i]->isAtEnd())
        {
            delete explosions[i];
            std::vector<BillboardSceneNode*>::iterator it = explosions.begin();
            for (int j = 0; j < i; j++)
                ++it;
            explosions.erase(it);
        }
    }
}

static void dropLastExplosions()
{
    for (auto explosion : explosions)
        delete explosion;
    explosions.clear();
}

static void     addExplosions(SceneDatabase* scene)
{
    const int count = explosions.size();
    for (int i = 0; i < count; i++)
        scene->addDynamicNode(explosions[i]);
}

#ifdef ROBOT
static void     handleMyTankKilled(int reason)
{
    // restore the sound, this happens when paused tank dies
    if (savedVolume != -1)
    {
        setSoundVolume(savedVolume);
        savedVolume = -1;
    }

    // blow me up
    myTank->explodeTank();
    if (reason == GotRunOver)
        playLocalSound(SFX_RUNOVER);
    else
        playLocalSound(SFX_DIE);
}
#endif

static const void *handleMsgSetVars(const void *msg)
{
    uint16_t numVars;
    uint8_t nameLen = 0, valueLen = 0;

    char name[MaxPacketLen];
    char value[MaxPacketLen];

    msg = nboUnpackUShort(msg, numVars);
    for (int i = 0; i < numVars; i++)
    {
        msg = nboUnpackUByte(msg, nameLen);
        msg = nboUnpackString(msg, name, nameLen);
        name[nameLen] = '\0';

        msg = nboUnpackUByte(msg, valueLen);
        msg = nboUnpackString(msg, value, valueLen);
        value[valueLen] = '\0';

        if ((name[0] != '_') && (name[0] != '$'))
        {
            logDebugMessage(1, "Server BZDB change blocked: '%s' = '%s'\n",
                            name, value);
        }
        else
        {
            BZDB.set(name, value);
            BZDB.setPersistent(name, false);
            BZDB.setPermission(name, StateDatabase::Locked);
        }
    }
    return msg;
}

void handleFlagDropped(Player* tank)
{
    // skip it if player doesn't actually have a flag
    if (tank->getFlag() == Flags::Null) return;

    if (tank == myTank)
    {
        // make sure the player must reload after theft
        if (tank->getFlag() == Flags::Thief)
            myTank->forceReload(BZDB.eval(StateDatabase::BZDB_THIEFDROPTIME));
        //drop lock if i had GM
        if (myTank->getFlag() == Flags::GuidedMissile)
            myTank->setTarget(NULL);

        // update display and play sound effects
        playLocalSound(SFX_DROP_FLAG);
        updateFlag(Flags::Null);
    }
    else if (isViewTank(tank))
        playLocalSound(SFX_DROP_FLAG);

    // add message
    std::string message("dropped ");
    message += tank->getFlag()->flagName;
    message += " flag";
    addMessage(tank, message);

    // player no longer has flag
    tank->setFlag(Flags::Null);
}

static void handleFlagTransferred( Player *fromTank, Player *toTank, int flagIndex)
{
    Flag f = world->getFlag(flagIndex);

    fromTank->setFlag(Flags::Null);
    toTank->setFlag(f.type);

    if ((fromTank == myTank) || (toTank == myTank))
        updateFlag(myTank->getFlag());

    const float *pos = toTank->getPosition();
    if (f.type->flagTeam != ::NoTeam)
    {
        if ((toTank->getTeam() == myTank->getTeam()) && (f.type->flagTeam != myTank->getTeam()))
            playWorldSound(SFX_TEAMGRAB, pos);
        else if ((fromTank->getTeam() == myTank->getTeam()) && (f.type->flagTeam == myTank->getTeam()))
        {
            hud->setAlert(1, "Flag Alert!!!", 3.0f, true);
            playLocalSound(SFX_ALERT);
        }
    }

    std::string message("stole ");
    message += fromTank->getCallSign();
    message += "'s flag";
    addMessage(toTank, message);
}

static bool     gotBlowedUp(BaseLocalPlayer* tank,
                            BlowedUpReason reason,
                            PlayerId killer,
                            const ShotPath* hit, int phydrv)
{
    if (tank && (tank->getTeam() == ObserverTeam || !tank->isAlive()))
        return false;

    int shotId = -1;
    FlagType* flagType = Flags::Null;
    if (hit)
    {
        shotId = hit->getShotId();
        flagType = hit->getFlag();
    }

    // you can't take it with you
    const FlagType* flag = tank->getFlag();
    if (flag != Flags::Null)
    {
        if (myTank->isAutoPilot())
            teachAutoPilot( myTank->getFlag(), -1 );

        // tell other players I've dropped my flag
        lookupServer(tank)->sendDropFlag(tank->getPosition());

        // drop it
        handleFlagDropped(tank);
    }

    // restore the sound, this happens when paused tank dies
    // (genocide or team flag captured)
    if (savedVolume != -1)
    {
        setSoundVolume(savedVolume);
        savedVolume = -1;
    }

    // take care of explosion business -- don't want to wait for
    // round trip of killed message.  waiting would simplify things,
    // but the delay (2-3 frames usually) can really fool and irritate
    // the player.  we have to be careful to ignore our own Killed
    // message when it gets back to us -- do this by ignoring killed
    // message if we're already dead.
    // don't die if we had the shield flag and we've been shot.
    if (reason != GotShot || flag != Flags::Shield)
    {
        // blow me up

        // todo hook this back up for 2.4.4. or later
        TankDeathOverride *death = NULL;
        EFFECTS.addDeathEffect(tank->getColor(), tank->getPosition(), tank->getAngle(),reason,tank, flagType);

        tank->setDeathEffect(death);
        tank->explodeTank();

        if (isViewTank(tank))
        {
            if (reason == GotRunOver)
                playLocalSound(SFX_RUNOVER);
            else
                playLocalSound(SFX_DIE);
            ForceFeedback::death();
        }
        else
        {
            const float* pos = tank->getPosition();
            if (reason == GotRunOver)
            {
                playWorldSound(SFX_RUNOVER, pos,
                               getLocalPlayer(killer) == myTank);
            }
            else
            {
                playWorldSound(SFX_EXPLOSION, pos,
                               getLocalPlayer(killer) == myTank);
            }
        }

        if (tank != myTank &&(!death || death->ShowExplosion()))
        {
            const float* pos = tank->getPosition();
            float explodePos[3];
            explodePos[0] = pos[0];
            explodePos[1] = pos[1];
            explodePos[2] = pos[2] + tank->getMuzzleHeight();
            addTankExplosion(explodePos);
        }

        // tell server I'm dead in case it doesn't already know
        if (reason == GotShot || reason == GotRunOver ||
                reason == GenocideEffect || reason == SelfDestruct ||
                reason == WaterDeath || reason == DeathTouch)
            lookupServer(tank)->sendKilled(killer, reason, shotId, flagType, phydrv);
    }

    // print reason if it's my tank
    if ((tank == myTank) &&
            (((reason < LastReason) && blowedUpMessage[reason]) ||
             (reason == PhysicsDriverDeath)))
    {

        std::string blowedUpNotice;
        if (reason < LastReason)
            blowedUpNotice = blowedUpMessage[reason];
        else if (reason == PhysicsDriverDeath)
        {
            const PhysicsDriver* driver = PHYDRVMGR.getDriver(phydrv);
            if (driver)
                blowedUpNotice = driver->getDeathMsg();
            else
                blowedUpNotice = "Killed by unknown obstacle";
        }
        else
            blowedUpNotice = "Invalid reason";

        // first, check if i'm the culprit
        if (reason == GotShot && getLocalPlayer(killer) == myTank)
            blowedUpNotice = "Shot myself";
        else
        {
            // 1-4 are messages sent when the player dies because of someone else
            if (reason >= GotShot && reason <= GenocideEffect)
            {
                Player *killerPlayer = lookupPlayer(killer);
                if (!killerPlayer)
                    blowedUpNotice = "Killed by the server";
                else
                {

                    // matching the team-display style of other kill messages
                    TeamColor team = killerPlayer->getTeam();
                    if (hit)
                        team = hit->getTeam();
                    if (World::getWorld()->allowTeams() && (myTank->getTeam() == team) && (team != RogueTeam) && (team != ObserverTeam))
                    {
                        blowedUpNotice += "teammate " ;
                        blowedUpNotice += killerPlayer->getCallSign();
                    }
                    else
                    {
                        blowedUpNotice += killerPlayer->getCallSign();
                        blowedUpNotice += " (";
                        blowedUpNotice += Team::getName(killerPlayer->getTeam());
                        blowedUpNotice += ")";
                        if (flagType != Flags::Null)
                        {
                            blowedUpNotice += " with ";
                            blowedUpNotice += flagType->flagAbbv;
                        }
                    }
                }
            }
        }
        hud->setAlert(0, blowedUpNotice.c_str(), 4.0f, true);
        controlPanel->addMessage(blowedUpNotice);
    }

    // make sure shot is terminated locally (if not globally) so it can't
    // hit me again if I had the shield flag.  this is important for the
    // shots that aren't stopped by a hit and so may stick around to hit
    // me on the next update, making the shield useless.
    return (reason == GotShot && flag == Flags::Shield && shotId != -1);
}

static void     checkEnvironment()
{
    if (!myTank) return;

    if (myTank->getTeam() == ObserverTeam )
    {
        if (BZDB.evalInt("showVelocities") <= 2)
            return;

        // Check for an observed tanks hit.
        Player *target = ROAM.getTargetTank();
        const ShotPath* hit = NULL;
        FlagType* flagd;
        float minTime = Infinity;
        int i;

        // Always a possibility of failure
        if (target == NULL)
            return;

        if (ROAM.getMode() != Roaming::roamViewFP)
            // Only works if we are driving with the target
            return;

        if (!target->isAlive() || target->isPaused())
            // If he's dead or paused, don't bother checking
            return;

        flagd = target->getFlag();
        if ((flagd == Flags::Narrow) || (flagd == Flags::Tiny))
            // Don't bother trying to figure this out with a narrow or tiny flag yet.
            return;

        myTank->checkHit(myTank, hit, minTime);
        for (i = 0; i < curMaxPlayers; i++)
            if (remotePlayers[i])
                myTank->checkHit(remotePlayers[i], hit, minTime);

        if (!hit)
            return;

        Player* hitter = lookupPlayer(hit->getPlayer());
        std::ostringstream smsg;
        if (hitter->getId() == target->getId())
            return;

        // Don't report collisions when teammates can't be killed.
        // This is required because checkHit() tests as if we were observer.
        TeamColor team = hitter->getTeam();
        if (!World::getWorld()->allowTeamKills() && team == target->getTeam() &&
                team != RogueTeam && team != ObserverTeam)
            return;

        smsg << "local collision with "
             << hit->getShotId()
             << " from "
             << hitter->getCallSign()
             << std::endl;
        addMessage(target, smsg.str());

        if (target->hitMap.find(hit->getShotId()) == target->hitMap.end())
            target->computedHits++;

        target->hitMap[hit->getShotId()] = true;
        return;
    }

    // skip this if i'm dead or paused
    if (!myTank->isAlive() || myTank->isPaused()) return;

    FlagType* flagd = myTank->getFlag();
    if (flagd->flagTeam != NoTeam)
    {
        // have I captured a flag?
        TeamColor base = world->whoseBase(myTank->getPosition());
        TeamColor team = myTank->getTeam();
        if ((base != NoTeam) &&
                ((flagd->flagTeam == team && base != team) ||
                 (flagd->flagTeam != team && base == team)))
            serverLink->sendCaptureFlag(base);
    }
    else if (flagd == Flags::Null && (myTank->getLocation() == LocalPlayer::OnGround ||
                                      myTank->getLocation() == LocalPlayer::OnBuilding))
    {
        // Don't grab too fast
        static TimeKeeper lastGrabSent;
        if (TimeKeeper::getTick()-lastGrabSent > 0.2)
        {
            // grab any and all flags i'm driving over
            const float* tpos = myTank->getPosition();
            const float radius = myTank->getRadius();
            const float radius2 = (radius + BZDBCache::flagRadius) * (radius + BZDBCache::flagRadius);
            for (int i = 0; i < numFlags; i++)
            {
                if (world->getFlag(i).type == Flags::Null || world->getFlag(i).status != FlagOnGround)
                    continue;
                const float* fpos = world->getFlag(i).position;
                if ((fabs(tpos[2] - fpos[2]) < 0.1f) && ((tpos[0] - fpos[0]) * (tpos[0] - fpos[0]) +
                        (tpos[1] - fpos[1]) * (tpos[1] - fpos[1]) < radius2))
                {
                    serverLink->sendGrabFlag(i);
                    lastGrabSent=TimeKeeper::getTick();
                }
            }
        }
    }

    // see if i've been shot
    const ShotPath* hit = NULL;
    float minTime = Infinity;

    myTank->checkHit(myTank, hit, minTime);
    int i;
    for (i = 0; i < curMaxPlayers; i++)
        if (remotePlayers[i])
            myTank->checkHit(remotePlayers[i], hit, minTime);

    // Check Server Shots
    myTank->checkHit( World::getWorld()->getWorldWeapons(), hit, minTime);

    // used later
    float waterLevel = World::getWorld()->getWaterLevel();

    if (hit)
    {
        // i got shot!  terminate the shot that hit me and blow up.
        // force shot to terminate locally immediately (no server round trip);
        // this is to ensure that we don't get shot again by the same shot
        // after dropping our shield flag.
        if (hit->isStoppedByHit())
            serverLink->sendEndShot(hit->getPlayer(), hit->getShotId(), 1);

        FlagType* killerFlag = hit->getFlag();
        bool stopShot;

        if (killerFlag == Flags::Thief)
        {
            if (myTank->getFlag() != Flags::Null)
                serverLink->sendTransferFlag(myTank->getId(), hit->getPlayer());
            stopShot = true;
        }
        else
            stopShot = gotBlowedUp(myTank, GotShot, hit->getPlayer(), hit);

        if (stopShot || hit->isStoppedByHit())
        {
            Player* hitter = lookupPlayer(hit->getPlayer());
            if (hitter) hitter->endShot(hit->getShotId());
        }
    }
    // if not dead yet, see if i'm sitting on death
    else if (myTank->getDeathPhysicsDriver() >= 0)
    {
        gotBlowedUp(myTank, DeathTouch, ServerPlayer, NULL,
                    myTank->getDeathPhysicsDriver());
    }
    // if not dead yet, see if i've dropped below the death level
    else if ((waterLevel > 0.0f) && (myTank->getPosition()[2] <= waterLevel))
        gotBlowedUp(myTank, WaterDeath, ServerPlayer);
    // if not dead yet, see if i got run over by the steamroller
    else
    {
        const float* myPos = myTank->getPosition();
        const float myRadius = myTank->getRadius();
        for (i = 0; i < curMaxPlayers; i++)
        {
            if (remotePlayers[i] && !remotePlayers[i]->isPaused() &&
                    ((remotePlayers[i]->getFlag() == Flags::Steamroller) ||
                     ((myPos[2] < 0.0f) && remotePlayers[i]->isAlive() &&
                      !remotePlayers[i]->isPhantomZoned())))
            {
                const float* pos = remotePlayers[i]->getPosition();
                if (pos[2] < 0.0f) continue;
                if (remotePlayers[i]->getTeam() != RogueTeam && !World::getWorld()->allowTeamKills() &&
                        remotePlayers[i]->getTeam() == myTank->getTeam()) continue;
                if (!myTank->isPhantomZoned())
                {
                    const float radius = myRadius +
                                         BZDB.eval(StateDatabase::BZDB_SRRADIUSMULT) * remotePlayers[i]->getRadius();
                    const float distSquared =
                        hypotf(hypotf(myPos[0] - pos[0],
                                      myPos[1] - pos[1]), (myPos[2] - pos[2]) * 2.0f);
                    if (distSquared < radius)
                        gotBlowedUp(myTank, GotRunOver, remotePlayers[i]->getId());
                }
            }
        }
    }
}

bool inLookRange(float angle, float distance, float bestDistance, RemotePlayer *player)
{
    // usually about 17 degrees
    if (angle >= BZDB.eval(StateDatabase::BZDB_TARGETINGANGLE))
        return false;


    if (distance > BZDB.eval(StateDatabase::BZDB_TARGETINGDISTANCE) || distance > bestDistance)
        return false;

    if (myTank->getFlag() == Flags::Blindness)
        return false;

    if (player->getFlag() == Flags::Stealth ||
            player->getFlag() == Flags::Cloaking)
        return myTank->getFlag() == Flags::Seer;

    return true;
}

static bool isKillable(const Player *target)
{
    if (target == myTank)
        return false;
    if (target->getTeam() == RogueTeam)
        return true;
    if (myTank->getFlag() == Flags::Colorblindness)
        return true;
    if (! World::getWorld()->allowTeamKills() || ! World::getWorld()->allowTeams())
        return true;
    if (target->getTeam() != myTank->getTeam())
        return true;

    return false;
}

static bool isFriendly(const Player *target)
{
    if (target == myTank)
        return true;

    if (!World::getWorld()->allowTeams())
        return false;

    if (target->getTeam() == RogueTeam)
        return false;
    if (myTank->getFlag() == Flags::Colorblindness)
        return false;

    return target->getTeam() == myTank->getTeam();
}

void setLookAtMarker(void)
{
    // get info about my tank
    const float c = cosf(- myTank->getAngle());
    const float s = sinf(- myTank->getAngle());
    const float *p = myTank->getPosition();
    const fvec3 myPos(p[0],p[1],p[2]);

    // initialize best target
    Player *bestTarget = NULL;
    float bestDistance = Infinity;

    // figure out which tank is centered in my sights
    for (int i = 0; i < curMaxPlayers; i++)
    {
        if (!remotePlayers[i] || !remotePlayers[i]->isAlive())
            continue;

        // compute position in my local coordinate system
        const fvec3 rPos(remotePlayers[i]->getPosition()[0],remotePlayers[i]->getPosition()[1],
                         remotePlayers[i]->getPosition()[2]);
        const float x = (c * (rPos.x - myPos.x)) - (s * (rPos.y - myPos.y));
        const float y = (s * (rPos.x - myPos.x)) + (c * (rPos.y - myPos.y));

        // ignore things behind me
        if (x < 0.0f)
            continue;

        // get distance and sin(angle) from directly forward
        const float d = hypotf(x, y);
        const float a = fabsf(y / d);


        if (inLookRange(a, d, bestDistance, remotePlayers[i]))
        {
            // check and see if we can cast a ray from our point to the object
            fvec3 vec = rPos - myPos;

            Ray ray = Ray(myPos, vec);

            // get the list of objects that fall in this ray
            const ObsList *olist = COLLISIONMGR.rayTest (&ray, d);

            bool blocked = false;
            if (olist && olist->count > 0)
            {
                for (int o = 0; o < olist->count; o++)
                {
                    const Obstacle *obs = olist->list[o];
                    const float timet = obs->intersect(ray);
                    if (timet > 1.0f)
                    {
                        blocked = true;
                        break;
                    }
                }
            }

            // if there is nothing between us then go and add it to the list
            if (!blocked)
            {
                // is it better?
                bestTarget = remotePlayers[i];
                bestDistance = d;
            }
        }
    }

    if (!bestTarget)
        return;

    if (myTank->getFlag() == Flags::Blindness)
        return;

    std::string label = bestTarget->getCallSign();
    if (bestTarget->getFlag() != Flags::Null)
    {
        std::string flagName = bestTarget->getFlag()->flagAbbv;
        label += std::string("(") + flagName + std::string(")");
    }

    // Color enhanced marker depending on Local and RemotePlayer's Flag

    TeamColor markercolor = bestTarget->getTeam();

    if (bestTarget->getFlag() == Flags::Masquerade &&
            myTank->getFlag() != Flags::Seer)
        markercolor = myTank->getTeam();

    if (myTank->getFlag() == Flags::Colorblindness)
        markercolor = RogueTeam;

    hud->AddEnhancedNamedMarker(Float3ToVec3(bestTarget->getPosition()),
                                Float3ToVec3(Team::getTankColor(markercolor)),
                                label, isFriendly(bestTarget), 2.0f);
}

static inline bool tankHasShotType(const Player* tank, const FlagType* ft)
{
    const int maxShots = tank->getMaxShots();
    for (int i = 0; i < maxShots; i++)
    {
        const ShotPath* sp = tank->getShot(i);
        if ((sp != NULL) && (sp->getFlag() == ft))
            return true;
    }
    return false;
}

void setTarget()
{
    // get info about my tank
    const float c = cosf(-myTank->getAngle());
    const float s = sinf(-myTank->getAngle());
    const float x0 = myTank->getPosition()[0];
    const float y0 = myTank->getPosition()[1];

    // initialize best target
    Player* bestTarget = NULL;
    float bestDistance = Infinity;
    bool lockedOn = false;

    // figure out which tank is centered in my sights
    for (int i = 0; i < curMaxPlayers; i++)
    {
        if (!remotePlayers[i] || !remotePlayers[i]->isAlive()) continue;

        // compute position in my local coordinate system
        const float* pos = remotePlayers[i]->getPosition();
        const float x = c * (pos[0] - x0) - s * (pos[1] - y0);
        const float y = s * (pos[0] - x0) + c * (pos[1] - y0);

        // ignore things behind me
        if (x < 0.0f) continue;

        // get distance and sin(angle) from directly forward
        const float d = hypotf(x, y);
        const float a = fabsf(y / d);

        // see if it's inside lock-on angle (if we're trying to lock-on)
        if (a < BZDB.eval(StateDatabase::BZDB_LOCKONANGLE) &&   // about 8.5 degrees
                ((myTank->getFlag() == Flags::GuidedMissile) ||     // am i locking on?
                 tankHasShotType(myTank, Flags::GuidedMissile)) &&
                remotePlayers[i]->getFlag() != Flags::Stealth &&        // can't lock on stealth
                !remotePlayers[i]->isPaused() &&                // can't lock on paused
                !remotePlayers[i]->isNotResponding() &&         // can't lock on not responding
                d < bestDistance)                   // is it better?
        {
            bestTarget = remotePlayers[i];
            bestDistance = d;
            lockedOn = true;
        }
        else if (a < BZDB.eval(StateDatabase::BZDB_TARGETINGANGLE) &&               // about 17 degrees
                 ((remotePlayers[i]->getFlag() != Flags::Stealth) || (myTank->getFlag() == Flags::Seer))
                 && // can't "see" stealth unless have seer
                 d < bestDistance && !lockedOn)         // is it better?
        {
            bestTarget = remotePlayers[i];
            bestDistance = d;
        }
    }
    if (!lockedOn) myTank->setTarget(NULL);
    if (!bestTarget) return;

    const bool forbidIdentify = BZDB.isTrue("_forbidIdentify");

    if (lockedOn)
    {
        myTank->setTarget(bestTarget);
        myTank->setNemesis(bestTarget);

        std::string msg("Locked on ");
        if (!forbidIdentify)
        {
            msg += bestTarget->getCallSign();
            msg += " (";
            msg += Team::getName(bestTarget->getTeam());
            if (bestTarget->getFlag() != Flags::Null)
            {
                msg += ") with ";
                msg += bestTarget->getFlag()->flagName;
            }
            else
                msg += ")";
        }
        hud->setAlert(1, msg.c_str(), 2.0f, 1);
        msg = ColorStrings[DefaultColor] + msg;
        addMessage(NULL, msg);
    }
    else if (forbidIdentify)
    {
        if (sentForbidIdentify == 10 || sentForbidIdentify == 0)
            addMessage(NULL, "'identify' disabled on this server");
        if (sentForbidIdentify == 10)
            sentForbidIdentify = 0;
        if (sentForbidIdentify < 10)
            sentForbidIdentify++;
    }
    else if (myTank->getFlag() == Flags::Colorblindness)
    {
        std::string msg("Looking at a tank");
        hud->setAlert(1, msg.c_str(), 2.0f, 0);
        msg = ColorStrings[DefaultColor] + msg;
        addMessage(NULL, msg);
    }
    else
    {
        std::string msg("Looking at ");
        msg += bestTarget->getCallSign();
        msg += " (";
        msg += Team::getName(bestTarget->getTeam());
        msg += ")";
        if (bestTarget->getFlag() != Flags::Null)
        {
            msg += " with ";
            msg += bestTarget->getFlag()->flagName;
        }
        hud->setAlert(1, msg.c_str(), 2.0f, 0);
        msg = ColorStrings[DefaultColor] + msg;
        addMessage(NULL, msg);
        myTank->setNemesis(bestTarget);
    }
}

static void setHuntTarget()
{
    if (BZDB.isTrue("_forbidHunting"))
        return;

    // get info about my tank
    const float c = cosf(-myTank->getAngle());
    const float s = sinf(-myTank->getAngle());
    const float x0 = myTank->getPosition()[0];
    const float y0 = myTank->getPosition()[1];

    // initialize best target
    Player* bestTarget = NULL;
    float bestDistance = Infinity;
    bool lockedOn = false;

    // figure out which tank is centered in my sights
    for (int i = 0; i < curMaxPlayers; i++)
    {
        if (!remotePlayers[i] || !remotePlayers[i]->isAlive()) continue;

        // compute position in my local coordinate system
        const float* pos = remotePlayers[i]->getPosition();
        const float x = c * (pos[0] - x0) - s * (pos[1] - y0);
        const float y = s * (pos[0] - x0) + c * (pos[1] - y0);

        // ignore things behind me
        if (x < 0.0f) continue;

        // get distance and sin(angle) from directly forward
        const float d = hypotf(x, y);
        const float a = fabsf(y / d);

        // see if it's inside lock-on angle (if we're trying to lock-on)
        if (a < BZDB.eval(StateDatabase::BZDB_LOCKONANGLE) &&                   // about 8.5 degrees
                myTank->getFlag() == Flags::GuidedMissile &&    // am i locking on?
                remotePlayers[i]->getFlag() != Flags::Stealth &&    // can't lock on stealth
                !remotePlayers[i]->isPaused() &&            // can't lock on paused
                !remotePlayers[i]->isNotResponding() &&     // can't lock on not responding
                d < bestDistance)               // is it better?
        {
            bestTarget = remotePlayers[i];
            bestDistance = d;
            lockedOn = true;
        }
        else if (a < BZDB.eval(StateDatabase::BZDB_TARGETINGANGLE) &&               // about 17 degrees
                 ((remotePlayers[i]->getFlag() != Flags::Stealth) || (myTank->getFlag() == Flags::Seer))
                 && // can't "see" stealth unless have seer
                 d < bestDistance && !lockedOn)         // is it better?
        {
            bestTarget = remotePlayers[i];
            bestDistance = d;
        }
    }
    if (!bestTarget) return;


    if (bestTarget->isHunted() && myTank->getFlag() != Flags::Blindness &&
            myTank->getFlag() != Flags::Colorblindness &&
            bestTarget->getFlag() != Flags::Stealth)
    {
        if (myTank->getTarget() == NULL)   // Don't interfere with GM lock display
        {
            std::string msg("SPOTTED: ");
            msg += bestTarget->getCallSign();
            msg += " (";
            msg += Team::getName(bestTarget->getTeam());
            if (bestTarget->getFlag() != Flags::Null)
            {
                msg += ") with ";
                msg += bestTarget->getFlag()->flagName;
            }
            else
                msg += ")";
            hud->setAlert(1, msg.c_str(), 2.0f, 0);
        }
        if (!pulse.isOn())
        {
            const float* bestTargetPosition = bestTarget->getPosition();
            playWorldSound(SFX_HUNT, bestTargetPosition);
            pulse.setClock(1.0f);
        }
    }
}

static void     updateDaylight(double offset, SceneRenderer& renderer)
{
    static const double SecondsInDay = 86400.0;

    // update sun, moon & sky
    renderer.setTimeOfDay(unixEpoch + offset / SecondsInDay);
}

#ifdef ROBOT

//
// some robot stuff
//

static std::vector<BzfRegion*>  obstacleList;  // for robots

static void     addObstacle(std::vector<BzfRegion*>& rgnList, const Obstacle& obstacle)
{
    float p[4][2];
    const float* c = obstacle.getPosition();
    const float tankRadius = BZDBCache::tankRadius;

    if (BZDBCache::tankHeight < c[2])
        return;

    const float a = obstacle.getRotation();
    const float w = obstacle.getWidth() + tankRadius;
    const float h = obstacle.getBreadth() + tankRadius;
    const float xx =  w * cosf(a);
    const float xy =  w * sinf(a);
    const float yx = -h * sinf(a);
    const float yy =  h * cosf(a);
    p[0][0] = c[0] - xx - yx;
    p[0][1] = c[1] - xy - yy;
    p[1][0] = c[0] + xx - yx;
    p[1][1] = c[1] + xy - yy;
    p[2][0] = c[0] + xx + yx;
    p[2][1] = c[1] + xy + yy;
    p[3][0] = c[0] - xx + yx;
    p[3][1] = c[1] - xy + yy;

    int numRegions = rgnList.size();
    for (int k = 0; k < numRegions; k++)
    {
        BzfRegion* region = rgnList[k];
        int side[4];
        if ((side[0] = region->classify(p[0], p[1])) == 1 ||
                (side[1] = region->classify(p[1], p[2])) == 1 ||
                (side[2] = region->classify(p[2], p[3])) == 1 ||
                (side[3] = region->classify(p[3], p[0])) == 1)
            continue;
        if (side[0] == -1 && side[1] == -1 && side[2] == -1 && side[3] == -1)
        {
            rgnList[k] = rgnList[numRegions-1];
            rgnList[numRegions-1] = rgnList[rgnList.size()-1];
            rgnList.pop_back();
            numRegions--;
            k--;
            delete region;
            continue;
        }
        for (int j = 0; j < 4; j++)
        {
            if (side[j] == -1) continue;      // to inside
            // split
            const float* p1 = p[j];
            const float* p2 = p[(j+1)&3];
            BzfRegion* newRegion = region->orphanSplitRegion(p2, p1);
            if (!newRegion) continue;     // no split
            if (region != rgnList[k]) rgnList.push_back(region);
            region = newRegion;
        }
        if (region != rgnList[k]) delete region;
    }
}

static void     makeObstacleList()
{
    const float tankRadius = BZDBCache::tankRadius;
    int i;
    const int count = obstacleList.size();
    for (i = 0; i < count; i++)
        delete obstacleList[i];
    obstacleList.clear();

    // FIXME -- shouldn't hard code game area
    float gameArea[4][2];
    float worldSize = BZDBCache::worldSize;
    gameArea[0][0] = -0.5f * worldSize + tankRadius;
    gameArea[0][1] = -0.5f * worldSize + tankRadius;
    gameArea[1][0] =  0.5f * worldSize - tankRadius;
    gameArea[1][1] = -0.5f * worldSize + tankRadius;
    gameArea[2][0] =  0.5f * worldSize - tankRadius;
    gameArea[2][1] =  0.5f * worldSize - tankRadius;
    gameArea[3][0] = -0.5f * worldSize + tankRadius;
    gameArea[3][1] =  0.5f * worldSize - tankRadius;
    obstacleList.push_back(new BzfRegion(4, gameArea));

    const ObstacleList& boxes = OBSTACLEMGR.getBoxes();
    const int numBoxes = boxes.size();
    for (i = 0; i < numBoxes; i++)
        addObstacle(obstacleList, *boxes[i]);
    const ObstacleList& pyramids = OBSTACLEMGR.getPyrs();
    const int numPyramids = pyramids.size();
    for (i = 0; i < numPyramids; i++)
        addObstacle(obstacleList, *pyramids[i]);
    const ObstacleList& teleporters = OBSTACLEMGR.getTeles();
    const int numTeleporters = teleporters.size();
    for (i = 0; i < numTeleporters; i++)
        addObstacle(obstacleList, *teleporters[i]);
    const ObstacleList& meshes = OBSTACLEMGR.getMeshes();
    const int numMeshes = meshes.size();
    for (i = 0; i < numMeshes; i++)
        addObstacle(obstacleList, *meshes[i]);
    if (World::getWorld()->allowTeamFlags())
    {
        const ObstacleList& bases = OBSTACLEMGR.getBases();
        const int numBases = bases.size();
        for (i = 0; i < numBases; i++)
        {
            const BaseBuilding* base = (const BaseBuilding*) bases[i];
            if ((base->getHeight() != 0.0f) || (base->getPosition()[2] != 0.0f))
                addObstacle(obstacleList, *base);
        }
    }
}

static void     setRobotTarget(RobotPlayer* robot)
{
    Player* bestTarget = NULL;
    float bestPriority = 0.0f;
    for (int j = 0; j < curMaxPlayers; j++)
        if (remotePlayers[j] && remotePlayers[j]->getId() != robot->getId() &&
                remotePlayers[j]->isAlive() && robot->validTeamTarget(remotePlayers[j]))
        {

            if (remotePlayers[j]->isPhantomZoned() && !robot->isPhantomZoned())
                continue;

            if (World::getWorld()->allowTeamFlags() &&
                    ((robot->getTeam() == RedTeam && remotePlayers[j]->getFlag() == Flags::RedTeam) ||
                     (robot->getTeam() == GreenTeam && remotePlayers[j]->getFlag() == Flags::GreenTeam) ||
                     (robot->getTeam() == BlueTeam && remotePlayers[j]->getFlag() == Flags::BlueTeam) ||
                     (robot->getTeam() == PurpleTeam && remotePlayers[j]->getFlag() == Flags::PurpleTeam)))
            {
                bestTarget = remotePlayers[j];
                break;
            }

            const float priority = robot->getTargetPriority(remotePlayers[j]);
            if (priority > bestPriority)
            {
                bestTarget = remotePlayers[j];
                bestPriority = priority;
            }
        }
    if (myTank->isAlive() &&
            ((robot->getTeam() == RogueTeam) ||  robot->validTeamTarget(myTank)))
    {
        const float priority = robot->getTargetPriority(myTank);
        if (priority > bestPriority)
            bestTarget = myTank;
    }
    robot->setTarget(bestTarget);
}

static void     updateRobots(float dt)
{
    static float newTargetTimeout = 1.0f;
    static float clock = 0.0f;
    bool pickTarget = false;
    int i;

    // see if we should look for new targets
    clock += dt;
    if (clock > newTargetTimeout)
    {
        while (clock > newTargetTimeout)
            clock -= newTargetTimeout;
        pickTarget = true;
    }

    // start dead robots
    for (i = 0; i < numRobots; i++)
    {
        if (!gameOver && robots[i]
                && !robots[i]->isAlive() && !robots[i]->isExploding() && pickTarget)
            robotServer[i]->sendAlive();
    }

    // retarget robots
    for (i = 0; i < numRobots; i++)
    {
        if (robots[i] && robots[i]->isAlive()
                && (pickTarget || !robots[i]->getTarget()
                    || !robots[i]->getTarget()->isAlive()))
            setRobotTarget(robots[i]);
    }

    // do updates
    for (i = 0; i < numRobots; i++)
        if (robots[i])
            robots[i]->update();
}


static void     checkEnvironment(RobotPlayer* tank)
{
    // skip this if i'm dead or paused
    if (!tank->isAlive() || tank->isPaused()) return;

    // see if i've been shot
    const ShotPath* hit = NULL;
    float minTime = Infinity;
    tank->checkHit(myTank, hit, minTime);
    int i;
    for (i = 0; i < curMaxPlayers; i++)
    {
        if (remotePlayers[i] && remotePlayers[i]->getId() != tank->getId())
            tank->checkHit(remotePlayers[i], hit, minTime);
    }

    // Check Server Shots
    tank->checkHit( World::getWorld()->getWorldWeapons(), hit, minTime);

    float waterLevel = World::getWorld()->getWaterLevel();

    if (hit)
    {
        // i got shot!  terminate the shot that hit me and blow up.
        // force shot to terminate locally immediately (no server round trip);
        // this is to ensure that we don't get shot again by the same shot
        // after dropping our shield flag.
        if (hit->isStoppedByHit())
            lookupServer(tank)->sendEndShot(hit->getPlayer(), hit->getShotId(), 1);

        FlagType* killerFlag = hit->getFlag();
        bool stopShot;

        if (killerFlag == Flags::Thief)
        {
            if (tank->getFlag() != Flags::Null)
                serverLink->sendTransferFlag(tank->getId(), hit->getPlayer());
            stopShot = true;
        }
        else
            stopShot = gotBlowedUp(tank, GotShot, hit->getPlayer(), hit);

        if (stopShot || hit->isStoppedByHit())
        {
            Player* hitter = lookupPlayer(hit->getPlayer());
            if (hitter) hitter->endShot(hit->getShotId());
        }
    }
    // if not dead yet, see if i'm sitting on death
    else if (tank->getDeathPhysicsDriver() >= 0)
    {
        gotBlowedUp(tank, DeathTouch, ServerPlayer, NULL,
                    tank->getDeathPhysicsDriver());
    }
    // if not dead yet, see if the robot dropped below the death level
    else if ((waterLevel > 0.0f) && (tank->getPosition()[2] <= waterLevel))
        gotBlowedUp(tank, WaterDeath, ServerPlayer);

    // if not dead yet, see if i got run over by the steamroller
    else
    {
        bool dead = false;
        const float* myPos = tank->getPosition();
        const float myRadius = tank->getRadius();
        if (((myTank->getFlag() == Flags::Steamroller) ||
                ((tank->getFlag() == Flags::Burrow) && myTank->isAlive() &&
                 !myTank->isPhantomZoned()))
                && !myTank->isPaused())
        {
            const float* pos = myTank->getPosition();
            if (pos[2] >= 0.0f)
            {
                const float radius = myRadius +
                                     (BZDB.eval(StateDatabase::BZDB_SRRADIUSMULT) * myTank->getRadius());
                const float distSquared =
                    hypotf(hypotf(myPos[0] - pos[0],
                                  myPos[1] - pos[1]), (myPos[2] - pos[2]) * 2.0f);
                if (distSquared < radius)
                {
                    gotBlowedUp(tank, GotRunOver, myTank->getId());
                    dead = true;
                }
            }
        }
        for (i = 0; !dead && i < curMaxPlayers; i++)
        {
            if (remotePlayers[i] && !remotePlayers[i]->isPaused() &&
                    ((remotePlayers[i]->getFlag() == Flags::Steamroller) ||
                     ((tank->getFlag() == Flags::Burrow) && remotePlayers[i]->isAlive() &&
                      !remotePlayers[i]->isPhantomZoned())))
            {
                const float* pos = remotePlayers[i]->getPosition();
                if (pos[2] < 0.0f) continue;
                const float radius = myRadius +
                                     (BZDB.eval(StateDatabase::BZDB_SRRADIUSMULT) * remotePlayers[i]->getRadius());
                const float distSquared =
                    hypotf(hypotf(myPos[0] - pos[0],
                                  myPos[1] - pos[1]), (myPos[2] - pos[2]) * 2.0f);
                if (distSquared < radius)
                {
                    gotBlowedUp(tank, GotRunOver, remotePlayers[i]->getId());
                    dead = true;
                }
            }
        }
    }
}

static void     checkEnvironmentForRobots()
{
    for (int i = 0; i < numRobots; i++)
        if (robots[i])
            checkEnvironment(robots[i]);
}

static void     sendRobotUpdates()
{
    for (int i = 0; i < numRobots; i++)
        if (robots[i] && robotServer[i] && robots[i]->isDeadReckoningWrong())
            robotServer[i]->sendPlayerUpdate(robots[i]);
}

static void     addRobots()
{
    uint16_t code, len;
    char msg[MaxPacketLen];
    char callsign[CallSignLen];
    int i, j;

    // add solo robots only when the server allows them
    if (BZDB.isTrue(StateDatabase::BZDB_DISABLEBOTS))
    {
        numRobots = 0;
        if (numRobotTanks > 0)
            addMessage(NULL, "Solo robots are prohibited on this server.");
        return;
    }

    for (i = 0, j = 0; i < numRobotTanks; i++)
    {
        robotServer[j] = new ServerLink(serverNetworkAddress, startupInfo.serverPort);
        if (!robotServer[j] || robotServer[j]->getState() != ServerLink::Okay)
        {
            delete robotServer[j];
            continue;
        }
        else
        {
            snprintf(callsign, CallSignLen, "%.29s%2.2x", myTank->getCallSign(), (short int)j);
            robots[j] = new RobotPlayer(robotServer[j]->getId(), callsign,
                                        robotServer[j], myTank->getMotto());
            robots[j]->setTeam(AutomaticTeam);
            robotServer[j]->sendEnter(ComputerPlayer, robots[j]->getTeam(),
                                      robots[j]->getCallSign(),
                                      robots[j]->getMotto(), "");
        }
        j++;
    }
    numRobots = j;

    for (j = 0; j < numRobots; j++)
    {
        // wait for response
        if (robotServer[j] && (robotServer[j]->read(code, len, msg, -1) < 0 || code != MsgAccept))
        {
            delete robots[j];
            delete robotServer[j];
            robots[j] = NULL;
            robotServer[j] = NULL;
        }
    }

    int k;
    // packing
    for (k = 0, j = 0; j < numRobots; j++)
    {
        if (k != j)
        {
            robotServer[k] = robotServer[j];
            robots[k]      = robots[j];
        }
        if (robotServer[j])
            k++;
    }
    numRobots = k;

    if (numRobots > 0)
    {
        makeObstacleList();
        RobotPlayer::setObstacleList(&obstacleList);
    }
}

#endif


static void setTankFlags()
{
    // scan through flags and, for flags on
    // tanks, tell the tank about its flag.
    const int maxFlags = world->getMaxFlags();
    for (int i = 0; i < maxFlags; i++)
    {
        const Flag& flag = world->getFlag(i);
        if (flag.status == FlagOnTank)
        {
            for (int j = 0; j < curMaxPlayers; j++)
            {
                if (remotePlayers[j] && remotePlayers[j]->getId() == flag.owner)
                {
                    remotePlayers[j]->setFlag(flag.type);
                    break;
                }
            }
        }
    }
}


static void enteringServer(const void *buf)
{
    // the server sends back the team the player was joined to
    const void *tmpbuf = buf;
    uint16_t team, type, wins, losses, tks;
    char callsign[CallSignLen];
    char motto[MottoLen];
    tmpbuf = nboUnpackUShort(tmpbuf, type);
    tmpbuf = nboUnpackUShort(tmpbuf, team);
    tmpbuf = nboUnpackUShort(tmpbuf, wins);           // not used
    tmpbuf = nboUnpackUShort(tmpbuf, losses);         // not used
    tmpbuf = nboUnpackUShort(tmpbuf, tks);            // not used
    tmpbuf = nboUnpackString(tmpbuf, callsign, CallSignLen);  // not used
    tmpbuf = nboUnpackString(tmpbuf, motto, MottoLen);

    // if server assigns us a different team, display a message
    std::string teamMsg;
    if (myTank->getTeam() != AutomaticTeam)
    {
        teamMsg = TextUtils::format("%s team was unavailable, you were joined ",
                                    Team::getName(myTank->getTeam()));
        if ((TeamColor)team == ObserverTeam)
            teamMsg += "as an Observer";
        else
        {
            teamMsg += TextUtils::format("to the %s",
                                         Team::getName((TeamColor)team));
        }
    }
    else
    {
        if ((TeamColor)team == ObserverTeam)
            teamMsg = "You were joined as an observer";
        else
        {
            if (team != RogueTeam)
                teamMsg = TextUtils::format("You joined the %s",
                                            Team::getName((TeamColor)team));
            else
                teamMsg = TextUtils::format("You joined as a %s",
                                            Team::getName((TeamColor)team));
        }
    }
    if (myTank->getTeam() != (TeamColor)team)
    {
        myTank->setTeam((TeamColor)team);
        hud->setAlert(1, teamMsg.c_str(), 8.0f,
                      (TeamColor)team==ObserverTeam?true:false);
        addMessage(NULL, teamMsg.c_str(), 3, true);
    }

    // observer colors are actually cyan, make them black
    const bool observer = (myTank->getTeam() == ObserverTeam);
    const GLfloat* borderColor;
    if (observer)
    {
        static const GLfloat black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        borderColor = black;
    }
    else
        borderColor = Team::getRadarColor(myTank->getTeam());
    controlPanel->setControlColor(borderColor);
    radar->setControlColor(borderColor);

    if ((myTank->getTeam() == ObserverTeam) || devDriving)
    {
        const std::string roamStr = BZDB.get("roamView");
        Roaming::RoamingView roamView = ROAM.parseView(roamStr);
        if (roamView <= Roaming::roamViewDisabled)
            roamView = Roaming::roamViewFP;
        ROAM.setMode(roamView);
        //    ROAM.resetCamera();
    }
    else
        ROAM.setMode(Roaming::roamViewDisabled);

    myTank->setMotto(motto);  // use motto provided by the server

    setTankFlags();

    // clear now invalid token
    startupInfo.token[0] = '\0';

    // add robot tanks
#if defined(ROBOT)
    addRobots();
#endif

    // resize background and adjust time (this is needed even if we
    // don't sync with the server)
    sceneRenderer->getBackground()->resize();
    float syncTime = BZDB.eval(StateDatabase::BZDB_SYNCTIME);
    if (syncTime < 0.0f)
        updateDaylight(epochOffset, *sceneRenderer);
    else
    {
        epochOffset = (double)syncTime;
        updateDaylight(epochOffset, *sceneRenderer);
    }
    lastEpochOffset = epochOffset;

    // restore the sound
    if (savedVolume != -1)
    {
        setSoundVolume(savedVolume);
        savedVolume = -1;
    }

    // initialize some other stuff
    updateFlag(Flags::Null);
    updateHighScores();
    hud->setHeading(myTank->getAngle());
    hud->setAltitude(myTank->getPosition()[2]);
    hud->setTimeLeft((uint32_t)~0);
    fireButton = false;
    firstLife = true;

    BZDB.setBool("displayMainFlags", true);
    BZDB.setBool("displayRadarFlags", true);
    BZDB.setBool("displayRadar", true);
    BZDB.setBool("displayConsole", true);

    entered = true;
}

static void cleanWorldCache()
{
    // setup the world cache limit
    int cacheLimit = (10 * 1024 * 1024);
    if (BZDB.isSet("worldCacheLimit"))
    {
        const int dbCacheLimit = BZDB.evalInt("worldCacheLimit");
        // the old limit was 100 Kbytes, too small
        if (dbCacheLimit == (100 * 1024))
            BZDB.setInt("worldCacheLimit", cacheLimit);
        else
            cacheLimit = dbCacheLimit;
    }
    else
        BZDB.setInt("worldCacheLimit", cacheLimit);

    const std::string worldPath = getCacheDirName();

    while (true)
    {
        char *oldestFile = NULL;
        int oldestSize = 0;
        int totalSize = 0;

#ifdef _WIN32
        std::string pattern = worldPath + "*.bwc";
        WIN32_FIND_DATA findData;
        HANDLE h = FindFirstFile(pattern.c_str(), &findData);
        if (h != INVALID_HANDLE_VALUE)
        {
            FILETIME oldestTime;
            while (FindNextFile(h, &findData))
            {
                if ((oldestFile == NULL) ||
                        (CompareFileTime(&oldestTime, &findData.ftLastAccessTime) > 0))
                {
                    if (oldestFile)
                        free(oldestFile);
                    oldestFile = strdup(findData.cFileName);
                    oldestSize = findData.nFileSizeLow;
                    oldestTime = findData.ftLastAccessTime;
                }
                totalSize += findData.nFileSizeLow;
            }
            FindClose(h);
        }
#else
        DIR *directory = opendir(worldPath.c_str());
        if (directory)
        {
            struct dirent* contents;
            struct stat statbuf;
            time_t oldestTime = 0;
            while ((contents = readdir(directory)))
            {
                const std::string filename = contents->d_name;
                const std::string fullname = worldPath + filename;
                stat(fullname.c_str(), &statbuf);
                if (S_ISREG(statbuf.st_mode) && (filename.size() > 4) &&
                        (filename.substr(filename.size() - 4) == ".bwc"))
                {
                    if ((oldestFile == NULL) || (statbuf.st_atime < oldestTime))
                    {
                        if (oldestFile)
                            free(oldestFile);
                        oldestFile = strdup(contents->d_name);
                        oldestSize = statbuf.st_size;
                        oldestTime = statbuf.st_atime;
                    }
                    totalSize += statbuf.st_size;
                }
            }
            closedir(directory);
        }
#endif

        // any valid cache files?
        if (oldestFile == NULL)
            return;

        // is the cache small enough?
        if (totalSize < cacheLimit)
        {
            if (oldestFile != NULL)
            {
                free(oldestFile);
                oldestFile = NULL;
            }
            return;
        }

        // remove the oldest file
        logDebugMessage(1,"cleanWorldCache: removed %s\n", oldestFile);
        remove((worldPath + oldestFile).c_str());
        free(oldestFile);
        totalSize -= oldestSize;
    }
}


static void markOld(std::string &fileName)
{
#ifdef _WIN32
    FILETIME ft;
    HANDLE h = CreateFile(fileName.c_str(),
                          FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA, 0, NULL,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        SYSTEMTIME st;
        memset(&st, 0, sizeof(st));
        st.wYear = 1900;
        st.wMonth = 1;
        st.wDay = 1;
        SystemTimeToFileTime(&st, &ft);
        SetFileTime(h, &ft, &ft, &ft);
        GetLastError();
        CloseHandle(h);
    }
#else
    struct utimbuf times;
    times.actime = 0;
    times.modtime = 0;
    utime(fileName.c_str(), &times);
#endif
}


static void sendFlagNegotiation()
{
    char msg[MaxPacketLen];
    FlagTypeMap::iterator i;
    char *buf = msg;

    /* Send MsgNegotiateFlags to the server with
     * the abbreviations for all the flags we support.
     */
    for (i = FlagType::getFlagMap().begin();
            i != FlagType::getFlagMap().end();
            ++i)
        buf = (char*) i->second->pack(buf);
    serverLink->send(MsgNegotiateFlags, buf - msg, msg);
}


#if defined(FIXME) && defined(ROBOT)
static void saveRobotInfo(Playerid id, void *msg)
{
    for (int i = 0; i < numRobots; i++)
        if (robots[i])
        {
            void *tmpbuf = msg;
            uint16_t team, type, wins, losses, tks;
            char callsign[CallSignLen];
            char motto[MottoLen];
            tmpbuf = nboUnpackUShort(tmpbuf, type);
            tmpbuf = nboUnpackUShort(tmpbuf, team);
            tmpbuf = nboUnpackUShort(tmpbuf, wins);
            tmpbuf = nboUnpackUShort(tmpbuf, losses);
            tmpbuf = nboUnpackUShort(tmpbuf, tks);
            tmpbuf = nboUnpackString(tmpbuf, callsign, CallSignLen);
            tmpbuf = nboUnpackString(tmpbuf, motto, MottoLen);
            std::cerr << "id " << id.port << ':' <<
                      id.number << ':' <<
                      callsign << ' ' <<
                      robots[i]->getId().port << ':' <<
                      robots[i]->getId().number << ':' <<
                      robots[i]->getCallsign() << std::endl;
            if (strncmp(robots[i]->getCallSign(), callsign, CallSignLen))
            {
                // check for real robot id
                char buffer[100];
                snprintf(buffer, 100, "id test %p %p %p %8.8x %8.8x\n",
                         robots[i], tmpbuf, msg, *(int *)tmpbuf, *((int *)tmpbuf + 1));
                std::cerr << buffer;
                if (tmpbuf < (char *)msg + len)
                {
                    PlayerId id;
                    tmpbuf = nboUnpackUByte(tmpbuf, id);
                    robots[i]->id.serverHost = id.serverHost;
                    robots[i]->id.port = id.port;
                    robots[i]->id.number = id.number;
                    robots[i]->server->send(MsgIdAck, 0, NULL);
                }
            }
        }
}
#endif

static void resetServerVar(const std::string& name, void*)
{
    // reset server-side variables
    if (BZDB.getPermission(name) == StateDatabase::Locked)
    {
        const std::string defval = BZDB.getDefault(name);
        BZDB.set(name, defval);
    }
}

void        leaveGame()
{
    entered = false;
    joiningGame = false;

    // no more radar
    //  radar->setWorld(NULL);

    //  controlPanel->setRadarRenderer(NULL);
    /*
      delete radar;
      radar = NULL;
    */

#if defined(ROBOT)
    // shut down robot connections
    int i;
    for (i = 0; i < numRobots; i++)
    {
        if (robots[i] && robotServer[i])
            robotServer[i]->send(MsgExit, 0, NULL);
        delete robots[i];
        delete robotServer[i];
        robots[i] = NULL;
        robotServer[i] = NULL;
    }
    numRobots = 0;

    const int count = obstacleList.size();
    for (i = 0; i < count; i++)
        delete obstacleList[i];
    obstacleList.clear();
#endif

    // my tank goes away
    const bool sayGoodbye = (myTank != NULL);
    LocalPlayer::setMyTank(NULL);
    delete myTank;
    myTank = NULL;

    // reset the daylight time
    const bool syncTime = (BZDB.eval(StateDatabase::BZDB_SYNCTIME) >= 0.0f);
    const bool fixedTime = BZDB.isSet("fixedTime");
    if (syncTime)
    {
        // return to the desired user time
        epochOffset = userTimeEpochOffset;
    }
    else if (fixedTime)
    {
        // save the current user time
        userTimeEpochOffset = epochOffset;
    }
    else
    {
        // revert back to when the client was started?
        epochOffset = userTimeEpochOffset;
    }
    updateDaylight(epochOffset, *sceneRenderer);
    lastEpochOffset = epochOffset;
    BZDB.set(StateDatabase::BZDB_SYNCTIME,
             BZDB.getDefault(StateDatabase::BZDB_SYNCTIME));

    // flush downloaded textures (before the BzMaterials are nuked)
    Downloads::removeTextures();

    // delete world
    World::setWorld(NULL);
    delete world;
    world = NULL;
    teams = NULL;
    curMaxPlayers = 0;
    numFlags = 0;
    remotePlayers = NULL;

    // update UI
    hud->setPlaying(false);
    hud->setCracks(false);
    hud->setPlayerHasHighScore(false);
    hud->setTeamHasHighScore(false);
    hud->setHeading(0.0f);
    hud->setAltitude(0.0f);
    hud->setAltitudeTape(false);

    // shut down server connection
    if (sayGoodbye) serverLink->send(MsgExit, 0, NULL);
    ServerLink::setServer(NULL);
    delete serverLink;
    serverLink = NULL;
    serverNetworkAddress = Address();

    // reset viewpoint
    float eyePoint[3], targetPoint[3];
    eyePoint[0] = 0.0f;
    eyePoint[1] = 0.0f;
    eyePoint[2] = 0.0f + BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
    targetPoint[0] = eyePoint[0] - 1.0f;
    targetPoint[1] = eyePoint[1] + 0.0f;
    targetPoint[2] = eyePoint[2] + 0.0f;
    sceneRenderer->getViewFrustum().setProjection((float)(60.0 * M_PI / 180.0),
            NearPlaneNormal,
            FarPlaneDefault,
            FarDeepPlaneDefault,
            mainWindow->getWidth(),
            mainWindow->getHeight(),
            mainWindow->getViewHeight());
    sceneRenderer->getViewFrustum().setView(eyePoint, targetPoint);

    // reset some flags
    gameOver = false;
    serverError = false;
    serverDied = false;

    // delete scene database (after the world has been destroyed)
    sceneRenderer->setSceneDatabase(NULL);

    // reset the BZDB variables
    BZDB.iterate(resetServerVar, NULL);

    // purge any custom flags we may have accumulated
    Flags::clearCustomFlags();

    return;
}


static void joinInternetGame()
{
    // check server address
    if (serverNetworkAddress.isAny())
    {
        HUDDialogStack::get()->setFailedMessage("Server not found");
        return;
    }

    // check for a local server block
    ServerAccessList.reload();
    std::vector<std::string> nameAndIp;
    nameAndIp.push_back(startupInfo.serverName);
    nameAndIp.push_back(serverNetworkAddress.getDotNotation());
    if (!ServerAccessList.authorized(nameAndIp))
    {
        HUDDialogStack::get()->setFailedMessage("Server Access Denied Locally");
        std::string msg = ColorStrings[WhiteColor];
        msg += "NOTE: ";
        msg += ColorStrings[GreyColor];
        msg += "server access is controlled by ";
        msg += ColorStrings[YellowColor];
        msg += ServerAccessList.getFilePath();
        addMessage(NULL, msg);
        return;
    }

    // open server
    ServerLink* _serverLink = new ServerLink(serverNetworkAddress,
            startupInfo.serverPort);

#if defined(ROBOT)
    numRobots = 0;
#endif

    delete serverLink;
    serverLink = _serverLink;

    // assume everything's okay for now
    serverDied = false;
    serverError = false;

    if (!serverLink)
    {
        HUDDialogStack::get()->setFailedMessage("Memory error");
        return;
    }

    // printError("Join Game");
    // check server
    if (serverLink->getState() != ServerLink::Okay)
    {
        switch (serverLink->getState())
        {
        case ServerLink::BadVersion:
        {
            char versionError[37];
            snprintf(versionError, sizeof(versionError), "Incompatible server version %s", serverLink->getVersion());
            HUDDialogStack::get()->setFailedMessage(versionError);
            break;
        }

        // you got banned
        case ServerLink::Refused:
        {
            const std::string& rejmsg = serverLink->getRejectionMessage();

            // add to the HUD
            std::string msg = ColorStrings[RedColor];
            msg += "You have been banned from this server";
            HUDDialogStack::get()->setFailedMessage(msg.c_str());

            // add to the console
            msg = ColorStrings[RedColor];
            msg += "You have been banned from this server:";
            addMessage(NULL, msg);
            msg = ColorStrings[YellowColor];
            msg += rejmsg;
            addMessage(NULL, msg);

            break;
        }

        case ServerLink::Rejected:
            // the server is probably full or the game is over.  if not then
            // the server is having network problems.
            HUDDialogStack::get()->setFailedMessage
            ("Game is full or over.  Try again later.");
            break;

        case ServerLink::SocketError:
            HUDDialogStack::get()->setFailedMessage("Error connecting to server.");
            break;

        case ServerLink::CrippledVersion:
            // can't connect to (otherwise compatible) non-crippled server
            HUDDialogStack::get()->setFailedMessage
            ("Cannot connect to full version server.");
            break;

        default:
            HUDDialogStack::get()->setFailedMessage
            (TextUtils::format
             ("Internal error connecting to server (error code %d).",
              serverLink->getState()).c_str());
            break;
        }
        return;
    }

    // use parallel UDP if desired and using server relay
    if (startupInfo.useUDPconnection)
        serverLink->sendUDPlinkRequest();
    else
        printError("No UDP connection, see Options to enable.");

    HUDDialogStack::get()->setFailedMessage("Connection Established...");

    sendFlagNegotiation();
    joiningGame = true;
    scoreboard->huntReset();
    GameTime::reset();
}


static void addVarToAutoComplete(const std::string& name, void* UNUSED(userData))
{
    if ((name.size() <= 0) || (name[0] != '_'))
    {
        return; // we're skipping "poll"
    }
    if (BZDB.getPermission(name) == StateDatabase::Server)
        completer.registerWord(name);
    return;
}

static void joinInternetGame2()
{
    justJoined = true;

    HUDDialogStack::get()->setFailedMessage("Entering game...");

    ServerLink::setServer(serverLink);
    World::setWorld(world);

    // prep teams
    teams = world->getTeams();

    // prep players
    curMaxPlayers = 0;
    remotePlayers = world->getPlayers();

    // reset the autocompleter
    completer.setDefaults();
    BZDB.iterate(addVarToAutoComplete, NULL);

    // prep flags
    numFlags = world->getMaxFlags();

    // make scene database
    setSceneDatabase();
    mainWindow->getWindow()->yieldCurrent();

    // make radar
    //  radar = new RadarRenderer(*sceneRenderer, *world);
    //  mainWindow->getWindow()->yieldCurrent();
    radar->setWorld(world);
    controlPanel->setRadarRenderer(radar);
    controlPanel->resize();

    // make local player
    myTank = new LocalPlayer(serverLink->getId(), startupInfo.callsign,
                             startupInfo.motto);
    myTank->setTeam(startupInfo.team);
    LocalPlayer::setMyTank(myTank);

    if (world->allowRabbit() && myTank->getTeam() != ObserverTeam)
        myTank->setTeam(HunterTeam);

    // tell server we want to join
    serverLink->sendEnter(TankPlayer, myTank->getTeam(),
                          myTank->getCallSign(),
                          myTank->getMotto(),
                          startupInfo.token);
    startupInfo.token[0] = '\0';

    // hopefully it worked!  pop all the menus.
    HUDDialogStack* stack = HUDDialogStack::get();
    while (stack->isActive())
        stack->pop();
    joiningGame = false;
}


static void     renderDialog()
{
    if (HUDDialogStack::get()->isActive())
    {
        glMatrixMode(GL_PROJECTION);
        mainWindow->setProjectionPlay();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        OpenGLGState::resetState();
        HUDDialogStack::get()->render();
        glPopMatrix();
    }
}


static void checkDirtyControlPanel(ControlPanel *cp)
{
    if (cp)
    {
        if (HUDDialogStack::get()->isActive())
            cp->invalidate();
    }
    return;
}


static void renderRoamMouse()
{
    if (!ROAM.isRoaming() ||
            !myTank || (myTank->getTeam() != ObserverTeam) ||
            !(leftMouseButton || rightMouseButton || middleMouseButton))
        return;

    const int sx = mainWindow->getWidth();
    const int sy = mainWindow->getHeight();
    const int ox = mainWindow->getOriginX();
    const int oy = mainWindow->getOriginY();
    int mx, my;
    mainWindow->getWindow()->getMouse(mx, my);
    my = sy - my - 1; // flip the y axis
    const int xc = ox + (sx / 2);
    const int y2 = oy + (mainWindow->getViewHeight() / 2);
    const int yc = (sy - y2 - 1); // flip the y axis

    glPushAttrib(GL_ALL_ATTRIB_BITS);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    mainWindow->setProjectionPlay();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glShadeModel(GL_SMOOTH);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    static const float color0[4] = { 0.0f, 0.0f, 0.0f, 0.1f };
    static const float color1[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glLineWidth(1.49f);
    glBegin(GL_LINES);
    glColor4fv(color0);
    glVertex2i(xc, yc);
    glColor4fv(color1);
    glVertex2i(mx, my);
    glEnd();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glPopAttrib();
}


static void drawUI()
{
    // setup the triangle counts  (FIXME: hackish)
    if (showFPS && showDrawTime)
    {
        hud->setFrameTriangleCount(sceneRenderer->getFrameTriangleCount());
        // NOTE:  the radar triangle count is actually from the previous frame
        if (radar)
            hud->setFrameRadarTriangleCount(radar->getFrameTriangleCount());
        else
            hud->setFrameRadarTriangleCount(0);
    }
    else
    {
        hud->setFrameTriangleCount(0);
        hud->setFrameRadarTriangleCount(0);
    }

    const World *_world = World::getWorld();

    // update the HUD (player list, alerts)
    if (_world && hud)
        hud->render(*sceneRenderer);

    // draw the control panel
    if (controlPanel)
        controlPanel->render(*sceneRenderer);

    // draw the radar
    if (radar && _world)
    {
        const bool showBlankRadar = !myTank || (myTank && myTank->isPaused());
        const bool observer = myTank && (myTank->getTeam() == ObserverTeam);
        radar->render(*sceneRenderer, showBlankRadar, observer);
    }

    // update the HUD (menus)
    renderDialog();

    // render the drag-line
    renderRoamMouse();

    return;
}


//
// stuff to draw a frame
//

static bool trackPlayerShot(Player* target,
                            float* eyePoint, float* targetPoint)
{
    // follow the first shot
    if (BZDB.isTrue("trackShots"))
    {
        const int maxShots = target->getMaxShots();
        const ShotPath* sp = NULL;
        // look for the oldest active shot
        float remaining = +MAXFLOAT;
        for (int s = 0; s < maxShots; s++)
        {
            const ShotPath* spTmp = target->getShot(s);
            if (spTmp != NULL)
            {
                const float t = float(spTmp->getReloadTime() -
                                      (spTmp->getCurrentTime() - spTmp->getStartTime()));
                if ((t > 0.0f) && (t < remaining))
                {
                    sp = spTmp;
                    remaining = t;
                }
            }
        }
        if (sp != NULL)
        {
            const float* pos = sp->getPosition();
            const float* vel = sp->getVelocity();
            const float speed = sqrtf(vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2]);
            if (speed > 0.0f)
            {
                const float ilen = 1.0f / speed;
                const float dir[3] = {ilen * vel[0], ilen * vel[1], ilen * vel[2]};
                float topDir[3] = {1.0f, 0.0f, 0.0f};
                const float hlen = sqrtf(dir[0]*dir[0] + dir[1]*dir[1]);
                if (hlen > 0.0f)
                {
                    topDir[2] = hlen;
                    const float hfactor = -fabsf(dir[2] / hlen);
                    topDir[0] = hfactor * dir[0];
                    topDir[1] = hfactor * dir[1];
                }
                const float offset = -10.0f;
                const float tOffset = +2.0f;
                eyePoint[0] = pos[0] + (offset * dir[0]) + (tOffset * topDir[0]);
                eyePoint[1] = pos[1] + (offset * dir[1]) + (tOffset * topDir[1]);
                eyePoint[2] = pos[2] + (offset * dir[2]) + (tOffset * topDir[2]);
                targetPoint[0] = eyePoint[0] + dir[0];
                targetPoint[1] = eyePoint[1] + dir[1];
                targetPoint[2] = eyePoint[2] + dir[2];
                return true;
            }
        }
    }
    return false;
}

static void setupNearPlane()
{
    NearPlane = NearPlaneNormal;

    const bool showTreads = BZDB.isTrue("showTreads");
    if (!showTreads || !myTank)
        return;

    const Player* tank = myTank;
    if (ROAM.isRoaming())
    {
        if (ROAM.getMode() != Roaming::roamViewFP)
            return;
        if (!devDriving)
            tank = ROAM.getTargetTank();
    }
    if (tank == NULL)
        return;

    const float halfLength = 0.5f * BZDBCache::tankLength;
    const float length = tank->getDimensions()[1];
    if (fabsf(length - halfLength) > 0.1f)
        NearPlane = NearPlaneClose;

    return;
}


static void setupFarPlane()
{
    FarPlane = FarPlaneScale * BZDBCache::worldSize;
    FarPlaneCull = false;
    FarDeepPlane = FarPlane * FarDeepPlaneScale;

    const bool mapFog = BZDB.get(StateDatabase::BZDB_FOGMODE) != "none";

    float farDist = FarPlane;

    if (BZDB.get("_cullDist") == "fog")
    {
        if (mapFog && !BZDB.isTrue("_fogNoSky"))
        {
            const float fogMargin = 1.01f;
            const std::string& fogMode = BZDB.get("_fogMode");
            if (fogMode == "linear")
                farDist = fogMargin * BZDB.eval("_fogEnd");
            else
            {
                const float density = BZDB.eval("_fogDensity");
                if (density > 0.0f)
                {
                    const float fogFactor = 0.01f;
                    if (fogMode == "exp2")
                        farDist = fogMargin * sqrtf(-logf(fogFactor)) / density;
                    else   // default to 'exp'
                        farDist = fogMargin * -logf(fogFactor) / density;
                }
                else
                {
                    // default far plane
                }
            }
        }
        else
        {
            // default far plane
        }
    }
    else
    {
        const float dist = BZDB.eval("_cullDist");
        if (dist > 0.0f)
            farDist = dist;
        else
        {
            // default far plane
        }
    }

    if (farDist < FarPlane)
    {
        FarPlane = farDist;
        FarPlaneCull = true;
    }

    return;
}


void drawFrame(const float dt)
{
    // get view type (constant for entire game)
    static SceneRenderer::ViewType viewType = sceneRenderer->getViewType();
    // get media object
    static BzfMedia* media = PlatformFactory::getMedia();

    static const float    defaultPos[3] = { 0.0f, 0.0f, 0.0f };
    static const float    defaultDir[3] = { 1.0f, 0.0f, 0.0f };
    static int frameCount = 0;
    static float cumTime = 0.0f;

    const float* myTankPos;
    const float* myTankDir;
    float myTankAngle = 0.0f;
    GLfloat fov;
    GLfloat eyePoint[3];
    GLfloat targetPoint[3];

    checkDirtyControlPanel(controlPanel);

    if (!unmapped)
    {
        // compute fps
        frameCount++;
        cumTime += float(dt);
        if (cumTime >= 2.0)
        {
            if (showFPS) hud->setFPS(float(frameCount) / cumTime);
            cumTime = 0.00000001f;
            frameCount = 0;
        }

        // drift clouds
        sceneRenderer->getBackground()->addCloudDrift(1.0f * dt, 0.731f * dt);

        // get tank camera info
        float muzzleHeight;
        if (!myTank)
        {
            myTankPos = defaultPos;
            myTankDir = defaultDir;
            muzzleHeight = BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
            fov = BZDB.eval("defaultFOV");
        }
        else
        {
            myTankPos = myTank->getPosition();
            myTankDir = myTank->getForward();
            myTankAngle = myTank->getAngle();
            muzzleHeight = myTank->getMuzzleHeight();

            if (myTank->getFlag() == Flags::WideAngle)
                fov = 120.0f;
            else
                fov = BZDB.eval("displayFOV");
            if (viewType == SceneRenderer::ThreeChannel)
                fov *= 0.75f;
        }
        fov *= (float)(M_PI / 180.0);

        // set projection and view
        eyePoint[0] = myTankPos[0];
        eyePoint[1] = myTankPos[1];
        eyePoint[2] = myTankPos[2] + muzzleHeight;
        targetPoint[0] = eyePoint[0] + myTankDir[0];
        targetPoint[1] = eyePoint[1] + myTankDir[1];
        targetPoint[2] = eyePoint[2] + myTankDir[2];

        if (devDriving || ROAM.isRoaming())
        {
            hud->setAltitude(-1.0f);
            float roamViewAngle;
            const Roaming::RoamingCamera* roam = ROAM.getCamera();
            if (!(ROAM.getMode() == Roaming::roamViewFree) &&
                    (ROAM.getTargetTank() || (devDriving && myTank)))
            {
                Player *target;
                if (!devDriving)
                    target = ROAM.getTargetTank();
                else
                    target = myTank;
                const float *targetTankDir = target->getForward();
                // fixed camera tracking target
                if (ROAM.getMode() == Roaming::roamViewTrack)
                {
                    eyePoint[0] = roam->pos[0];
                    eyePoint[1] = roam->pos[1];
                    eyePoint[2] = roam->pos[2];
                    targetPoint[0] = target->getPosition()[0];
                    targetPoint[1] = target->getPosition()[1];
                    targetPoint[2] = target->getPosition()[2] +
                                     target->getMuzzleHeight();
                }
                // camera following target
                else if (ROAM.getMode() == Roaming::roamViewFollow)
                {
                    if (!trackPlayerShot(target, eyePoint, targetPoint))
                    {
                        const bool slowKB = BZDB.isTrue("slowKeyboard");
                        if (slowKB == (BZDB.eval("roamSmoothTime") < 0.0f))
                        {
                            eyePoint[0] = target->getPosition()[0] - targetTankDir[0] * 40;
                            eyePoint[1] = target->getPosition()[1] - targetTankDir[1] * 40;
                            eyePoint[2] = target->getPosition()[2] + muzzleHeight * 6;
                            targetPoint[0] = target->getPosition()[0];
                            targetPoint[1] = target->getPosition()[1];
                            targetPoint[2] = target->getPosition()[2];
                        }
                        else
                        {
                            // the same as for the roamViewTrack mode
                            eyePoint[0] = roam->pos[0];
                            eyePoint[1] = roam->pos[1];
                            eyePoint[2] = roam->pos[2];
                            targetPoint[0] = target->getPosition()[0];
                            targetPoint[1] = target->getPosition()[1];
                            targetPoint[2] = target->getPosition()[2] +
                                             target->getMuzzleHeight();
                            if (BZDB.isSet("followOffsetZ"))
                                targetPoint[2] += BZDB.eval("followOffsetZ");
                        }
                    }
                }
                // target's view
                else if (ROAM.getMode() == Roaming::roamViewFP)
                {
                    if (!trackPlayerShot(target, eyePoint, targetPoint))
                    {
                        eyePoint[0] = target->getPosition()[0];
                        eyePoint[1] = target->getPosition()[1];
                        eyePoint[2] = target->getPosition()[2] + target->getMuzzleHeight();
                        targetPoint[0] = eyePoint[0] + targetTankDir[0];
                        targetPoint[1] = eyePoint[1] + targetTankDir[1];
                        targetPoint[2] = eyePoint[2] + targetTankDir[2];
                        hud->setAltitude(target->getPosition()[2]);
                    }
                }
                // track team flag
                else if (ROAM.getMode() == Roaming::roamViewFlag)
                {
                    Flag* targetFlag = ROAM.getTargetFlag();
                    eyePoint[0] = roam->pos[0];
                    eyePoint[1] = roam->pos[1];
                    eyePoint[2] = roam->pos[2];
                    targetPoint[0] = targetFlag->position[0];
                    targetPoint[1] = targetFlag->position[1];
                    targetPoint[2] = targetFlag->position[2];
                    if (targetFlag->status != FlagOnTank)
                        targetPoint[2] += muzzleHeight;
                    else
                    {
                        targetPoint[2] -= (BZDBCache::tankHeight -
                                           BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT));
                    }
                }
                roamViewAngle = (float) (atan2(targetPoint[1]-eyePoint[1],
                                               targetPoint[0]-eyePoint[0]) * 180.0f / M_PI);
            }
            // free Roaming
            else
            {
                float dir[3];
                dir[0] = cosf((float)(roam->phi * M_PI / 180.0)) * cosf((float)(roam->theta * M_PI / 180.0));
                dir[1] = cosf((float)(roam->phi * M_PI / 180.0)) * sinf((float)(roam->theta * M_PI / 180.0));
                dir[2] = sinf((float)(roam->phi * M_PI / 180.0));
                eyePoint[0] = roam->pos[0];
                eyePoint[1] = roam->pos[1];
                eyePoint[2] = roam->pos[2];
                targetPoint[0] = eyePoint[0] + dir[0];
                targetPoint[1] = eyePoint[1] + dir[1];
                targetPoint[2] = eyePoint[2] + dir[2];
                roamViewAngle = roam->theta;
            }
            if (!devDriving)
            {
                float virtPos[] = {eyePoint[0], eyePoint[1], 0};
                if (myTank)
                    myTank->move(virtPos, (float)(roamViewAngle * M_PI / 180.0));
            }
            fov = (float)(roam->zoom * M_PI / 180.0);
            moveSoundReceiver(eyePoint[0], eyePoint[1], eyePoint[2], myTankAngle, false);
        }

        // only use a close plane for drawing in the
        // cockpit, and even then only for odd sized tanks
        setupNearPlane();

        // based on fog and _cullDist
        setupFarPlane();

        ViewFrustum& viewFrustum = sceneRenderer->getViewFrustum();

        viewFrustum.setProjection(fov, NearPlane, FarPlane, FarDeepPlane,
                                  mainWindow->getWidth(),
                                  mainWindow->getHeight(),
                                  mainWindow->getViewHeight());
        viewFrustum.setFarPlaneCull(FarPlaneCull);

        viewFrustum.setView(eyePoint, targetPoint);

        // add dynamic nodes
        SceneDatabase* scene = sceneRenderer->getSceneDatabase();
        World *_world = World::getWorld();
        if (scene && myTank && _world)
        {

            int i;
            const bool seerView = (myTank->getFlag() == Flags::Seer);
            const bool showTreads = BZDB.isTrue("showTreads");

            // add my tank if required
            const bool inCockpit = (!devDriving || (ROAM.getMode() == Roaming::roamViewFP));
            const bool showMyTreads = showTreads ||
                                      (devDriving && (ROAM.getMode() != Roaming::roamViewFP));
            myTank->addToScene(scene, myTank->getTeam(),
                               inCockpit, seerView,
                               showMyTreads, showMyTreads /*showIDL*/);

            // add my shells
            myTank->addShots(scene, false);

            // add server shells
            world->getWorldWeapons()->addShots(scene, false);

            // add antidote flag
            myTank->addAntidote(scene);

            // add flags
            world->addFlags(scene, seerView);


            // add other tanks and shells
            for (i = 0; i < curMaxPlayers; i++)
            {
                if (remotePlayers[i])
                {
                    const bool colorblind = (myTank->getFlag() == Flags::Colorblindness);
                    remotePlayers[i]->addShots(scene, colorblind);

                    TeamColor effectiveTeam = RogueTeam;
                    if (!colorblind)
                    {
                        if ((remotePlayers[i]->getFlag() == Flags::Masquerade)
                                && (myTank->getFlag() != Flags::Seer)
                                && (myTank->getTeam() != ObserverTeam))
                            effectiveTeam = myTank->getTeam();
                        else
                            effectiveTeam = remotePlayers[i]->getTeam();
                    }

                    const bool inCockpt  = ROAM.isRoaming() && !devDriving &&
                                           (ROAM.getMode() == Roaming::roamViewFP) &&
                                           ROAM.getTargetTank() &&
                                           (ROAM.getTargetTank()->getId() == i);
                    const bool showPlayer = !inCockpt || showTreads;

                    // add player tank if required
                    remotePlayers[i]->addToScene(scene, effectiveTeam,
                                                 inCockpt, seerView,
                                                 showPlayer, showPlayer /*showIDL*/);
                }
            }

            // add explosions
            addExplosions(scene);

            // if inside a building, add some eighth dimension scene nodes.
            const std::vector<const Obstacle*>& list = myTank->getInsideBuildings();
            for (unsigned int n = 0; n < list.size(); n++)
            {
                const Obstacle* obs = list[n];
                const int nodeCount = obs->getInsideSceneNodeCount();
                SceneNode** nodeList = obs->getInsideSceneNodeList();
                for (int o = 0; o < nodeCount; o++)
                    scene->addDynamicNode(nodeList[o]);
            }
        }

        // turn blanking and inversion on/off as appropriate
        sceneRenderer->setBlank(myTank && (myTank->isPaused() ||
                                           myTank->getFlag() == Flags::Blindness));
        sceneRenderer->setInvert(myTank && myTank->isPhantomZoned());

        // turn on scene dimming when showing menu, when we're dead
        // and no longer exploding, or when we are in a building.
        bool insideDim = false;
        if (myTank)
        {
            const float hnp = 0.5f * NearPlane; // half near plane distance
            const float* eye = viewFrustum.getEye();
            const float* dir = viewFrustum.getDirection();
            float clipPos[3];
            clipPos[0] = eye[0] + (dir[0] * hnp);
            clipPos[1] = eye[1] + (dir[1] * hnp);
            clipPos[2] = eye[2];
            const Obstacle* obs;
            obs = world->inBuilding(clipPos, myTank->getAngle(), hnp, 0.0f, 0.0f);
            if (obs != NULL)
                insideDim = true;
        }
        sceneRenderer->setDim(HUDDialogStack::get()->isActive() || insideDim ||
                              ((myTank && !ROAM.isRoaming() && !devDriving) &&
                               !myTank->isAlive() && !myTank->isExploding()));

        // turn on panel dimming when showing the menu (both radar and chat)
        if (HUDDialogStack::get()->isActive())
        {
            if (controlPanel)
                controlPanel->setDimming(0.8f);
            if (radar)
                radar->setDimming(0.8f);
        }
        else
        {
            if (controlPanel)
                controlPanel->setDimming(0.0f);
            if (radar)
                radar->setDimming(0.0f);
        }

        // set hud state
        hud->setDim(HUDDialogStack::get()->isActive());
        hud->setPlaying(myTank && (myTank->isAlive() && !myTank->isPaused()));
        hud->setRoaming(ROAM.isRoaming());
        hud->setCracks(myTank && !firstLife && !justJoined && !myTank->isAlive());

        // get frame start time
        if (showDrawTime)
        {
#if defined(DEBUG_RENDERING)
            // get an accurate measure of frame time (at expense of frame rate)
            if (BZDB.isTrue("glFinish"))
                glFinish();
#endif
            media->stopwatch(true);
        }

        // so observers can have enhanced radar
        if (ROAM.isRoaming() && myTank && !devDriving)
        {
            if (ROAM.getMode() == Roaming::roamViewFP && ROAM.getTargetTank())
                myTank->setZpos(ROAM.getTargetTank()->getPosition()[2]);
            else
                myTank->setZpos(ROAM.getCamera()->pos[2]);
        }

        // let the hud save off the view matrix so it can do view projections
        if (hud)
        {
            hud->saveMatrixes(viewFrustum.getViewMatrix(),
                              viewFrustum.getProjectionMatrix());
        }

        // draw frame
        if (viewType == SceneRenderer::ThreeChannel)
        {
            // draw center channel
            sceneRenderer->render(false);
            drawUI();

            // set up for drawing left channel
            mainWindow->setQuadrant(MainWindow::LowerLeft);
            // FIXME -- this assumes up is along +z
            const float cFOV = cosf(fov);
            const float sFOV = sinf(fov);
            targetPoint[0] = eyePoint[0] + cFOV*myTankDir[0] - sFOV*myTankDir[1];
            targetPoint[1] = eyePoint[1] + cFOV*myTankDir[1] + sFOV*myTankDir[0];
            targetPoint[2] = eyePoint[2] + myTankDir[2];
            viewFrustum.setView(eyePoint, targetPoint);

            // draw left channel
            sceneRenderer->render(false, true, true);

            // set up for drawing right channel
            mainWindow->setQuadrant(MainWindow::LowerRight);
            // FIXME -- this assumes up is along +z
            targetPoint[0] = eyePoint[0] + cFOV*myTankDir[0] + sFOV*myTankDir[1];
            targetPoint[1] = eyePoint[1] + cFOV*myTankDir[1] - sFOV*myTankDir[0];
            targetPoint[2] = eyePoint[2] + myTankDir[2];
            viewFrustum.setView(eyePoint, targetPoint);

            // draw right channel
            sceneRenderer->render(true, true, true);

#if defined(DEBUG_RENDERING)
            // set up for drawing rear channel
            mainWindow->setQuadrant(MainWindow::UpperLeft);
            // FIXME -- this assumes up is along +z
            targetPoint[0] = eyePoint[0] - myTankDir[0];
            targetPoint[1] = eyePoint[1] - myTankDir[1];
            targetPoint[2] = eyePoint[2] + myTankDir[2];
            viewFrustum.setView(eyePoint, targetPoint);

            // draw rear channel
            sceneRenderer->render(true, true, true);
#endif
            // back to center channel
            mainWindow->setQuadrant(MainWindow::UpperRight);
        }
        else if (viewType == SceneRenderer::Stacked)
        {
            float EyeDisplacement = 0.25f * BZDBCache::tankWidth;
            float FocalPlane = BZDB.eval(StateDatabase::BZDB_BOXBASE);
            if (BZDB.isSet("eyesep"))
                EyeDisplacement = BZDB.eval("eyesep");
            if (BZDB.isSet("focal"))
                FocalPlane = BZDB.eval("focal");

            // setup view for left eye
            viewFrustum.setOffset(EyeDisplacement, FocalPlane);

            // draw left eye's view
            sceneRenderer->render(false);
            drawUI();

            // set up view for right eye
            mainWindow->setQuadrant(MainWindow::UpperHalf);
            viewFrustum.setOffset(-EyeDisplacement, FocalPlane);

            // draw right eye's view
            sceneRenderer->render(true, true);
            drawUI();

            // draw common stuff

            // back to left channel
            mainWindow->setQuadrant(MainWindow::LowerHalf);
        }
        else if (viewType == SceneRenderer::Stereo)
        {
            float EyeDisplacement = 0.25f * BZDBCache::tankWidth;
            float FocalPlane = BZDB.eval(StateDatabase::BZDB_BOXBASE);
            if (BZDB.isSet("eyesep"))
                EyeDisplacement = BZDB.eval("eyesep");
            if (BZDB.isSet("focal"))
                FocalPlane = BZDB.eval("focal");

            // setup view for left eye
#ifdef USE_GL_STEREO
            glDrawBuffer(GL_BACK_LEFT);
#endif
            viewFrustum.setOffset(EyeDisplacement, FocalPlane);

            // draw left eye's view
            sceneRenderer->render(false);
#ifndef USE_GL_STEREO
            drawUI();
#endif

            // set up view for right eye
#ifdef USE_GL_STEREO
            glDrawBuffer(GL_BACK_RIGHT);
#else
            mainWindow->setQuadrant(MainWindow::UpperLeft);
#endif
            viewFrustum.setOffset(-EyeDisplacement, FocalPlane);

            // draw right eye's view
            sceneRenderer->render(true, true);
#ifndef USE_GL_STEREO
            drawUI();
#endif

            // draw common stuff
#ifdef USE_GL_STEREO
            glDrawBuffer(GL_BACK);
            drawUI();
#endif

#ifndef USE_GL_STEREO
            // back to left channel
            mainWindow->setQuadrant(MainWindow::UpperRight);
#endif
        }
        else if (viewType == SceneRenderer::Anaglyph)
        {
            float EyeDisplacement = 0.25f * BZDBCache::tankWidth;
            float FocalPlane = BZDB.eval(StateDatabase::BZDB_BOXBASE);
            if (BZDB.isSet("eyesep"))
                EyeDisplacement = BZDB.eval("eyesep");
            if (BZDB.isSet("focal"))
                FocalPlane = BZDB.eval("focal");

            // setup view for left eye
            glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
            viewFrustum.setOffset(EyeDisplacement, FocalPlane);

            // draw left eye's view
            sceneRenderer->render(false);
            drawUI();

            // set up view for right eye
            glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);
            // for red/blue to somewhat work ...
            //glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
            viewFrustum.setOffset(-EyeDisplacement, FocalPlane);

            // draw right eye's view
            sceneRenderer->render(true, true);
            drawUI();
        }
        else if (viewType == SceneRenderer::Interlaced)
        {
            float EyeDisplacement = 0.25f * BZDBCache::tankWidth;
            float FocalPlane = BZDB.eval(StateDatabase::BZDB_BOXBASE);
            const int width = mainWindow->getWidth();
            const int height = mainWindow->getHeight();
            if (BZDB.isSet("eyesep"))
                EyeDisplacement = BZDB.eval("eyesep");
            if (BZDB.isSet("focal"))
                FocalPlane = BZDB.eval("focal");

            if (BZDBCache::stencilShadows)
            {
                BZDB.set("stencilShadows", "0");
                addMessage(NULL, "Disabled stencilShadows for interlaced mode");
            }

            OpenGLGState::resetState();
            // enable stencil test
            glEnable(GL_STENCIL_TEST);

            // clear stencil
            glClearStencil(0);
            // Clear color and stencil buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            // All drawing commands fail the stencil test, and are not
            // drawn, but increment the value in the stencil buffer.
            glStencilFunc(GL_NEVER, 0x0, 0x0);
            glStencilOp(GL_INCR, GL_INCR, GL_INCR);
            glColor3f(1.0f, 1.0f, 1.0f);
            for (int y=0; y<=height; y+=2)
            {
                glBegin(GL_LINES);
                glVertex2i(0, y);
                glVertex2i(width, y);
                glEnd();
            }

            // draw except where the stencil pattern is 0x1
            // do not change the stencil buffer
            glStencilFunc(GL_NOTEQUAL, 0x1, 0x1);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            // setup view for left eye
            viewFrustum.setOffset(EyeDisplacement, FocalPlane);
            // draw left eye's view
            sceneRenderer->render(false);

            // draw where the stencil pattern is 0x1
            // do not change the stencil buffer
            glStencilFunc(GL_EQUAL, 0x1, 0x1);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            // set up view for right eye
            viewFrustum.setOffset(-EyeDisplacement, FocalPlane);
            // draw right eye's view
            sceneRenderer->render(true, true);

            glStencilFunc(GL_ALWAYS, 0x1, 0x1);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            drawUI();

        }
        else
        {
            // bind the multisample framebuffer, if enabled
            int maxSamples = OpenGLGState::getMaxSamples();
            bool useMultisampling = maxSamples > 1 && BZDB.evalInt("multisample") > 1;
            if(useMultisampling)
            {
                int samples = BZDB.evalInt("multisample");
                if (samples > maxSamples)
                    samples = maxSamples;
                glFramebuffer.checkState(mainWindow->getWidth(), mainWindow->getHeight(), samples);
                glBindFramebuffer(GL_FRAMEBUFFER, glFramebuffer.getFramebuffer());
            }

            // normal rendering
            sceneRenderer->render();

            // blit the multisample framebuffer (if enabled) to the main framebuffer
            if(useMultisampling)
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, glFramebuffer.getFramebuffer());
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glBlitFramebuffer(0, 0, mainWindow->getWidth(), mainWindow->getHeight(),
                                  0, 0, mainWindow->getWidth(), mainWindow->getHeight(),
                                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            // draw other stuff
            drawUI();
        }


        // get frame end time
        if (showDrawTime)
        {
#if defined(DEBUG_RENDERING)
            // get an accurate measure of frame time (at expense of frame rate)
            if (BZDB.isTrue("glFinish"))
                glFinish();
#endif
            hud->setDrawTime((float)media->stopwatch(false));
        }

        // draw a fake cursor if requested.  this is mostly intended for
        // pass through 3D cards that don't have cursor support.
        if (BZDB.isTrue("fakecursor"))
        {
            int mx, my;
            const int width = mainWindow->getWidth();
            const int height = mainWindow->getHeight();
            const int ox = mainWindow->getOriginX();
            const int oy = mainWindow->getOriginY();
            mainWindow->getWindow()->getMouse(mx, my);
            my = height - my - 1;

            glMatrixMode(GL_PROJECTION);
            mainWindow->setProjectionPlay();
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            glColor3f(0.0f, 0.0f, 0.0f);
            glRecti(mx - 8, my - 2, mx - 2, my + 2);
            glRecti(mx + 2, my - 2, mx + 8, my + 2);
            glRecti(mx - 2, my - 8, mx + 2, my - 2);
            glRecti(mx - 2, my + 2, mx + 2, my + 8);

            glColor3f(1.0f, 1.0f, 1.0f);
            glRecti(mx - 7, my - 1, mx - 3, my + 1);
            glRecti(mx + 3, my - 1, mx + 7, my + 1);
            glRecti(mx - 1, my - 7, mx + 1, my - 3);
            glRecti(mx - 1, my + 3, mx + 1, my + 7);

            glPopMatrix();
        }

        mainWindow->getWindow()->swapBuffers();

        // remove dynamic nodes from this frame
        if (scene)
            scene->removeDynamicNodes();

    }
    else
    {
        // wait around a little to avoid spinning the CPU when iconified
        TimeKeeper::sleep(0.05f);
    }
}


//============================================================================//

static void roamSmoothFollow(Roaming::RoamingCamera& deltaCamera)
{
    Player* p = ROAM.getTargetTank();
    if (!p)
        return;

    const float dist   = BZDB.eval("followDist");
    const float height = BZDB.eval("followHeight");
    const float speedX = BZDB.eval("followSpeedX");
    const float speedY = BZDB.eval("followSpeedY");
    const float speedZ = BZDB.eval("followSpeedZ");

    const float* pos = p->getPosition();
    const float* fwd = p->getForward();
    const float target[3] =
    {
        pos[0] - (fwd[0] * dist),
        pos[1] - (fwd[1] * dist),
        pos[2] + height
    };
    const float* current = ROAM.getCamera()->pos;
    const float delta[3] =
    {
        target[0] - current[0],
        target[1] - current[1],
        target[2] - current[2]
    };

    const float theta = ROAM.getCamera()->theta;
    const float c = cosf(theta * (float)(M_PI / 180.0f));
    const float s = sinf(theta * (float)(M_PI / 180.0f));
    const float f[2] = { +c, +s };
    const float r[2] = { +s, -c };

    deltaCamera.pos[0] = +speedX * ((delta[0] * f[0]) + (delta[1] * f[1]));
    deltaCamera.pos[1] = -speedY * ((delta[0] * r[0]) + (delta[1] * r[1]));
    deltaCamera.pos[2] = +speedZ * delta[2];
    deltaCamera.theta = 0.0f;
    deltaCamera.phi = 0.0f;
}


enum MouseButtonBits
{
    leftMouseBit   = (1 << 0),
    rightMouseBit  = (1 << 1),
    middleMouseBit = (1 << 2)
};

enum MouseCtrlType
{
    NoCtrl,
    ShiftX, // left/right
    ShiftY, // backwards/forewards
    ShiftZ, // up/down
    SpinX,  // tilt (phi)
    SpinY,  // -- not used --
    SpinZ   // heading (theta)
};

struct MouseCtrlPair
{
    MouseCtrlType x;
    MouseCtrlType y;
};

static const MouseCtrlPair mouseCtrlMap[8] =
{
//  X       Y
    { NoCtrl, NoCtrl }, // . . .
    { SpinZ,  ShiftY }, // L . .
    { SpinZ,  SpinX  }, // . R .
    { ShiftX, ShiftY }, // L R .
    { ShiftX, ShiftZ }, // . . M
    { SpinZ,  ShiftZ }, // L . M
    { SpinZ,  ShiftZ }, // . R M
    { ShiftX, ShiftY }  // L R M
};


static void setupRoamingCamera(float dt)
{

    static Roaming::RoamingCamera prevDeltaCamera;
    static bool inited = false;
    static int prevMouseBits = 0;
    int currMouseBits = (leftMouseButton   ? leftMouseBit   : 0) |
                        (rightMouseButton  ? rightMouseBit  : 0) |
                        (middleMouseButton ? middleMouseBit : 0);

    if (!inited)
    {
        memset(&prevDeltaCamera, 0, sizeof(Roaming::RoamingCamera));
        inited = true;
    }

    Roaming::RoamingCamera deltaCamera;
    memset(&deltaCamera, 0, sizeof(Roaming::RoamingCamera));

    // move roaming camera
    if (myTank)
    {
        int mx, my;
        mainWindow->getMousePosition(mx, my);

        const MouseCtrlPair currCtrl = mouseCtrlMap[currMouseBits];
        const MouseCtrlPair prevCtrl = mouseCtrlMap[prevMouseBits];

        if (currCtrl.x == prevCtrl.x)
        {
            if (currCtrl.y != prevCtrl.y)
            {
                mainWindow->warpMouseCenterY();
                my = 0;
            }
        }
        else if (currCtrl.y == prevCtrl.y)
        {
            mainWindow->warpMouseCenterX();
            mx = 0;
        }
        else
        {
            mainWindow->warpMouse();
            mx = my = 0;
        }

        if (currMouseBits != 0)
        {
            // mouse control
            const float spinMult  =  -100.0f;
            const float shiftMult = -1.25f * BZDBCache::worldSize;
            const int wx = mainWindow->getWidth();
            const int wy = mainWindow->getViewHeight();
            const int ws = (wx < wy) ? wx : wy;
            const float wf = 1.0f / (float(ws * ws) * 0.25f);
            const float sx = float(mx * abs(mx)) * wf;
            const float sy = float(my * abs(my)) * wf;
            switch (currCtrl.x)
            {
            case SpinZ:
            {
                deltaCamera.theta  = spinMult  * sx;
                break;
            }
            case ShiftX:
            {
                deltaCamera.pos[1] = shiftMult * sx;
                break;
            }
            default:
            {
                break;
            }
            }
            switch (currCtrl.y)
            {
            case SpinX:
            {
                deltaCamera.phi    =  spinMult * sy;
                break;
            }
            case ShiftY:
            {
                deltaCamera.pos[0] = shiftMult * sy;
                break;
            }
            case ShiftZ:
            {
                deltaCamera.pos[2] = shiftMult * sy;
                break;
            }
            default:
            {
                break;
            }
            }
        }
        else
        {
            // keyboard control
            bool control = ((shiftKeyStatus & BzfKeyEvent::ControlKey) != 0);
            bool alt     = ((shiftKeyStatus & BzfKeyEvent::AltKey) != 0);
            bool shift   = ((shiftKeyStatus & BzfKeyEvent::ShiftKey) != 0);
            if (display->hasGetKeyMode())
                display->getModState (shift, control, alt);
            if (!control && !shift)
                deltaCamera.pos[0] = (float)(4 * myTank->getSpeed()) * BZDBCache::tankSpeed;
            if (alt)
                deltaCamera.pos[1] = (float)(4 * myTank->getRotation()) * BZDBCache::tankSpeed;
            else
                deltaCamera.theta  = ROAM.getZoom() * (float)myTank->getRotation();
            if (control)
                deltaCamera.phi    = 2.0f * ROAM.getZoom() / 3.0f * (float)myTank->getSpeed();
            if (shift)
                deltaCamera.pos[2] = (float)(4 * myTank->getSpeed()) * BZDBCache::tankSpeed;
        }
    }

    // adjust for slow keyboard
    float st = BZDB.eval("roamSmoothTime");
    if (BZDB.isTrue("slowKeyboard") != (st < 0.0f))
    {
        if (ROAM.getMode() == Roaming::roamViewFollow)
            roamSmoothFollow(deltaCamera);
        st = fabsf(st);
        if (st < 0.1f)
            st = 0.1f;
        const float at = (dt / st);
        const float bt = 1.0f - at;
        deltaCamera.pos[0] = (at * deltaCamera.pos[0]) + (bt * prevDeltaCamera.pos[0]);
        deltaCamera.pos[1] = (at * deltaCamera.pos[1]) + (bt * prevDeltaCamera.pos[1]);
        deltaCamera.pos[2] = (at * deltaCamera.pos[2]) + (bt * prevDeltaCamera.pos[2]);
        deltaCamera.theta  = (at * deltaCamera.theta)  + (bt * prevDeltaCamera.theta);
        deltaCamera.phi    = (at * deltaCamera.phi)    + (bt * prevDeltaCamera.phi);
    }

    deltaCamera.zoom = roamDZoom;

    ROAM.updatePosition(&deltaCamera, dt);

    // copy the old delta values
    memcpy(&prevDeltaCamera, &deltaCamera, sizeof(Roaming::RoamingCamera));

    prevMouseBits = currMouseBits;

    return;
}


//============================================================================//

static void     prepareTheHUD()
{
    // prep the HUD
    if (myTank)
    {
        const float* myPos = myTank->getPosition();
        hud->setHeading(myTank->getAngle());
        hud->setAltitude(myPos[2]);
        if (world->allowTeamFlags())
        {
            const float* myTeamColor = Team::getTankColor(myTank->getTeam());
            // markers for my team flag
            for (int i = 0; i < numFlags; i++)
            {
                Flag& flag = world->getFlag(i);
                if ((flag.type->flagTeam == myTank->getTeam())
                        && ((flag.status != FlagOnTank) ||
                            (flag.owner != myTank->getId())))
                {
                    const float* flagPos = flag.position;
                    float heading = atan2f(flagPos[1] - myPos[1],flagPos[0] - myPos[0]);
                    hud->addMarker(heading, myTeamColor);
                    hud->AddEnhancedMarker(Float3ToVec3(flagPos), Float3ToVec3(myTeamColor),
                                           false, BZDBCache::flagPoleSize * 2.0f);
                }
            }
        }
        if (myTank->getAntidoteLocation())
        {
            // marker for my antidote flag
            const GLfloat* antidotePos = myTank->getAntidoteLocation();
            float heading = atan2f(antidotePos[1] - myPos[1],
                                   antidotePos[0] - myPos[0]);
            const float antidoteColor[] = {1.0f, 1.0f, 0.0f};
            hud->addMarker(heading, antidoteColor);
            hud->AddEnhancedMarker(Float3ToVec3(antidotePos), Float3ToVec3(antidoteColor), false,
                                   BZDBCache::flagPoleSize * 2.0f);
        }
    }
    return;
}


static void     updatePauseCountdown(float dt)
{
    if (!myTank)
        pauseCountdown = 0.0f;
    if (pauseCountdown > 0.0f && !myTank->isAlive())
    {
        pauseCountdown = 0.0f;
        hud->setAlert(1, NULL, 0.0f, true);
    }
    if (pauseCountdown > 0.0f)
    {
        const int oldPauseCountdown = (int)(pauseCountdown + 0.99f);
        pauseCountdown -= dt;
        if (pauseCountdown <= 0.0f)
        {

            /* make sure it is really safe to pause..  since the server
             * might make us drop our flag, make sure the player is on the
             * ground and not in a building.  prevents getting kicked
             * later for being in places we shouldn't without holding the
             * right flags.
             */
            if (myTank->getLocation() == LocalPlayer::InBuilding)
            {
                // custom message when trying to pause while in a building
                // (could get stuck on un-pause if flag is taken/lost)
                hud->setAlert(1, "Can't pause while inside a building", 1.0f, false);

            }
            else if (myTank->getLocation() == LocalPlayer::InAir)
            {
                // custom message when trying to pause when jumping/falling
                hud->setAlert(1, "Can't pause when you are in the air", 1.0f, false);

            }
            else if (myTank->getLocation() != LocalPlayer::OnGround &&
                     myTank->getLocation() != LocalPlayer::OnBuilding)
            {
                // catch-all message when trying to pause when you should not
                hud->setAlert(1, "Unable to pause right now", 1.0f, false);

            }
            else if (myTank->isPhantomZoned())
            {
                // custom message when trying to pause while zoned
                hud->setAlert(1, "Can't pause when you are in the phantom zone", 1.0f, false);

            }
            else
            {
                // okay, now we pause.  first drop any team flag we may have.
                const FlagType* flagd = myTank->getFlag();
                if (flagd->flagTeam != NoTeam)
                    serverLink->sendDropFlag(myTank->getPosition());

                if (World::getWorld()->allowRabbit() && (myTank->getTeam() == RabbitTeam))
                    serverLink->sendNewRabbit();

                // now actually pause
                myTank->setPause(true);
                hud->setAlert(1, NULL, 0.0f, true);
                controlPanel->addMessage("Paused");

                // turn off the sound
                if (savedVolume == -1)
                {
                    savedVolume = getSoundVolume();
                    setSoundVolume(0);
                }

                // ungrab mouse
                mainWindow->ungrabMouse();
            }
        }
        else if ((int)(pauseCountdown + 0.99f) != oldPauseCountdown &&
                 !pausedByUnmap)
        {
            // update countdown alert
            char msgBuf[40];
            sprintf(msgBuf, "Pausing in %d", (int)(pauseCountdown + 0.99f));
            hud->setAlert(1, msgBuf, 1.0f, false);
        }
    }
    return;
}


static void     updateDestructCountdown(float dt)
{
    if (!myTank)
        destructCountdown = 0.0f;
    if (destructCountdown > 0.0f && !myTank->isAlive())
    {
        destructCountdown = 0.0f;
        hud->setAlert(1, NULL, 0.0f, true);
    }
    if (destructCountdown > 0.0f)
    {
        const int oldDestructCountdown = (int)(destructCountdown + 0.99f);
        destructCountdown -= dt;
        if (destructCountdown <= 0.0f)
        {
            // now actually destruct
            gotBlowedUp( myTank, SelfDestruct, myTank->getId() );

            hud->setAlert(1, NULL, 0.0f, true);
        }
        else if ((int)(destructCountdown + 0.99f) != oldDestructCountdown)
        {
            // update countdown alert
            char msgBuf[40];
            sprintf(msgBuf, "Self Destructing in %d", (int)(destructCountdown + 0.99f));
            hud->setAlert(1, msgBuf, 1.0f, false);
        }
    }
    return;
}


//
// main playing loop
//

static void     playingLoop()
{
    int i;

    // main loop
    while (!CommandsStandard::isQuit())
    {

        BZDBCache::update();

        // set this step game time
        GameTime::setStepTime();

        // get delta time
        TimeKeeper prevTime = TimeKeeper::getTick();
        TimeKeeper::setTick();
        const float dt = float(TimeKeeper::getTick() - prevTime);

        mainWindow->getWindow()->yieldCurrent();

        // see if the world collision grid needs to be updated
        if (world)
            world->checkCollisionManager();

        mainWindow->getWindow()->yieldCurrent();

        // try to join a game if requested.  do this *before* handling
        // events so we do a redraw after the request is posted and
        // before we actually try to join.
        if (joinRequested)
        {
            // if already connected to a game then first sign off
            if (myTank) leaveGame();

            // Erase any existing token
            startupInfo.token[0] = '\0';

            // get token if we have a password
            if (startupInfo.password[0] != '\0')
            {
                ServerAuth* serverAuth = new ServerAuth;
                serverAuth->requestToken(&startupInfo);
                // wait no more than 10 seconds for a token
                for (int j = 0; j < 40; j++)
                {
                    cURLManager::perform();
                    if (startupInfo.token[0] != '\0') break;
                    TimeKeeper::sleep(0.25f);
                }
                delete serverAuth;

                // don't let the bad token specifier slip through to the server,
                // just erase it
                if (strcmp(startupInfo.token, "badtoken") == 0)
                    startupInfo.token[0] = '\0';
            }

            ares->queryHost(startupInfo.serverName);
            waitingDNS = true;

            // don't try again
            joinRequested = false;
        }

        if (waitingDNS)
        {
            fd_set readers, writers;
            int nfds = -1;
            struct timeval timeout;
            timeout.tv_sec  = 0;
            timeout.tv_usec = 0;
            FD_ZERO(&readers);
            FD_ZERO(&writers);
            ares->setFd(&readers, &writers, nfds);
            nfds = select(nfds + 1, (fd_set*)&readers, (fd_set*)&writers, 0,
                          &timeout);
            ares->process(&readers, &writers);

            struct in_addr inAddress;
            AresHandler::ResolutionStatus status = ares->getHostAddress(&inAddress);
            if (status == AresHandler::Failed)
            {
                HUDDialogStack::get()->setFailedMessage("Server not found");
                waitingDNS = false;
            }
            else if (status == AresHandler::HbNSucceeded)
            {
                // now try connecting
                serverNetworkAddress = Address(inAddress);
                joinInternetGame();
                waitingDNS = false;
            }
        }
        mainWindow->getWindow()->yieldCurrent();

        // handle pending events for some small fraction of time
        clockAdjust = 0.0f;
        processInputEvents(0.1f);

        if (mainWindow->haveJoystick())
        {
            static const BzfKeyEvent::Button button_map[] =
            {
                BzfKeyEvent::BZ_Button_1,
                BzfKeyEvent::BZ_Button_2,
                BzfKeyEvent::BZ_Button_3,
                BzfKeyEvent::BZ_Button_4,
                BzfKeyEvent::BZ_Button_5,
                BzfKeyEvent::BZ_Button_6,
                BzfKeyEvent::BZ_Button_7,
                BzfKeyEvent::BZ_Button_8,
                BzfKeyEvent::BZ_Button_9,
                BzfKeyEvent::BZ_Button_10,
                BzfKeyEvent::BZ_Button_11,
                BzfKeyEvent::BZ_Button_12,
                BzfKeyEvent::BZ_Button_13,
                BzfKeyEvent::BZ_Button_14,
                BzfKeyEvent::BZ_Button_15,
                BzfKeyEvent::BZ_Button_16,
                BzfKeyEvent::BZ_Button_17,
                BzfKeyEvent::BZ_Button_18,
                BzfKeyEvent::BZ_Button_19,
                BzfKeyEvent::BZ_Button_20,
                BzfKeyEvent::BZ_Button_21,
                BzfKeyEvent::BZ_Button_22,
                BzfKeyEvent::BZ_Button_23,
                BzfKeyEvent::BZ_Button_24,
                BzfKeyEvent::BZ_Button_25,
                BzfKeyEvent::BZ_Button_26,
                BzfKeyEvent::BZ_Button_27,
                BzfKeyEvent::BZ_Button_28,
                BzfKeyEvent::BZ_Button_29,
                BzfKeyEvent::BZ_Button_30,
                BzfKeyEvent::BZ_Button_31,
                BzfKeyEvent::BZ_Button_32,
            };

            static unsigned long old_buttons = 0;
            const int button_count = bzcountof(button_map);
            unsigned long new_buttons = mainWindow->getJoyButtonSet();
            if (old_buttons != new_buttons)
                for (int j = 0; j < button_count; j++)
                {
                    if ((old_buttons & (1<<j)) != (new_buttons & (1<<j)))
                    {
                        BzfKeyEvent ev;
                        ev.button = button_map[j];
                        ev.ascii = 0;
                        ev.shift = 0;
                        doKey(ev, (new_buttons & (1<<j)) != 0);
                    }
                }
            old_buttons = new_buttons;

            static const BzfKeyEvent::Button hat_map[] =
            {
                BzfKeyEvent::BZ_Hatswitch_1_upleft,
                BzfKeyEvent::BZ_Hatswitch_1_up,
                BzfKeyEvent::BZ_Hatswitch_1_upright,
                BzfKeyEvent::BZ_Hatswitch_1_right,
                BzfKeyEvent::BZ_Hatswitch_1_downright,
                BzfKeyEvent::BZ_Hatswitch_1_down,
                BzfKeyEvent::BZ_Hatswitch_1_downleft,
                BzfKeyEvent::BZ_Hatswitch_1_left,
                BzfKeyEvent::BZ_Hatswitch_2_upleft,
                BzfKeyEvent::BZ_Hatswitch_2_up,
                BzfKeyEvent::BZ_Hatswitch_2_upright,
                BzfKeyEvent::BZ_Hatswitch_2_right,
                BzfKeyEvent::BZ_Hatswitch_2_downright,
                BzfKeyEvent::BZ_Hatswitch_2_down,
                BzfKeyEvent::BZ_Hatswitch_2_downleft,
                BzfKeyEvent::BZ_Hatswitch_2_left,
                BzfKeyEvent::BZ_Hatswitch_3_upleft,
                BzfKeyEvent::BZ_Hatswitch_3_up,
                BzfKeyEvent::BZ_Hatswitch_3_upright,
                BzfKeyEvent::BZ_Hatswitch_3_right,
                BzfKeyEvent::BZ_Hatswitch_3_downright,
                BzfKeyEvent::BZ_Hatswitch_3_down,
                BzfKeyEvent::BZ_Hatswitch_3_downleft,
                BzfKeyEvent::BZ_Hatswitch_3_left,
                BzfKeyEvent::BZ_Hatswitch_4_upleft,
                BzfKeyEvent::BZ_Hatswitch_4_up,
                BzfKeyEvent::BZ_Hatswitch_4_upright,
                BzfKeyEvent::BZ_Hatswitch_4_right,
                BzfKeyEvent::BZ_Hatswitch_4_downright,
                BzfKeyEvent::BZ_Hatswitch_4_down,
                BzfKeyEvent::BZ_Hatswitch_4_downleft,
                BzfKeyEvent::BZ_Hatswitch_4_left,
            };

            //  Evdev //   SDL  //      DX     //     atan2    // buttons //
            //--------//--------//-------------//--------------//---------//
            //   -1   //  9 1 3 // 315   0  45 // -135 -90 -45 // 0  1  2 //
            // -1 0 1 //  8 0 2 // 270   ?  90 //  180   0   0 // 7 -1  3 //
            //    1   // 12 4 6 // 225 180 135 //  135  90  45 // 6  5  4 //

            const int max_hats = 4;
            const int num_buttons = bzcountof(hat_map) / max_hats; // 8
            int num_hats = mainWindow->getNumHats();
            if (num_hats > max_hats) num_hats = max_hats; // num_hats min= max_hats;
            static std::vector<int> hats(max_hats, -1);
            const float variance = 360 / num_buttons / 2; // 45/2 or 22.5
            BzfKeyEvent ev; // must be out here because of false doKey
            ev.ascii = 0;
            ev.shift = 0;
            for (int hat = 0; hat < num_hats; hat++)
            {
                float hatX, hatY;
                mainWindow->getJoyHat(hat, hatX, hatY);
                if (hatX == 0 && hatY == 0)
                {
                    if (hats[hat] != -1)
                    {
                        doKey(ev, false); // unset when centered
                        hats[hat] = -1;
                    }
                }
                else
                {
                    int button = -1; // buttons are counted clockwise to left
                    float angle = atan2(hatY, hatX) * 180 / (float)M_PI;
                    for (int b = -1; b < num_buttons; b++)
                    {
                        float testangle = -180 + 2 * variance * (b + 1); // -180 to 180 by 45
                        if (testangle - variance <= angle && angle < testangle + variance)
                        {
                            button = b; // 0 to 7
                            if (b == -1) button = num_buttons - 1; // 7
                            if (button != hats[hat])
                            {
                                if (hats[hat] != -1)
                                    doKey(ev, false); // unset when spinning
                                ev.button = hat_map[button + hat * num_buttons];
                                doKey(ev, true);
                                hats[hat] = button;
                            }
                        }
                    }
                }
            }
        }

        mainWindow->getWindow()->yieldCurrent();

        // invoke callbacks
        callPlayingCallbacks();

        mainWindow->getWindow()->yieldCurrent();

        // quick out
        if (CommandsStandard::isQuit())
            break;

        // if server died then leave the game (note that this may cause
        // further server errors but that's okay).
        if (serverError ||
                (serverLink && serverLink->getState() == ServerLink::Hungup))
        {
            // if we haven't reported the death yet then do so now
            if (serverDied ||
                    (serverLink && serverLink->getState() == ServerLink::Hungup))
                printError("Server has unexpectedly disconnected");
            leaveGame();
        }

        // update time of day -- update sun and sky every few seconds
        float syncTime = BZDB.eval(StateDatabase::BZDB_SYNCTIME);
        if (syncTime < 0.0f)
        {
            if (!BZDB.isSet("fixedTime"))
                epochOffset += (double)dt;
            epochOffset += (double)(50.0f * dt * clockAdjust);
        }
        else
        {
            epochOffset = (double)syncTime;
            lastEpochOffset += (double)dt;
        }
        if (fabs(epochOffset - lastEpochOffset) >= 4.0)
        {
            updateDaylight(epochOffset, *sceneRenderer);
            lastEpochOffset = epochOffset;
        }

        // update the wind
        if (world)
            world->updateWind(dt);

        // move roaming camera
        if (ROAM.isRoaming())
        {
            setupRoamingCamera(dt);
            ROAM.buildRoamingLabel();
        }

        // update test video format timer
        if (testVideoFormatTimer > 0.0f)
        {
            testVideoFormatTimer -= dt;
            if (testVideoFormatTimer <= 0.0f)
            {
                testVideoFormatTimer = 0.0f;
                setVideoFormat(testVideoPrevFormat);
            }
        }

        // update the countdowns
        updatePauseCountdown(dt);
        updateDestructCountdown(dt);

        // notify if input changed
        if ((myTank != NULL) && (myTank->queryInputChange() == true))
        {
            controlPanel->addMessage(
                LocalPlayer::getInputMethodName(myTank->getInputMethod()) + " movement");
        }

        // update other tank's shots
        for (i = 0; i < curMaxPlayers; i++)
        {
            if (remotePlayers[i])
                remotePlayers[i]->updateShots(dt);
        }

        // update servers shots
        const World *_world = World::getWorld();
        if (_world)
            _world->getWorldWeapons()->updateShots(dt);

        // update track marks  (before any tanks are moved)
        TrackMarks::update(dt);

        // do dead reckoning on remote players
        for (i = 0; i < curMaxPlayers; i++)
        {
            if (remotePlayers[i])
            {
                const bool wasNotResponding = remotePlayers[i]->isNotResponding();
                remotePlayers[i]->doDeadReckoning();
                const bool isNotResponding = remotePlayers[i]->isNotResponding();
                if (!wasNotResponding && isNotResponding)
                    addMessage(remotePlayers[i], "not responding");
                else if (wasNotResponding && !isNotResponding)
                    addMessage(remotePlayers[i], "okay");
            }
        }

        // do motion
        if (myTank && _world)
        {
            if (myTank->isAlive() && !myTank->isPaused())
            {
                doMotion();
                if (scoreboard->getHuntState()==ScoreboardRenderer::HUNT_ENABLED)
                {
                    setHuntTarget(); //spot hunt target
                }
                if (myTank->getTeam() != ObserverTeam &&
                        ((fireButton && myTank->getFlag() == Flags::MachineGun) ||
                         (myTank->getFlag() == Flags::TriggerHappy)))
                    myTank->fireShot();

                setLookAtMarker();

                // see if we have a target, if so lock on to the bastage
                const Player* targetdPlayer = myTank->getTarget();
                if (targetdPlayer && targetdPlayer->isAlive() && targetdPlayer->getFlag() != Flags::Stealth)
                {
                    hud->AddLockOnMarker(Float3ToVec3(myTank->getTarget()->getPosition()),
                                         myTank->getTarget()->getCallSign(),
                                         !isKillable(myTank->getTarget()));
                }
                else // if we should not have a target, force that target to be cleared
                    myTank->setTarget(NULL);

            }
            else
            {
                int mx, my;
                mainWindow->getMousePosition(mx, my);
            }
            myTank->update();
        }

#ifdef ROBOT
        if (entered)
            updateRobots(dt);
#endif

        // check for flags and hits
        checkEnvironment();

#ifdef ROBOT
        if (entered)
            checkEnvironmentForRobots();
#endif

        // adjust properties based on flags (dimensions, cloaking, etc...)
        if (myTank && _world)
            myTank->updateTank(dt, true);
        for (i = 0; i < curMaxPlayers; i++)
        {
            if (remotePlayers[i])
                remotePlayers[i]->updateTank(dt, false);
        }

        // reposition flags
        updateFlags(dt);

        // update explosion animations
        updateExplosions(dt);

        // update mesh animations
        if (world)
            world->updateAnimations(dt);

        // prep the HUD
        prepareTheHUD();

        // draw the frame
        drawFrame(dt);

        // play the sounds
        updateSound();


        bool sendUpdate = myTank && myTank->isDeadReckoningWrong();
        if (myTank && myTank->getTeam() == ObserverTeam)
        {
            if (BZDB.isTrue("sendObserverHeartbeat"))
            {
                double heartbeatTime = BZDB.isSet("observerHeartbeat")
                                       ? BZDB.eval("observerHeartbeat") : 30.0f;
                if (lastObserverUpdateTime + heartbeatTime < TimeKeeper::getTick().getSeconds())
                {
                    lastObserverUpdateTime = TimeKeeper::getTick().getSeconds();
                    sendUpdate = true;
                }
                else
                    sendUpdate = false;
            }
            else
                sendUpdate = false;
        }
        // send my data
        if ( sendUpdate)
        {
            // also calls setDeadReckoning()
            serverLink->sendPlayerUpdate(myTank);
        }

#ifdef ROBOT
        if (entered)
            sendRobotUpdates();
#endif

        FlagSceneNode::freeFlag();

        cURLManager::perform();

        // check if we are waiting for initial texture downloading
        if (Downloads::requestFinalized())
        {
            // downloading is terminated. go!
            Downloads::finalizeDownloads();
            if (downloadingInitialTexture)
            {
                joinInternetGame2();
                downloadingInitialTexture = false;
            }
            else
                setSceneDatabase();
        }

        // limit the fps to save battery life by minimizing cpu usage
        if (BZDB.evalInt("saveEnergy") == 1)
        {
            static TimeKeeper lastTime = TimeKeeper::getCurrent();
            float fpsLimit = BZDB.eval("fpsLimit");
            if (fpsLimit < 15 || isnan(fpsLimit))
                fpsLimit = 15;
            TimeKeeper nextTime(lastTime);
            nextTime += 1.0f / fpsLimit;
            float remaining;
            while (1)
            {
                remaining = (float)(nextTime - TimeKeeper::getCurrent());
                if (remaining > 1.0f)
                    break;
                if (remaining <= 0.0f)
                    break;
                // Instead of sleeping try to handle network packets
                char msg[MaxPacketLen];
                uint16_t code, len;

                // handle server messages
                if (serverLink && !serverError)
                {
                    int e = 0;
                    e = serverLink->read(code, len, msg, int(remaining * 1000.0f));
                    if (e == 1)
                        handleServerMessage(true, code, len, msg);
                    if (e == -2)
                    {
                        printError("Server communication error");
                        serverError = true;
                        break;
                    }
                }
                else
                {
                    TimeKeeper::sleep(remaining);
                    break;
                }
            }
            lastTime = TimeKeeper::getCurrent();
        } // end energy saver check

        // handle incoming packets
        doMessages();

    } // end main client loop

    dropLastExplosions();
}


//
// game initialization
//

static void     defaultErrorCallback(const char* msg)
{
    std::string message = ColorStrings[RedColor];
    message += msg;
    controlPanel->addMessage(message);
}

static void     startupErrorCallback(const char* msg)
{
    controlPanel->addMessage(msg);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    controlPanel->render(*sceneRenderer);
    mainWindow->getWindow()->swapBuffers();
}


void            startPlaying(BzfDisplay* _display,
                             SceneRenderer& renderer)
{
    // initialization
    display = _display;
    sceneRenderer = &renderer;
    mainWindow = &sceneRenderer->getWindow();

    lastObserverUpdateTime = TimeKeeper::getTick().getSeconds();

    // register some commands
    for (unsigned int c = 0; c < bzcountof(commandList); ++c)
        CMDMGR.add(commandList[c].name, commandList[c].func, commandList[c].help);

    // initialize the tank display lists
    // (do this before calling SceneRenderer::render())
    TankGeometryMgr::init();
    SphereLodSceneNode::init();

    // make control panel
    ControlPanel _controlPanel(*mainWindow, *sceneRenderer);
    controlPanel = &_controlPanel;

    // make the radar
    RadarRenderer _radar(*sceneRenderer, world);
    radar = &_radar;

    // tie the radar to the control panel
    controlPanel->setRadarRenderer(radar);

    // tell the control panel how many frame buffers there are.  we
    // cheat when drawing the control panel, not drawing it if it
    // hasn't changed.  that only works if we've filled all the
    // frame buffers (e.g. front and back buffers) with the correct
    // data.
    // FIXME -- assuming the contents of any frame buffer except the
    // front buffer are anything but garbage violates the OpenGL
    // spec.  we really should redraw the control panel every frame
    // but this works on every system so far.
    {
        int n = 3;  // assume triple buffering
        switch (sceneRenderer->getViewType())
        {
        case SceneRenderer::Stacked:
        case SceneRenderer::Stereo:
#ifndef USE_GL_STEREO
            // control panel drawn twice per frame
            n *= 2;
#endif
            break;

        case SceneRenderer::ThreeChannel:
        default:
            // only one copy of control panel visible
            break;
        }
        controlPanel->setNumberOfFrameBuffers(n);
    }

    // if no configuration go into a decent setup for a modern machine
    if (!startupInfo.hasConfiguration)
    {
        BZDB.set("texture", "1");
        sceneRenderer->setQuality(3);
        TextureManager::instance().setMaxFilter(OpenGLTexture::Max);
    }

// should we grab the mouse?
#if defined(DEBUG)      // don't grab for debug builds
    setGrabMouse(false);
#elif defined(__linux__)      // linux usually has a virtual root window so grab mouse always
    setGrabMouse(true);
#else
    if (!BZDB.isSet("_window")) // otherwise, grab if fullscreen.
        setGrabMouse(true);
#endif

    // show window and clear it immediately
    mainWindow->showWindow(true);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glDisable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    mainWindow->getWindow()->swapBuffers();

    // resize and draw basic stuff
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    controlPanel->resize();
    sceneRenderer->render();
    controlPanel->render(*sceneRenderer);
    mainWindow->getWindow()->swapBuffers();

    // startup error callback adds message to control panel and
    // forces an immediate redraw.
    setErrorCallback(startupErrorCallback);

    // initialize epoch offset (time)
    userTimeEpochOffset = (double)mktime(&userTime);
    epochOffset = userTimeEpochOffset;
    updateDaylight(epochOffset, *sceneRenderer);
    lastEpochOffset = epochOffset;

    // catch kill signals before changing video mode so we can
    // put it back even if we die.  ignore a few signals.
    bzSignal(SIGILL, SIG_PF(dying));
    bzSignal(SIGABRT, SIG_PF(dying));
    bzSignal(SIGSEGV, SIG_PF(dying));
    bzSignal(SIGTERM, SIG_PF(suicide));
#if !defined(_WIN32)
    if (bzSignal(SIGINT, SIG_IGN) != SIG_IGN)
        bzSignal(SIGINT, SIG_PF(suicide));
    bzSignal(SIGPIPE, SIG_PF(hangup));
    bzSignal(SIGHUP, SIG_IGN);
    if (bzSignal(SIGQUIT, SIG_IGN) != SIG_IGN)
        bzSignal(SIGQUIT, SIG_PF(dying));
#ifndef GUSI_20
    bzSignal(SIGBUS, SIG_PF(dying));
#endif
    bzSignal(SIGUSR1, SIG_IGN);
    bzSignal(SIGUSR2, SIG_IGN);
#endif /* !defined(_WIN32) */

    std::string videoFormat;
    int format = -1;
    if (BZDB.isSet("resolution"))
    {
        videoFormat = BZDB.get("resolution");
        if (videoFormat.length() != 0)
            format = display->findResolution(videoFormat.c_str());
    };
    // set the resolution (only if in full screen mode)
    if (!BZDB.isSet("_window") && BZDB.isSet("resolution"))
    {
        if (videoFormat.length() != 0)
        {
            if (display->isValidResolution(format) &&
                    display->getResolution() != format &&
                    display->setResolution(format))
            {

                // handle resize
                if (BZDB.isSet("geometry"))
                {
                    int w, h, x, y, count;
                    char xs, ys;
                    count = sscanf(BZDB.get("geometry").c_str(),
                                   "%dx%d%c%d%c%d", &w, &h, &xs, &x, &ys, &y);
                    if (w < 256) w = 256;
                    if (h < 192) h = 192;
                    if (count == 6)
                    {
                        if (xs == '-') x = display->getWidth() - x - w;
                        if (ys == '-') y = display->getHeight() - y - h;
                        mainWindow->setPosition(x, y);
                    }
                    mainWindow->setSize(w, h);
                }
                else
                    mainWindow->setFullscreen();

                // more resize handling
                mainWindow->getWindow()->callResizeCallbacks();
                mainWindow->warpMouse();
            }
        }
    }
    // otherwise, use the default resolution if we do switch to fullscreen
    else
        display->setDefaultResolution();

    // grab mouse if we should
    if (shouldGrabMouse())
        mainWindow->grabMouse();

    // draw again
    glClear(GL_COLOR_BUFFER_BIT);
    sceneRenderer->render();
    controlPanel->render(*sceneRenderer);
    mainWindow->getWindow()->swapBuffers();
    mainWindow->getWindow()->yieldCurrent();

    // make heads up display
    HUDRenderer _hud(display, renderer);
    hud = &_hud;
    scoreboard = hud->getScoreboard();

    // initialize control panel and hud
    updateFlag(Flags::Null);
    updateHighScores();
    notifyBzfKeyMapChanged();

    // make background renderer
    BackgroundRenderer background;
    sceneRenderer->setBackground(&background);

    static const GLfloat  zero[3] = { 0.0f, 0.0f, 0.0f };

    TextureManager &tm = TextureManager::instance();

    bool done = false;
    int explostion = 1;
    while (!done)
    {
        char text[256];
        sprintf(text, "explode%d", explostion);

        int tex = tm.getTextureID(text, false);

        if (tex < 0)
            done = true;
        else
        {
            // make explosion scene node
            BillboardSceneNode* explosion = new BillboardSceneNode(zero);
            explosion->setTexture(tex);
            explosion->setTextureAnimation(8, 8);

            // add it to list of prototype explosions
            prototypeExplosions.push_back(explosion);
            explostion++;
        }
    }

    // let other stuff do initialization
    sceneBuilder = new SceneDatabaseBuilder(sceneRenderer);
    World::init();

    // prepare dialogs
    mainMenu = new MainMenu;

    // normal error callback (doesn't force a redraw)
    setErrorCallback(defaultErrorCallback);

    // print debugging info
    {
        // Application version
        logDebugMessage(1,"BZFlag version:   %s\n", getAppVersion());

        // Protocol version
        logDebugMessage(1,"BZFlag protocol:  %s\n", getProtocolVersion());

        // OpenGL Driver Information
        logDebugMessage(1,"OpenGL vendor:    %s\n", (const char*)glGetString(GL_VENDOR));
        logDebugMessage(1,"OpenGL version:   %s\n", (const char*)glGetString(GL_VERSION));
        logDebugMessage(1,"OpenGL renderer:  %s\n", (const char*)glGetString(GL_RENDERER));

        // Depth Buffer bitplanes
        GLint zDepth;
        glGetIntegerv(GL_DEPTH_BITS, &zDepth);
        logDebugMessage(1,"Depth Buffer:     %i bitplanes\n", zDepth);
    }

    // windows version can be very helpful in debug logs
#ifdef _WIN32
#pragma warning(disable: 4996)
    if (debugLevel >= 1)
    {
        OSVERSIONINFO info;
        ZeroMemory(&info, sizeof(OSVERSIONINFO));
        info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&info);
        logDebugMessage(1,"Running on Windows %s%d.%d %s\n",
                        (info.dwPlatformId == VER_PLATFORM_WIN32_NT) ? "NT " : "",
                        info.dwMajorVersion, info.dwMinorVersion,
                        info.szCSDVersion);
    }
#endif

    // send informative header to the console
    {
        std::string tmpString;

        controlPanel->addMessage("");
        // print app version
        tmpString = ColorStrings[RedColor];
        tmpString += "BZFlag version: ";
        tmpString += getAppVersion();
        tmpString += " (";
        tmpString += getProtocolVersion();
        tmpString += ")";
        controlPanel->addMessage(tmpString);
        // print copyright
        tmpString = ColorStrings[YellowColor];
        tmpString += bzfcopyright;
        controlPanel->addMessage(tmpString);
        // print license
        tmpString = ColorStrings[CyanColor];
        tmpString += "Distributed under the terms of the LGPL or MPL";
        controlPanel->addMessage(tmpString);
        // print author
        tmpString = ColorStrings[GreenColor];
        tmpString += "Author: Chris Schoeneman <crs23@bigfoot.com>";
        controlPanel->addMessage(tmpString);
        // print maintainer
        tmpString = ColorStrings[CyanColor];
        tmpString += "Maintainer: Tim Riker <Tim@Rikers.org>";
        controlPanel->addMessage(tmpString);
        // print audio driver
        std::string audioStr;
        PlatformFactory::getMedia()->audioDriver(audioStr);
        if (tmpString != "")
        {
            tmpString = ColorStrings[BlueColor];
            tmpString += "Audio Driver: " + audioStr;
            controlPanel->addMessage(tmpString);
        }
        // print GL renderer
        tmpString = ColorStrings[PurpleColor];
        tmpString += "OpenGL Driver: ";
        tmpString += (const char*)glGetString(GL_RENDERER);
        controlPanel->addMessage(tmpString);
    }

    // get current MOTD
    if (!BZDB.isTrue("disableMOTD"))
    {
        motd = new MessageOfTheDay;
        motd->getURL(BZDB.get("motdServer"));
    }

    // inform user of silencePlayers on startup
    for (unsigned int j = 0; j < silencePlayers.size(); j ++)
    {
        std::string aString = silencePlayers[j];
        aString += " Silenced";
        if (silencePlayers[j] == "*")
            aString = "Silenced All Msgs";
        else if (silencePlayers[j] == "-")
            aString = "Silenced Unregistered Players";
        controlPanel->addMessage(aString);
    }

    // enter game if we have all the info we need, otherwise
    // pop up main menu
    if (startupInfo.autoConnect &&
            startupInfo.callsign[0] && startupInfo.serverName[0])
    {
        joinRequested    = true;
        // show join menu to see connection errors
        mainMenu->createControls();
        HUDDialogStack::get()->push(mainMenu);
        mainMenu->execute();
        HUDui::setFocus(HUDui::getFocus()->getNext()); // select "Connect"
        HUDDialogStack::get()->top()->execute(); // show "Trying Automatic Connection..."

    }
    else
    {
        mainMenu->createControls();
        HUDDialogStack::get()->push(mainMenu);
    }

    if (BZDB.isTrue("fakecursor"))
        mainWindow->getWindow()->hideMouse();

    // start timing
    TimeKeeper::setTick();
    updateDaylight(epochOffset, *sceneRenderer);

    worldDownLoader = new WorldDownLoader;

    // start game loop
    playingLoop();

    delete worldDownLoader;

    // restore the sound.  if we don't do this then we'll save the
    // wrong volume when we dump out the configuration file if the
    // app exits when the game is paused.
    if (savedVolume != -1)
    {
        setSoundVolume(savedVolume);
        savedVolume = -1;
    }

    // hide window
    mainWindow->showWindow(false);

    // clean up
    TankGeometryMgr::kill();
    SphereLodSceneNode::kill();
    if (resourceDownloader)
        delete resourceDownloader;
    delete motd;
    for (unsigned int ext = 0; ext < prototypeExplosions.size(); ext++)
        delete prototypeExplosions[ext];
    prototypeExplosions.clear();
    leaveGame();
    setErrorCallback(NULL);
    while (HUDDialogStack::get()->isActive())
        HUDDialogStack::get()->pop();
    delete mainMenu;
    delete sceneBuilder;
    sceneRenderer->setBackground(NULL);
    sceneRenderer->setSceneDatabase(NULL);
    World::done();
    mainWindow = NULL;
    sceneRenderer = NULL;
    display = NULL;
    cleanWorldCache();
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
