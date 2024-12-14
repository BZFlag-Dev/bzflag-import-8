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
#include "bzfs.h"

// implementation-specific system headers
#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <vector>
#include <string>
#include <time.h>
#ifdef HAVE_MINIUPNPC_MINIUPNPC_H
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#endif
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

// implementation-specific bzflag headers
#include "NetHandler.h"
#include "version.h"
#include "md5.h"
#include "BZDBCache.h"
#include "ShotUpdate.h"
#include "PhysicsDriver.h"
#include "CommandManager.h"
#include "ConfigFileManager.h"
#include "bzsignal.h"

// implementation-specific bzfs-specific headers
#include "RejoinList.h"
#include "ListServerConnection.h"
#include "WorldInfo.h"
#include "WorldWeapons.h"
#include "BZWReader.h"
#include "PackVars.h"
#include "SpawnPosition.h"
#include "DropGeometry.h"
#include "commands.h"
#include "MasterBanList.h"
#include "Filter.h"
#include "WorldEventManager.h"
#include "WorldGenerators.h"


// common implementation headers
#include "Obstacle.h"
#include "ObstacleMgr.h"
#include "WallObstacle.h"
#include "CollisionManager.h"
#include "BaseBuilding.h"
#include "AnsiCodes.h"
#include "GameTime.h"
#include "bzfsAPI.h"
#include "Teleporter.h"

// only include this if we are going to use plugins and export the API
#ifdef BZ_PLUGINS
#include "bzfsPlugins.h"
#endif

Shots::Manager ShotManager;

unsigned int maxNonPlayerDataChunk = 2048;
std::map<int, NetConnectedPeer> netConnectedPeers;

VotingArbiter *votingarbiter = NULL;

// pass through the SELECT loop
static bool dontWait = true;

// every ListServerReAddTime seconds add ourself to the list
// server again.  this is in case the list server has reset
// or dropped us for some reason.
static const double ListServerReAddTime = 15 * 60;

static const float FlagHalfLife = 10.0f;

// do NOT change
static const int InvalidPlayer = -1;

float speedTolerance = 1.125f;
static bool doSpeedChecks = true;

// Command Line Options
CmdLineOptions *clOptions;

// server address to listen on
static Address serverAddress;
// well known service socket
static int wksSocket;
bool handlePings = true;
static PingPacket pingReply;
// highest fd used
static int maxFileDescriptor;
// team info
TeamInfo team[NumTeams];
// num flags in flag list
int numFlags;
bool done = false;
// true if hit time/score limit
bool gameOver = true;
static int exitCode = 0;
// "real" players, i.e. do not count observers
uint16_t maxRealPlayers = MaxPlayers;
// players + observers
uint16_t maxPlayers = MaxPlayers;
// highest active id
uint16_t curMaxPlayers = 0;
int debugLevel = 0;

static float maxWorldHeight = 0.0f;
static bool disableHeightChecks = false;

std::string hexDigest;

TimeKeeper gameStartTime;
TimeKeeper countdownPauseStart = TimeKeeper::getNullTime();
bool countdownActive = false;
int countdownDelay = -1;
int countdownResumeDelay = -1;
int readySetGo = -1; // match countdown timer

static ListServerLink *listServerLink = NULL;
static int listServerLinksCount = 0;

// FIXME: should be static, but needed by SpawnPosition
WorldInfo *world = NULL;
// FIXME: should be static, but needed by RecordReplay
char *worldDatabase = NULL;
uint32_t worldDatabaseSize = 0;
char worldSettings[4 + WorldSettingsSize];
float pluginWorldSize = -1;
float pluginWorldHeight = -1;
bool checkShotMismatch = true;
Filter   filter;

BasesList bases;

// global keeper of world Events
WorldEventManager   worldEventManager;

// FIXME - define a well-known constant for a null playerid in address.h?
// might be handy in other players, too.
// Client does not check for rabbit to be 255, but it still works
// because 255 should be > curMaxPlayers and thus no matching player will
// be found.
// FIXME: should be static, but needed by RecordReplay
uint8_t rabbitIndex = NoPlayer;

static RejoinList rejoinList;

static TimeKeeper lastWorldParmChange;
static bool       playerHadWorld = false;

bool          publiclyDisconnected = false;


void sendFilteredMessage(int playerIndex, PlayerId dstPlayer, const char *message, MessageType type = ChatMessage);
static void dropAssignedFlag(int playerIndex);
static std::string evaluateString(const std::string&);

// logging to the API
class APILoggingCallback : public LoggingCallback
{
public:
    void log ( int level, const char* message )
    {
        bz_LoggingEventData_V1 data;
        data.level = level;
        data.message = message;

        worldEventManager.callEvents(bz_eLoggingEvent,&data);
    }
};

APILoggingCallback apiLoggingCallback;

class BZFSNetLogCB : NetworkDataLogCallback
{
public:
    void Init()
    {
        addNetworkLogCallback(this);
    }

    void kill()
    {
        removeNetworkLogCallback(this);
    }

    virtual ~BZFSNetLogCB()
    {
    }

    virtual void networkDataLog ( bool send, bool udp, const unsigned char *data, unsigned int size, void *param )
    {
        // let any listeners know we got net data
        NetHandler *h = (NetHandler*)param;

        bz_NetTransferEventData_V1 eventData;
        if (send)
            eventData.eventType = bz_eNetDataSendEvent;
        else
            eventData.eventType = bz_eNetDataReceiveEvent;
        eventData.send = send;
        eventData.udp = udp;
        eventData.iSize = size;
        if (h)
            eventData.playerID = h->getPlayerID();

        // make a copy of the data, just in case any plug-ins decide to MESS with it.
        eventData.data = (unsigned char*)malloc(size);
        memcpy(eventData.data,data,size);
        worldEventManager.callEvents(eventData.eventType,&eventData);
        free(eventData.data);
    }
};

BZFSNetLogCB netLogCB;

int getCurMaxPlayers()
{
    return curMaxPlayers;
}

static bool realPlayer(const PlayerId& id)
{
    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(id);
    return playerData && playerData->player.isPlaying() && !playerData->isParting;
}

static bool realPlayerWithNet(const PlayerId& id)
{
    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(id);
    return playerData && playerData->netHandler && playerData->player.isPlaying() && !playerData->isParting;
}


static int pwrite(GameKeeper::Player &playerData, const void *b, int l)
{
    if (!playerData.netHandler)
        return l;

    int result = playerData.netHandler->pwrite(b, l);
    if (result == -1)
        removePlayer(playerData.getIndex(), "ECONNRESET/EPIPE", false);
    return result;
}

static char sMsgBuf[MaxPacketLen];
char *getDirectMessageBuffer()
{
    return &sMsgBuf[2*sizeof(uint16_t)];
}

// FIXME? 4 bytes before msg must be valid memory, will get filled in with len+code
// usually, the caller gets a buffer via getDirectMessageBuffer(), but for example
// for MsgShotBegin the receiving buffer gets used directly
static int directMessage(GameKeeper::Player &playerData,
                         uint16_t code, int len, void *msg)
{
    if (playerData.isParting)
        return -1;
    // send message to one player
    void *bufStart = (char *)msg - 2*sizeof(uint16_t);

    void *buf = bufStart;
    buf = nboPackUShort(buf, uint16_t(len));
    buf = nboPackUShort(buf, code);
    return pwrite(playerData, bufStart, len + 4);
}

void directMessage(int playerIndex, uint16_t code, int len, void *msg)
{
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return;

    directMessage(*playerData, code, len, msg);
}

void broadcastMessage(uint16_t code, int len, void *msg)
{
    // send message to everyone
    for (int i = 0; i < curMaxPlayers; i++)
    {
        if (realPlayerWithNet(i))
            directMessage(i, code, len, msg);
    }

    // record the packet
    if (Record::enabled())
        Record::addPacket(code, len, msg);

    return;
}


//
// global variable callback
//
static void onGlobalChanged(const std::string& name, void*)
{
    // This Callback is removed in replay mode. As
    // well, the /set and /reset commands are blocked.

    std::string value = BZDB.get(name);
    void *bufStart = getDirectMessageBuffer();
    void *buf = nboPackUShort(bufStart, 1);
    buf = nboPackUByte(buf, (uint8_t)name.length());
    buf = nboPackString(buf, name.c_str(), name.length());
    buf = nboPackUByte(buf, (uint8_t)value.length());
    buf = nboPackString(buf, value.c_str(), value.length());
    broadcastMessage(MsgSetVar, (char*)buf - (char*)bufStart, bufStart);
}


static void sendUDPupdate(int playerIndex)
{
    // confirm inbound UDP with a TCP message
    directMessage(playerIndex, MsgUDPLinkEstablished, 0, getDirectMessageBuffer());
    // request/test outbound UDP with a UDP back to where we got client's packet
    directMessage(playerIndex, MsgUDPLinkRequest, 0, getDirectMessageBuffer());
}

static int lookupPlayer(const PlayerId& id)
{
    if (id == ServerPlayer)
        return id;

    if (!realPlayer(id))
        return InvalidPlayer;

    return id;
}

static void setNoDelay(int fd)
{
    // turn off TCP delay (collection).  we want packets sent immediately.
#if defined(_WIN32)
    BOOL on = TRUE;
#else
    int on = 1;
#endif
    struct protoent *p = getprotobyname("tcp");
    if (p && setsockopt(fd, p->p_proto, TCP_NODELAY, (SSOType)&on, sizeof(on)) < 0)
        nerror("enabling TCP_NODELAY");
}

void sendFlagUpdate(FlagInfo &flag)
{
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUShort(bufStart,1);
    bool hide
        = (flag.flag.type->flagTeam == ::NoTeam)
          && (flag.player == -1);
    buf = flag.pack(buf, hide);
    broadcastMessage(MsgFlagUpdate, (char*)buf - (char*)bufStart, bufStart);
}

static float nextGameTime()
{
    float nextTime = +MAXFLOAT;
    const TimeKeeper nowTime = TimeKeeper::getCurrent();
    for (int i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *gkPlayer = GameKeeper::Player::getPlayerByIndex(i);
        if ((gkPlayer != NULL) && gkPlayer->player.isHuman())
        {
            const TimeKeeper& pTime = gkPlayer->getNextGameTime();
            const float pNextTime = (float)(pTime - nowTime);
            if (pNextTime < nextTime)
                nextTime = pNextTime;
        }
    }
    return nextTime;
}

static int makeGameTime(void* bufStart, float lag)
{
    void *buf = bufStart;
    buf = GameTime::pack(buf, lag);
    return ((char*)buf - (char*)bufStart);
}

static void sendGameTime(GameKeeper::Player* gkPlayer)
{
    if (Replay::enabled())
        return;
    if (gkPlayer != NULL)
    {
        void* buf = getDirectMessageBuffer();
        const float lag = gkPlayer->lagInfo.getLagAvg();
        const int length = makeGameTime(buf, lag);
        directMessage(*gkPlayer, MsgGameTime, length, buf);
        gkPlayer->updateNextGameTime();
    }
    return;
}

static void sendPendingGameTime()
{
    const TimeKeeper nowTime = TimeKeeper::getCurrent();
    for (int i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *gkPlayer = GameKeeper::Player::getPlayerByIndex(i);
        if ((gkPlayer != NULL)
                && gkPlayer->player.isHuman()
                && (gkPlayer->getNextGameTime() - nowTime) < 0.0f)
            sendGameTime(gkPlayer);
    }
    return;
}


// Update the player "playerIndex" with all the flags status
static void sendFlagUpdate(int playerIndex)
{
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return;
    int result;

    void *buf, *bufStart = getDirectMessageBuffer();

    buf = nboPackUShort(bufStart,0); //placeholder
    int cnt = 0;
    int length = sizeof(uint16_t);
    for (int flagIndex = 0; flagIndex < numFlags; flagIndex++)
    {
        FlagInfo *flag = FlagInfo::get(flagIndex);
        if (!flag)
            continue;
        if (flag->exist())
        {
            if ((length + sizeof(uint16_t) + FlagPLen) > MaxPacketLen - 2*sizeof(uint16_t))
            {
                nboPackUShort(bufStart, cnt);
                result = directMessage(*playerData, MsgFlagUpdate, (char*)buf - (char*)bufStart, bufStart);
                if (result == -1)
                    return;
                cnt = 0;
                length = sizeof(uint16_t);
                buf = nboPackUShort(bufStart,0); //placeholder
            }

            bool hide = (flag->flag.type->flagTeam == ::NoTeam) &&
                        (flag->player == -1);
            buf = flag->pack(buf, hide);
            length += sizeof(uint16_t)+FlagPLen;
            cnt++;
        }
    }

    if (cnt > 0)
    {
        nboPackUShort(bufStart, cnt);
        directMessage(*playerData, MsgFlagUpdate, (char*)buf - (char*)bufStart, bufStart);
    }
}


void sendTeamUpdate(int playerIndex, int teamIndex1, int teamIndex2)
{
    // If teamIndex1 is -1, send all teams
    // If teamIndex2 is -1, just send teamIndex1 team
    // else send both teamIndex1 and teamIndex2 teams

    void *buf, *bufStart = getDirectMessageBuffer();
    if (teamIndex1 == -1)
    {
        buf = nboPackUByte(bufStart, CtfTeams);
        for (int t = 0; t < CtfTeams; t++)
        {
            buf = nboPackUShort(buf, t);
            buf = team[t].team.pack(buf);
        }
    }
    else if (teamIndex2 == -1)
    {
        buf = nboPackUByte(bufStart, 1);
        buf = nboPackUShort(buf, teamIndex1);
        buf = team[teamIndex1].team.pack(buf);
    }
    else
    {
        buf = nboPackUByte(bufStart, 2);
        buf = nboPackUShort(buf, teamIndex1);
        buf = team[teamIndex1].team.pack(buf);
        buf = nboPackUShort(buf, teamIndex2);
        buf = team[teamIndex2].team.pack(buf);
    }

    if (playerIndex == -1)
        broadcastMessage(MsgTeamUpdate, (char*)buf - (char*)bufStart, bufStart);
    else
        directMessage(playerIndex, MsgTeamUpdate, (char*)buf - (char*)bufStart, bufStart);
}

static void sendClosestFlagMessage(int playerIndex, FlagType *type, glm::vec3 &pos)
{
    if (!type)
        return;
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackVector(bufStart, pos);
    buf = nboPackStdString(buf, std::string(type->flagName));
    directMessage(playerIndex, MsgNearFlag,(char*)buf - (char*)bufStart, bufStart);
}

static void sendPlayerUpdate(GameKeeper::Player *playerData, int index)
{
    if (!playerData->player.isPlaying())
        return;

    void *bufStart = getDirectMessageBuffer();
    void *buf      = playerData->packPlayerUpdate(bufStart);

    if (playerData->getIndex() == index)
    {
        // send all players info about player[playerIndex]
        broadcastMessage(MsgAddPlayer, (char*)buf - (char*)bufStart, bufStart);
    }
    else
        directMessage(index, MsgAddPlayer, (char*)buf - (char*)bufStart, bufStart);
}

void sendAutopilotStatus(GameKeeper::Player *playerData, int index)
{
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, playerData->getIndex());
    buf = nboPackUByte(buf, playerData->player.isAutoPilot());

    if (playerData->getIndex() == index)
    {
        // send all players autopilot status for player[playerIndex]
        broadcastMessage(MsgAutoPilot, (char*)buf - (char*)bufStart, bufStart);
    }
    else
        directMessage(index, MsgAutoPilot, (char*)buf - (char*)bufStart, bufStart);
}

std::string GetPlayerIPAddress( int i)
{
    std::string tmp = "localhost";
    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(i);
    if (!playerData || !playerData->netHandler)
        return tmp;

    unsigned int address = (unsigned int)playerData->netHandler->getIPAddress().s_addr;
    unsigned char* a = (unsigned char*)&address;

    tmp = TextUtils::format("%d.%d.%d.%d",(int)a[0],(int)a[1],(int)a[2],(int)a[3]);
    return tmp;
}

void sendPlayerInfo()
{
    void *buf, *bufStart = getDirectMessageBuffer();
    int i, numPlayers = 0;
    for (i = 0; i < int(NumTeams); i++)
        numPlayers += team[i].team.size;
    buf = nboPackUByte(bufStart, numPlayers);
    for (i = 0; i < curMaxPlayers; ++i)
    {
        GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(i);
        if (!playerData)
            continue;

        if (playerData->player.isPlaying())
        {
            // see if any events want to update the playerInfo before it is sent out
            bz_GetPlayerInfoEventData_V1 playerInfoData;
            playerInfoData.playerID = i;
            playerInfoData.callsign = playerData->player.getCallSign();
            playerInfoData.team = convertTeam(playerData->player.getTeam());
            playerInfoData.verified = playerData->accessInfo.isVerified();
            playerInfoData.registered = playerData->accessInfo.isRegistered();
            playerInfoData.admin = playerData->accessInfo.showAsAdmin();
            playerInfoData.ipAddress =GetPlayerIPAddress(i);

            worldEventManager.callEvents(bz_eGetPlayerInfoEvent,&playerInfoData);

            buf = PackPlayerInfo(buf,i,GetPlayerProperties(playerInfoData.registered,playerInfoData.verified,playerInfoData.admin));
        }
    }
    broadcastMessage(MsgPlayerInfo, (char*)buf - (char*)bufStart, bufStart);
}

// Send score updates to players
void sendPlayerScores(GameKeeper::Player ** players, int nPlayers)
{
    void *buf, *bufStart;
    bufStart = getDirectMessageBuffer();

    buf = nboPackUByte(bufStart, nPlayers);

    for (int i = 0; i < nPlayers; i++)
    {
        GameKeeper::Player *player = players[i];

        buf = nboPackUByte(buf, player->getIndex());
        buf = player->score.pack(buf);
    }
    broadcastMessage(MsgScore, (char*)buf-(char*)bufStart, bufStart);
}

void sendIPUpdate(int targetPlayer, int playerIndex)
{
    // targetPlayer = -1: send to all players with the PLAYERLIST permission
    // playerIndex = -1: send info about all players

    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (playerIndex >= 0)
    {
        if (!playerData || !playerData->player.isPlaying())
            return;
    }

    // send to who?
    std::vector<int> receivers
        = GameKeeper::Player::allowed(PlayerAccessInfo::playerList, targetPlayer);

    // pack and send the message(s)
    void *buf, *bufStart = getDirectMessageBuffer();
    if (playerIndex >= 0)
    {
        buf = nboPackUByte(bufStart, 1);
        buf = playerData->packAdminInfo(buf);
        for (unsigned int i = 0; i < receivers.size(); ++i)
        {
            directMessage(receivers[i], MsgAdminInfo,
                          (char*)buf - (char*)bufStart, bufStart);
        }
        if (Record::enabled())
        {
            Record::addPacket(MsgAdminInfo,
                              (char*)buf - (char*)bufStart, bufStart, HiddenPacket);
        }
    }
    else
    {
        int ipsPerPackage = (MaxPacketLen - 3) / (PlayerIdPLen + 7);
        int i, c = 0;
        buf = nboPackUByte(bufStart, 0); // will be overwritten later
        for (i = 0; i < curMaxPlayers; ++i)
        {
            playerData = GameKeeper::Player::getPlayerByIndex(i);
            if (!playerData)
                continue;
            if (playerData->player.isPlaying())
            {
                buf = playerData->packAdminInfo(buf);
                ++c;
            }
            if (c == ipsPerPackage || ((i + 1 == curMaxPlayers) && c))
            {
                int size = (char*)buf - (char*)bufStart;
                buf = nboPackUByte(bufStart, c);
                c = 0;
                for (unsigned int j = 0; j < receivers.size(); ++j)
                    directMessage(receivers[j], MsgAdminInfo, size, bufStart);
            }
        }
    }
}

void resetTeamScores ( void )
{
    // reset team scores
    for (int i = RedTeam; i <= PurpleTeam; i++)
    {
        bz_TeamScoreChangeEventData_V1 eventData = bz_TeamScoreChangeEventData_V1(convertTeam(i), bz_eWins,
                team[i].team.getWins(), 0);
        worldEventManager.callEvents(&eventData);
        eventData = bz_TeamScoreChangeEventData_V1(convertTeam(i), bz_eWins, team[i].team.getLosses(), 0);
        worldEventManager.callEvents(&eventData);
        team[i].team.setLosses(0);
        team[i].team.setWins(0);
    }
    sendTeamUpdate();
}

void resetPlayerScores ( void )
{
    // Players to notify of new scores
    GameKeeper::Player **playersToUpdate = new GameKeeper::Player*[curMaxPlayers];
    int nPlayersToUpdate = 0;

    for (int i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *player;

        player = GameKeeper::Player::getPlayerByIndex(i);

        if (player)
        {
            player->score.reset();
            playersToUpdate[nPlayersToUpdate++] = player;
        }
    }

    // Tell the players the new scores
    sendPlayerScores(playersToUpdate, nPlayersToUpdate);

    delete[] playersToUpdate;
}

void pauseCountdown ( int pausedBy )
{
    if (clOptions->countdownPaused)
        return;

    const char* callsign = bz_getPlayerCallsign(pausedBy);

    clOptions->countdownPaused = true;
    countdownResumeDelay = -1; // reset back to "unset"

    if (callsign != NULL)
        sendMessage(ServerPlayer, AllPlayers, TextUtils::format("Countdown paused by %s", callsign).c_str());
    else
        sendMessage(ServerPlayer, AllPlayers, "Countdown paused");

    // fire off a game pause event
    bz_GamePauseResumeEventData_V2 pauseEventData;
    pauseEventData.eventType = bz_eGamePauseEvent;
    pauseEventData.playerID = pausedBy;
    pauseEventData.actionBy = callsign;
    worldEventManager.callEvents(bz_eGamePauseEvent, &pauseEventData);
}

void resumeCountdown ( int resumedBy )
{
    if (!clOptions->countdownPaused)
        return;

    const char* callsign = bz_getPlayerCallsign(resumedBy);

    clOptions->countdownPaused = false;
    countdownResumeDelay = (int) BZDB.eval(StateDatabase::BZDB_COUNTDOWNRESDELAY);

    if (countdownResumeDelay <= 0)
    {
        // resume instantly
        countdownResumeDelay = -1; // reset back to "unset"

        if (callsign != NULL)
            sendMessage(ServerPlayer, AllPlayers, TextUtils::format("Countdown resumed by %s", callsign).c_str());
        else
            sendMessage(ServerPlayer, AllPlayers, "Countdown resumed");

        // fire off a game resume event
        bz_GamePauseResumeEventData_V2 resumeEventData;
        resumeEventData.eventType = bz_eGameResumeEvent;
        resumeEventData.playerID = resumedBy;
        resumeEventData.actionBy = callsign;
        worldEventManager.callEvents(bz_eGameResumeEvent, &resumeEventData);
    }
    else
    {
        // resume after number of seconds in countdownResumeDelay
        if (callsign != NULL)
            sendMessage(ServerPlayer, AllPlayers, TextUtils::format("Countdown is being resumed by %s", callsign).c_str());
        else
            sendMessage(ServerPlayer, AllPlayers, "Countdown is being resumed");
    }
}

void startCountdown ( int delay, float limit, int playerID )
{
    const char* callsign = bz_getPlayerCallsign(playerID);

    if (callsign != NULL)
        sendMessage(ServerPlayer, AllPlayers, TextUtils::format("Team scores reset, countdown started by %s.",
                    callsign).c_str());
    else
        sendMessage(ServerPlayer, AllPlayers, "Team scores reset, countdown started.");

    clOptions->timeLimit = limit;
    countdownDelay = delay;

    // let everyone know what's going on
    long int timeArray[4];
    std::string matchBegins;
    if (countdownDelay == 0)
        matchBegins = "Match begins now!";
    else
    {
        TimeKeeper::convertTime(countdownDelay, timeArray);
        std::string countdowntime = TimeKeeper::printTime(timeArray);
        matchBegins = TextUtils::format("Match begins in about %s", countdowntime.c_str());
    }
    sendMessage(ServerPlayer, AllPlayers, matchBegins.c_str());

    TimeKeeper::convertTime(clOptions->timeLimit, timeArray);
    std::string timelimit = TimeKeeper::printTime(timeArray);
    matchBegins = TextUtils::format("Match duration is %s", timelimit.c_str());
    sendMessage(ServerPlayer, AllPlayers, matchBegins.c_str());

    // make sure the game always start unpaused
    clOptions->countdownPaused = false;
    clOptions->countdownStarter = playerID;
    countdownPauseStart = TimeKeeper::getNullTime();
}

void cancelCountdown ( int playerID )
{
    const char* callsign = bz_getPlayerCallsign(playerID);

    if (callsign != NULL)
        sendMessage(ServerPlayer, AllPlayers, TextUtils::format("Countdown cancelled by %s.", callsign).c_str());
    else
        sendMessage(ServerPlayer, AllPlayers, "Countdown cancelled");

    countdownDelay = -1;
    readySetGo = -1;
}

PingPacket getTeamCounts()
{
    pingReply.rogueCount = 0;
    pingReply.redCount = 0;
    pingReply.greenCount = 0;
    pingReply.blueCount = 0;
    pingReply.purpleCount = 0;
    pingReply.observerCount = 0;

    if (gameOver && clOptions->timeLimit > 0.0f && !clOptions->timeManualStart)
    {
        // pretend there are no players if the game is over, but only
        // for servers with automatic countdown because we want the server
        // to become empty, so a new countdown can start.
        // Servers with -timemanual (match servers) or plugins whch handle gameover
        // usually want people to join even when last game has just ended.
        // (FIXME: the countdown/gameover handling really needs a new concept,
        //   originally it was not possible to even join a server when gameover
        //   was reached, but this was changed for manual countdown (match) servers)
    }
    else
    {
        // update player counts in ping reply.

        for (int i = 0; i < curMaxPlayers; i++)
        {
            GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(i);
            if (p == NULL)
                continue;

            if (p->player.isHuman())
            {
                switch (p->player.getTeam())
                {
                case RabbitTeam:
                case RogueTeam:
                case HunterTeam:
                    pingReply.rogueCount++;
                    break;

                case RedTeam:
                    pingReply.redCount++;
                    break;

                case GreenTeam:
                    pingReply.greenCount++;
                    break;

                case BlueTeam:
                    pingReply.blueCount++;
                    break;

                case PurpleTeam:
                    pingReply.purpleCount++;
                    break;

                case ObserverTeam:
                    pingReply.observerCount++;
                    break;

                default:
                    break;
                }
            }
        }
    }
    return pingReply;
}


//============================================================================//

static std::string publicOwner = "";


const std::string& getPublicOwner()
{
    return publicOwner;
}


void setPublicOwner(const std::string& owner)
{
    publicOwner = owner;
}


//============================================================================//

void publicize()
{
    /* // hangup any previous list server sockets
    if (listServerLinksCount)
      listServerLink.closeLink(); */

    listServerLinksCount = 0;

    if (listServerLink)
        delete listServerLink;

    if (clOptions->publicizeServer)
    {
        // list server initialization
        for (std::vector<std::string>::const_iterator i = clOptions->listServerURL.begin(); i < clOptions->listServerURL.end();
                ++i)
        {
            listServerLink = new ListServerLink(i->c_str(),
                                                clOptions->publicizedAddress, clOptions->publicizedTitle, clOptions->advertiseGroups);
            listServerLinksCount++;
        }
    }
    else
    {
        // don't use a list server; we need a ListServerLink object anyway
        // pass no arguments to the constructor, so the object will exist but do nothing if called
        listServerLink = new ListServerLink();
        listServerLinksCount = 0;
    }
}


static bool serverStart()
{
#ifdef SO_REUSEADDR
#if defined(_WIN32)
    const BOOL optOn = TRUE;
    BOOL opt;
#else
    const int optOn = 1;
    int opt;
#endif
#endif
    maxFileDescriptor = 0;

    // init addr:port structure
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = serverAddress;

    pingReply.serverId.port = addr.sin_port = htons(clOptions->wksPort);

    // open well known service port
    wksSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (wksSocket == -1)
    {
        nerror("couldn't make connect socket");
        return false;
    }
#ifdef SO_REUSEADDR
    /* set reuse address */
    opt = optOn;
    if (setsockopt(wksSocket, SOL_SOCKET, SO_REUSEADDR, (SSOType)&opt, sizeof(opt)) < 0)
    {
        nerror("serverStart: setsockopt SO_REUSEADDR");
        close(wksSocket);
        return false;
    }
#endif
    if (bind(wksSocket, (const struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        nerror("couldn't bind connect socket");
        close(wksSocket);
        return false;
    }

    if (listen(wksSocket, 5) == -1)
    {
        nerror("couldn't make connect socket queue");
        close(wksSocket);
        return false;
    }

    addr.sin_port = htons(clOptions->wksPort);
    if (!NetHandler::initHandlers(addr))
    {
        close(wksSocket);
        return false;
    }

    listServerLinksCount = 0;
    publicize();
    return true;
}


static void serverStop()
{
    // shut down server
    // first ignore further attempts to kill me
    bzSignal(SIGINT, SIG_IGN);
    bzSignal(SIGTERM, SIG_IGN);

    // reject attempts to talk to server
    shutdown(wksSocket, 2);
    close(wksSocket);

    // tell players to quit
    for (int i = 0; i < curMaxPlayers; i++)
        directMessage(i, MsgSuperKill, 0, getDirectMessageBuffer());

    // close connections
    NetHandler::destroyHandlers();
}


static void relayPlayerPacket(int index, uint16_t len, const void *rawbuf, uint16_t code)
{
    if (Record::enabled())
        Record::addPacket(code, len, (const char*)rawbuf + 4);

    // relay packet to all players except origin
    for (int i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(i);
        if (!playerData)
            continue;
        PlayerInfo& pi = playerData->player;

        if (i != index && pi.isPlaying())
            pwrite(*playerData, rawbuf, len + 4);
    }
}

void addWall(float x, float y, float z, float r, float w, float h)
{
    const auto pos = glm::vec3(x, y, z);
    WallObstacle* wall = new WallObstacle(pos, r, w, h, false);
    OBSTACLEMGR.addWorldObstacle(wall);
}

void makeWalls ( void )
{
    float worldSize = BZDBCache::worldSize;
    if (pluginWorldSize > 0)
        worldSize = pluginWorldSize;

    float wallHeight = BZDB.eval(StateDatabase::BZDB_WALLHEIGHT);
    if (pluginWorldHeight > 0)
        wallHeight = pluginWorldHeight;

    double halfSize = worldSize * 0.5;
    double angleDelta = 360.0 / clOptions->wallSides;
    double startAngle = -angleDelta*0.5;
    double radius = sqrt(halfSize*halfSize + halfSize*halfSize);

    float degToRad = (float)(M_PI/180.0);

    double segmentLen = (sin(angleDelta*0.5*(degToRad)) * radius)*2;

    if (0)
    {
        for ( int w = 0; w < clOptions->wallSides; w++ )
        {
            float midpointRad = sqrtf((float)radius*(float)radius-((float)segmentLen*0.5f)*((float)segmentLen*0.5f));
            float midpointAngle = (float)startAngle + ((float)angleDelta*0.5f) + ((float)angleDelta*w);

            float x = sinf(midpointAngle*degToRad)*midpointRad;
            float y = cosf(midpointAngle*degToRad)*midpointRad;

            addWall(x, y, 0.0f, (270.0f-midpointAngle)*degToRad, (float)segmentLen, wallHeight);

        }
    }
    else
    {
        addWall(0.0f, 0.5f * worldSize, 0.0f, (float)(1.5 * M_PI), 0.5f * worldSize, wallHeight);
        addWall(0.5f * worldSize, 0.0f, 0.0f, (float)M_PI, 0.5f * worldSize, wallHeight);
        addWall(0.0f, -0.5f * worldSize, 0.0f, (float)(0.5 * M_PI), 0.5f * worldSize, wallHeight);
        addWall(-0.5f * worldSize, 0.0f, 0.0f, 0.0f, 0.5f * worldSize, wallHeight);
    }
}


bool defineWorld ( void )
{
    // clean up old database
    if (world)
        delete world;
    if (worldDatabase)
        delete[] worldDatabase;

    bz_GetWorldEventData_V1   worldData;
    worldData.ctf     = (clOptions->gameType == ClassicCTF);
    worldData.rabbit  = (clOptions->gameType == RabbitChase);
    worldData.openFFA = (clOptions->gameType == OpenFFA);
    worldData.worldFile = clOptions->worldFile;

    worldData.eventTime = TimeKeeper::getCurrent().getSeconds();
    worldEventManager.callEvents(bz_eGetWorldEvent, &worldData);

    if (!worldData.generated && worldData.worldFile.size())
    {
        clOptions->worldFile = worldData.worldFile.c_str();
        if (worldData.ctf)
            clOptions->gameType = ClassicCTF;
        else if (worldData.rabbit)
            clOptions->gameType = RabbitChase;
        else if (worldData.openFFA)
            clOptions->gameType = OpenFFA;
        else
            clOptions->gameType = TeamFFA;

        // todo.. load this maps options and vars and stuff.
    }

    // make world and add buildings
    if (worldData.worldFile.size())
    {
        BZWReader* reader = new BZWReader(std::string(worldData.worldFile.c_str()));
        world = reader->defineWorldFromFile();
        delete reader;

        if (clOptions->gameType == ClassicCTF)
        {
            for (int i = RedTeam; i <= PurpleTeam; i++)
            {
                if ((clOptions->maxTeam[i] > 0) && bases.find(i) == bases.end())
                {
                    std::cerr << "base was not defined for "
                              << Team::getName((TeamColor)i)
                              << std::endl;
                    return false;
                }
            }
        }
    }
    else
    {
        // check and see if anyone wants to define the world from an event

        world = new WorldInfo;
        if (!worldData.generated)   // if the plugin didn't make a world, make one
        {
            delete world;
            if (clOptions->gameType == ClassicCTF)
                world = defineTeamWorld();
            else
                world = defineRandomWorld();
        }
        else
        {
            makeWalls();

            OBSTACLEMGR.makeWorld();
            world->finishWorld();
        }
    }

    if (world == NULL)
        return false;

    maxWorldHeight = world->getMaxWorldHeight();

    // package up world
    world->packDatabase();

    // now get world packaged for network transmission
    worldDatabaseSize = 4 + WorldCodeHeaderSize +
                        world->getDatabaseSize() + 4 + WorldCodeEndSize;

    worldDatabase = new char[worldDatabaseSize];
    // this should NOT happen but it does sometimes
    if (!worldDatabase)
        return false;
    memset(worldDatabase, 0, worldDatabaseSize);

    void *buf = worldDatabase;
    buf = nboPackUShort(buf, WorldCodeHeaderSize);
    buf = nboPackUShort(buf, WorldCodeHeader);
    buf = nboPackUShort(buf, mapVersion);
    buf = nboPackUInt(buf, world->getUncompressedSize());
    buf = nboPackUInt(buf, world->getDatabaseSize());
    buf = nboPackString(buf, world->getDatabase(), world->getDatabaseSize());
    buf = nboPackUShort(buf, WorldCodeEndSize);
    buf = nboPackUShort(buf, WorldCodeEnd);

    TimeKeeper startTime = TimeKeeper::getCurrent();
    MD5 md5;
    md5.update((unsigned char *)worldDatabase, worldDatabaseSize);
    md5.finalize();
    hexDigest = (clOptions->worldFile == "") ? 't' : 'p';
    hexDigest += md5.hexdigest();
    TimeKeeper endTime = TimeKeeper::getCurrent();
    logDebugMessage(3,"MD5 generation: %.3f seconds\n", endTime - startTime);
    logDebugMessage(3,"MD5 = %s\n", hexDigest.c_str()+1);

    // water levels probably require flags on buildings
    const float waterLevel = world->getWaterLevel();
    if (!clOptions->flagsOnBuildings && (waterLevel > 0.0f))
    {
        clOptions->flagsOnBuildings = true;
        clOptions->respawnOnBuildings = true;
        logDebugMessage(1,"WARNING: enabling flag and tank spawns on buildings due to waterLevel\n");
    }

    // reset other stuff
    int i;
    for (i = 0; i < NumTeams; i++)
    {
        team[i].team.size = 0;
        team[i].team.setWins(0);
        team[i].team.setLosses(0);
    }
    FlagInfo::setNoFlagInAir();
    for (i = 0; i < numFlags; i++)
    {
        FlagInfo *flag = FlagInfo::get(i);
        if (!flag)
            continue;
        resetFlag(*flag);
    }
    bz_EventData eventData = bz_EventData(bz_eWorldFinalized);
    worldEventManager.callEvents(&eventData);
    return true;
}

bool saveWorldCache( const char* fileName )
{
    FILE* file;
    if (fileName)
        file = fopen (fileName, "wb");
    else
        file = fopen (clOptions->cacheOut.c_str(), "wb");
    if (file == NULL)
        return false;
    size_t written =
        fwrite (worldDatabase, sizeof(char), worldDatabaseSize, file);
    fclose (file);
    if (written != worldDatabaseSize)
        return false;
    return true;
}

float getMaxWorldHeight( void )
{
    float heightFudge = 1.10f; /* 10% */
    float wingsGravity = BZDB.eval(StateDatabase::BZDB_WINGSGRAVITY);
    float normalGravity = BZDB.eval(StateDatabase::BZDB_GRAVITY);

    if ((wingsGravity < 0.0f) && (normalGravity < 0.0f))
    {
        float wingsMaxHeight = BZDB.eval(StateDatabase::BZDB_WINGSJUMPVELOCITY);
        wingsMaxHeight *= wingsMaxHeight;
        wingsMaxHeight *= (1 + BZDB.eval(StateDatabase::BZDB_WINGSJUMPCOUNT));
        wingsMaxHeight /= (-wingsGravity * 0.5f);

        float normalMaxHeight = BZDB.eval(StateDatabase::BZDB_JUMPVELOCITY);
        normalMaxHeight *= normalMaxHeight;
        normalMaxHeight /= (-normalGravity * 0.5f);

        float maxHeight = (wingsMaxHeight > normalMaxHeight) ? wingsMaxHeight : normalMaxHeight;

        // final adjustments
        maxHeight *= heightFudge;
        maxHeight += maxWorldHeight;

        return maxHeight;
    }

    return -1;
}

TeamColor whoseBase(float x, float y, float z)
{
    if (clOptions->gameType!= ClassicCTF)
        return NoTeam;

    float highest = -1;
    int highestteam = -1;

    for (BasesList::iterator it = bases.begin(); it != bases.end(); ++it)
    {
        float baseZ = it->second.findBaseZ(x,y,z);
        if (baseZ > highest)
        {
            highest = baseZ;
            highestteam = it->second.getTeam();
        }
    }

    if (highestteam == -1)
        return NoTeam;
    else
        return TeamColor(highestteam);
}


#ifdef PRINTSCORE
static void dumpScore()
{
    if (!clOptions->printScore)
        return;
    if (clOptions->timeLimit > 0.0f)
        std::cout << "#time " << clOptions->timeLimit - clOptions->timeElapsed << std::endl;
    std::cout << "#teams";
    for (int i = int(RedTeam); i < NumTeams; i++)
        std::cout << ' ' << team[i].team.getWins() << '-' << team[i].team.getLosses() << ' ' << Team::getName(TeamColor(i));
    GameKeeper::Player::dumpScore();
    std::cout << "#end\n";
}
#endif

static void handleTcp(NetHandler &netPlayer, int i, const RxStatus e);

static void acceptClient()
{
    // client (not a player yet) is requesting service.
    // accept incoming connection on our well known port
    struct sockaddr_in clientAddr;
    AddrLen addr_len = sizeof(clientAddr);
    int fd = accept(wksSocket, (struct sockaddr*)&clientAddr, &addr_len);
    if (fd == -1)
    {
        nerror("accepting on wks");
        return;
    }

    // don't buffer info, send it immediately
    setNoDelay(fd);
    BzfNetwork::setNonBlocking(fd);

    int keepalive = 1, n;
    n = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
                   (SSOType)&keepalive, sizeof(int));
    if (n < 0)
        nerror("couldn't set keepalive");

    // they aren't a player yet till they send us the connection string
    NetConnectedPeer peer;
    peer.netHandler = new NetHandler(clientAddr, fd);
    peer.apiHandler = NULL;
    peer.player = -1;
    peer.socket = fd;
    peer.deleteMe = false;
    peer.sent = false;
    peer.minSendTime = 0;
    peer.lastSend = TimeKeeper::getCurrent();
    peer.startTime = TimeKeeper::getCurrent();
    peer.deleteWhenDoneSending = false;
    peer.inactivityTimeout = 30;

    netConnectedPeers[fd] = peer;
}

PlayerId getNewPlayerID()
{
    PlayerId playerIndex;

    // find open slot in players list
    PlayerId minPlayerId = 0, maxPlayerId = (PlayerId)MaxPlayers;
    if (Replay::enabled())
    {
        minPlayerId = MaxPlayers;
        maxPlayerId = MaxPlayers + ReplayObservers;
    }
    playerIndex = GameKeeper::Player::getFreeIndex(minPlayerId, maxPlayerId);

    if (playerIndex >= maxPlayerId)
        return 0xff;

    if (playerIndex >= curMaxPlayers)
        curMaxPlayers = playerIndex + 1;

    return playerIndex;
}

static bool MakePlayer ( NetHandler *handler )
{
    // send server version and playerid
    char buffer[9];
    memcpy(buffer, getServerVersion(), 8);
    // send 0xff if list is full
    buffer[8] = (char)0xff;

    PlayerId playerIndex = getNewPlayerID();

    if (playerIndex != 0xff)
    {
        logDebugMessage(1,"Player [%d] accept() from %s:%d on %i\n", playerIndex,
                        inet_ntoa(handler->getUADDR().sin_addr), ntohs(handler->getUADDR().sin_port), handler->getFD());

        if (playerIndex >= curMaxPlayers)
            curMaxPlayers = playerIndex+1;
    }
    else     // full? reject by closing socket
    {
        logDebugMessage(1,"all slots occupied, rejecting accept() from %s:%d on %i\n",
                        inet_ntoa(handler->getUADDR().sin_addr), ntohs(handler->getUADDR().sin_port), handler->getFD());

        // send back 0xff before closing
        send(handler->getFD(), (const char*)buffer, sizeof(buffer), 0);

        close(handler->getFD());
        return false;
    }

    buffer[8] = (uint8_t)playerIndex;
    send(handler->getFD(), (const char*)buffer, sizeof(buffer), 0);

    // FIXME add new client server welcome packet here when client code is ready
    new GameKeeper::Player(playerIndex, handler, handleTcp);

    // send the GameTime
    GameKeeper::Player* gkPlayer =
        GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (gkPlayer != NULL
            && gkPlayer->player.isHuman() )
        sendGameTime(gkPlayer);

    checkGameOn();
    return true;
}

void checkGameOn()
{
    // if game was over and this is the first player then game is on
    if (gameOver)
    {
        int count = GameKeeper::Player::count();
        if (count == 0)
        {
            gameOver = false;
            gameStartTime = TimeKeeper::getCurrent();
            if (clOptions->timeLimit > 0.0f && !clOptions->timeManualStart)
            {
                clOptions->timeElapsed = 0.0f;
                countdownActive = true;
            }
        }
    }
}


static void respondToPing(Address addr)
{
    // reply with current game info
    pingReply.sourceAddr = addr;
    // Total up rogue + rabbit + hunter teams
    pingReply.rogueCount = (uint8_t)team[0].team.size + (uint8_t)team[6].team.size + (uint8_t)team[7].team.size;
    pingReply.redCount = (uint8_t)team[1].team.size;
    pingReply.greenCount = (uint8_t)team[2].team.size;
    pingReply.blueCount = (uint8_t)team[3].team.size;
    pingReply.purpleCount = (uint8_t)team[4].team.size;
    pingReply.observerCount = (uint8_t)team[5].team.size;
}


void sendPlayerMessage(GameKeeper::Player *playerData, PlayerId dstPlayer,
                       const char *message)
{
    const PlayerId srcPlayer = playerData->getIndex();
    MessageType type = ChatMessage;

    // Check for spoofed /me messages
    if ((strlen(message) >= 3) && (message[0] == '*') && (message[strlen(message)-2] == '\t')
            && (message[strlen(message)-1] == '*'))
        return;

    // reformat any '/me' action messages
    // this is here instead of in commands.cxx to allow player-player/player-channel targetted messages
    if (strncasecmp(message, "/me", 3) == 0)
    {

        // don't bother with empty messages
        if (message[3] == '\0' || (isspace(message[3]) && message[4] == '\0'))
        {
            char reply[MessageLen] = {0};
            snprintf(reply, MessageLen, "%s, the /me command requires an argument", playerData->player.getCallSign());
            sendMessage(ServerPlayer, srcPlayer, reply);
            return;
        }

        // don't intercept other messages beginning with /me...
        if (!isspace(message[3]))
        {
            parseServerCommand(message, srcPlayer, dstPlayer);
            return;
        }

        // check for permissions
        if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::actionMessage))
        {
            char reply[MessageLen] = {0};
            snprintf(reply, MessageLen, "%s, you are not presently authorized to perform /me actions",
                     playerData->player.getCallSign());
            sendMessage(ServerPlayer, srcPlayer, reply);
            return;
        }

        // Trim off the command to leave the player's message
        message = message + 4;

        // Set the message type to an action message
        type = ActionMessage;
    }

    // check for a server command
    else if ((message[0] == '/') && (isalpha(message[1]) || message[1] == '?'))
    {
        // record server commands
        if (Record::enabled())
        {
            void *buf, *bufStart = getDirectMessageBuffer();
            buf = nboPackUByte(bufStart, srcPlayer);
            buf = nboPackUByte(buf, dstPlayer);
            buf = nboPackUByte(buf, type);
            buf = nboPackString(buf, message, strlen(message) + 1);
            Record::addPacket(MsgMessage, (char*)buf - (char*)bufStart, bufStart,
                              HiddenPacket);
        }
        parseServerCommand(message, srcPlayer, dstPlayer);
        return; // bail out
    }

    // check if the player has permission to use the admin channel
    if ((dstPlayer == AdminPlayers) &&
            !playerData->accessInfo.hasPerm(PlayerAccessInfo::adminMessageSend))
    {
        sendMessage(ServerPlayer, srcPlayer,
                    "You do not have permission to speak on the admin channel.");
        return; // bail out
    }

    // check for bogus targets
    if ((dstPlayer < LastRealPlayer) && !realPlayer(dstPlayer))
    {
        sendMessage(ServerPlayer, srcPlayer,
                    "The player you tried to talk to does not exist!");
        return; // bail out
    }

    sendChatMessage(srcPlayer, dstPlayer, message, type);
    return;
}

void sendChatMessage(PlayerId srcPlayer, PlayerId dstPlayer, const char *message, MessageType type)
{
    bz_ChatEventData_V2 chatData;
    chatData.from = BZ_SERVER;
    if (srcPlayer != ServerPlayer)
        chatData.from = srcPlayer;

    chatData.to = BZ_NULLUSER;

    if (dstPlayer == AllPlayers)
        chatData.to = BZ_ALLUSERS;
    else if ( dstPlayer == AdminPlayers )
        chatData.team = eAdministrators;
    else if ( dstPlayer > LastRealPlayer )
        chatData.team = convertTeam((TeamColor)(FirstTeam - dstPlayer));
    else
        chatData.to = dstPlayer;

    if (type == ActionMessage)
        chatData.messageType = eActionMessage;
    else
        chatData.messageType = eChatMessage;

    chatData.message = message;

    // send any events that want to watch the chat
    if (chatData.message.size())
        worldEventManager.callEvents(bz_eRawChatMessageEvent,&chatData);

    if (chatData.message.size())
        sendFilteredMessage(srcPlayer, dstPlayer, chatData.message.c_str(), type);
}

void sendFilteredMessage(int sendingPlayer, PlayerId recipientPlayer, const char *message, MessageType type)
{
    const char* msg = message;
    char filtered[MessageLen];

    if (clOptions->filterChat)
    {
        strncpy(filtered, message, MessageLen - 1);
        filtered[MessageLen - 1] = '\0';
        if (clOptions->filterSimple)
            clOptions->filter.filter(filtered, true);
        else
            clOptions->filter.filter(filtered, false);
        if (strcmp(message,filtered) != 0)  // the filter did do something so barf a message
        {
            bz_MessageFilteredEventData_V1    eventData;

            eventData.playerID = sendingPlayer;
            eventData.rawMessage = message;
            eventData.filteredMessage = filtered;

            worldEventManager.callEvents(bz_eMessageFilteredEvent,&eventData);

            size_t size = MessageLen-1;
            if (eventData.filteredMessage.size() < size)
                size = eventData.filteredMessage.size();
            strncpy(filtered, eventData.filteredMessage.c_str(), size);
            filtered[size] = '\0';
        }

        msg = filtered;

    }

    // if the message is empty, stop.
    if (strlen(msg) == 0)
        return;

    // check that the player has the talk permission
    GameKeeper::Player *senderData = GameKeeper::Player::getPlayerByIndex(sendingPlayer);

    if (!senderData && sendingPlayer != ServerPlayer)
        return;

    if (senderData)
    {
        if (!senderData->accessInfo.hasPerm(PlayerAccessInfo::talk))
        {
            // If the user does not have the TALK permission, he can't send any messages
            // He's only allowed to talk with admins, if he has the adminMessageSend permission
            if (senderData->accessInfo.hasPerm(PlayerAccessInfo::adminMessageSend))
            {
                if (recipientPlayer == AdminPlayers)
                {
                    sendMessage(sendingPlayer, recipientPlayer, msg, type);
                    return;
                }

                // Let the user send a private message to admins
                GameKeeper::Player *recipientData = GameKeeper::Player::getPlayerByIndex(recipientPlayer);
                if (recipientData && recipientData->accessInfo.hasPerm(PlayerAccessInfo::adminMessageReceive) &&
                        !recipientData->accessInfo.hasPerm(PlayerAccessInfo::hideAdmin))
                {
                    sendMessage(sendingPlayer, recipientPlayer, msg, type);
                    return;
                }
            }

            sendMessage(ServerPlayer, sendingPlayer, "We're sorry, you are not allowed to talk!");
            return; // bail out
        }

        if (recipientPlayer < LastRealPlayer && !senderData->accessInfo.hasPerm(PlayerAccessInfo::privateMessage))
        {
            sendMessage(ServerPlayer, sendingPlayer, "You are not allowed to send private messages to other players!");
            return;
        }

    }

    sendMessage(sendingPlayer, recipientPlayer, msg, type);
}

void sendMessage(int playerIndex, PlayerId dstPlayer, const char *message, MessageType type)
{
    long int msglen = strlen(message) + 1; // include null terminator
    const char *msg = message;

    if (message[0] == '/' && message[1] == '/')
    {
        ++msg;
        --msglen;
    }

    // Should cut the message
    if (msglen > MessageLen)
    {
        logDebugMessage(1,"WARNING: Network message being sent is too long! "
                        "(message is %li, cutoff at %d)\n", msglen, MessageLen);
        msglen = MessageLen;
    }

    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, playerIndex);
    buf = nboPackUByte(buf, dstPlayer);
    buf = nboPackUByte(buf, type);
    buf = nboPackString(buf, msg, msglen);

    ((char*)bufStart)[MessageLen - 1 + 3] = '\0'; // always terminate

    int len = 3 + msglen;
    bool broadcast = false;

    if (dstPlayer <= LastRealPlayer)
    {
        directMessage(dstPlayer, MsgMessage, len, bufStart);
        if (playerIndex <= LastRealPlayer && dstPlayer != playerIndex)
            directMessage(playerIndex, MsgMessage, len, bufStart);
    }
    // FIXME this teamcolor <-> player id conversion is in several files now
    else if (LastRealPlayer < dstPlayer && dstPlayer <= FirstTeam)
    {
        TeamColor _team = TeamColor(FirstTeam - dstPlayer);
        // send message to all team members only
        GameKeeper::Player *playerData;
        for (int i = 0; i < curMaxPlayers; i++)
            if ((playerData = GameKeeper::Player::getPlayerByIndex(i))
                    && playerData->player.isPlaying()
                    && playerData->player.isTeam(_team))
                directMessage(i, MsgMessage, len, bufStart);
    }
    else if (dstPlayer == AdminPlayers)
    {
        // admin messages

        // Notify any plugins
        if (playerIndex == ServerPlayer)
        {
            bz_ServerMsgEventData_V1 serverMsgData;
            serverMsgData.to = BZ_NULLUSER;
            serverMsgData.team = eAdministrators;
            serverMsgData.message = message;
            worldEventManager.callEvents(bz_eServerMsgEvent, &serverMsgData);
        }

        std::vector<int> admins
            = GameKeeper::Player::allowed(PlayerAccessInfo::adminMessageReceive);
        for (unsigned int i = 0; i < admins.size(); ++i)
            directMessage(admins[i], MsgMessage, len, bufStart);

    }
    else
    {
        // message to all players

        // Notify any plugins
        if (playerIndex == ServerPlayer)
        {
            bz_ServerMsgEventData_V1 serverMsgData;
            serverMsgData.to = BZ_ALLUSERS;
            serverMsgData.message = message;
            worldEventManager.callEvents(bz_eServerMsgEvent, &serverMsgData);
        }

        broadcastMessage(MsgMessage, len, bufStart);
        broadcast = true;
    }

    if (Record::enabled() && !broadcast)   // don't record twice
        Record::addPacket(MsgMessage, len, bufStart, HiddenPacket);
}

static void rejectPlayer(int playerIndex, uint16_t code, const char *reason)
{
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUShort(bufStart, code);
    buf = nboPackString(buf, reason, strlen(reason) + 1);
    directMessage(playerIndex, MsgReject, sizeof (uint16_t) + MessageLen, bufStart);
    // Fixing security hole, because a client can ignore the reject message
    // then he can avoid a ban, hostban...
    const std::string msg = "/rejected: " + stripAnsiCodes(reason);
    removePlayer(playerIndex, msg.c_str());
}

// FIXME this is a workaround for a bug, still needed?
// Team Size is wrong at some time
static void fixTeamCount()
{
    int playerIndex, teamNum;
    for (teamNum = RogueTeam; teamNum < NumTeams; teamNum++)
        team[teamNum].team.size = 0;
    for (playerIndex = 0; playerIndex < curMaxPlayers; playerIndex++)
    {
        GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(playerIndex);
        if (p && p->player.isPlaying())
        {
            teamNum = p->player.getTeam();
            if (teamNum == RabbitTeam)
                teamNum = HunterTeam;
            team[teamNum].team.size++;
        }
    }
}


// helper struct and predicates used in autoTeamSelect()
struct TeamSize
{
    TeamColor color;
    int       current;
    int       max;
    bool operator<(const TeamSize &x) const
    {
        return x.current < current;
    }
};

bool teamFull(const TeamSize &x)
{
    return x.current == x.max;
}
bool teamEmpty(const TeamSize &x)
{
    return x.current == 0;
}

struct teamHasSize
{
    int sz;
    teamHasSize(int sz_) : sz(sz_) {}
    bool operator() (const TeamSize &x) const
    {
        return x.current == sz;
    }
};

struct teamHasntSize
{
    int sz;
    teamHasntSize(int sz_) : sz(sz_) {}
    bool operator() (const TeamSize &x) const
    {
        return x.current != sz;
    }
};


static TeamColor teamSelect(TeamColor t, std::vector<TeamSize> teams)
{
    if (teams.empty())
        return RogueTeam;

    // see if the player's choice was a weak team
    for (unsigned int i = 0; i < teams.size(); i++)
        if (teams[i].color == t)
            return t;

    // eliminate all teams except those with the lowest total player scores
    if (teams.size() > 1)
    {
        std::vector<int> totalPlayerScores;
        for (unsigned int i = 0; i < teams.size(); ++i)
            totalPlayerScores.push_back(0);

        for (int i = 0; i < GameKeeper::Player::count(); ++i)
        {
            GameKeeper::Player *thisPlayer = GameKeeper::Player::getPlayerByIndex(i);
            if (thisPlayer == 0)
                continue;

            unsigned int thisTeamIndex = 0;
            while (thisTeamIndex < teams.size())
            {
                if (teams[thisTeamIndex].color == thisPlayer->player.getTeam())
                    break;

                ++thisTeamIndex;
            }
            if (thisTeamIndex < teams.size())
                totalPlayerScores[thisTeamIndex] += thisPlayer->score.getWins() -
                                                    thisPlayer->score.getLosses();
        }

        int minScore = totalPlayerScores[0];
        for (unsigned int i = 1; i < teams.size(); ++i)
            if (totalPlayerScores[i] < minScore)
                minScore = totalPlayerScores[i];
        unsigned int i = 0;
        while (i < teams.size())
        {
            if (totalPlayerScores[i] > minScore)
            {
                teams.erase(teams.begin() + i);
                totalPlayerScores.erase(totalPlayerScores.begin() + i);
            }
            else
                ++i;
        }
    }

    return teams[rand() % teams.size()].color;
}

static TeamColor autoTeamSelect(TeamColor t)
{
    // Asking for Observer gives observer
    if (t == ObserverTeam)
        return ObserverTeam;

    // When replaying, joining tank can only be observer
    if (Replay::enabled())
        return ObserverTeam;

    // count current number of players
    int numplayers = 0, i = 0;
    for (i = 0; i < int(NumTeams); i++)
        if (i != int(ObserverTeam))
            numplayers += team[i].team.size;

    // if no player are available, join as Observer
    if (numplayers == maxRealPlayers)
        return ObserverTeam;

    // if we're running rabbit chase, all non-observers start as hunters
    if (clOptions->gameType == RabbitChase)
        return HunterTeam;

    // If tank ask for rogues, and rogues are allowed, give it
    if ((t == RogueTeam)
            && team[RogueTeam].team.size < clOptions->maxTeam[RogueTeam])
        return RogueTeam;

    // If no auto-team, server or client, go back with client choice
    if (!clOptions->autoTeam && t != AutomaticTeam)
        return t;

    // Fill a vector with teams status, not putting in not enabled teams
    std::vector<TeamSize> teams;

    for (i = (int)RedTeam; i < (int)ObserverTeam; i++)
    {
        TeamSize currTeam = {(TeamColor)i,
                             team[i].team.size,
                             clOptions->maxTeam[i]
                            };
        if (currTeam.max > 0)
            teams.push_back(currTeam);
    }

    // Give rogue if that is the only team
    if (teams.empty())
        return RogueTeam;

    // Sort it by current team number
    std::sort(teams.begin(), teams.end());

    // all teams are empty, select just one of them
    if (teams[0].current == 0)
        return teamSelect(t, teams);

    int maxTeamSize = teams[0].current;

    teams.erase(std::remove_if(teams.begin(), teams.end(), teamFull), teams.end());
    // no non-full teams? then there must be a free rogue spot
    if (teams.empty())
        return RogueTeam;

    bool unBalanced = (teams.back().current < maxTeamSize);

    if (unBalanced)
    {
        // if you come with a 1-1-x-x try to add a player to these 1 to have team
        if ((maxTeamSize == 1) && (teams.size() >= 2) && (teams[1].current == 1))
        {
            // remove empty teams
            teams.erase(std::remove_if(teams.begin(), teams.end(), teamEmpty), teams.end());
        }
        else
        {
            // remove biggest teams
            teams.erase(std::remove_if(teams.begin(), teams.end(), teamHasSize(maxTeamSize)), teams.end());
            // Search for the lowest existing team and remove uppers. If there
            // are non empty teams don't start a new team but try to balance the lower
            if (teams[0].current > 0)
            {
                teams.erase(std::remove_if(teams.begin(), teams.end(), teamEmpty), teams.end());
                const int lowerTeam = teams.back().current;
                teams.erase(std::remove_if(teams.begin(), teams.end(), teamHasntSize(lowerTeam)), teams.end());
            }
        }
    }
    return teamSelect(t, teams);
}

static std::string evaluateString(const std::string &raw)
{
    std::string eval;
    const int rawLen = (int)raw.size();
    for (int i = 0; i < rawLen; i++)
    {
        char current = raw[i];
        if (current != '\\')
            eval += current;
        else
        {
            char next = raw[i+1];
            switch (next)
            {
            case '\\' :
            {
                eval += '\\';
                i++;
                break;
            }
            case 'n' :
            {
                eval += "\\n";
                i++;
                break;
            }
            case '{' :
            {
                unsigned int start = (i + 2);
                std::string::size_type end = raw.find_first_of('}', start);
                if (end == std::string::npos)
                {
                    i = rawLen; // unterminated, ignore the rest of the string
                }
                else
                {
                    const std::string var = raw.substr(start, end - start);
                    i += (end - start) + 2;
                    if (BZDB.isSet(var))
                        eval += BZDB.get(var);
                    else
                        eval += "*BADBZDB*";
                }
                break;
            }
            case '(' :
            {
                unsigned int start = (i + 2);
                std::string::size_type end = raw.find_first_of(')', start);
                if (end == std::string::npos)
                {
                    i = rawLen; // unterminated, ignore the rest of the string
                }
                else
                {
                    const std::string var = raw.substr(start, end - start);
                    i += (end - start) + 2;
                    if (var == "uptime")
                    {
                        char buffer[16];
                        const float uptime = (float)(TimeKeeper::getCurrent() - TimeKeeper::getStartTime());
                        snprintf(buffer, 16, "%i", (int)uptime);
                        eval += buffer;
                    }
                    else
                        eval += "*BADVAR*";
                }
                break;
            }
            default:
            {
                break;
            }
            }
        }
    }
    return eval;
}

static bool handicapAllowed( void )
{
    return (clOptions->gameOptions & short(HandicapGameStyle)) != 0;
}

// calculate handicap value for playerIndex and store in score object
static void recalcHandicap(int playerIndex)
{
    if (!handicapAllowed())
        return;

    int relscore = 0;

    GameKeeper::Player *me  = GameKeeper::Player::getPlayerByIndex(playerIndex);
    for (int i = 0; i < curMaxPlayers; i++)
    {
        if (i == playerIndex)
            continue;
        GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(i);
        if (p != NULL && realPlayer(i) && !p->player.isObserver())
            relscore += me->player.howManyTimesKilledBy(i) - p->player.howManyTimesKilledBy(playerIndex);
    }

    bz_ComputeHandicap_V1 data;
    data.playerID = playerIndex;
    data.desiredHandicap = std::max(0, relscore);

    worldEventManager.callEvents(bz_eComputeHandicapEvent, &data);

    me->score.setHandicap(data.desiredHandicap);
}

// calculate handicap values for all players
void recalcAllHandicaps()
{
    if (!handicapAllowed())
        return;

    bz_EventData beginData = bz_EventData(bz_eBeginHandicapRefreshEvent);
    worldEventManager.callEvents(&beginData);

    for (int i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(i);
        if (p && realPlayer(i) && !p->player.isObserver())
            recalcHandicap(i);
    }

    bz_EventData endData = bz_EventData(bz_eEndHandicapRefreshEvent);
    worldEventManager.callEvents(&endData);
}

// send handicap values for all players to all players
void broadcastHandicaps(int toPlayer)
{
    if (!handicapAllowed())
        return;

    int numHandicaps = 0;

    void *bufStart = getDirectMessageBuffer();
    void *buf = nboPackUByte(bufStart, numHandicaps);
    for (int i = 0; i < curMaxPlayers; i++)
    {
        if (i == toPlayer)
            continue;
        GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(i);
        if (p && realPlayer(i) && !p->player.isObserver())
        {
            numHandicaps++;
            buf = nboPackUByte(buf, i);
            buf = nboPackShort(buf, p->score.getHandicap());
        }
    }
    nboPackUByte(bufStart, numHandicaps);
    if (toPlayer >= 0)
        directMessage(toPlayer, MsgHandicap, (char*)buf-(char*)bufStart, bufStart);
    else
        broadcastMessage(MsgHandicap, (char*)buf-(char*)bufStart, bufStart);
}


static bool spawnSoon = false;


void AddPlayer(int playerIndex, GameKeeper::Player *playerData)
{
    playerData->addWasDelayed = false;
    uint16_t rejectCode;
    char rejectMsg[MessageLen];
    // check for a name clash
    bool resultEnter = playerData->loadEnterData(rejectCode, rejectMsg);

    // Name clash ... if the new player is not verified, reject it
    // We cannot have 2 players with same nick
    if (!resultEnter && playerData->_LSAState != GameKeeper::Player::verified)
    {
        rejectPlayer(playerIndex, rejectCode, rejectMsg);
        return;
    }

    if (!resultEnter)
    {
        // Find the user already logged on and kick it. The new player
        // has been globally authenticated.
        for (int i = 0; i < curMaxPlayers; i++)
        {
            // don't kick _us_, kick the other guy
            if (playerIndex == i)
                continue;
            GameKeeper::Player *otherPlayer
                = GameKeeper::Player::getPlayerByIndex(i);
            if (!otherPlayer)
                continue;

            // check and see if the other player was reged
            if (strcasecmp(otherPlayer->player.getCallSign(), playerData->player.getCallSign()) == 0)
            {
                sendMessage(ServerPlayer, i,
                            "Another client has demonstrated ownership of your "
                            "callsign with the correct password.  You have been "
                            "ghosted.");
                removePlayer(i, "Ghost");
                break;
            }
        }
    }

    if (clOptions->filterCallsigns)
    {
        int filterIndex = 0;
        Filter::Action filterAction = filter.check(*playerData, filterIndex);
        if (filterAction == Filter::DROP)
        {
            rejectPlayer(playerIndex, RejectBadCallsign, "Player has been banned");
            return;
        }
    }

    const bool playerIsAntiBanned =
        playerData->accessInfo.hasPerm(PlayerAccessInfo::antiban);

    // check against the ip ban list
    in_addr playerIP = playerData->netHandler->getIPAddress();
    BanInfo info(playerIP);
    if (!playerIsAntiBanned && !clOptions->acl.validate(playerIP,&info))
    {
        std::string rejectionMessage;

        rejectionMessage = BanRefusalString;
        rejectionMessage += " "; // add space between the ban reason and the protocol string, "REFUSED"

        if (info.reason.size ())
            rejectionMessage += info.reason;
        else
            rejectionMessage += "General Ban";

        rejectionMessage += ColorStrings[WhiteColor];
        rejectionMessage += " (";

        double duration = info.banEnd - TimeKeeper::getCurrent();
        if (duration < 365.0f * 24 * 3600)
        {
            long int timeArray[4];
            TimeKeeper::convertTime(duration, timeArray);
            std::string bantime = TimeKeeper::printTime(timeArray);
            rejectionMessage += bantime;
            rejectionMessage += " remaining";
        }
        else
            rejectionMessage += "indefinite";

        rejectionMessage += ")";

        rejectionMessage += ColorStrings[GreenColor];
        if (info.fromMaster)
            rejectionMessage += " [you are on the master ban list]";

        rejectPlayer (playerIndex, RejectIPBanned, rejectionMessage.c_str ());
        return;
    }

    // check against the id ban list
    const std::string& bzid = playerData->getBzIdentifier();
    IdBanInfo idInfo("");
    if (!playerIsAntiBanned && !clOptions->acl.idValidate(bzid.c_str(), &idInfo))
    {
        std::string rejectionMessage;

        rejectionMessage = BanRefusalString;
        if (idInfo.reason.size())
            rejectionMessage += idInfo.reason;
        else
            rejectionMessage += "General Ban";

        rejectionMessage += ColorStrings[WhiteColor];
        if (idInfo.bannedBy.size())
        {
            rejectionMessage += " by ";
            rejectionMessage += ColorStrings[BlueColor];
            rejectionMessage += idInfo.bannedBy;
        }

        rejectionMessage += ColorStrings[GreenColor];
        if (idInfo.fromMaster)
            rejectionMessage += " [from the master server]";
        rejectPlayer(playerIndex, RejectIDBanned, rejectionMessage.c_str());
        return;
    }

    // check against id and hostname ban lists (on the next cycle)
    playerData->setNeedThisHostbanChecked(true);

    // see if any watchers don't want this guy
    bz_AllowPlayerEventData_V1 allowData;
    allowData.callsign = playerData->player.getCallSign();
    allowData.ipAddress = playerData->netHandler->getTargetIP();
    allowData.playerID = playerIndex;

    worldEventManager.callEvents(bz_eAllowPlayer,&allowData);
    if (!allowData.allow)
    {
        rejectPlayer(playerIndex, RejectBadRequest, allowData.reason.c_str());
        return;
    }

    // pick a team
    TeamColor t = autoTeamSelect(playerData->player.getTeam());

    bz_GetAutoTeamEventData_V1 autoTeamData;
    autoTeamData.playerID = playerIndex;
    autoTeamData.team = convertTeam(t);
    autoTeamData.callsign = playerData->player.getCallSign();

    worldEventManager.callEvents(bz_eGetAutoTeamEvent,&autoTeamData);

    t = (TeamColor)convertTeam((bz_eTeamType)autoTeamData.team);  // team may be modified
    playerData->player.setTeam(t);
    playerData->player.endShotCredit = 0;     // reset shotEndCredit
    playerData->player.endShotShieldCredit = 0;   // endShotCredit for holding the shield flag (0 or 1)

    // count current number of players and players+observers
    int numplayers = 0;
    for (int i = 0; i < int(ObserverTeam); i++)
        numplayers += team[i].team.size;
    const int numplayersobs = numplayers + team[ObserverTeam].team.size;

    // reject player if asks for bogus team or rogue and rogues aren't allowed
    // or if the team is full or if the server is full
    if (!playerData->player.isHuman() && !playerData->player.isBot())
    {
        rejectPlayer(playerIndex, RejectBadType,
                     "Communication error joining game [Rejected].");
        return;
    }
    else if (t == NoTeam)
    {
        rejectPlayer(playerIndex, RejectBadTeam,
                     "Communication error joining game [Rejected].");
        return;
    }
    else if (t == ObserverTeam && playerData->player.isBot())
    {
        rejectPlayer(playerIndex, RejectServerFull,
                     "This game is full.  Try again later.");
        return;
    }
    else if (numplayersobs == maxPlayers)
    {
        // server is full
        rejectPlayer(playerIndex, RejectServerFull,
                     "This game is full.  Try again later.");
        return;
    }
    else if (team[int(t)].team.size >= clOptions->maxTeam[int(t)])
    {
        rejectPlayer(playerIndex, RejectTeamFull,
                     "This team is full.  Try another team.");
        return ;
    }
    // accept player
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, playerIndex);
    int result = directMessage(*playerData, MsgAccept,
                               (char*)buf-(char*)bufStart, bufStart);
    if (result == -1)
        return;

    //send SetVars
    {
        // scoping is mandatory
        PackVars pv(bufStart, playerIndex);
        BZDB.iterate(PackVars::packIt, &pv);
    }

    // abort if we hung up on the client
    if (!GameKeeper::Player::getPlayerByIndex(playerIndex))
        return;

    // player is signing on (has already connected via addClient).
    playerData->signingOn(clOptions->gameType == ClassicCTF);

    // update team state and if first player on team, reset it's score
    int teamIndex = int(playerData->player.getTeam());
    team[teamIndex].team.size++;
    if (team[teamIndex].team.size == 1
            && Team::isColorTeam((TeamColor)teamIndex))
    {
        team[teamIndex].team.setWins(0);
        team[teamIndex].team.setLosses(0);
    }

    // send new player updates on each player, all existing flags, and all teams.
    // don't send robots any game info.  watch out for connection being closed
    // because of an error.
    if (!playerData->player.isBot())
    {
        sendTeamUpdate(playerIndex);
        sendFlagUpdate(playerIndex);
        GameKeeper::Player *otherData;
        for (int i = 0; i < curMaxPlayers
                && GameKeeper::Player::getPlayerByIndex(playerIndex); i++)
            if (i != playerIndex)
            {
                otherData = GameKeeper::Player::getPlayerByIndex(i);
                if (otherData)
                {
                    sendPlayerUpdate(otherData, playerIndex);
                    if (otherData->player.isAutoPilot())
                        sendAutopilotStatus(otherData, playerIndex);
                }
            }

        broadcastHandicaps(playerIndex);
    }

    // if new player connection was closed (because of an error) then stop here
    if (!GameKeeper::Player::getPlayerByIndex(playerIndex))
        return;

    // see if the API wants to set the motto
    bz_GetPlayerMottoData_V2 mottoEvent(playerData->player.getMotto());
    mottoEvent.record = bz_getPlayerByIndex(playerIndex);
    worldEventManager.callEvents(&mottoEvent);
    playerData->player.setMotto(mottoEvent.motto.c_str());

    // broadcast motto only if player has SHOWMOTTO permission
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::showMotto) && strlen(playerData->player.getMotto()) != 0)
    {
        sendMessage(ServerPlayer, playerIndex, "\"showMotto\" permission is required to show your motto");
        playerData->player.setMotto("");
    }

    // send MsgAddPlayer to everybody -- this concludes MsgEnter response
    // to joining player
    sendPlayerUpdate(playerData, playerIndex);

    // send update of info for team just joined
    sendTeamUpdate(-1, teamIndex);

    // send IP update to everyone with PLAYERLIST permission
    sendIPUpdate(-1, playerIndex);
    sendIPUpdate(playerIndex, -1);

    // send rabbit information
    if (clOptions->gameType == RabbitChase)
    {
        bufStart = getDirectMessageBuffer();
        buf = nboPackUByte(bufStart, rabbitIndex);
        directMessage(playerIndex, MsgNewRabbit, (char*)buf-(char*)bufStart, bufStart);
    }

    // again check if player was disconnected
    if (!GameKeeper::Player::getPlayerByIndex(playerIndex))
        return;

    // send time update to new player if we're counting down
    if (countdownActive && clOptions->timeLimit > 0.0f
            && !playerData->player.isBot())
    {
        float timeLeft;

        if (countdownPauseStart)
        {
            // the game is paused
            timeLeft = -1.0f;
        }
        else
        {
            timeLeft = clOptions->timeLimit - (float)(TimeKeeper::getCurrent() - gameStartTime);
            if (timeLeft < 0.0f)
            {
                // oops
                timeLeft = 0.0f;
            }
        }

        bufStart = getDirectMessageBuffer();
        buf = nboPackInt(bufStart, (int32_t)timeLeft);
        result = directMessage(*playerData, MsgTimeUpdate,
                               (char*)buf-(char*)bufStart, bufStart);
        if (result == -1)
            return;
    }

    // if first player on team add team's flag
    if (team[teamIndex].team.size == 1
            && Team::isColorTeam((TeamColor)teamIndex))
    {
        if (clOptions->gameType == ClassicCTF)
        {
            int flagid = FlagInfo::lookupFirstTeamFlag(teamIndex);
            if (flagid >= 0 && !FlagInfo::get(flagid)->exist())
            {
                // reset those flags
                for (int n = 0; n < clOptions->numTeamFlags[teamIndex]; n++)
                    resetFlag(*FlagInfo::get(n + flagid));
            }
        }
    }

    fixTeamCount();

    // tell the list server the new number of players
    listServerLink->queueMessage(ListServerLink::ADD);

#ifdef PRINTSCORE
    dumpScore();
#endif
    char message[MessageLen] = {0};

#ifdef SERVERLOGINMSG
    snprintf(message, MessageLen, "BZFlag server %s, http://BZFlag.org/", getAppVersion());
    sendMessage(ServerPlayer, playerIndex, message);

    if (clOptions->servermsg != "")
    {
        const std::string srvmsg = evaluateString(clOptions->servermsg);

        // split the servermsg into several lines if it contains '\n'
        const char* i = srvmsg.c_str();
        const char* j;
        while ((j = strstr(i, "\\n")) != NULL)
        {
            unsigned int l = j - i < MessageLen - 1 ? j - i : MessageLen - 1;
            strncpy(message, i, l);
            message[l] = '\0';
            sendMessage(ServerPlayer, playerIndex, message);
            i = j + 2;
        }
        strncpy(message, i, MessageLen - 1);
        message[strlen(i) < MessageLen - 1 ? strlen(i) : MessageLen - 1] = '\0';
        sendMessage(ServerPlayer, playerIndex, message);
    }

    // look for a startup message -- from a file
    static const std::vector<std::string>* lines = clOptions->textChunker.getTextChunk("srvmsg");
    if (lines != NULL)
    {
        for (int i = 0; i < (int)lines->size(); i ++)
        {
            const std::string srvmsg = evaluateString((*lines)[i]);
            sendMessage(ServerPlayer, playerIndex, srvmsg.c_str());
        }
    }
#endif

    if (playerData->player.isObserver())
        sendMessage(ServerPlayer, playerIndex, "You are in observer mode.");

    // no quick rejoining, make 'em wait
    // you can switch to observer immediately, or switch from observer
    // to regular player immediately, but only if last time time you
    // were a regular player isn't in the rejoin list. As well, this all
    // only applies if the game isn't currently empty.
    if ((t != ObserverTeam) && (GameKeeper::Player::count() >= 0))
    {
        float waitTime = rejoinList.waitTime (playerIndex);
        if (waitTime > 0.0f)
        {
            char buffer[MessageLen];
            logDebugMessage(2,"Player %s [%d] rejoin wait of %.1f seconds\n",playerData->player.getCallSign(), playerIndex,
                            waitTime);
            snprintf (buffer, MessageLen, "You are unable to begin playing for %.1f seconds.", waitTime);
            sendMessage(ServerPlayer, playerIndex, buffer);
        }
    }

    if (GameKeeper::Player::getPlayerByIndex(playerIndex)
            && playerData->accessInfo.isRegistered()
            && playerData->_LSAState != GameKeeper::Player::verified)
    {
        // If the name is registered but not authenticated, tell them to identify
        sendMessage(ServerPlayer, playerIndex,
                    "This callsign is registered.  "
                    "You must use global authentication.");
    }

    dropAssignedFlag(playerIndex);

    sendPlayerInfo();

    // call any on join events
    bz_PlayerJoinPartEventData_V1 joinEventData;
    joinEventData.eventType = bz_ePlayerJoinEvent;
    joinEventData.playerID = playerIndex;
    joinEventData.record = bz_getPlayerByIndex(playerIndex);

    if ((playerData->player.getTeam() != NoTeam) && strlen(playerData->player.getCallSign()))
        worldEventManager.callEvents(bz_ePlayerJoinEvent,&joinEventData);

    if (spawnSoon)
        playerAlive(playerIndex);

    playerData->player.setCompletelyAdded();
}


void resetFlag(FlagInfo &flag)
{
    // NOTE -- must not be called until world is defined
    assert(world != NULL);

    // first drop the flag if someone has it
    if (flag.flag.status == FlagOnTank)
    {
        int player = flag.player;

        sendDrop(flag);

        // trigger the API event
        bz_FlagDroppedEventData_V1 data;
        data.playerID = player;
        data.flagID = flag.getIndex();
        data.flagType = flag.flag.type->flagAbbv.c_str();
        data.pos[0] = flag.flag.position[0];
        data.pos[1] = flag.flag.position[1];
        data.pos[2] = flag.flag.position[2];
        worldEventManager.callEvents(bz_eFlagDroppedEvent,&data);
    }

    float baseSize = BZDB.eval(StateDatabase::BZDB_BASESIZE);

    // reposition flag (middle of the map might be a bad idea)
    auto flagPos = glm::vec3(0.0f);

    TeamColor teamIndex = flag.teamIndex();
    if ((teamIndex >= ::RedTeam) &&  (teamIndex <= ::PurpleTeam)
            && (bases.find(teamIndex) != bases.end()))
    {
        if (!world->getFlagSpawnPoint(&flag, flagPos))
        {
            // return the flag to the center of the top of one of the team
            // bases.. we assume it'll fit.
            TeamBases &teamBases = bases[teamIndex];
            const TeamBase &base = teamBases.getRandomBase(flag.getIndex());
            flagPos[0] = base.position[0];
            flagPos[1] = base.position[1];
            flagPos[2] = base.position[2] + base.size[2];
        }
    }
    else
    {
        // random position (not in a building)
        const float waterLevel = world->getWaterLevel();
        float minZ = 0.0f;
        if (waterLevel > minZ)
            minZ = waterLevel;
        float maxZ = MAXFLOAT;
        if (!clOptions->flagsOnBuildings)
            maxZ = 0.0f;
        float worldSize = BZDBCache::worldSize;
        int i;
        for (i = 0; i < 10000; i++)
        {
            if (!world->getFlagSpawnPoint(&flag, flagPos))
            {
                flagPos[0] = (worldSize - baseSize) * ((float)bzfrand() - 0.5f);
                flagPos[1] = (worldSize - baseSize) * ((float)bzfrand() - 0.5f);
                flagPos[2] = world->getMaxWorldHeight() * (float)bzfrand();
            }
            if (DropGeometry::dropFlag(flagPos, minZ, maxZ))
                break;
        }
        if (i == 10000)
            std::cerr << "Unable to position flags on this world.\n";
    }

    bool teamIsEmpty = true;
    if (teamIndex != ::NoTeam)
        teamIsEmpty = (team[teamIndex].team.size == 0);

//   bz_FlagResetEventData_V1 eventData;
//   memcpy(eventData.pos,flagPos,sizeof(float)*3);
//   eventData.teamIsEmpty = teamIsEmpty;
//   eventData.flagID = flag.getIndex();
//   eventData.flagType = flag.flag.type->label().c_str();

    // reset a flag's info
    flag.resetFlag(flagPos, teamIsEmpty);

    sendFlagUpdate(flag);
}


void sendDrop(FlagInfo &flag)
{
    // see if someone had grabbed flag.  tell 'em to drop it.
    const int playerIndex = flag.player;

    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return;

    flag.player      = -1;
    playerData->player.resetFlag();

    void *bufStart = getDirectMessageBuffer();
    void *buf  = nboPackUByte(bufStart, playerIndex);
    buf = flag.pack(buf);
    broadcastMessage(MsgDropFlag, (char*)buf-(char*)bufStart, bufStart);
}

void zapFlag(FlagInfo &flag)
{
    // called when a flag must just disappear -- doesn't fly
    // into air, just *poof* vanishes.

    int player = flag.player;

    sendDrop(flag);

    // trigger the API event
    bz_FlagDroppedEventData_V1 data;
    data.playerID = player;
    data.flagID = flag.getIndex();
    data.flagType = flag.flag.type->flagAbbv.c_str();
    data.pos[0]   = flag.flag.position[0];
    data.pos[1]   = flag.flag.position[1];
    data.pos[2]   = flag.flag.position[2];

    worldEventManager.callEvents(bz_eFlagDroppedEvent,&data);

    // if flag was flying then it flies no more
    flag.landing(TimeKeeper::getSunExplodeTime());

    flag.flag.status = FlagNoExist;

    // reset flag status
    resetFlag(flag);
}

// try to get over a bug where extraneous flag are attached to a tank
// not really found why, but this should fix
// Should be called when we sure that tank does not hold any
static void dropAssignedFlag(int playerIndex)
{
    for (int flagIndex = 0; flagIndex < numFlags; flagIndex++)
    {
        FlagInfo *flag = FlagInfo::get(flagIndex);
        if (!flag)
            continue;
        if (flag->flag.status == FlagOnTank && flag->flag.owner == playerIndex)
            resetFlag(*flag);
    }
} // dropAssignedFlag

static void anointNewRabbit(int killerId = NoPlayer)
{
    GameKeeper::Player *killerData
        = GameKeeper::Player::getPlayerByIndex(killerId);
    GameKeeper::Player *oldRabbitData
        = GameKeeper::Player::getPlayerByIndex(rabbitIndex);
    int oldRabbit = rabbitIndex;
    rabbitIndex = NoPlayer;

    if (clOptions->rabbitSelection == KillerRabbitSelection)
        // check to see if the rabbit was just killed by someone; if so, make them the rabbit if they're still around.
        if (killerId != oldRabbit && killerData && killerData->player.isPlaying()
                && killerData->player.canBeRabbit())
            rabbitIndex = killerId;

    if (rabbitIndex == NoPlayer)
        rabbitIndex = GameKeeper::Player::anointRabbit(oldRabbit);

    if (rabbitIndex != oldRabbit)
    {
        logDebugMessage(3,"rabbitIndex is set to %d\n", rabbitIndex);
        if (oldRabbitData)
            oldRabbitData->player.wasARabbit();
        if (rabbitIndex != NoPlayer)
        {
            GameKeeper::Player *rabbitData
                = GameKeeper::Player::getPlayerByIndex(rabbitIndex);
            rabbitData->player.setTeam(RabbitTeam);
            void *buf, *bufStart = getDirectMessageBuffer();
            buf = nboPackUByte(bufStart, rabbitIndex);
            broadcastMessage(MsgNewRabbit, (char*)buf-(char*)bufStart, bufStart);
        }
    }
    else
    {
        logDebugMessage(3,"no other than old rabbit to choose from, rabbitIndex is %d\n",
                        rabbitIndex);
    }
}


static void pausePlayer(int playerIndex, bool paused = true)
{
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return;

    if (!playerData->player.isAlive())
    {
        logDebugMessage(2,"Player %s [%d] %spause while not alive\n",
                        playerData->player.getCallSign(), playerIndex, paused ? "" : "un");
        return;
    }
    if (playerData->player.isPaused() == paused)
    {
        logDebugMessage(2,"Player %s [%d] duplicate %spause\n",
                        playerData->player.getCallSign(), playerIndex, paused ? "" : "un");
        return;
    }
    // TODO: enforce 5-second delay from one pause to the next

    playerData->player.setPaused(paused);
    if (clOptions->gameType == RabbitChase)
    {
        if (paused && (rabbitIndex == playerIndex))
            anointNewRabbit();
        else if (!paused && (rabbitIndex == NoPlayer))
            anointNewRabbit();
    }

    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, playerIndex);
    buf = nboPackUByte(buf, paused);
    broadcastMessage(MsgPause, (char*)buf-(char*)bufStart, bufStart);

    bz_PlayerPausedEventData_V1   pauseEventData;
    pauseEventData.playerID = playerIndex;
    pauseEventData.pause = paused;

    worldEventManager.callEvents(bz_ePlayerPausedEvent,&pauseEventData);
}

static void autopilotPlayer(int playerIndex, bool autopilot)
{
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return;

    // Allow disabling but not enabling autopilot if bots are disabled
    if (autopilot && BZDB.isTrue(StateDatabase::BZDB_DISABLEBOTS))
    {
        sendMessage(ServerPlayer, playerIndex, "I'm sorry, we do not allow autopilot on this server.");
        removePlayer(playerIndex, "AutopilotPlayer");
        return;
    }

    // Ensure that observers can't toggle autopilot
    if (playerData->player.getTeam() == ObserverTeam)
        return;

    playerData->player.setAutoPilot(autopilot);

    // Send the status update to everyone
    sendAutopilotStatus(playerData, playerIndex);
}

void zapFlagByPlayer(int playerIndex)
{
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return;

    int flagid = playerData->player.getFlag();
    if (flagid < 0)
        return;

    FlagInfo &flag = *FlagInfo::get(flagid);
    flag.player = playerIndex;
    // do not simply zap team flag
    Flag &carriedflag = flag.flag;
    if (carriedflag.type->flagTeam != ::NoTeam)
        dropPlayerFlag(*playerData, playerData->lastState.pos);
    else
        zapFlag(flag);
}

void flushKilledByCounts( int removeID )
{
    for (int i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *player = GameKeeper::Player::getPlayerByIndex(i);
        if (player)
            player->player.flushKiller(removeID);
    }
}

void removePlayer(int playerIndex, const char *reason, bool notify)
{
    // player is signing off or sent a bad packet.  since the
    // bad packet can come before MsgEnter, we must be careful
    // not to undo operations that haven't been done.
    // first shutdown connection

    // remove the player from any kill counts
    flushKilledByCounts(playerIndex);

    // clear any shots they had flying around
    ShotManager.RemovePlayer(playerIndex);

    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return;

    playerData->isParting = true;

    // call any on part events
    bz_PlayerJoinPartEventData_V1 partEventData;
    partEventData.eventType = bz_ePlayerPartEvent;
    partEventData.playerID = playerIndex;
    partEventData.record = bz_getPlayerByIndex(playerIndex);
    if (reason)
        partEventData.reason = reason;

    if ((playerData->player.getTeam() != NoTeam) && strlen(playerData->player.getCallSign()))
        worldEventManager.callEvents(bz_ePlayerPartEvent,&partEventData);

    if (notify && playerData->netHandler != nullptr)
    {
        // send a super kill to be polite
        // send message to one player
        // do not use directMessage as he can remove player
        void *buf  = sMsgBuf;
        buf = nboPackUShort(buf, 0);
        buf = nboPackUShort(buf, MsgSuperKill);
        playerData->netHandler->pwrite(sMsgBuf, 4);
    }


    // if there is an active poll, cancel any vote this player may have made
    static VotingArbiter *arbiter = (VotingArbiter *)BZDB.getPointer("poll");
    if ((arbiter != NULL) && (arbiter->knowsPoll()))
        arbiter->retractVote(std::string(playerData->player.getCallSign()));

    // status message
    std::string timeStamp = TimeKeeper::timestamp();
    logDebugMessage(1,"Player %s [%d] removed at %s: %s\n",
                    playerData->player.getCallSign(),
                    playerIndex, timeStamp.c_str(), reason);
    bool wasPlaying = playerData->player.isPlaying();
    if (playerData->netHandler != nullptr)
        playerData->netHandler->closing();

    zapFlagByPlayer(playerIndex);

    // player is outta here.  if player never joined a team then
    // don't count as a player.

    if (wasPlaying)
    {
        // make them wait from the time they left, but only if they
        // have spawned at least once and are not already waiting
        if (!playerData->player.hasNeverSpawned() &&
                (rejoinList.waitTime (playerIndex) <= 0.0f) &&
                !playerData->accessInfo.hasPerm(PlayerAccessInfo::rejoin))
            rejoinList.add (playerIndex);

        // tell everyone player has left
        void *buf, *bufStart = getDirectMessageBuffer();
        buf = nboPackUByte(bufStart, playerIndex);
        broadcastMessage(MsgRemovePlayer, (char*)buf-(char*)bufStart, bufStart);

        for (int i = 0; i < curMaxPlayers; i++)
        {
            GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(i);
            if ((p == NULL) || !p->playerHandler || playerIndex == p->getIndex())
                continue;
            p->playerHandler->playerRemoved(playerIndex);
        }

        // decrease team size
        int teamNum = int(playerData->player.getTeam());
        --team[teamNum].team.size;

        // if last active player on team then remove team's flag if no one
        // is carrying it
        if (Team::isColorTeam((TeamColor)teamNum)
                && team[teamNum].team.size == 0 &&
                (clOptions->gameType == ClassicCTF))
        {
            int flagid = FlagInfo::lookupFirstTeamFlag(teamNum);
            if (flagid >= 0)
            {
                GameKeeper::Player *otherData;
                for (int n = 0; n < clOptions->numTeamFlags[teamNum]; n++)
                {
                    FlagInfo &flag = *FlagInfo::get(flagid+n);
                    otherData
                        = GameKeeper::Player::getPlayerByIndex(flag.player);
                    if (!otherData || otherData->player.isTeam((TeamColor)teamNum))
                        zapFlag(flag);
                }
            }
        }

        // send team update
        sendTeamUpdate(-1, teamNum);
    }

    playerData->close();

    if (wasPlaying)
    {
        // 'fixing' the count after deleting player
        fixTeamCount();

        // tell the list server the new number of players
        listServerLink->queueMessage(ListServerLink::ADD);
    }

    if (clOptions->gameType == RabbitChase)
        if (playerIndex == rabbitIndex)
            anointNewRabbit();

    // recompute curMaxPlayers
    if (playerIndex + 1 == curMaxPlayers)
        while (true)
        {
            curMaxPlayers--;
            if (curMaxPlayers <= 0
                    || GameKeeper::Player::getPlayerByIndex(curMaxPlayers - 1))
                break;
        }

    if (wasPlaying)
    {
        // if everybody left then reset world
        if (GameKeeper::Player::count() == 0)
        {

            if (clOptions->oneGameOnly)
            {
                done = true;
                exitCode = 0;
            }
            else
            {
                // republicize ourself.  this dereferences the URL chain
                // again so we'll notice any pointer change when any game
                // is over (i.e. all players have quit).
                publicize();
            }
        }
        else
        {
            recalcAllHandicaps();
            broadcastHandicaps();
        }
    }
}

// are the two teams foes with the current game style?
bool areFoes(TeamColor team1, TeamColor team2)
{
    if (!allowTeams())
        return true;
    return team1!=team2 || (team1==RogueTeam);
}


static void sendWorld(int playerIndex, uint32_t ptr)
{
    playerHadWorld = true;
    // send another small chunk of the world database
    assert((world != NULL) && (worldDatabase != NULL));
    void *buf, *bufStart = getDirectMessageBuffer();
    uint32_t size = MaxPacketLen - 2*sizeof(uint16_t) - sizeof(uint32_t);
    uint32_t left = worldDatabaseSize - ptr;
    if (ptr >= worldDatabaseSize)
    {
        size = 0;
        left = 0;
    }
    else if (ptr + size >= worldDatabaseSize)
    {
        size = worldDatabaseSize - ptr;
        left = 0;
    }
    buf = nboPackUInt(bufStart, uint32_t(left));
    buf = nboPackString(buf, (char*)worldDatabase + ptr, size);
    directMessage(playerIndex, MsgGetWorld, (char*)buf - (char*)bufStart, bufStart);
}


static void makeGameSettings()
{
    void* buf = worldSettings;

    // the header
    buf = nboPackUShort (buf, WorldSettingsSize); // length
    buf = nboPackUShort (buf, MsgGameSettings);   // code

    // the settings
    buf = nboPackFloat  (buf, BZDBCache::worldSize);
    buf = nboPackUShort (buf, clOptions->gameType);
    buf = nboPackUShort (buf, clOptions->gameOptions);
    // An hack to fix a bug on the client
    buf = nboPackUShort (buf, PlayerSlot);
    buf = nboPackUShort (buf, clOptions->maxShots);
    buf = nboPackUShort (buf, numFlags);
    buf = nboPackFloat  (buf, clOptions->linearAcceleration);
    buf = nboPackFloat  (buf, clOptions->angularAcceleration);
    buf = nboPackUShort (buf, clOptions->shakeTimeout);
    buf = nboPackUShort (buf, clOptions->shakeWins);
    buf = nboPackUInt   (buf, 0); // FIXME - used to be sync time

    return;
}


static void sendGameSettings(int playerIndex)
{
    GameKeeper::Player *playerData;
    playerData = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (playerData == NULL)
        return;

    pwrite (*playerData, worldSettings, 4 + WorldSettingsSize);

    return;
}


static void sendQueryGame(int playerIndex)
{
    // much like a ping packet but leave out useless stuff (like
    // the server address, which must already be known, and the
    // server version, which was already sent).
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUShort(bufStart, pingReply.gameType);
    buf = nboPackUShort(buf, pingReply.gameOptions);
    buf = nboPackUShort(buf, pingReply.maxPlayers);
    buf = nboPackUShort(buf, pingReply.maxShots);
    buf = nboPackUShort(buf, team[0].team.size);
    buf = nboPackUShort(buf, team[1].team.size);
    buf = nboPackUShort(buf, team[2].team.size);
    buf = nboPackUShort(buf, team[3].team.size);
    buf = nboPackUShort(buf, team[4].team.size);
    buf = nboPackUShort(buf, team[5].team.size);
    buf = nboPackUShort(buf, pingReply.rogueMax);
    buf = nboPackUShort(buf, pingReply.redMax);
    buf = nboPackUShort(buf, pingReply.greenMax);
    buf = nboPackUShort(buf, pingReply.blueMax);
    buf = nboPackUShort(buf, pingReply.purpleMax);
    buf = nboPackUShort(buf, pingReply.observerMax);
    buf = nboPackUShort(buf, pingReply.shakeWins);
    // 1/10ths of second
    buf = nboPackUShort(buf, pingReply.shakeTimeout);
    buf = nboPackUShort(buf, pingReply.maxPlayerScore);
    buf = nboPackUShort(buf, pingReply.maxTeamScore);
    buf = nboPackUShort(buf, pingReply.maxTime);
    buf = nboPackUShort(buf, (uint16_t)clOptions->timeElapsed);

    // send it
    directMessage(playerIndex, MsgQueryGame, (char*)buf-(char*)bufStart, bufStart);
}

static void sendQueryPlayers(int playerIndex)
{
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return;

    // count the number of active players
    int numPlayers = GameKeeper::Player::count();

    // first send number of teams and players being sent
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUShort(bufStart, NumTeams);
    buf = nboPackUShort(buf, numPlayers);
    int result = directMessage(*playerData, MsgQueryPlayers,
                               (char*)buf-(char*)bufStart, bufStart);
    if (result == -1)
        return;

    // now send the teams and players
    sendTeamUpdate(playerIndex);
    GameKeeper::Player *otherData;
    for (int i = 0; i < curMaxPlayers
            && GameKeeper::Player::getPlayerByIndex(playerIndex); i++)
    {
        if (i == playerIndex)
            continue;
        otherData = GameKeeper::Player::getPlayerByIndex(i);
        if (otherData)
            sendPlayerUpdate(otherData, playerIndex);
    }
}

void playerAlive(int playerIndex)
{
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return;

    if (!playerData->player.isPlaying())
    {
        spawnSoon = true;
        return;
    }
    spawnSoon = false;
    // ignore multiple MsgAlive; also observer should not send MsgAlive;
    // diagnostic?
    if (playerData->player.isAlive() || playerData->player.isObserver())
        return;

    // make sure the user identifies themselves if required.
    if (!playerData->accessInfo.isAllowedToEnter())
    {
        sendMessage(ServerPlayer, playerIndex, "This callsign is registered.  You must use global identification");
        sendMessage(ServerPlayer, playerIndex, "before playing or use a different callsign.");
        removePlayer(playerIndex, "unidentified");
        return;
    }

    bz_AllowSpawnData_V2  spawnAllowData;
    spawnAllowData.playerID = playerIndex;
    spawnAllowData.team = convertTeam(playerData->player.getTeam());

    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::spawn))
    {
        sendMessage(ServerPlayer, playerIndex, "You do not have permission to spawn on this server.");
        sendMessage(ServerPlayer, playerIndex, "This server may require identification before you can join.");
        sendMessage(ServerPlayer, playerIndex, "register on https://forums.bzflag.org/ and use that callsign/password.");
        spawnAllowData.allow = false;
    }

    // a plug-in has set the spawnability of a player, let's not kick them for that
    if (!playerData->player.isAllowedToSpawn())
    {
        spawnAllowData.allow = false;
        spawnAllowData.kickPlayer = false;
    }

    // check for any spawn allow events
    worldEventManager.callEvents(bz_eAllowSpawn, &spawnAllowData);

    if (!spawnAllowData.allow)
    {
        // check if the player has been notified that they may not spawn
        if (!playerData->player.notifiedOfSpawnable())
        {
            sendMessage(ServerPlayer, playerIndex, spawnAllowData.message.c_str());
            playerData->player.setNotifiedOfSpawnable(true);
        }

        // client won't send another enter so kick em =(
        if (spawnAllowData.kickPlayer)
            removePlayer(playerIndex, spawnAllowData.kickReason.c_str());

        return;
    }

    // player is coming alive.
    dropAssignedFlag(playerIndex);

    // get the spawn position
    SpawnPosition spawnPosition
    (playerIndex,
     (!clOptions->respawnOnBuildings) || (playerData->player.isBot()),
     clOptions->gameType == ClassicCTF);

    // see if there is anyone to handle the spawn event, and if they want to change it.
    bz_GetPlayerSpawnPosEventData_V1  spawnData;
    spawnData.playerID = playerIndex;
    spawnData.team   = convertTeam(playerData->player.getTeam());
    spawnData.pos[0]   = spawnPosition.getX();
    spawnData.pos[1]   = spawnPosition.getY();
    spawnData.pos[2]   = spawnPosition.getZ();
    spawnData.rot      = spawnPosition.getAzimuth();

    worldEventManager.callEvents(bz_eGetPlayerSpawnPosEvent,&spawnData);

    // update last position immediately
    playerData->player.setRestartOnBase(false);
    playerData->setPlayerState(glm::make_vec3(spawnData.pos), spawnData.rot);

    // send MsgAlive
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, playerIndex);
    buf = nboPackVector(buf, playerData->lastState.pos);
    buf = nboPackFloat(buf, playerData->lastState.azimuth);
    broadcastMessage(MsgAlive, (char*)buf - (char*)bufStart, bufStart);

    // call any events for a playerspawn
    bz_PlayerSpawnEventData_V1    spawnEvent;
    spawnEvent.playerID = playerIndex;
    spawnEvent.team = convertTeam(playerData->player.getTeam());

    playerStateToAPIState(spawnEvent.state, playerData->lastState);

    worldEventManager.callEvents(bz_ePlayerSpawnEvent,&spawnEvent);

    if (clOptions->gameType == RabbitChase)
    {
        playerData->player.wasNotARabbit();
        if (rabbitIndex == NoPlayer)
            anointNewRabbit();
    }
}

// TODO: In 2.6, we should just handle the kills fully from the server
void cleanupGameOver()
{
    double spawnDelay = (double)BZDB.eval(StateDatabase::BZDB_EXPLODETIME);
    for (int i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(i);
        if (p != NULL && p->player.getTeam() != ObserverTeam)
        {
            if (p->player.isAlive())
            {
                p->player.setDead();
                p->player.setSpawnDelay(spawnDelay);
            }
            zapFlagByPlayer(i);
            p->player.setRestartOnBase(true);
        }
    }
}

void checkTeamScore(int playerIndex, int teamIndex)
{
    if (clOptions->maxTeamScore == 0 || !Team::isColorTeam(TeamColor(teamIndex))) return;
    if (team[teamIndex].team.getWins() - team[teamIndex].team.getLosses() >= clOptions->maxTeamScore)
    {
        void *buf, *bufStart = getDirectMessageBuffer();
        buf = nboPackUByte(bufStart, playerIndex);
        buf = nboPackUShort(buf, uint16_t(teamIndex));
        broadcastMessage(MsgScoreOver, (char*)buf-(char*)bufStart, bufStart);

        cleanupGameOver();

        gameOver = true;
        if (clOptions->oneGameOnly)
        {
            done = true;
            exitCode = 0;
        }
    }
}

bool allowTeams ( void )
{
    return clOptions->gameType != OpenFFA;
}

// FIXME - needs extra checks for killerIndex=ServerPlayer (world weapons)
// (was broken before); it turns out that killerIndex=-1 for world weapon?
// No need to check on victimIndex.
//   It is taken as the index of the udp table when called by incoming message
//   It is taken by killerIndex when autocalled, but only if != -1
// killer could be InvalidPlayer or a number within [0 curMaxPlayer)
void playerKilled(int victimIndex, int killerIndex, int reason,
                  int16_t shotIndex, const FlagType* flagType, int phydrv, bool respawnOnBase )
{
    GameKeeper::Player *killerData = NULL;
    GameKeeper::Player *victimData
        = GameKeeper::Player::getPlayerByIndex(victimIndex);

    if (!victimData || !victimData->player.isPlaying())
        return;

    if (killerIndex != InvalidPlayer && killerIndex != ServerPlayer)
        killerData = GameKeeper::Player::getPlayerByIndex(killerIndex);

    // log the kill with the player
    if (killerData || killerIndex == ServerPlayer)
        victimData->player.killedBy(killerIndex);

    // aliases for convenience
    // Warning: killer should not be used when killerIndex == InvalidPlayer or ServerPlayer
    PlayerInfo *killer = realPlayer(killerIndex) ? &killerData->player : 0,
                *victim = &victimData->player;

    // victim was already dead. keep score.
    if (!victim->isAlive()) return;

    victim->setRestartOnBase(respawnOnBase);
    victim->setSpawnDelay((double)BZDB.eval(StateDatabase::BZDB_EXPLODETIME));
    victim->setDead();

    // call any events for a playerdeath
    bz_PlayerDieEventData_V2  dieEvent;
    dieEvent.playerID = victimIndex;
    dieEvent.team = convertTeam(victim->getTeam());
    dieEvent.killerID = killerIndex;
    dieEvent.shotID = shotIndex;

    if (killer)
        dieEvent.killerTeam = convertTeam(killer->getTeam());

    dieEvent.driverID = phydrv;
    dieEvent.flagKilledWith = flagType->flagAbbv;

    playerStateToAPIState(dieEvent.state, victimData->lastState);

    if (victim->haveFlag())   // this may not return true in the current protocol since tanks drop flags before death, when that's fixed the else wont' be needed
    {
        int flagid = victim->getFlag();
        if (flagid >= 0)
            dieEvent.flagHeldWhenKilled = flagid;
    }
    else if (victimData->lastHeldFlagID >= 0)
    {
        auto f = FlagInfo::get(victimData->lastHeldFlagID);

        if (f->flag.status != FlagOnGround) // if the last held flag was just a moment ago, they probably died with this.
            dieEvent.flagHeldWhenKilled = victimData->lastHeldFlagID;
    }

    worldEventManager.callEvents(bz_ePlayerDieEvent,&dieEvent);

    // If a plugin changed the killer, we need to update the data.
    if (dieEvent.killerID != killerIndex)
    {
        killerIndex = dieEvent.killerID;
        if (killerIndex != InvalidPlayer && killerIndex != ServerPlayer)
            killerData = GameKeeper::Player::getPlayerByIndex(killerIndex);
        killer = realPlayer(killerIndex) ? &killerData->player : 0;
    }

    // Call a new event when all the plugins have finished reassigning any kills
    worldEventManager.callEvents(bz_ePlayerDeathFinalizedEvent, &dieEvent);

    // killing rabbit or killing anything when I am a dead ex-rabbit is allowed
    bool teamkill = false;
    if (killer)
    {
        const bool rabbitinvolved = killer->isARabbitKill(*victim);
        const bool foe = areFoes(victim->getTeam(), killer->getTeam());
        teamkill = !foe && !rabbitinvolved;
    }

    // send MsgKilled
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, victimIndex);
    buf = nboPackUByte(buf, killerIndex);
    buf = nboPackShort(buf, reason);
    buf = nboPackShort(buf, shotIndex);
    buf = flagType->pack(buf);
    if (reason == PhysicsDriverDeath)
        buf = nboPackInt(buf, phydrv);
    broadcastMessage(MsgKilled, (char*)buf-(char*)bufStart, bufStart);

    // update tk-score
    if ((victimIndex != killerIndex) && teamkill)
    {
        killerData->score.tK();
        char message[MessageLen];
        if (clOptions->tkAnnounce)
        {
            snprintf(message, MessageLen, "Team kill: %s killed %s",
                     killerData->player.getCallSign(), victimData->player.getCallSign());
            sendMessage(ServerPlayer, AdminPlayers, message);
        }
        if (killerData->score.isTK())
        {
            strcpy(message, "You have been automatically kicked for team killing" );
            sendMessage(ServerPlayer, killerIndex, message);
            snprintf(message, MessageLen, "Player %s removed: team killing", killerData->player.getCallSign());
            sendMessage(ServerPlayer, AdminPlayers, message);
            removePlayer(killerIndex, "teamkilling");
        }
    }

    // zap flag player was carrying.  clients should send a drop flag
    // message before sending a killed message, so this shouldn't happen.
    zapFlagByPlayer(victimIndex);

    victimData = GameKeeper::Player::getPlayerByIndex(victimIndex);
    // victimData will be NULL if the player has been kicked for TK'ing
    // so don't bother doing any score stuff for him
    if (victimData != NULL)
    {
        // change the player score
        bufStart = getDirectMessageBuffer();
        victimData->score.killedBy();
        if (killer)
        {
            if (victimIndex != killerIndex)
            {
                if (teamkill)
                {
                    if (clOptions->teamKillerDies)
                        playerKilled(killerIndex, killerIndex, reason, -1, Flags::Null, -1);
                    else
                        killerData->score.killedBy();
                }
                else
                    killerData->score.kill();
            }

            // send killer & victim
            GameKeeper::Player *kAndV[] = {killerData, victimData};
            sendPlayerScores(kAndV, 2);
        }
        else
        {
            // send victim
            sendPlayerScores(&victimData, 1);
        }

        if (handicapAllowed())
        {
            bufStart = getDirectMessageBuffer();
            if (killer)
            {
                recalcHandicap(killerIndex);
                buf = nboPackUByte(bufStart, 2);
                buf = nboPackUByte(buf, killerIndex);
                buf = nboPackShort(buf, killerData->score.getHandicap());
            }
            else
                buf = nboPackUByte(bufStart, 1);
            recalcHandicap(victimIndex);
            buf = nboPackUByte(buf, victimIndex);
            buf = nboPackShort(buf, victimData->score.getHandicap());
            broadcastMessage(MsgHandicap, (char*)buf-(char*)bufStart, bufStart);
        }

        // see if the player reached the score limit
        if (clOptions->maxPlayerScore != 0
                && killerIndex != InvalidPlayer
                && killerIndex != ServerPlayer
                && killerData->score.reached())
        {
            bufStart = getDirectMessageBuffer();
            buf = nboPackUByte(bufStart, killerIndex);
            buf = nboPackUShort(buf, uint16_t(NoTeam));
            broadcastMessage(MsgScoreOver, (char*)buf-(char*)bufStart, bufStart);

            cleanupGameOver();

            gameOver = true;
            if (clOptions->oneGameOnly)
            {
                done = true;
                exitCode = 0;
            }
        }
    }

    if (clOptions->gameType == RabbitChase)
    {
        if (victimIndex == rabbitIndex)
            anointNewRabbit(killerIndex);
    }
    else if (Score::KeepTeamScores)
    {
        // change the team scores -- rogues don't have team scores.  don't
        // change team scores for individual player's kills in capture the
        // flag mode.
        // Team score is even not used on RabbitChase
        int winningTeam = (int)NoTeam;
        if ( clOptions->gameType == OpenFFA || clOptions->gameType == TeamFFA )
        {
            int killerTeam = -1;
            if (killer && victim->getTeam() == killer->getTeam())
            {
                if (!killer->isTeam(RogueTeam))
                {
                    int delta = 0;
                    if (killerIndex == victimIndex)
                        delta += 1;
                    else
                        delta += 2;

                    int old = team[int(victim->getTeam())].team.getLosses();
                    team[int(victim->getTeam())].team.setLosses(old+delta);
                    bz_TeamScoreChangeEventData_V1 eventData = bz_TeamScoreChangeEventData_V1(convertTeam(victim->getTeam()), bz_eLosses,
                            old, old+delta);
                    worldEventManager.callEvents(&eventData);
                }
            }
            else
            {
                if (killer && !killer->isTeam(RogueTeam))
                {
                    winningTeam = int(killer->getTeam());

                    int old = team[winningTeam].team.getWins();
                    team[winningTeam].team.setWins(old+1);
                    bz_TeamScoreChangeEventData_V1 eventData = bz_TeamScoreChangeEventData_V1(convertTeam(killer->getTeam()), bz_eWins, old,
                            old+1);
                    worldEventManager.callEvents(&eventData);
                }
                if (!victim->isTeam(RogueTeam))
                {
                    int old = team[int(victim->getTeam())].team.getLosses();
                    team[int(victim->getTeam())].team.setLosses(old+1);
                    bz_TeamScoreChangeEventData_V1 eventData = bz_TeamScoreChangeEventData_V1(convertTeam(victim->getTeam()), bz_eLosses,
                            old, old+1);
                    worldEventManager.callEvents(&eventData);
                }
                if (killer)
                    killerTeam = killer->getTeam();
            }
            sendTeamUpdate(-1,int(victim->getTeam()), killerTeam);
        }
#ifdef PRINTSCORE
        dumpScore();
#endif
        if (winningTeam != (int)NoTeam)
            checkTeamScore(killerIndex, winningTeam);
    }
}

void doSpawns()
{
    TimeKeeper curTime = TimeKeeper::getCurrent();
    for (int i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(i);
        if (p == NULL)
            continue;
        if (p->player.waitingToSpawn() && (p->player.getNextSpawnTime() <= curTime))
        {
            // Let them live!
            playerAlive(i);
        }
    }
}

// Currently only used for the server-side ID flag
static void searchFlag(GameKeeper::Player &playerData)
{
    if (!playerData.player.isAlive())
        return;

    // Only continue if the player has the ID flag
    int flagId = playerData.player.getFlag();
    if (flagId < 0)
        return;

    FlagInfo &playerFlag = *FlagInfo::get(flagId);
    if (playerFlag.flag.type != Flags::Identify)
        return;

    float radius = BZDB.eval(StateDatabase::BZDB_IDENTIFYRANGE);

    const PlayerId playerIndex = playerData.getIndex();

    const auto &tpos = playerData.lastState.pos;
    float radius2 = radius * radius;

    int closestFlag = -1;
    for (int i = 0; i < numFlags; i++)
    {
        FlagInfo &flag = *FlagInfo::get(i);
        if (!flag.exist())
            continue;
        if (flag.flag.status != FlagOnGround)
            continue;

        const auto &fpos = flag.flag.position;
        float dist = glm::distance2(tpos, fpos);

        if (dist < radius2)
        {
            radius2     = dist;
            closestFlag = i;
        }
    }

    if (closestFlag < 0)
    {
        playerData.setLastIdFlag(-1);
        return;
    }

    FlagInfo &flag = *FlagInfo::get(closestFlag);
    if (closestFlag != playerData.getLastIdFlag())
    {
        sendClosestFlagMessage(playerIndex,
                               flag.flag.type,
                               flag.flag.position);
        playerData.setLastIdFlag(closestFlag);
    }
}


void grabFlag(int playerIndex, FlagInfo &flag, bool checkPos)
{
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);

    // player wants to take possession of flag
    if (!playerData ||
            playerData->player.isObserver() ||
            !playerData->player.isAlive() ||
            playerData->player.haveFlag() ||
            (checkPos && flag.flag.status != FlagOnGround))
        return;

    const auto &fpos = flag.flag.position;
    if (checkPos)
    {
        //last Pos might be lagged by TankSpeed so include in calculation
        const float tankRadius = BZDBCache::tankRadius;
        const float tankSpeed = BZDBCache::tankSpeed;
        const float radius2 = powf(tankSpeed + tankRadius + BZDBCache::flagRadius, 2);
        const auto &tpos = playerData->lastState.pos;
        const float delta = glm::distance2(glm::vec2(tpos), glm::vec2(fpos));

        if ((fabs(tpos[2] - fpos[2]) < 0.1f) && (delta > radius2))
        {
            logDebugMessage(2,"Player %s [%d] %f %f %f tried to grab distant flag %f %f %f: distance=%f\n",
                            playerData->player.getCallSign(), playerIndex,
                            tpos[0], tpos[1], tpos[2], fpos[0], fpos[1], fpos[2], sqrt(delta));
            // @TODO make a better test for this to reduce false positives
            //removePlayer(playerIndex, "attempted illegal flag grab");
            return;
        }
    }

    bz_AllowFlagGrabData_V1 allow;
    allow.playerID = playerIndex;
    allow.flagID = flag.getIndex();
    allow.flagType = flag.flag.type->flagAbbv.c_str();

    worldEventManager.callEvents(bz_eAllowFlagGrab,&allow);

    if (!allow.allow)
        return;

    // okay, player can have it
    flag.grab(playerIndex);
    playerData->player.setFlag(flag.getIndex());

    playerData->lastHeldFlagID = flag.getIndex();

    // send MsgGrabFlag
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, playerIndex);
    buf = flag.pack(buf);

    broadcastMessage(MsgGrabFlag, (char*)buf-(char*)bufStart, bufStart);

    playerData->flagHistory.add(flag.flag.type);

    bz_FlagGrabbedEventData_V1 data;
    data.flagID = flag.getIndex();
    data.flagType = flag.flag.type->flagAbbv.c_str();
    data.pos[0]   = fpos[0];
    data.pos[1]   = fpos[1];
    data.pos[2]   = fpos[2];
    data.playerID = playerIndex;

    worldEventManager.callEvents(bz_eFlagGrabbedEvent,&data);
}


void dropFlag(FlagInfo& drpFlag, const glm::vec3 &dropPos)
{
    assert(world != NULL);

    const float size = BZDBCache::worldSize * 0.5f;
    glm::vec3 pos;
    pos[0] = ((dropPos[0] < -size) || (dropPos[0] > size)) ? 0.0f : dropPos[0];
    pos[1] = ((dropPos[1] < -size) || (dropPos[1] > size)) ? 0.0f : dropPos[1];
    pos[2] = (dropPos[2] > maxWorldHeight) ? maxWorldHeight : dropPos[2];

    // player wants to drop flag.  we trust that the client won't tell
    // us to drop a sticky flag until the requirements are satisfied.
    const int flagIndex = drpFlag.getIndex();
    if (drpFlag.flag.status != FlagOnTank)
        return;
    int flagTeam = drpFlag.flag.type->flagTeam;
    bool isTeamFlag = (flagTeam != ::NoTeam);

    // limited flags that have been fired should be disposed of
    bool limited = clOptions->flagLimit[drpFlag.flag.type] != -1;
    if (limited && drpFlag.numShots > 0) drpFlag.grabs = 0;


    const float waterLevel = world->getWaterLevel();
    float minZ = 0.0f;
    if (waterLevel > minZ)
        minZ = waterLevel;
    const float maxZ = MAXFLOAT;

    auto landing = pos;
    bool safelyDropped =
        DropGeometry::dropTeamFlag(landing, minZ, maxZ, flagTeam);

    bool vanish;

    if (isTeamFlag)
        vanish = false;
    else if (--drpFlag.grabs <= 0)
    {
        vanish = true;
        drpFlag.grabs = 0;
    }
    else if (!clOptions->flagsOnBuildings && (landing[2] > 0.0f))
        vanish = true;
    else
        vanish = !safelyDropped;

    // With Team Flag, we should absolutely go for finding a landing
    // position, while, for other flags, we could stay with default, or
    // just let them vanish
    if (isTeamFlag && !safelyDropped)
    {
        // figure out landing spot -- if flag in a Bad Place
        // when dropped, move to safety position or make it going
        if (!world->getFlagDropPoint(&drpFlag, pos, landing))
        {
            // try the center
            landing[0] = landing[1] = landing[2] = 0.0f;
            safelyDropped =
                DropGeometry::dropTeamFlag(landing, minZ, maxZ, flagTeam);
            if (!safelyDropped)
            {
                // ok, we give up, send it home
                TeamBases &teamBases = bases[flagTeam];
                const TeamBase &base = teamBases.getRandomBase(flagIndex);
                landing[0] = base.position[0];
                landing[1] = base.position[1];
                landing[2] = base.position[2] + base.size[2];
            }
        }
    }

    if (isTeamFlag)
    {
        // flag timeout gets started when the LAST team flag held is dropped,
        // AND the team is already empty
        if (team[drpFlag.flag.type->flagTeam].team.size == 0)
        {
            // see if the flag being dropped was the last team flag being held
            bool startTimeout = true;
            for (int i = 0; i < numFlags; i++)
            {
                FlagInfo* fi = FlagInfo::get(i);
                if (fi->teamIndex() == drpFlag.teamIndex() && fi->getIndex() != drpFlag.getIndex())
                {
                    // this is another flag belonging to the same team
                    if (realPlayer(fi->player))
                    {
                        startTimeout = false; // can't start timeout if another team flag is being carried
                        break;
                    }
                }
            }

            if (startTimeout)
            {
                team[flagIndex + 1].flagTimeout = TimeKeeper::getCurrent();
                team[flagIndex + 1].flagTimeout += (float)clOptions->teamFlagTimeout;
            }
        }
    }

    int player = drpFlag.player;

    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(player);
    playerData->lastHeldFlagID = drpFlag.getIndex();

    drpFlag.dropFlag(pos, landing, vanish);

    // player no longer has flag -- send MsgDropFlag
    sendDrop(drpFlag);

    // notify of new flag state
    sendFlagUpdate(drpFlag);

    // trigger the api event
    bz_FlagDroppedEventData_V1 data;
    data.playerID = player;
    data.flagID = flagIndex;
    data.flagType = drpFlag.flag.type->flagAbbv.c_str();
    data.pos[0]   = pos[0];
    data.pos[1]   = pos[1];
    data.pos[2]   = pos[2];

    worldEventManager.callEvents(bz_eFlagDroppedEvent,&data);

}


void dropPlayerFlag(GameKeeper::Player &playerData, const glm::vec3 &dropPos)
{
    const int flagIndex = playerData.player.getFlag();
    if (flagIndex < 0)
        return;

    FlagInfo &flag = *FlagInfo::get(flagIndex);
    if (flag.flag.type == Flags::Shield)
    {
        playerData.player.endShotCredit -= playerData.player.endShotShieldCredit;
        playerData.player.endShotShieldCredit = 0;
    }

    dropFlag(flag, dropPos);

    return;
}

bool captureFlag(int playerIndex, TeamColor teamCaptured, TeamColor teamCapped, bool checkCheat)
{
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return false;

    // Sanity check
    if (teamCaptured < RedTeam || teamCaptured > PurpleTeam)
        return false;

    if (teamCapped != NoTeam && (teamCapped < RedTeam || teamCapped > PurpleTeam))
        return false;

    // player captured a flag.  can either be enemy flag in player's own
    // team base, or player's own flag in enemy base.
    int flagIndex = playerData->player.getFlag();
    if (flagIndex < 0)
        return false;
    FlagInfo &flag = *FlagInfo::get(flagIndex);

    TeamColor teamIndex;

    if (teamCapped != NoTeam)
        teamIndex = teamCapped;
    else
        teamIndex = flag.teamIndex();

    if (teamIndex == ::NoTeam)
        return false;
    /*
     * The flag object always shows that it is the player's own team.
     * TODO: understand this situation better and change or document it.
     *if (teamIndex != teamCaptured)
     *    logDebugMessage(1,"Player %s [%d] claimed to capture %s flag while carrying %s flag\n",
     *  playerData->player.getCallSign(), playerIndex,
     *  Team::getName(teamCaptured), Team::getName(teamIndex));
     */

    if (checkCheat)   //cheat checking
    {
        TeamColor base = whoseBase(playerData->lastState.pos[0],
                                   playerData->lastState.pos[1],
                                   playerData->lastState.pos[2]);
        if ((teamIndex == playerData->player.getTeam() &&
                base == playerData->player.getTeam()))
        {
            logDebugMessage(1,"Player %s [%d] might have sent MsgCaptureFlag for taking their own "
                            "flag onto their own base\n",
                            playerData->player.getCallSign(), playerIndex);
            //return; //sanity check
        }
        if ((teamIndex != playerData->player.getTeam() &&
                base != playerData->player.getTeam()))
        {
            logDebugMessage(1,"Player %s [%d] (%s) might have tried to capture %s flag without "
                            "reaching their own base. (Player position: %f %f %f)\n",
                            playerData->player.getCallSign(), playerIndex,
                            Team::getName(playerData->player.getTeam()),
                            Team::getName(teamIndex),
                            playerData->lastState.pos[0], playerData->lastState.pos[1],
                            playerData->lastState.pos[2]);
            //char message[MessageLen];
            //strcpy(message, "Autokick: Tried to capture opponent flag without landing on your base");
            //sendMessage(ServerPlayer, playerIndex, message);
            //removePlayer(playerIndex, "capturecheat"); //FIXME: kicks honest players at times
            //return;
        }
    }

    bz_AllowCTFCaptureEventData_V1 allowCap;

    allowCap.teamCapped = convertTeam(teamIndex);
    allowCap.teamCapping = convertTeam(teamCaptured);
    allowCap.playerCapping = playerIndex;
    glm::vec3 allowCapPos;
    playerData->getPlayerState(allowCapPos, allowCap.rot);
    allowCap.pos[0] = allowCapPos[0];
    allowCap.pos[1] = allowCapPos[1];
    allowCap.pos[2] = allowCapPos[2];

    allowCap.allow = true;

    worldEventManager.callEvents(bz_eAllowCTFCaptureEvent,&allowCap);

    if (!allowCap.allow)
        return false;

    // player no longer has flag and put flag back at it's base
    playerData->player.resetFlag();
    resetFlag(flag);

    // send MsgCaptureFlag
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, playerIndex);
    buf = nboPackUShort(buf, uint16_t(flagIndex));
    buf = nboPackUShort(buf, uint16_t(teamCaptured));
    broadcastMessage(MsgCaptureFlag, (char*)buf-(char*)bufStart, bufStart);

    // find any events for capturing the flags on the capped team or events for ANY team
    bz_CTFCaptureEventData_V1 eventData;
    eventData.teamCapped = convertTeam(teamIndex);
    eventData.teamCapping = convertTeam(teamCaptured);
    eventData.playerCapping = playerIndex;
    glm::vec3 eventDataPos;
    playerData->getPlayerState(eventDataPos, eventData.rot);
    eventData.pos[0] = eventDataPos[0];
    eventData.pos[1] = eventDataPos[1];
    eventData.pos[2] = eventDataPos[2];

    worldEventManager.callEvents(bz_eCaptureEvent,&eventData);

    // everyone on losing team is dead
    double spawnDelay = (double)BZDB.eval(StateDatabase::BZDB_EXPLODETIME);
    for (int i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(i);
        if ((p == NULL) || (teamIndex != (int)p->player.getTeam()))
            continue;
        if (p->player.isAlive())
        {
            p->player.setDead();
            p->player.setSpawnDelay(spawnDelay);
        }
        p->player.setRestartOnBase(true);
        zapFlagByPlayer(i);
    }

    if (Score::KeepTeamScores)
    {
        // update score (rogues can't capture flags)
        int winningTeam = (int)NoTeam;
        if (teamIndex != int(playerData->player.getTeam()))
        {
            // player captured enemy flag
            winningTeam = int(playerData->player.getTeam());

            int old = team[winningTeam].team.getWins();
            team[winningTeam].team.setWins(old+1);
            bz_TeamScoreChangeEventData_V1 eventData2 = bz_TeamScoreChangeEventData_V1(convertTeam(winningTeam), bz_eWins, old,
                    old+1);
            worldEventManager.callEvents(&eventData2);
        }
        int old = team[teamIndex].team.getLosses();
        team[teamIndex].team.setLosses(old+1);
        bz_TeamScoreChangeEventData_V1 eventData3 = bz_TeamScoreChangeEventData_V1(convertTeam(teamIndex), bz_eLosses, old,
                old+1);
        worldEventManager.callEvents(&eventData3);

        sendTeamUpdate(-1, winningTeam, teamIndex);
#ifdef PRINTSCORE
        dumpScore();
#endif
        if (winningTeam != (int)NoTeam)
            checkTeamScore(playerIndex, winningTeam);
    }

    return true;
}

static void shotUpdate(int playerIndex, void *buf, int len)
{
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return;

    const PlayerInfo &shooter = playerData->player;
    if (!shooter.isAlive() || shooter.isObserver())
        return;

    ShotUpdate shot;
    PlayerId targetId;
    nboUnpackUByte(shot.unpack(buf), targetId);

    // verify playerId
    if (shot.player != playerIndex)
    {
        logDebugMessage(2,"Player %s [%d] shot playerid mismatch\n", shooter.getCallSign(),
                        playerIndex);
        return;
    }

    if (!playerData->updateShot(shot.id & 0xff, shot.id >> 8))
        return;

    uint32_t shotGUID = ShotManager.FindShotGUID(playerIndex,shot.id & 0xff);
    ShotManager.SetShotTarget(shotGUID,targetId);

    // TODO, Remove this and let the GM update logic send the updates,
    broadcastMessage(MsgGMUpdate, len, buf);
}

static void shotFired(int playerIndex, void *buf, int len)
{
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (!playerData)
        return;

    bool repack = false;
    const PlayerInfo &shooter = playerData->player;
    if (!shooter.isAlive() || shooter.isObserver())
        return;
    FiringInfo firingInfo;
    firingInfo.unpack(buf);
    const ShotUpdate &shot = firingInfo.shot;

    // verify playerId
    if (shot.player != playerIndex)
    {
        logDebugMessage(2,"Player %s [%d] shot playerid mismatch\n", shooter.getCallSign(),
                        playerIndex);
        return;
    }

    // make sure the shooter flag is a valid index to prevent segfaulting later
    if (!shooter.haveFlag())
    {
        firingInfo.flagType = Flags::Null;
        repack = true;
    }

    float shotSpeed = BZDB.eval(StateDatabase::BZDB_SHOTSPEED);
    auto fInfo = FlagInfo::get(shooter.getFlag());
    // verify player flag
    if ((firingInfo.flagType != Flags::Null)
            && (firingInfo.flagType != fInfo->flag.type))
    {
        std::string fireFlag = "unknown";
        std::string holdFlag = "unknown";
        if (firingInfo.flagType)
            fireFlag = firingInfo.flagType->flagAbbv;
        if (fInfo->flag.type)
        {
            if (fInfo->flag.type == Flags::Null)
                holdFlag = "none";
            else
                holdFlag = fInfo->flag.type->flagAbbv;
        }

        // probably a cheater using wrong shots.. exception for thief since they steal someone elses
        if (firingInfo.flagType != Flags::Thief && checkShotMismatch)
        {
            // bye bye supposed cheater
            logDebugMessage(1,"Kicking Player %s [%d] Player using wrong shots\n", shooter.getCallSign(), playerIndex);
            sendMessage(ServerPlayer, playerIndex, "Autokick: Your shots do not to match the expected shot type.");
            removePlayer(playerIndex, "Player shot mismatch");
        }

        logDebugMessage(2,"Player %s [%d] shot flag mismatch %s %s\n", shooter.getCallSign(),
                        playerIndex, fireFlag.c_str(), holdFlag.c_str());
        return;
    }

    if (shooter.haveFlag())
        firingInfo.flagType = fInfo->flag.type;
    else
        firingInfo.flagType = Flags::Null;

    if (!playerData->addShot(shot.id & 0xff, shot.id >> 8, firingInfo))
        return;

    const float maxTankSpeed  = BZDBCache::tankSpeed;
    const float tankSpeedMult = BZDB.eval(StateDatabase::BZDB_VELOCITYAD);
    float tankSpeed       = maxTankSpeed;
    float lifetime        = BZDB.eval(StateDatabase::BZDB_RELOADTIME);
    if (handicapAllowed())
    {
        tankSpeed *= BZDB.eval(StateDatabase::BZDB_HANDICAPVELAD);
        shotSpeed *= BZDB.eval(StateDatabase::BZDB_HANDICAPSHOTAD);
    }
    if (firingInfo.flagType == Flags::ShockWave)
    {
        shotSpeed = 0.0f;
        tankSpeed = 0.0f;
    }
    else if (firingInfo.flagType == Flags::Velocity)
        tankSpeed *= tankSpeedMult;
    else if (firingInfo.flagType == Flags::Thief)
        tankSpeed *= BZDB.eval(StateDatabase::BZDB_THIEFVELAD);
    else if ((firingInfo.flagType == Flags::Burrow)
             && (firingInfo.shot.pos[2] < BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT)))
        tankSpeed *= BZDB.eval(StateDatabase::BZDB_BURROWSPEEDAD);
    else if (firingInfo.flagType == Flags::Agility)
        tankSpeed *= BZDB.eval(StateDatabase::BZDB_AGILITYADVEL);
    else
    {
        //If shot is different height than player, can't be sure they didn't drop V in air
        if (playerData->lastState.pos[2]
                != (shot.pos[2]-BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT)))
            tankSpeed *= tankSpeedMult;
    }

    // FIXME, we should look at the actual TankSpeed ;-)
    shotSpeed += tankSpeed;

    // verify lifetime
    if (fabs(firingInfo.lifetime - lifetime) > Epsilon)
    {
        logDebugMessage(2,"Player %s [%d] shot lifetime mismatch %f %f\n",
                        shooter.getCallSign(),
                        playerIndex, firingInfo.lifetime, lifetime);
        return;
    }

    if (doSpeedChecks)
    {
        // verify velocity
        if (hypotf(shot.vel[0], hypotf(shot.vel[1], shot.vel[2])) > shotSpeed * 1.01f)
        {
            logDebugMessage(2,"Player %s [%d] shot over speed %f %f\n", shooter.getCallSign(),
                            playerIndex, hypotf(shot.vel[0], hypotf(shot.vel[1], shot.vel[2])),
                            shotSpeed);
            return;
        }

        // verify position
        float muzzleFront = BZDB.eval(StateDatabase::BZDB_MUZZLEFRONT);
        float muzzleHeight = BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
        if (firingInfo.flagType == Flags::Obesity)
            muzzleFront *= BZDB.eval(StateDatabase::BZDB_OBESEFACTOR);
        const PlayerState &last = playerData->lastState;
        float dx = last.pos[0] - shot.pos[0];
        float dy = last.pos[1] - shot.pos[1];
        float dz = last.pos[2] + muzzleHeight - shot.pos[2];

        // ignore z error for falling tanks
        if (last.status & PlayerState::Falling)
            dz = 0.0f;
        float delta = dx*dx + dy*dy + dz*dz;
        if (delta > (maxTankSpeed * tankSpeedMult + 2.0f * muzzleFront) *
                (maxTankSpeed * tankSpeedMult + 2.0f * muzzleFront))
        {
            logDebugMessage(2,"Player %s [%d] shot origination %f %f %f too far from tank %f %f %f: distance=%f\n",
                            shooter.getCallSign(), playerIndex,
                            shot.pos[0], shot.pos[1], shot.pos[2],
                            last.pos[0], last.pos[1], last.pos[2], sqrt(delta));
            return;
        }
    }

    // ask the API if it wants to modify this shot
    bz_ShotFiredEventData_V1 shotEvent;

    shotEvent.pos[0] = shot.pos[0];
    shotEvent.pos[1] = shot.pos[1];
    shotEvent.pos[2] = shot.pos[2];

    shotEvent.vel[0] = shot.vel[0];
    shotEvent.vel[1] = shot.vel[1];
    shotEvent.vel[2] = shot.vel[2];

    shotEvent.playerID = shooter.getPlayerIndex();

    shotEvent.shotID = firingInfo.shot.id;

    shotEvent.type = firingInfo.flagType->flagAbbv;

    worldEventManager.callEvents(bz_eShotFiredEvent,&shotEvent);

    if (shotEvent.changed)
    {
        if (shotEvent.type == "DELETE")
            return;
        firingInfo.flagType = Flag::getDescFromAbbreviation(shotEvent.type.c_str());
        repack = true;
    }

    // repack if changed
    if (repack)
    {
        void *bufStart = getDirectMessageBuffer();
        firingInfo.pack(bufStart);
        buf = bufStart;
    }


    // if shooter has a flag

    char message[MessageLen];
    if (shooter.haveFlag())
    {

        fInfo->numShots++; // increase the # shots fired

        int limit = clOptions->flagLimit[fInfo->flag.type];
        if (limit != -1)   // if there is a limit for players flag
        {
            int shotsLeft = limit -  fInfo->numShots;
            if (shotsLeft > 0)   //still have some shots left
            {
                // give message each shot below 5, each 5th shot & at start
                if (shotsLeft % 5 == 0 || shotsLeft <= 3 || shotsLeft == limit-1)
                {
                    if (shotsLeft > 1)
                        snprintf(message, MessageLen, "%d shots left",shotsLeft);
                    else
                        strcpy(message,"1 shot left");
                    sendMessage(ServerPlayer, playerIndex, message);
                }
            }
            else     // no shots left
            {
                if (shotsLeft == 0 || (limit == 0 && shotsLeft < 0))
                {
                    // drop flag at last known position of player
                    // also handle case where limit was set to 0
                    const auto &lastPos = playerData->lastState.pos;
                    fInfo->grabs = 0; // recycle this flag now
                    dropFlag(*fInfo, lastPos);
                }
                else     // more shots fired than allowed
                {
                    // do nothing for now -- could return and not allow shot
                }
            } // end no shots left
        } // end is limit
    } // end of player has flag

    if (firingInfo.flagType == Flags::GuidedMissile)
        playerData->player.endShotCredit--;

    ShotManager.AddShot(firingInfo,playerData->getIndex());

    broadcastMessage(MsgShotBegin, len, buf);

}

static void shotEnded(const PlayerId& id, int16_t shotIndex, uint16_t reason)
{
    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(id);

    if (!playerData && id != ServerPlayer)
        return;

    if (id != ServerPlayer)
        playerData->removeShot(shotIndex & 0xff, shotIndex >> 8);

    ShotManager.RemoveShot(ShotManager.FindShotGUID(id,shotIndex & 0xff));

    // shot has ended prematurely -- send MsgShotEnd
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, id);
    buf = nboPackShort(buf, shotIndex);
    buf = nboPackUShort(buf, reason);
    broadcastMessage(MsgShotEnd, (char*)buf-(char*)bufStart, bufStart);

    bz_ShotEndedEventData_V1 shotEvent;
    shotEvent.playerID = (int)id;
    shotEvent.shotID = shotIndex;
    shotEvent.explode = reason == 0;
    worldEventManager.callEvents(bz_eShotEndedEvent,&shotEvent);
}

static void sendTeleport(int playerIndex, uint16_t from, uint16_t to)
{
    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, playerIndex);
    buf = nboPackUShort(buf, from);
    buf = nboPackUShort(buf, to);
    broadcastMessage(MsgTeleport, (char*)buf-(char*)bufStart, bufStart);
}


/* Players who are paused or have never spawned and observers should not be
 * sending updates. Don't bother to kick observers who try and fail to cheat.
 */
static bool invalidPlayerAction(PlayerInfo &p, int t, const char *action)
{
    const char *state = NULL;
    if (p.isObserver())
        state = "as an observer";
    else if (p.isPaused())
    {
        if (strcmp(action, "die") != 0) // allow self destruct while paused
            state = "while paused";
    }
    else if (p.hasNeverSpawned())
        state = "before spawning";
    if (state)
    {
        logDebugMessage(1,"Player %s [%d] tried to %s %s\n", p.getCallSign(), t, action, state);
        char buffer[MessageLen];
        snprintf(buffer, MessageLen, "Autokick: Looks like you tried to %s %s.", action, state);
        sendMessage(ServerPlayer, t, buffer);
        snprintf(buffer, MessageLen, "Invalid attempt to %s %s", action, state);
        removePlayer(t, buffer);
        return true;
    }
    return false;
}


static void lagKick(int playerIndex)
{
    char message[MessageLen];
    snprintf(message, MessageLen,
             "You have been kicked due to excessive lag (you have been warned %d times).",
             clOptions->maxlagwarn);
    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (playerData != NULL)
    {
        sendMessage(ServerPlayer, playerIndex, message);
        snprintf(message, MessageLen,"Lagkick: %s", playerData->player.getCallSign());
        sendMessage(ServerPlayer, AdminPlayers, message);
        removePlayer(playerIndex, "lag");
    }
}

static void jitterKick(int playerIndex)
{
    char message[MessageLen];
    snprintf(message, MessageLen,
             "You have been kicked due to excessive jitter"
             " (you have been warned %d times).",
             clOptions->maxjitterwarn);
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (playerData != NULL)
    {
        sendMessage(ServerPlayer, playerIndex, message);
        snprintf(message, MessageLen,"Jitterkick: %s",
                 playerData->player.getCallSign());
        sendMessage(ServerPlayer, AdminPlayers, message);
        removePlayer(playerIndex, "jitter", true);
    }
}

void packetLossKick(int playerIndex)
{
    char message[MessageLen];
    snprintf(message, MessageLen,
             "You have been kicked due to excessive packetloss (you have been warned %d times).",
             clOptions->maxpacketlosswarn);
    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerIndex);
    if (playerData != NULL)
    {
        sendMessage(ServerPlayer, playerIndex, message);
        snprintf(message, MessageLen,"Packetlosskick: %s",
                 playerData->player.getCallSign());
        sendMessage(ServerPlayer, AdminPlayers, message);
        removePlayer(playerIndex, "packetloss", true);
    }
}

static void adjustTolerances()
{
    // check for handicap adjustment
    if (handicapAllowed())
    {
        const float speedAdj = BZDB.eval(StateDatabase::BZDB_HANDICAPVELAD);
        speedTolerance *= speedAdj * speedAdj;
    }

    // check for physics driver disabling
    disableHeightChecks = BZDB.isTrue("_disableHeightChecks");
    bool disableSpeedChecks = BZDB.isTrue("_disableSpeedChecks");
    int i = 0;
    const PhysicsDriver* phydrv = PHYDRVMGR.getDriver(i);
    while (phydrv)
    {
        const float* v = phydrv->getLinearVel();
        const float av = phydrv->getAngularVel();
        if (!phydrv->getIsDeath())
        {
            if (!phydrv->getIsSlide() &&
                    ((v[0] != 0.0f) || (v[1] != 0.0f) || (av != 0.0f)))
                disableSpeedChecks = true;
            if (v[2] > 0.0f)
                disableHeightChecks = true;
        }
        i++;
        phydrv = PHYDRVMGR.getDriver(i);
    }


    if (disableSpeedChecks)
    {
        doSpeedChecks = false;
        speedTolerance = MAXFLOAT;
        logDebugMessage(1,"Warning: disabling speed checking due to physics drivers\n");
    }
    if (disableHeightChecks)
        logDebugMessage(1,"Warning: disabling height checking due to physics drivers\n");

    return;
}


bool isSpamOrGarbage(char* message, GameKeeper::Player* playerData, int t)
{
    // Shortcut to the player info
    PlayerInfo &player = playerData->player;

    // Grab the length of the raw message
    const int totalChars = strlen(message);

    // Count visible and bad characters
    int badChars = 0;
    int visibleChars = 0;
    for (int i=0; i < totalChars; i++)
    {
        // Is it a visible character?
        if (TextUtils::isVisible(message[i]))
            visibleChars++;
        // Not visible? Then is it something other than a space?
        else if (message[i] != 32)
            badChars++;
    }

    // Kick the player if any bad characters are found
    if (badChars > 0)
    {
        sendMessage(ServerPlayer, t, "You were kicked because of a garbage message.");
        logDebugMessage(2,"Kicking player %s [%d] for sending a garbage message: %d disallowed chars\n",
                        player.getCallSign(), t, badChars);
        removePlayer(t, "garbage");

        // Ignore garbage message
        return true;
    }

    // Ignore message if there are no visible characters
    if (visibleChars == 0)
        return true;

    // Get last message and last message time
    const std::string &oldMsg = player.getLastMsg();
    float dt = (float)(TimeKeeper::getCurrent() - player.getLastMsgTime());

    // Ignore whitespace
    std::string newMsg = TextUtils::no_whitespace(message);

    // if it's first message, or enough time since last message - can't
    // be spam yet
    if (oldMsg.length() > 0 && dt < clOptions->msgTimer)
    {
        // might be spam, start doing comparisons
        // does it match the last message? (disregarding whitespace and case)
        if (TextUtils::compare_nocase(newMsg, oldMsg) == 0)
        {
            player.incSpamWarns();
            sendMessage(ServerPlayer, t, "***Server Warning: Please do not spam.");

            // has this player already had his share of warnings?
            if (player.getSpamWarns() > clOptions->spamWarnMax
                    || clOptions->spamWarnMax == 0)
            {
                sendMessage(ServerPlayer, t, "You were kicked because of spamming.");
                logDebugMessage(2,"Kicking player %s [%d] for spamming too much: "
                                "2 messages sent within %fs after %d warnings\n",
                                player.getCallSign(), t, dt, player.getSpamWarns());
                removePlayer(t, "spam");
                return true;
            }
        }
    }

    // record this message for next time
    player.setLastMsg(newMsg);
    return false;
}

float squareAndAdd ( float v1, float v2 )
{
    return v1*v1+v2*v2;
}

static bool isTeleporterMotion ( GameKeeper::Player &playerData, PlayerState &state, float maxSquaredMovement,
                                 float realDist2 )
{
    const ObstacleList &teleporterList = OBSTACLEMGR.getTeles();
    unsigned int i;
    unsigned int maxTele = teleporterList.size();
    float finalDist2   = realDist2;
    float initialDist2   = realDist2;

    // find the porter they WERE close to, and the porter they ARE close too
    // and save off the distances from each.
    for (i = 0; i < maxTele; i++)
    {
        Obstacle *currentTele = teleporterList[i];
        const auto &telePosition = currentTele->getPosition();
        float deltaInitial2 = squareAndAdd(state.pos[0] - telePosition[0],state.pos[1] - telePosition[1]);
        float deltaFinal2 = squareAndAdd(playerData.lastState.pos[0] - telePosition[0],
                                         playerData.lastState.pos[1] - telePosition[1]);

        if (deltaInitial2 < initialDist2)
            initialDist2 = deltaInitial2;

        if (deltaFinal2 < finalDist2)
            finalDist2 = deltaFinal2;
    }

    // if the distance from where you were, to your probable entry porter
    // plus the distance from where you are to your probable exit porter
    // is greater then the max move, your a cheater.
    if (sqrt(initialDist2) + sqrt(finalDist2) <= sqrt(maxSquaredMovement))
        return false;

    return true;
}

// check the distance to see if they went WAY too far
static bool isCheatingMovement(GameKeeper::Player &playerData, PlayerState &state, float maxMovement, int t)
{
    // easy out, if we arn't doing distance checks.
    if (!BZDB.isTrue("_enableDistanceCheck"))
        return false;

    float movementDelta[2];
    movementDelta[0] = state.pos[0] - playerData.lastState.pos[0];
    movementDelta[1] = state.pos[1] - playerData.lastState.pos[1];

    float realDist2 = squareAndAdd(movementDelta[0], movementDelta[1]);
    logDebugMessage(4,"isCheatingMovement: dist %f, maxDist %f\n",sqrt(realDist2),maxMovement);

    // if the movement is less then the max, they are cool
    if (realDist2 <= (maxMovement*maxMovement))
        return false;

    bool kickem = !isTeleporterMotion(playerData, state, maxMovement * maxMovement, realDist2);

    if (kickem)
        logDebugMessage(1,"Kicking Player %s [%d] too large movement (tank: %f, allowed: %f)\n",
                        playerData.player.getCallSign(),t,sqrt(realDist2),maxMovement);
    return kickem;
}

static void handleCommand(int t, void *rawbuf, bool udp)
{
    if (!rawbuf)
    {
        std::cerr << "WARNING: handleCommand got a null rawbuf?!" << std::endl;
        return;
    }

    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(t);
    if (!playerData)
        return;
    NetHandler *handler = playerData->netHandler;

    uint16_t len, code;
    const void *buf = rawbuf;
    buf = nboUnpackUShort(buf, len);
    buf = nboUnpackUShort(buf, code);
    char buffer[MessageLen];

    if (udp)
    {
        switch (code)
        {
        case MsgShotBegin:
        case MsgShotEnd:
        case MsgPlayerUpdate:
        case MsgPlayerUpdateSmall:
        case MsgGMUpdate:
        case MsgUDPLinkRequest:
        case MsgUDPLinkEstablished:
            break;
        default:
            logDebugMessage(1,"Player [%d] sent packet type (%x) via udp, "
                            "possible attack from %s\n",
                            t, code, handler->getTargetIP());
            return;
        }
    }

    if (!playerData->player.isCompletelyAdded())
    {
        switch (code)
        {
        case MsgEnter:
        case MsgQueryGame:
        case MsgQueryPlayers:
        case MsgWantWHash:
        case MsgNegotiateFlags:
        case MsgGetWorld:
        case MsgUDPLinkRequest:
        case MsgUDPLinkEstablished:
        case MsgWantSettings:
        case MsgExit:
        case MsgAlive:
        case MsgAutoPilot:
            break;

        case MsgMessage:
        case MsgPlayerUpdateSmall:
            // FIXME: this is a workaround for a protocol problem
            logDebugMessage(4,"Ignoring premature message 0x%4hx from host %s\n",code,handler->getTargetIP());
            return;

        default:
            logDebugMessage(1,"Host %s tried to send invalid message before Enter; 0x%4hx\n",handler->getTargetIP(),code);
            rejectPlayer(t, RejectBadRequest, "invalid request");
            return;
        }
    }

    if (!playerData->hadEnter)
    {
        switch (code)
        {
        case MsgExit:
        case MsgAlive:
        case MsgAutoPilot:
        case MsgMessage:
        case MsgPlayerUpdateSmall:
            logDebugMessage(1, "Host %s tried to send invalid message before Enter; 0x%4hx\n", handler->getTargetIP(), code);
            rejectPlayer(t, RejectBadRequest, "invalid request");
            return;
        }
    }

    bz_MsgDebugEventData_V1 debugEventData;
    debugEventData.code[0] = ((char*)&code)[0];
    debugEventData.code[1] = ((char*)&code)[1];
    debugEventData.len = (size_t)len;
    debugEventData.msg = (unsigned char*)rawbuf + 2 * sizeof(uint16_t);
    debugEventData.playerID = playerData->getIndex();

    worldEventManager.callEvents(&debugEventData);

    switch (code)
    {
    // player joining
    case MsgEnter:
    {
        // a previous MsgEnter will have set the name a few lines down from here
        if (!playerData->accessInfo.getName().empty() && playerData->hasEntered)
        {
            logDebugMessage(1,"Player %s [%d] sent another MsgEnter\n",
                            playerData->player.getCallSign(), t);
            rejectPlayer(t, RejectBadRequest, "invalid request");
            break;
        }
        uint16_t rejectCode;
        char     rejectMsg[MessageLen];
        if (!playerData->player.unpackEnter(buf, rejectCode, rejectMsg))
        {
            rejectPlayer(t, rejectCode, rejectMsg);
            break;
        }
        playerData->hasEntered = true;
        playerData->accessInfo.setName(playerData->player.getCallSign());
        std::string timeStamp = TimeKeeper::timestamp();
        logDebugMessage(1,"Player %s [%d] has joined from %s at %s with token \"%s\"\n",
                        playerData->player.getCallSign(),
                        t, handler->getTargetIP(), timeStamp.c_str(),
                        playerData->player.getToken());

        if (!clOptions->publicizeServer)
            playerData->_LSAState = GameKeeper::Player::notRequired;
        else if (strlen(playerData->player.getCallSign()))
            playerData->_LSAState = GameKeeper::Player::required;
        playerData->hadEnter = true;
        dontWait = true;
        break;
    }

    // player closing connection
    case MsgExit:
        // data: <none>
        removePlayer(t, "left", false);
        break;

    case MsgNegotiateFlags:
    {
        FlagTypeMap::iterator it;
        FlagSet::iterator m_it;
        FlagOptionMap hasFlag;
        FlagSet missingFlags;
        unsigned short numClientFlags = len/2;

        /* Unpack incoming message containing the list of flags our client supports */
        for (int i = 0; i < numClientFlags; i++)
        {
            FlagType *fDesc;
            buf = FlagType::unpack(buf, fDesc);
            if (fDesc != Flags::Null)
                hasFlag[fDesc] = true;
        }

        /* Compare them to the flags this game might need, generating a list of missing flags */
        for (it = FlagType::getFlagMap().begin();
                it != FlagType::getFlagMap().end(); ++it)
        {
            if (!hasFlag[it->second])
            {
                if (clOptions->flagCount[it->second] > 0)
                    missingFlags.insert(it->second);
                if ((clOptions->numExtraFlags > 0) && !clOptions->flagDisallowed[it->second])
                    missingFlags.insert(it->second);
            }
        }

        /* Pack a message with the list of missing flags */
        char *bufStart = getDirectMessageBuffer();
        void *tmpbuf = bufStart;
        for (m_it = missingFlags.begin(); m_it != missingFlags.end(); ++m_it)
        {
            if ((*m_it) != Flags::Null)
            {
                if ((*m_it)->custom)
                {
                    // custom flag, tell the client about it
                    static char cfbuffer[MaxPacketLen];
                    char *cfbufStart = &cfbuffer[0] + 2 * sizeof(uint16_t);
                    char *cfbuf = cfbufStart;
                    cfbuf = (char*)(*m_it)->packCustom(cfbuf);
                    directMessage(t, MsgFlagType, cfbuf-cfbufStart, cfbufStart);
                }
                else
                {
                    // they should already know about this one, dump it back to them
                    tmpbuf = (*m_it)->pack(tmpbuf);
                }
            }
        }
        directMessage(t, MsgNegotiateFlags, (char*)tmpbuf-bufStart, bufStart);
        break;
    }



    // player wants more of world database
    case MsgGetWorld:
    {
        // data: count (bytes read so far)
        uint32_t ptr;
        buf = nboUnpackUInt(buf, ptr);
        sendWorld(t, ptr);
        break;
    }

    case MsgWantSettings:
    {
        sendGameSettings(t);
        break;
    }

    case MsgWantWHash:
    {
        void *obuf, *obufStart = getDirectMessageBuffer();
        if (clOptions->cacheURL.size() > 0)
        {
            obuf = nboPackString(obufStart, clOptions->cacheURL.c_str(),
                                 clOptions->cacheURL.size() + 1);
            directMessage(t, MsgCacheURL, (char*)obuf-(char*)obufStart, obufStart);
        }
        obuf = nboPackString(obufStart, hexDigest.c_str(), hexDigest.size() + 1);
        directMessage(t, MsgWantWHash, (char*)obuf-(char*)obufStart, obufStart);
        break;
    }

    case MsgQueryGame:
        sendQueryGame(t);
        break;

    case MsgQueryPlayers:
        sendQueryPlayers(t);
        break;

    // player is coming alive
    case MsgAlive:
    {
        // player is on the waiting list
        float waitTime = rejoinList.waitTime(t);
        if (waitTime > 0.0f)
        {
            snprintf (buffer, MessageLen, "You are unable to begin playing for %.1f seconds.", waitTime);
            sendMessage(ServerPlayer, t, buffer);

            // Make them wait for trying to rejoin quickly
            playerData->player.setSpawnDelay((double)waitTime);
            playerData->player.queueSpawn();

            break;
        }

        // player moved before countdown started
        if (clOptions->timeLimit>0.0f && !countdownActive)
            playerData->player.setPlayedEarly();
        playerData->player.queueSpawn();
        break;
    }

    // player declaring self destroyed
    case MsgKilled:
    {
        if (invalidPlayerAction(playerData->player, t, "die"))
            break;
        // data: id of killer, shot id of killer
        PlayerId killer;
        FlagType* flagType;
        int16_t shot, reason;
        int phydrv = -1;
        buf = nboUnpackUByte(buf, killer);
        buf = nboUnpackShort(buf, reason);
        buf = nboUnpackShort(buf, shot);
        buf = FlagType::unpack(buf, flagType);
        if (reason == PhysicsDriverDeath)
        {
            int32_t inPhyDrv;
            buf = nboUnpackInt(buf, inPhyDrv);
            phydrv = int(inPhyDrv);
        }

        // Sanity check on shot: Here we have the killer
        if (killer != ServerPlayer)
        {
            int si = (shot == -1 ? -1 : shot & 0x00FF);
            if ((si < -1) || (si >= clOptions->maxShots))
                break;
        }
        playerData->player.endShotCredit--;
        playerKilled(t, lookupPlayer(killer), reason, shot, flagType, phydrv);

        break;
    }

    // player requesting to grab flag
    case MsgGrabFlag:
    {
        // data: flag index
        uint16_t flag;

        if (invalidPlayerAction(playerData->player, t, "grab a flag"))
            break;

        buf = nboUnpackUShort(buf, flag);
        // Sanity check
        if (flag < numFlags)
            grabFlag(t, *FlagInfo::get(flag), true);
        break;
    }

    // player requesting to drop flag
    case MsgDropFlag:
    {
        // data: position of drop
        glm::vec3 pos;
        buf = nboUnpackVector(buf, pos);
        dropPlayerFlag(*playerData, pos);
        break;
    }

    // player has captured a flag
    case MsgCaptureFlag:
    {
        // data: team whose territory flag was brought to
        uint16_t _team;

        if (invalidPlayerAction(playerData->player, t, "capture a flag"))
            break;

        buf = nboUnpackUShort(buf, _team);
        captureFlag(t, TeamColor(_team));
        break;
    }

    // shot fired
    case MsgShotBegin:
        if (invalidPlayerAction(playerData->player, t, "shoot"))
            break;

        // Sanity check
        if (len == FiringInfoPLen)
            shotFired(t, const_cast<void *>(buf), int(len));
        break;

    // shot ended prematurely
    case MsgShotEnd:
    {
        if (invalidPlayerAction(playerData->player, t, "end a shot"))
            break;

        // endShot anti-cheat
        playerData->player.endShotCredit++;

        int pFlag = playerData->player.getFlag();
        if (pFlag >= 0)
        {
            FlagInfo &flag = *FlagInfo::get(pFlag);
            if (flag.flag.type == Flags::Shield)
                playerData->player.endShotShieldCredit = 1;
        }

        const int endShotLimit =  (int) BZDB.eval(StateDatabase::BZDB_ENDSHOTDETECTION);
        if ((BZDB.isTrue(StateDatabase::BZDB_ENDSHOTDETECTION) && endShotLimit > 0) &&
                (playerData->player.endShotCredit > endShotLimit))    // default endShotLimit 2
        {
            char testmessage[MessageLen];
            snprintf(testmessage, MessageLen, "Kicking Player %s EndShot credit: %d \n",
                     playerData->player.getCallSign(), playerData->player.endShotCredit );
            logDebugMessage(1,"endShot Detection: %s\n", testmessage);
            sendMessage(ServerPlayer, AdminPlayers, testmessage);
            sendMessage(ServerPlayer, t, "Autokick: wrong end shots detected.");
            removePlayer(t, "EndShot");
        }
        // endShotDetection finished

        // data: shooter id, shot number, reason
        PlayerId sourcePlayer;
        int16_t shot;
        uint16_t reason;
        buf = nboUnpackUByte(buf, sourcePlayer);
        buf = nboUnpackShort(buf, shot);
        buf = nboUnpackUShort(buf, reason);
        shotEnded(sourcePlayer, shot, reason);

        break;
    }

    // player teleported
    case MsgTeleport:
    {
        uint16_t from, to;

        if (invalidPlayerAction(playerData->player, t, "teleport"))
            break;

        buf = nboUnpackUShort(buf, from);
        buf = nboUnpackUShort(buf, to);

        // Validate the teleport source and destination
        const ObstacleList &teleporterList = OBSTACLEMGR.getTeles();
        unsigned int maxTele = teleporterList.size();

        if (from < maxTele * 2 && to < maxTele * 2)
            sendTeleport(t, from, to);
        else
        {
            logDebugMessage(2,"Player %s [%d] tried to send invalid teleport (from %u to %u)\n",
                            playerData->player.getCallSign(), t, from, to);
        }
        break;
    }

    // player sending a message
    case MsgMessage:
    {
        // data: target player/team, message string
        PlayerId dstPlayer;
        char message[MessageLen];
        buf = nboUnpackUByte(buf, dstPlayer);
        buf = nboUnpackString(buf, message, sizeof(message));
        message[MessageLen - 1] = '\0';
        playerData->player.hasSent();
        if (dstPlayer == AllPlayers)
        {
            logDebugMessage(1,"Player %s [%d] -> All: %s\n", playerData->player.getCallSign(),
                            t, message);
        }
        else if (dstPlayer == AdminPlayers)
        {
            logDebugMessage(1,"Player %s [%d] -> Admin: %s\n",
                            playerData->player.getCallSign(), t, message);
        }
        else if (dstPlayer > LastRealPlayer)
        {
            logDebugMessage(1,"Player %s [%d] -> Team: %s\n",
                            playerData->player.getCallSign(), t, message);
        }
        else
        {
            GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(dstPlayer);
            if (p != NULL)
            {
                logDebugMessage(1,"Player %s [%d] -> Player %s [%d]: %s\n",
                                playerData->player.getCallSign(), t, p->player.getCallSign(), dstPlayer, message);
            }
            else
            {
                logDebugMessage(1,"Player %s [%d] -> Player Unknown [%d]: %s\n",
                                playerData->player.getCallSign(), t, dstPlayer, message);
            }
        }
        // check for spamming or garbage
        if (isSpamOrGarbage(message, playerData, t))
            break;

        bz_ChatEventData_V2 chatData;
        chatData.from = t;
        chatData.to = BZ_NULLUSER;

        if (dstPlayer == AllPlayers)
            chatData.to = BZ_ALLUSERS;
        else if ( dstPlayer == AdminPlayers )
            chatData.team = eAdministrators;
        else if ( dstPlayer > LastRealPlayer )
            chatData.team =convertTeam((TeamColor)(FirstTeam - dstPlayer));
        else
            chatData.to = dstPlayer;


        chatData.messageType = eChatMessage;
        chatData.message = message;

        // send the actual Message after all the callbacks have done their magic to it.
        if (chatData.message.size())
            sendPlayerMessage(playerData, dstPlayer, chatData.message.c_str());
        break;
    }

    // player has transferred flag to another tank
    case MsgTransferFlag:
    {
        PlayerId from, to;

        buf = nboUnpackUByte(buf, from);
        if (from != t)
        {
            logDebugMessage(1,"Kicking Player %s [%d] Player trying to transfer flag\n",
                            playerData->player.getCallSign(), t);
            removePlayer(t, "flag transfer");
            break;
        }
        buf = nboUnpackUByte(buf, to);

        GameKeeper::Player *fromData = playerData;

        int flagIndex = fromData->player.getFlag();

        FlagInfo* flagInfo = FlagInfo::get(flagIndex);

        if (to == ServerPlayer)
        {
            if (flagIndex >= 0)
                zapFlag (*flagInfo);
            return;
        }

        // Sanity check
        if (to >= curMaxPlayers)
            return;

        if (flagIndex == -1)
            return;

        GameKeeper::Player *toData
            = GameKeeper::Player::getPlayerByIndex(to);
        if (!toData)
            return;

        // verify that the player stealing HAS thief
        int toPlayersFlag = toData->player.getFlag();
        bool cheater = false;
        if (!toData->player.haveFlag())
            cheater = true;

        FlagInfo *toFlag = FlagInfo::get(toPlayersFlag);
        // without flag or with a non thief flag? Then is probably cheating
        if (!toFlag || toFlag->flag.type != Flags::Thief)
            cheater = true;

        // TODO, check thief radius here

        if (cheater)
        {
            logDebugMessage(1,"No Thief check %s [%d] Player trying to transfer to target without thief\n",
                            playerData->player.getCallSign(), t);
            // There is a lot of reason why the player could not have thief,
            // network delay is one of this.
            // Don't kick then, just discard the message
            return;
        }

        bz_FlagTransferredEventData_V1 ftEventData;

        ftEventData.fromPlayerID = fromData->player.getPlayerIndex();
        ftEventData.toPlayerID = toData->player.getPlayerIndex();
        if (flagInfo)
            ftEventData.flagType = flagInfo->flag.type->flagAbbv.c_str();
        else
            ftEventData.flagType = "";
        ftEventData.action = ftEventData.ContinueSteal;

        worldEventManager.callEvents(bz_eFlagTransferredEvent,&ftEventData);

        if (ftEventData.action != ftEventData.CancelSteal)
        {
            int oFlagIndex = toData->player.getFlag();
            if (oFlagIndex >= 0)
                zapFlag (*FlagInfo::get(oFlagIndex));
        }

        if (ftEventData.action == ftEventData.ContinueSteal)
        {
            void *obufStart = getDirectMessageBuffer();
            void *obuf = nboPackUByte(obufStart, from);
            obuf = nboPackUByte(obuf, to);
            FlagInfo &flag = *FlagInfo::get(flagIndex);
            flag.flag.owner = to;
            flag.player = to;
            toData->player.resetFlag();
            toData->player.setFlag(flagIndex);
            fromData->player.resetFlag();
            obuf = flag.pack(obuf);
            broadcastMessage(MsgTransferFlag, (char*)obuf - (char*)obufStart,
                             obufStart);
        }
        break;
    }

    case MsgUDPLinkEstablished:
        break;

    case MsgNewRabbit:
    {
        if (clOptions->gameType == RabbitChase)
        {
            if (t == rabbitIndex)
                anointNewRabbit();
        }
        else
        {
            logDebugMessage(1,"Kicking Player %s [%d] Illegal rabbit\n",
                            playerData->player.getCallSign(), t);
            sendMessage(ServerPlayer, t, "Autokick: not a rabbit chase game.");
            removePlayer(t, "Illegal rabbit");
        }
        break;
    }

    case MsgPause:
    {
        uint8_t pause;
        nboUnpackUByte(buf, pause);
        pausePlayer(t, pause != 0);
        break;
    }

    case MsgAutoPilot:
    {
        uint8_t autopilot;
        nboUnpackUByte(buf, autopilot);

        bz_AutoPilotData_V1 autoPilotData;
        autoPilotData.playerID = t;
        autoPilotData.enabled = (autopilot != 0);

        autopilotPlayer(t, autoPilotData.enabled);

        worldEventManager.callEvents(bz_eAutoPilotEvent,&autoPilotData);
        break;
    }

    case MsgLagPing:
    {
        bool warn, kick, jittwarn, jittkick, plosswarn, plosskick, alagannouncewarn, lagannouncewarn;
        playerData->lagInfo.updatePingLag(buf, warn, kick, jittwarn, jittkick, plosswarn, plosskick, alagannouncewarn,
                                          lagannouncewarn);
        if (warn)
        {
            char message[MessageLen];
            snprintf(message, MessageLen, "*** Server Warning: your lag is too high (%d ms) ***",
                     playerData->lagInfo.getLag());
            sendMessage(ServerPlayer, t, message);
            if (kick)
                lagKick(t);
        }
        if (alagannouncewarn)
        {
            char message[MessageLen];
            snprintf(message, MessageLen, "*** Server Warning: player %s has too high lag (%d ms) ***",
                     playerData->player.getCallSign(), playerData->lagInfo.getLag());
            sendMessage(ServerPlayer, AdminPlayers, message);
        }
        if (lagannouncewarn)
        {
            char message[MessageLen];
            snprintf(message, MessageLen, "*** Server Warning:player %s has too high lag (%d ms) ***",
                     playerData->player.getCallSign(), playerData->lagInfo.getLag());
            sendMessage(ServerPlayer, AllPlayers, message);
        }
        if (jittwarn)
        {
            char message[MessageLen];
            snprintf(message, MessageLen,
                     "*** Server Warning: your jitter is too high (%d ms) ***",
                     playerData->lagInfo.getJitter());
            sendMessage(ServerPlayer, t, message);
            if (jittkick)
                jitterKick(t);
        }
        if (plosswarn)
        {
            char message[MessageLen];
            snprintf(message, MessageLen,
                     "*** Server Warning: your packetloss is too high (%d%%) ***",
                     playerData->lagInfo.getLoss());
            sendMessage(ServerPlayer, t, message);
            if (plosskick)
                packetLossKick(t);
        }

        break;
    }

    // player is sending his position/speed (bulk data)
    case MsgPlayerUpdate:
    case MsgPlayerUpdateSmall:
    {
        float timestamp;
        PlayerId id;
        PlayerState state;

        buf = nboUnpackFloat(buf, timestamp);
        buf = nboUnpackUByte(buf, id);
        buf = state.unpack(buf, code);

        // Verify that player update is actually for this player
        // TODO: Remove the player ID from this message so we don't have to check for this.
        if (id != t)
        {
            logDebugMessage(1, "Kicking Player %s [%d] sent player update as player index %d\n",
                            playerData->player.getCallSign(), t, id);
            sendMessage(ServerPlayer, t, "Autokick: Player sent spoofed player update.");
            removePlayer(t, "spoofed update");
            break;
        }

        bz_PlayerUpdateEventData_V1 puEventData;
        playerStateToAPIState(puEventData.lastState,state);

        // observer updates are not relayed
        if (playerData->player.isObserver())
        {
            // skip all of the checks
            playerData->setPlayerState(state, timestamp);
            break;
        }
        // tell the API that they moved.

        playerStateToAPIState(puEventData.state,state);
        puEventData.stateTime = TimeKeeper::getCurrent().getSeconds();
        puEventData.playerID = playerData->getIndex();
        worldEventManager.callEvents(bz_ePlayerUpdateEvent,&puEventData);

        // silently drop old packet
        if (state.order <= playerData->lastState.order)
            break;

        // Don't kick players up to 10 seconds after a world parm has changed,
        TimeKeeper now = TimeKeeper::getCurrent();

        if (now - lastWorldParmChange > 10.0f)
        {

            float maxHeight = getMaxWorldHeight();

            if (!disableHeightChecks && state.pos[2] > maxHeight)
            {
                logDebugMessage(1,"Kicking Player %s [%d] jumped too high [max: %f height: %f]\n",
                                playerData->player.getCallSign(), t, maxHeight, state.pos[2]);
                sendMessage(ServerPlayer, t, "Autokick: Player location was too high.");
                removePlayer(t, "too high");
                break;
            }

            bool InBounds = true;
            // exploding or dead players can do unpredictable things
            if (state.status != PlayerState::Exploding && state.status != PlayerState::DeadStatus)
            {
                // make sure the player is still in the map
                // test all the map bounds + some fudge factor, just in case
                static const float positionFudge = 10.0f; /* linear distance */
                float worldSize = BZDBCache::worldSize;
                if ( (state.pos[1] >= worldSize*0.5f + positionFudge) || (state.pos[1] <= -worldSize*0.5f - positionFudge))
                {
                    logDebugMessage(2,"Player %s [%d] y position %.2f is out of bounds (%.2f + %.2f)\n",
                                    playerData->player.getCallSign(), t, state.pos[1], worldSize * 0.5f, positionFudge);
                    InBounds = false;
                }
                else if ( (state.pos[0] >= worldSize*0.5f + positionFudge) || (state.pos[0] <= -worldSize*0.5f - positionFudge))
                {
                    logDebugMessage(2,"Player %s [%d] x position %.2f is out of bounds (%.2f + %.2f)\n",
                                    playerData->player.getCallSign(), t, state.pos[0], worldSize * 0.5f, positionFudge);
                    InBounds = false;
                }
            }

            static const float burrowFudge = 1.0f; /* linear distance */
            if (state.pos[2]<BZDB.eval(StateDatabase::BZDB_BURROWDEPTH) - burrowFudge)
            {
                logDebugMessage(2,"Player %s [%d] z depth %.2f is less than burrow depth (%.2f + %.2f)\n",
                                playerData->player.getCallSign(), t, state.pos[2], BZDB.eval(StateDatabase::BZDB_BURROWDEPTH), burrowFudge);
                InBounds = false;
            }

            // kick em cus they are most likely cheating or using a buggy client
            if (!InBounds)
            {
                logDebugMessage(1,"Kicking Player %s [%d] Out of map bounds at position (%.2f,%.2f,%.2f)\n",
                                playerData->player.getCallSign(), t,
                                state.pos[0], state.pos[1], state.pos[2]);
                sendMessage(ServerPlayer, t, "Autokick: Player location was outside the playing area.");
                removePlayer(t, "Out of map bounds");
            }

            // Speed problems occur around flag drops, so don't check for
            // a short period of time after player drops a flag. Currently
            // 2 second, adjust as needed.
            if (playerData->player.isFlagTransitSafe())
            {

                // we'll be checking against the player's flag type
                int pFlag = playerData->player.getFlag();

                // check for highspeed cheat; if inertia is enabled, skip test for now
                if (clOptions->linearAcceleration == 0.0f)
                {
                    // Doesn't account for going fast backwards, or jumping/falling
                    float curPlanarSpeedSqr = state.velocity[0]*state.velocity[0] +
                                              state.velocity[1]*state.velocity[1];

                    float maxPlanarSpeed = BZDBCache::tankSpeed;

                    bool logOnly = false;
                    if (BZDB.isTrue("_speedChecksLogOnly"))
                        logOnly = true;

                    // if tank is not driving cannot be sure it didn't toss
                    // (V) in flight

                    // if tank is not alive cannot be sure it didn't just toss
                    // (V)
                    if (pFlag >= 0)
                    {
                        FlagInfo &flag = *FlagInfo::get(pFlag);
                        if (flag.flag.type == Flags::Velocity)
                            maxPlanarSpeed *= BZDB.eval(StateDatabase::BZDB_VELOCITYAD);
                        else if (flag.flag.type == Flags::Thief)
                            maxPlanarSpeed *= BZDB.eval(StateDatabase::BZDB_THIEFVELAD);
                        else if (flag.flag.type == Flags::Agility)
                            maxPlanarSpeed *= BZDB.eval(StateDatabase::BZDB_AGILITYADVEL);
                        else if ((flag.flag.type == Flags::Burrow) &&
                                 (playerData->lastState.pos[2] == state.pos[2]) &&
                                 (playerData->lastState.velocity[2] == state.velocity[2]) &&
                                 (state.pos[2] <= BZDB.eval(StateDatabase::BZDB_BURROWDEPTH)))
                            // if we have burrow and are not actively burrowing
                            // You may have burrow and still be above ground. Must
                            // check z in ground!!
                            maxPlanarSpeed *= BZDB.eval(StateDatabase::BZDB_BURROWSPEEDAD);
                    }
                    float maxPlanarSpeedSqr = maxPlanarSpeed * maxPlanarSpeed;

                    // If player is moving vertically, or not alive the speed checks
                    // seem to be problematic. If this happens, just log it for now,
                    // but don't actually kick
                    // don't kick if the player is paused, because problems if have V
                    float smallTol = 0.001f;
                    if ((fabs(playerData->lastState.pos[2]-state.pos[2]) > smallTol)
                            ||  (fabs(playerData->lastState.velocity[2]-state.velocity[2])> smallTol)
                            ||  ((state.status & PlayerState::Alive) == 0)
                            ||  (playerData->player.isPaused()))
                        logOnly = true;

                    // allow a 10% tolerance level for speed if -speedtol is not sane
                    if (doSpeedChecks)
                    {
                        float realtol = 1.1f;
                        if (speedTolerance > 1.0f)
                            realtol = speedTolerance;

                        if (realtol < 100.0f )
                        {
                            maxPlanarSpeedSqr *= realtol;
                            maxPlanarSpeed *= realtol;

                            if (curPlanarSpeedSqr > maxPlanarSpeedSqr)
                            {
                                if (logOnly)
                                {
                                    logDebugMessage(1,"Logging Player %s [%d] tank too fast (tank: %f, allowed: %f){Dead or v[z] != 0}\n",
                                                    playerData->player.getCallSign(), t,
                                                    sqrt(curPlanarSpeedSqr), sqrt(maxPlanarSpeedSqr));
                                }
                                else
                                {
                                    logDebugMessage(1,  "Kicking Player %s [%d] tank too fast (tank: %f, allowed: %f)\n",
                                                    playerData->player.getCallSign(), t, sqrt(curPlanarSpeedSqr), sqrt(maxPlanarSpeedSqr));
                                    sendMessage(ServerPlayer, t, "Autokick: Player tank is moving too fast.");
                                    removePlayer(t, "too fast");
                                }
                                break;
                            }


                            // check the distance to see if they went WAY too far
                            // max time since last update
                            float timeDelta = (float)now.getSeconds() - playerData->serverTimeStamp;
                            if (timeDelta < 0.005f)
                                timeDelta = 0.005f;

                            // the maximum distance they could have moved (assume 0 lag)
                            float maxDist = maxPlanarSpeed * timeDelta;
                            logDebugMessage(4,"Speed log, max %f, time %f, dist %f\n",maxPlanarSpeed,timeDelta,maxDist);

                            if (isCheatingMovement(*playerData, state, maxDist, t) && !logOnly)
                            {
                                sendMessage(ServerPlayer, t, "Autokick: Player tank is moving too fast.");
                                removePlayer(t, "too fast");
                            }
                        }
                    }
                }
            }
        }

        playerData->setPlayerState(state, timestamp);

        // Player might already be dead and did not know it yet (e.g. teamkill)
        // do not propogate
        if (!playerData->player.isAlive() &&
                (state.status & short(PlayerState::Alive)))
            break;

        // observer shouldn't send bulk messages anymore, they used to
        // when it was a server-only hack; but the check does not hurt,
        // either
        if (playerData->player.isObserver())
            break;

        searchFlag(*playerData);

        relayPlayerPacket(t, len, rawbuf, code);
        break;
    }

    case MsgGMUpdate:
        shotUpdate(t, const_cast<void *>(buf), int(len));
        break;

    // FIXME handled inside uread, but not discarded
    case MsgUDPLinkRequest:
        break;

    // unknown msg type
    default:
        logDebugMessage(1,"Player [%d] sent unknown packet type (%x), possible attack from %s\n",
                        t, code, handler->getTargetIP());
        removePlayer(t, "Autokick: Sent unknown packet type");
    }
}

static void handleTcp(NetHandler &netPlayer, int i, const RxStatus e)
{
    if (e != ReadAll)
    {
        if (e == ReadReset)
            removePlayer(i, "ECONNRESET/EPIPE", false);
        else if (e == ReadError)
        {
            // dump other errors and remove the player
            nerror("error on read");
            removePlayer(i, "Read error", false);
        }
        else if (e == ReadDiscon)
        {
            // disconnected
            removePlayer(i, "Disconnected", false);
        }
        else if (e == ReadHuge)
            removePlayer(i, "large packet recvd", false);
        return;
    }

    uint16_t len, code;
    const void *buf = netPlayer.getTcpBuffer();
    buf = nboUnpackUShort(buf, len);
    buf = nboUnpackUShort(buf, code);

    // trying to get the real player from the message: bots share tcp
    // connection with the player
    PlayerId t = i;
    switch (code)
    {
    case MsgShotBegin:
    {
        nboUnpackUByte(buf, t);
        break;
    }
    case MsgPlayerUpdate:
    case MsgPlayerUpdateSmall:
    {
        float timestamp;
        buf = nboUnpackFloat(buf, timestamp);
        buf = nboUnpackUByte(buf, t);
        break;
    }
    default:
        break;
    }
    // Make sure is a bot
    GameKeeper::Player *playerData = NULL;
    if (t != i)
    {
        playerData = GameKeeper::Player::getPlayerByIndex(t);
        if (!playerData || !playerData->player.isBot())
        {
            t = i;
            playerData = GameKeeper::Player::getPlayerByIndex(t);
        }
        // Should check also if bot and player are related
    }
    else
        playerData = GameKeeper::Player::getPlayerByIndex(t);

    // simple ruleset, if player sends a MsgShotBegin over TCP he/she
    // must not be using the UDP link
    if (true && playerData != NULL && !playerData->player.isBot())
    {
        if (code == MsgShotBegin)
        {
            char message[MessageLen];
            snprintf(message, MessageLen, "Your end is not using UDP.");
            sendMessage(ServerPlayer, i, message);

            snprintf(message, MessageLen, "Turn on UDP on your firewall or router.");
            sendMessage(ServerPlayer, i, message);

            removePlayer(i, "no UDP");
            return;
        }
    }

    // handle the command
    handleCommand(t, netPlayer.getTcpBuffer(), false);
}

static void terminateServer(int UNUSED(sig))
{
    bzSignal(SIGINT, SIG_PF(terminateServer));
    bzSignal(SIGTERM, SIG_PF(terminateServer));
    exitCode = 0;
    done = true;
}


static std::string cmdSet(const std::string&, const CommandManager::ArgList& args, bool *worked)
{
    if (worked) *worked = true;
    switch (args.size())
    {
    case 2:
        if (BZDB.isSet(args[0]))
        {
            StateDatabase::Permission permission = BZDB.getPermission(args[0]);
            if ((permission == StateDatabase::ReadWrite) || (permission == StateDatabase::Locked))
            {
                BZDB.set(args[0], args[1], StateDatabase::Server);
                lastWorldParmChange = TimeKeeper::getCurrent();
                return args[0] + " set";
            }
            if (worked) *worked = false;
            return "variable " + args[0] + " is not writeable";
        }
        else
        {
            if (worked) *worked = false;
            return "variable " + args[0] + " does not exist";
        }
    case 1:
        if (BZDB.isSet(args[0]))
            return args[0] + " is " + BZDB.get(args[0]);
        else
        {
            if (worked) *worked = false;
            return "variable " + args[0] + " does not exist";
        }
    default:
        if (worked) *worked = false;
        return "usage: set <name> [<value>]";
    }
}

static void resetAllCallback(const std::string &name, void*)
{
    StateDatabase::Permission permission = BZDB.getPermission(name);
    if ((permission == StateDatabase::ReadWrite) || (permission == StateDatabase::Locked))
        BZDB.set(name, BZDB.getDefault(name), StateDatabase::Server);
}

static std::string cmdReset(const std::string&, const CommandManager::ArgList& args, bool* cmdError)
{
    if (cmdError) *cmdError = true;
    if (args.size() == 1)
    {
        if (args[0] == "*")
        {
            BZDB.iterate(resetAllCallback,NULL);
            if (cmdError) *cmdError = false;
            return "all variables reset";
        }
        else if (BZDB.isSet(args[0]))
        {
            StateDatabase::Permission permission = BZDB.getPermission(args[0]);
            if ((permission == StateDatabase::ReadWrite) || (permission == StateDatabase::Locked))
            {
                BZDB.set(args[0], BZDB.getDefault(args[0]), StateDatabase::Server);
                lastWorldParmChange = TimeKeeper::getCurrent();
                if (cmdError) *cmdError = false;
                return args[0] + " reset";
            }
            return "variable " + args[0] + " is not writeable";
        }
        else
            return "variable " + args[0] + " does not exist";
    }
    else
        return "usage: reset <name>";
}

static bool requestAuthentication;

static void doStuffOnPlayer(GameKeeper::Player &playerData)
{
    int p = playerData.getIndex();

    // kick idle players
    if (clOptions->idlekickthresh > 0)
    {
        if ((!playerData.accessInfo.hasPerm(PlayerAccessInfo::antikick)) &&
                (playerData.player.isTooMuchIdling(clOptions->idlekickthresh)))
        {
            char message[MessageLen]
                = "You were kicked because you were idle too long";
            sendMessage(ServerPlayer, p,  message);
            removePlayer(p, "idling");
            logDebugMessage(1,"Kicked player %s [%d] for idling (thresh = %f)\n",
                            playerData.player.getCallSign(), p, clOptions->idlekickthresh);
            return;
        }
    }

    // Check authorization
    if (playerData._LSAState == GameKeeper::Player::required)
    {
        requestAuthentication = true;
        playerData._LSAState = GameKeeper::Player::requesting;
    }
    else if (playerData.netHandler && playerData.netHandler->reverseDNSDone())
    {
        if ((playerData._LSAState == GameKeeper::Player::verified)  ||
                (playerData._LSAState == GameKeeper::Player::timedOut)  ||
                (playerData._LSAState == GameKeeper::Player::failed)    ||
                (playerData._LSAState == GameKeeper::Player::notRequired))
        {
            bz_ServerAddPlayerData_V1 eventData;
            eventData.player = bz_getPlayerByIndex(playerData.getIndex());
            worldEventManager.callEvents(&eventData);
            if (eventData.allow)
                AddPlayer(p, &playerData);
            else
            {
                playerData.addWasDelayed = true;
                playerData.addDelayStartTime = TimeKeeper::getCurrent().getSeconds();
            }
            playerData._LSAState = GameKeeper::Player::done;
        }
    }

    if (playerData.addWasDelayed) // check to see that the API doesn't leave them hanging
    {
        double delta = TimeKeeper::getCurrent().getSeconds() - playerData.addDelayStartTime;
        if (delta > BZDB.eval("_maxPlayerAddDelay"))
            AddPlayer(p, &playerData);
    }

    if (playerData.netHandler)
    {
        // Check for hung player connections that never entered the game completely
        if (!playerData.player.isCompletelyAdded()
                && TimeKeeper::getCurrent() - playerData.netHandler->getTimeAccepted() > 300.0f)
            rejectPlayer(p, RejectBadRequest, "Failed to connect within reasonable timeframe");

        // Check host bans
        const char *hostname = playerData.netHandler->getHostname();
        if (hostname && playerData.needsHostbanChecked())
        {
            if (!playerData.accessInfo.hasPerm(PlayerAccessInfo::antiban))
            {
                HostBanInfo hostInfo("*");
                if (!clOptions->acl.hostValidate(hostname, &hostInfo))
                {
                    std::string reason = "Host banned for: ";

                    if (hostInfo.reason.size())
                        reason += hostInfo.reason;
                    else
                        reason += "General Ban";

                    reason += ColorStrings[WhiteColor];
                    reason += " (";

                    double duration = hostInfo.banEnd - TimeKeeper::getCurrent();
                    if (duration < 365.0f * 24 * 3600)
                    {
                        long int timeArray[4];
                        TimeKeeper::convertTime(duration, timeArray);
                        std::string bantime = TimeKeeper::printTime(timeArray);
                        reason += bantime;
                        reason += " remaining";
                    }
                    else
                        reason += "indefinite";

                    reason += ")";
                    reason += ColorStrings[GreenColor];

                    if (hostInfo.fromMaster)
                        reason += " from the master server";

                    rejectPlayer(p, RejectHostBanned, reason.c_str());
                    return;
                }
            }
            playerData.setNeedThisHostbanChecked(false);
        }
    }

    // update notResponding
    if (playerData.player.hasStartedToNotRespond())
    {
        // if player is the rabbit, anoint a new one
        if (p == rabbitIndex)
        {
            anointNewRabbit();
            // Should recheck if player is still available
            if (!GameKeeper::Player::getPlayerByIndex(p))
                return;
        }
        // if player is holding a flag, drop it
        for (int j = 0; j < numFlags; j++)
        {
            if (FlagInfo::get(j)->player == p)
            {
                dropPlayerFlag(playerData, playerData.lastState.pos);
                // Should recheck if player is still available
                if (!GameKeeper::Player::getPlayerByIndex(p))
                    return;
            }
        }
    }

    // send lag pings
    bool warn;
    bool kick;
    int nextPingSeqno = playerData.lagInfo.getNextPingSeqno(warn, kick);
    if (nextPingSeqno > 0)
    {
        void *buf, *bufStart = getDirectMessageBuffer();
        buf = nboPackUShort(bufStart, nextPingSeqno);
        int result = directMessage(playerData, MsgLagPing,
                                   (char*)buf - (char*)bufStart, bufStart);
        if (result == -1)
            return;
        if (warn)
        {
            char message[MessageLen];
            snprintf(message, MessageLen, "*** Server Warning: your lag is too high (failed to return ping) ***");
            sendMessage(ServerPlayer, p, message);
            // Should recheck if player is still available
            if (!GameKeeper::Player::getPlayerByIndex(p))
                return;
            if (kick)
            {
                lagKick(p);
                return;
            }
        }
    }

    // kick any clients that need to be
    if (playerData.netHandler)
    {
        std::string reasonToKick = playerData.netHandler->reasonToKick();
        if (reasonToKick != "")
        {
            removePlayer(p, reasonToKick.c_str(), false);
            return;
        }
    }
}

void loadBadwordsList ()
{
    if (clOptions->filterFilename.length() != 0)
    {
        if (clOptions->filterChat || clOptions->filterCallsigns)
        {
            if (debugLevel >= 1)
            {
                unsigned int count;
                logDebugMessage(1,"Loading %s\n", clOptions->filterFilename.c_str());
                count = clOptions->filter.loadFromFile(clOptions->filterFilename, true);
                logDebugMessage(1,"Loaded %u words\n", count);
            }
            else
                clOptions->filter.loadFromFile(clOptions->filterFilename, false);
        }
        else
            logDebugMessage(1,"Bad word filter specified without -filterChat or -filterCallsigns\n");
    }
}

void rescanForBans ( bool isOperator, const char* callsign, int playerID )
{
    // Validate all of the current players

    std::string banner = "SERVER";
    if (callsign && strlen(callsign))
        banner = callsign;

    std::string reason;
    char kickmessage[MessageLen];

    // Check host bans
    GameKeeper::Player::setAllNeedHostbanChecked(true);

    // Check IP bans
    for (int i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *otherPlayer = GameKeeper::Player::getPlayerByIndex(i);
        if (otherPlayer && !clOptions->acl.validate(otherPlayer->netHandler->getIPAddress()))
        {
            // operators can override antiperms
            if (!isOperator)
            {
                // make sure this player isn't protected
                GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(i);
                if ((p != NULL) && (p->accessInfo.hasPerm(PlayerAccessInfo::antiban)))
                {
                    if (playerID != -1)
                    {
                        snprintf(kickmessage, MessageLen, "%s is protected from being banned (skipped).", p->player.getCallSign());
                        sendMessage(ServerPlayer, playerID, kickmessage);
                    }
                    continue;
                }
            }

            snprintf(kickmessage, MessageLen, "You were banned from this server by %s", banner.c_str());
            sendMessage(ServerPlayer, i, kickmessage);
            if (reason.length() > 0)
            {
                snprintf(kickmessage, MessageLen, "Reason given: %s", reason.c_str());
                sendMessage(ServerPlayer, i, kickmessage);
            }
            removePlayer(i, "/ban");
        }
    }
}

void initGroups()
{
    // make sure that the 'admin' & 'default' groups exist

    PlayerAccessInfo info;
    info.explicitAllows[PlayerAccessInfo::actionMessage] = true;
    info.explicitAllows[PlayerAccessInfo::adminMessageSend] = true;
    info.explicitAllows[PlayerAccessInfo::clientQuery] = true;
    info.explicitAllows[PlayerAccessInfo::date] = true;
    info.explicitAllows[PlayerAccessInfo::flagHistory] = true;
    info.explicitAllows[PlayerAccessInfo::idleStats] = true;
    info.explicitAllows[PlayerAccessInfo::lagStats] = true;
    info.explicitAllows[PlayerAccessInfo::privateMessage] = true;
    info.explicitAllows[PlayerAccessInfo::showMotto] = true;
    info.explicitAllows[PlayerAccessInfo::spawn] = true;
    info.explicitAllows[PlayerAccessInfo::talk] = true;
    info.groupState[PlayerAccessInfo::isGroup] = true;
    info.groupState[PlayerAccessInfo::isDefault] = true;
    groupAccess["EVERYONE"] = info;

    // VERIFIED
    info.explicitAllows.reset();
    info.groupState.reset();
    info.explicitAllows[PlayerAccessInfo::listPlugins] = true;
    info.explicitAllows[PlayerAccessInfo::poll] = true;
    info.explicitAllows[PlayerAccessInfo::report] = true;
    info.explicitAllows[PlayerAccessInfo::vote] = true;
    info.explicitAllows[PlayerAccessInfo::pollBan] = true;
    info.explicitAllows[PlayerAccessInfo::pollKick] = true;
    info.explicitAllows[PlayerAccessInfo::pollFlagReset] = true;
    info.groupState[PlayerAccessInfo::isGroup] = true;
    info.groupState[PlayerAccessInfo::isDefault] = true;
    groupAccess["VERIFIED"] = info;

    //  LOCAL.ADMIN
    info.explicitAllows.reset();
    info.groupState.reset();
    for (int i = 0; i < PlayerAccessInfo::lastPerm; i++)
        info.explicitAllows[i] = true;
    info.groupState[PlayerAccessInfo::isGroup] = true;
    info.groupState[PlayerAccessInfo::isDefault] = true;
    info.explicitAllows[PlayerAccessInfo::hideAdmin] = false;
    groupAccess["LOCAL.ADMIN"] = info;

    // load databases
    if (groupsFile.size())
        PlayerAccessInfo::readGroupsFile(groupsFile);
}

void sendBufferedNetDataForPeer (NetConnectedPeer &peer )
{
    if (peer.netHandler->hasTcpOutbound())
    {
        peer.netHandler->bufferedSend(NULL, 0);
        return;
    }

    if (peer.sendChunks.empty())
    {
        if (peer.deleteWhenDoneSending && peer.sent && !peer.netHandler->hasTcpOutbound())
            peer.deleteMe = true;
        return;
    }

    TimeKeeper now = TimeKeeper::getCurrent();

    if (peer.lastSend.getSeconds() + peer.minSendTime > now.getSeconds())
        return;

    peer.sent = true;
    peer.lastActivity = now;

    const std::string& chunk = peer.sendChunks.front();
    peer.netHandler->bufferedSend(chunk.data(), chunk.size());

    peer.sendChunks.pop_front();
}

std::string getIPFromHandler (NetHandler* netHandler)
{
    unsigned int address = (unsigned int)netHandler->getIPAddress().s_addr;
    unsigned char* a = (unsigned char*)&address;

    static std::string strRet;

    strRet = TextUtils::format("%d.%d.%d.%d",(int)a[0],(int)a[1],(int)a[2],(int)a[3]);

    return strRet;
}

static void processConnectedPeer(NetConnectedPeer& peer, int sockFD, fd_set& read_set, fd_set& UNUSED(write_set))
{
    const double connectionTimeout = 2.5; // timeout in seconds

    if (peer.deleteMe)
        return; // skip it, it's dead to us, we'll close and purge it later

    NetHandler* netHandler = peer.netHandler;

    // see if the sucker is banned

    const size_t headerLen = strlen(BZ_CONNECT_HEADER);
    size_t readLen = headerLen;

    if (peer.apiHandler == NULL && peer.player == -1)
    {
        // they arn't anything yet, see if they have any data

        if (peer.bufferedInput.size() >= headerLen )
            readLen = 1024;

        bool retry = false;

        RxStatus e = netHandler->receive(readLen,&retry);
        if (retry) // try one more time, just in case it was blocked
        {
            int retries = 1;
            if (BZDB.isSet("_maxConnectionRetries"))
                retries = (int) BZDB.eval("_maxConnectionRetries");

            retry = false;
            for ( int t = 0; t < retries; t++)
            {
                e = netHandler->receive(readLen,&retry);
                if (!retry)
                    break;
            }
        }

        if ((e == ReadAll) || (e == ReadPart))
        {
            peer.lastActivity = TimeKeeper::getCurrent();

            unsigned int readSize = netHandler->getTcpReadSize();

            if (readSize > 0)
            {
                // the dude has sent SOME data
                peer.sent = true;

                void *buf = netHandler->getTcpBuffer();

                const char* header = BZ_CONNECT_HEADER;

                char* tmp = (char*)malloc(readSize+1);
                strncpy(tmp,(char*)buf,readSize);
                tmp[readSize] = '\0';

                peer.bufferedInput += tmp;
                free(tmp);

                if (peer.bufferedInput.size() >= headerLen && strncmp(peer.bufferedInput.c_str(),header, headerLen) == 0)
                {
                    if (peer.bufferedInput.size() > headerLen+2)
                    {
                        peer.deleteMe = true;
                        return;
                    }

                    bz_AllowConnectionData_V1 data(getIPFromHandler(netHandler).c_str());
                    worldEventManager.callEvents(&data);
                    if (!data.allow)
                    {
                        logDebugMessage(2,"Game peer %s not allowed\n", netHandler->getTargetIP());
                        peer.deleteMe = true;
                        return;
                    }

                    netHandler->flushData();
                    // it's a player
                    if (!MakePlayer(netHandler))
                        peer.deleteMe = true;
                    else
                        peer.player = netHandler->getPlayerID();

                    return;
                }
                else
                {
                    // it isn't a player yet, see if anyone else wants it.
                    // build up a buffer of all the data that is pending
                    while (e == ReadAll)
                    {
                        netHandler->flushData();
                        e = netHandler->receive(256);

                        readSize = netHandler->getTcpReadSize();
                        buf = netHandler->getTcpBuffer();

                        peer.bufferedInput.append(static_cast<char const*>(buf), readSize);
                    }
                    netHandler->flushData();

                    bz_AllowConnectionData_V1 data(getIPFromHandler(netHandler).c_str());
                    worldEventManager.callEvents(&data);
                    if (!data.allow)
                    {
                        logDebugMessage(2,"Peer %s not allowed\n", netHandler->getTargetIP());
                        peer.deleteMe = true;
                        return;
                    }

                    in_addr IP = netHandler->getIPAddress();
                    BanInfo info(IP);
                    if (!clOptions->acl.validate(IP, &info))
                    {
                        logDebugMessage(2,"Peer %s banned\n", netHandler->getTargetIP());
                        std::string banMsg = "banned for " + info.reason + " by " + info.bannedBy;
                        peer.sendChunks.push_back(banMsg);
                        peer.deleteMe = true;
                    }

                    // call an event to let people know we got a new connect
                    bz_NewNonPlayerConnectionEventData_V1 eventData;

                    eventData.data = strdup(peer.bufferedInput.c_str());
                    eventData.size = peer.bufferedInput.size();
                    eventData.connectionID = sockFD;

                    worldEventManager.callEvents(bz_eNewNonPlayerConnection, &eventData);
                    free(eventData.data);

                    // if someone wanted him they'd have set his handler and he'll never get here again
                }
            }
        }
    }

    if (peer.apiHandler == NULL && peer.player == -1)
    {
        if (TimeKeeper::getCurrent().getSeconds() > peer.startTime.getSeconds() + connectionTimeout)
        {
            logDebugMessage(2,"Peer %s connection timeout with data \"%s\"\n",peer.netHandler->getTargetIP(),
                            TextUtils::escape_nonprintable(peer.bufferedInput,'"').c_str()); // FIXME: sanitize data
            std::string discoBuffer = getServerVersion();
            discoBuffer += "\r\n\r\n";
            peer.sendChunks.push_back(discoBuffer);
            peer.deleteMe = true; // nobody loves him
        }
    }

    // we like them see if they have gotten new data
    if (peer.apiHandler && peer.player < 0 && netHandler->isFdSet(&read_set))
    {
        in_addr IP = netHandler->getIPAddress();
        BanInfo info(IP);
        if (!clOptions->acl.validate(IP, &info))
        {
            logDebugMessage(2,"API peer %s banned\n", netHandler->getTargetIP());
            std::string banMsg = "banned for " + info.reason + " by " + info.bannedBy;
            peer.sendChunks.push_back(banMsg);
            peer.deleteMe = true;
        }

        if (!peer.deleteMe)
        {
            bool retry = false;

            RxStatus e = netHandler->receive(1024,&retry);
            if (retry) // try one more time, just in case it was blocked
            {
                int retries = 1;
                if (BZDB.isSet("_maxConnectionRetries"))
                    retries = (int) BZDB.eval("_maxConnectionRetries");

                retry = false;
                for ( int t = 0; t < retries; t++)
                {
                    e = netHandler->receive(1024,&retry);
                    if (!retry)
                        break;
                }
            }

            if (e == ReadPart || e == ReadAll )
            {
                peer.lastActivity = TimeKeeper::getCurrent();
                peer.apiHandler->pending(peer.socket,netHandler->getTcpBuffer(),netHandler->getTcpReadSize());
                netHandler->flushData();
            }
            else
            {
                // they done disconnected
                peer.apiHandler->disconnect(peer.socket);
                peer.deleteMe = true;
            }
        }
    }

    if (peer.player < 0) // only send data if he's not a player, there may be disco data
        sendBufferedNetDataForPeer(peer);
}

//
// global variable callbacks
//

static void bzdbGlobalCallback(const std::string& name, void* UNUSED(data))
{
    const std::string value = BZDB.get(name);
    bz_BZDBChangeData_V1 eventData(name, value);
    worldEventManager.callEvents(&eventData);
}

class UPnP
{
public:
    UPnP();
    void start();
    void stop();
private:
    void setIGD();
    void clearIGD();
    void setPorts();
    void setLocalInterface();
    void setRemoteInterface();
    void addPortForwarding();
    void deletePortForwarding();
#ifdef HAVE_MINIUPNPC_MINIUPNPC_H
    struct UPNPUrls urls;
    struct IGDdatas data;
#endif
    char    lanaddr[128];
    bool    IGD_Found;
    std::string remotePort;
    char    localPort[16];
};

UPnP bzUPnP;

UPnP::UPnP(): IGD_Found(false)
{
    lanaddr[0]   = 0;
    localPort[0] = 0;
}

void UPnP::setIGD()
{
#ifdef HAVE_MINIUPNPC_MINIUPNPC_H
    // Discover uPnP devices waiting for 200ms
#if (MINIUPNPC_API_VERSION >= 14)
    struct UPNPDev *devlist = upnpDiscover(200, NULL, NULL, 0, 0, 2, NULL);
#else
    struct UPNPDev *devlist = upnpDiscover(200, NULL, NULL, 0, 0, NULL);
#endif
    if (!devlist)
    {
        std::cerr << "No UPnP device found"
                  << std::endl;
        return;
    }
    // Select a good IGD (Internet Gateway Device)
    int i = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
    freeUPNPDevlist(devlist);
    if (!i)
        std::cerr << "No recognized device" << std::endl;
    else if (i != 1)
    {
        switch (i)
        {
        case 2:
            std::cerr << "Found a non connected IGD"
                      << urls.controlURL
                      << std::endl;
            break;
        default:
            std::cerr << "Not recognized as IGD"
                      << urls.controlURL
                      << std::endl;
            break;
        }
        FreeUPNPUrls(&urls);
    }
    else
        IGD_Found = true;
#endif
}

void UPnP::clearIGD()
{
    if (!IGD_Found)
        return;

#ifdef HAVE_MINIUPNPC_MINIUPNPC_H
    FreeUPNPUrls(&urls);
#endif
    IGD_Found = true;
}

void UPnP::setLocalInterface()
{
    // Use UPnP to set the local interface
    // Override with -i argument
    if (clOptions->pingInterface == "")
        clOptions->pingInterface = lanaddr;
}

void UPnP::setRemoteInterface()
{
#ifdef HAVE_MINIUPNPC_MINIUPNPC_H
    char externalIPAddress[128];
    int result = UPNP_GetExternalIPAddress(
                     urls.controlURL,
                     data.first.servicetype,
                     externalIPAddress);
    if (result != UPNPCOMMAND_SUCCESS)
    {
        std::cerr << "GetExternalIPAddress returned"
                  << result
                  << std::endl;
        FreeUPNPUrls(&urls);
        IGD_Found = false;
        return;
    }

    // Use UPnP to set the public IP interface
    // override with - publicaddr argument
    if ((clOptions->publicizedAddress.length() == 0)
            || (clOptions->publicizedAddress[0] == ':'))
    {
        if (clOptions->publicizedAddress.length() == 0)
            clOptions->publicizedAddress = externalIPAddress
                                           + TextUtils::format(":%d", clOptions->wksPort);
        else
            clOptions->publicizedAddress = externalIPAddress
                                           + clOptions->publicizedAddress;
    }
#endif
}

void UPnP::setPorts()
{
    size_t colonPos = clOptions->publicizedAddress.find(':');
    if (colonPos == std::string::npos)
        remotePort = "5154";
    else
        remotePort = std::string(clOptions->publicizedAddress, colonPos + 1);
    snprintf(localPort, sizeof(localPort), "%d", clOptions->wksPort);
}

void UPnP::addPortForwarding()
{
#ifdef HAVE_MINIUPNPC_MINIUPNPC_H
    int result;
    result = UPNP_AddPortMapping(
                 urls.controlURL,
                 data.first.servicetype,
                 remotePort.c_str(),
                 localPort,
                 lanaddr,
                 "bzfs",
                 "TCP",
                 0,
                 "0");
    if (result == UPNPCOMMAND_SUCCESS)
        result = UPNP_AddPortMapping(
                     urls.controlURL,
                     data.first.servicetype,
                     remotePort.c_str(),
                     localPort,
                     lanaddr,
                     "bzfs",
                     "UDP",
                     0,
                     "0");
    if (result != UPNPCOMMAND_SUCCESS)
    {
        switch (result)
        {
        case 402:
            std::cerr << "UPNP_AddPortMapping returned "
                      << "Invalid Args"
                      << std::endl;
            break;
        case 501:
            std::cerr << "UPNP_AddPortMapping returned "
                      << "Action Failed"
                      << std::endl;
            break;
        case 715:
            std::cerr << "UPNP_AddPortMapping returned "
                      << "WildCardNotPermittedInSrcIP"
                      << std::endl;
            break;
        case 716:
            std::cerr << "UPNP_AddPortMapping returned "
                      << "WildCardNotPermittedInExtPort"
                      << std::endl;
            break;
        case 718:
            std::cerr << "UPNP_AddPortMapping returned "
                      << "ConflictInMappingEntry"
                      << std::endl;
            break;
        case 724:
            std::cerr << "UPNP_AddPortMapping returned "
                      << "SamePortValuesRequired"
                      << std::endl;
            break;
        case 725:
            std::cerr << "UPNP_AddPortMapping returned "
                      << "OnlyPermanentLeasesSupported"
                      << std::endl;
            break;
        case 726:
            std::cerr << "UPNP_AddPortMapping returned "
                      << "RemoteHostOnlySupportsWildcard"
                      << std::endl;
            break;
        case 727:
            std::cerr << "UPNP_AddPortMapping returned "
                      << "ExternalPortOnlySupportsWildcard"
                      << std::endl;
            break;
        default:
            std::cerr << "UPNP_AddPortMapping returned "
                      << result
                      << std::endl;
            break;
        }
        FreeUPNPUrls(&urls);
        IGD_Found = false;
    }
#endif
}

void UPnP::deletePortForwarding()
{
#ifdef HAVE_MINIUPNPC_MINIUPNPC_H
    int result;
    result = UPNP_DeletePortMapping(
                 urls.controlURL,
                 data.first.servicetype,
                 remotePort.c_str(),
                 "TCP",
                 0);
    if (result == UPNPCOMMAND_SUCCESS)
        result = UPNP_DeletePortMapping(
                     urls.controlURL,
                     data.first.servicetype,
                     remotePort.c_str(),
                     "UDP",
                     0);
    if (result != UPNPCOMMAND_SUCCESS)
    {
        switch (result)
        {
        case 402:
            std::cerr << "UPNP_AddPortMapping returned "
                      << "Invalid Args"
                      << std::endl;
            break;
        case 714:
            std::cerr << "UPNP_AddPortMapping returned "
                      << "NoSuchEntryInArray"
                      << std::endl;
            break;
        default:
            std::cerr << "UPNP_AddPortMapping returned "
                      << result
                      << std::endl;
            break;
        }
        FreeUPNPUrls(&urls);
        IGD_Found = false;
    }
#endif
}

void UPnP::start()
{
    setIGD();
    if (!IGD_Found)
        return;

    setLocalInterface();
    setRemoteInterface();
    setPorts();
    if (!IGD_Found)
        return;
    addPortForwarding();
}

void UPnP::stop()
{
    if (!IGD_Found)
        return;
    deletePortForwarding();
    if (!IGD_Found)
        return;
    clearIGD();
}

/** main parses command line options and then enters an event and activity
 * dependant main loop.  once inside the main loop, the server is up and
 * running and should be ready to process connections and activity.
 */
int main(int argc, char **argv)
{
    int nfound;
    votingarbiter = (VotingArbiter *)NULL;

    loggingCallback = &apiLoggingCallback;

    netLogCB.Init();
#ifndef _WIN32
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    setvbuf(stderr, (char *)NULL, _IOLBF, 0);
#endif

    Record::init();

    // initialize
#if defined(_WIN32)
    {
        static const int major = 2, minor = 2;
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(major, minor), &wsaData))
        {
            logDebugMessage(2,"Failed to initialize Winsock.  Terminating.\n");
            return 1;
        }
        if (LOBYTE(wsaData.wVersion) != major || HIBYTE(wsaData.wVersion) != minor)
        {
            logDebugMessage(2,"Version mismatch in Winsock;"
                            "  got %d.%d, expected %d.%d.  Terminating.\n",
                            (int)LOBYTE(wsaData.wVersion),
                            (int)HIBYTE(wsaData.wVersion),
                            major,
                            minor);
            WSACleanup();
            return 1;
        }
    }
#else
    // don't die on broken pipe
    bzSignal(SIGPIPE, SIG_IGN);

#endif /* defined(_WIN32) */

    bzfsrand((unsigned int)time(0));

    Flags::init();

    ShotManager.Init();

    clOptions = new CmdLineOptions();

    // set default DB entries
    for (unsigned int gi = 0; gi < numGlobalDBItems; ++gi)
    {
        assert(globalDBItems[gi].name != NULL);
        if (globalDBItems[gi].value != NULL)
        {
            BZDB.set(globalDBItems[gi].name, globalDBItems[gi].value);
            BZDB.setDefault(globalDBItems[gi].name, globalDBItems[gi].value);
        }
        BZDB.setPersistent(globalDBItems[gi].name, globalDBItems[gi].persistent);
        BZDB.setPermission(globalDBItems[gi].name, globalDBItems[gi].permission);
        BZDB.addCallback(std::string(globalDBItems[gi].name), onGlobalChanged, (void*) NULL);
    }

    // add the global callback for worldEventManager
    BZDB.addGlobalCallback(bzdbGlobalCallback, NULL);

    CMDMGR.add("set", cmdSet, "set <name> [<value>]");
    CMDMGR.add("reset", cmdReset, "reset <name>");

    BZDBCache::init();

    // any set in parse this is a default value
    BZDB.setSaveDefault(true);

    // parse arguments  (finalized later)
    parse(argc, argv, *clOptions);
    setDebugTimestamp (clOptions->timestampLog, clOptions->timestampMicros, clOptions->timestampUTC);

    // no more defaults
    BZDB.setSaveDefault(false);

    if (clOptions->bzdbVars.length() > 0)
    {
        logDebugMessage(1,"Loading variables from %s\n", clOptions->bzdbVars.c_str());
        bool success = CFGMGR.read(clOptions->bzdbVars);
        if (success)
            logDebugMessage(1,"Successfully loaded variable(s)\n");
        else
            logDebugMessage(1,"WARNING: unable to load the variable file\n");
    }

    if (clOptions->publicizeServer && clOptions->publicizedKey.empty())
    {
        logDebugMessage(0,
                        "\nWARNING: Publicly listed bzfs servers must register"
                        " using the '-publickey <key>'option.\n\n");
    }

#ifdef BZ_PLUGINS
    // see if we are going to load any plugins
    initPlugins();
    // check for python by default
    //    loadPlugin(std::string("python"),std::string(""));
    for (unsigned int plugin = 0; plugin < clOptions->pluginList.size(); plugin++)
    {
        if (!loadPlugin(clOptions->pluginList[plugin].plugin,
                        clOptions->pluginList[plugin].command))
        {
            std::string text = "WARNING: unable to load the plugin; ";
            text += clOptions->pluginList[plugin].plugin + "\n";
            logDebugMessage(0,text.c_str());
        }
    }
#endif

    // start listening and prepare world database
    if (!defineWorld())
    {
#ifdef BZ_PLUGINS
        unloadPlugins();
#endif
#if defined(_WIN32)
        WSACleanup();
#endif /* defined(_WIN32) */
        std::cerr << "ERROR: A world was not specified" << std::endl;
        return 1;
    }
    else if (clOptions->cacheOut != "")
    {
        if (!saveWorldCache())
        {
            std::cerr << "ERROR: could not save world cache file: "
                      << clOptions->cacheOut << std::endl;
        }
        done = true;
    }

    // make flags, check sanity, etc...
    // (do this after the world has been loaded)
    finalizeParsing(argc, argv, *clOptions, world->getEntryZones());

    FlagInfo::setNoFlagInAir();
    for (int i = 0; i < numFlags; i++)
    {
        FlagInfo *flag = FlagInfo::get(i);
        if (!flag)
            continue;
        resetFlag(*flag);
    }

    // loading extra flag number
    FlagInfo::setExtra(clOptions->numExtraFlags);

    // loading lag announcement thresholds
    LagInfo::setAdminLagAnnounceThreshold(clOptions->adminlagannounce);
    LagInfo::setLagAnnounceThreshold(clOptions->lagannounce);

    // loading lag thresholds
    LagInfo::setThreshold(clOptions->lagwarnthresh,(float)clOptions->maxlagwarn);

    // loading jitter thresholds
    LagInfo::setJitterThreshold(clOptions->jitterwarnthresh,
                                (float)clOptions->maxjitterwarn);

    // loading packetloss thresholds
    LagInfo::setPacketLossThreshold(clOptions->packetlosswarnthresh, (float)clOptions->maxpacketlosswarn);

    // loading player callsign/motto filters
    PlayerInfo::setFilterParameters(clOptions->filterCallsigns,
                                    clOptions->filter,
                                    clOptions->filterSimple);

    GameKeeper::Player::setMaxShots(clOptions->maxShots);

    // enable replay server mode
    if (clOptions->replayServer)
    {

        Replay::init();

        // we don't send flags to a client that isn't expecting them
        numFlags = 0;
        clOptions->numExtraFlags = 0;

        // disable the BZDB callbacks
        for (unsigned int gi = 0; gi < numGlobalDBItems; ++gi)
        {
            assert(globalDBItems[gi].name != NULL);
            BZDB.removeCallback(std::string(globalDBItems[gi].name),
                                onGlobalChanged, (void*) NULL);
        }

        // maxPlayers is sent in the world data to the client.
        // the client then uses this to setup it's players
        // data structure, so we need to send it the largest
        // PlayerId it might see.
        maxPlayers = MaxPlayers + ReplayObservers;

        if (clOptions->maxTeam[ObserverTeam] == 0)
        {
            std::cerr << "replay needs at least 1 observer, set to 1" << std::endl;
            clOptions->maxTeam[ObserverTeam] = 1;
        }
        else if (clOptions->maxTeam[ObserverTeam] > ReplayObservers)
        {
            std::cerr << "observer count limited to " << ReplayObservers <<
                      " for replay" << std::endl;
            clOptions->maxTeam[ObserverTeam] = ReplayObservers;
        }
    }

    /* load the bad word filter if it was set */
    loadBadwordsList();

    /* initialize the poll arbiter for voting if necessary */
    if (clOptions->voteTime > 0)
    {
        votingarbiter =
            new VotingArbiter(clOptions->voteTime, clOptions->vetoTime,
                              clOptions->votesRequired, clOptions->votePercentage,
                              clOptions->voteRepeatTime);
        logDebugMessage(1,"There is a voting arbiter with the following settings:\n");
        logDebugMessage(1,"\tvote time is %d seconds\n", clOptions->voteTime);
        logDebugMessage(1,"\tveto time is %d seconds\n", clOptions->vetoTime);
        logDebugMessage(1,"\tvotes required are %d\n", clOptions->votesRequired);
        logDebugMessage(1,"\tvote percentage necessary is %f\n", clOptions->votePercentage);
        logDebugMessage(1,"\tvote repeat time is %d seconds\n", clOptions->voteRepeatTime);
        logDebugMessage(1,"\tavailable voters is initially set to %d\n", maxPlayers);

        // override the default voter count to the max number of players possible
        votingarbiter->setAvailableVoters(maxPlayers);
        BZDB.setPointer("poll", (void *)votingarbiter);
        BZDB.setPermission("poll", StateDatabase::ReadOnly);
    }

    // look up service name and use that port if no port given on
    // command line.  if no service then use default port.
    if (!clOptions->useGivenPort)
    {
        struct servent *service = getservbyname("bzfs", "tcp");
        if (service)
            clOptions->wksPort = ntohs(service->s_port);
    }

    if (clOptions->UPnP)
        bzUPnP.start();

    if (clOptions->pingInterface != "")
        serverAddress = Address::getHostAddress(clOptions->pingInterface);

    // my address to publish.  allow arguments to override (useful for
    // firewalls).  use my official hostname if it appears to be
    // canonicalized, otherwise use my IP in dot notation.
    // set publicized address if not set by arguments
    // Only set the public address through this automated generation if the
    // hostname is a FQDN or the IP address is not 0.0.0.0 or a private IP.
    if (clOptions->publicizedAddress.empty())
    {
        std::string generatedPublicAddress = Address::getHostName();
        if (generatedPublicAddress.find('.') == std::string::npos)
        {
            if (!serverAddress.isAny() && !serverAddress.isPrivate())
                generatedPublicAddress = serverAddress.getDotNotation();
            else
                generatedPublicAddress = "";
        }
        if (!generatedPublicAddress.empty())
            clOptions->publicizedAddress += generatedPublicAddress + TextUtils::format(":%d", clOptions->wksPort);
    }

    /* print debug information about how the server is running */
    if (clOptions->publicizeServer)
    {
        logDebugMessage(1,"Running a public server with the following settings:\n");
        logDebugMessage(1,"\tpublic address is %s\n", clOptions->publicizedAddress.c_str());
    }
    else
        logDebugMessage(1,"Running a private server with the following settings:\n");

    // get the master ban list
    if (clOptions->publicizeServer && !clOptions->suppressMasterBanList)
    {
        MasterBanList banList;
        std::vector<std::string>::const_iterator it;
        for (it = clOptions->masterBanListURL.begin();
                it != clOptions->masterBanListURL.end(); ++it)
        {
            clOptions->acl.merge(banList.get(it->c_str()));
            logDebugMessage(1,"Loaded master ban list from %s\n", it->c_str());
        }
    }

    Score::setTeamKillRatio(clOptions->teamKillerKickRatio);
    Score::setWinLimit(clOptions->maxPlayerScore);
    if (clOptions->rabbitSelection == RandomRabbitSelection)
        Score::setRandomRanking();
    // print networking info
    logDebugMessage(1,"\tlistening on %s:%i\n",
                    serverAddress.getDotNotation().c_str(), clOptions->wksPort);
    logDebugMessage(1,"\twith title of \"%s\"\n", clOptions->publicizedTitle.c_str());

    // prep ping reply
    pingReply.serverId.serverHost = serverAddress;
    pingReply.serverId.port = htons(clOptions->wksPort);
    pingReply.serverId.number = 0;
    pingReply.gameType = clOptions->gameType;
    pingReply.gameOptions = clOptions->gameOptions;
    pingReply.maxPlayers = (uint8_t)maxRealPlayers;
    pingReply.maxShots = clOptions->maxShots;
    pingReply.rogueMax = (uint8_t)clOptions->maxTeam[0];
    pingReply.redMax = (uint8_t)clOptions->maxTeam[1];
    pingReply.greenMax = (uint8_t)clOptions->maxTeam[2];
    pingReply.blueMax = (uint8_t)clOptions->maxTeam[3];
    pingReply.purpleMax = (uint8_t)clOptions->maxTeam[4];
    pingReply.observerMax = (uint8_t)clOptions->maxTeam[5];
    pingReply.shakeWins = clOptions->shakeWins;
    pingReply.shakeTimeout = clOptions->shakeTimeout;
    pingReply.maxTime = (uint16_t)clOptions->timeLimit;
    pingReply.maxPlayerScore = clOptions->maxPlayerScore;
    pingReply.maxTeamScore = clOptions->maxTeamScore;

    // adjust speed and height checking as required
    adjustTolerances();

    // setup the game settings
    makeGameSettings();

    // no original world weapons in replay mode
    if (Replay::enabled())
        world->getWorldWeapons().clear();

    TimeKeeper nextSuperFlagInsertion = TimeKeeper::getCurrent();
    const float flagExp = -logf(0.5f) / FlagHalfLife;

    // load up the access permissions & stuff
    initGroups();
    if (userDatabaseFile.size())
        PlayerAccessInfo::readPermsFile(userDatabaseFile);

    if (clOptions->startRecording)
        Record::start(ServerPlayer);

    // trap some signals
    if (bzSignal(SIGINT, SIG_IGN) != SIG_IGN)
        bzSignal(SIGINT, SIG_PF(terminateServer));
    bzSignal(SIGTERM, SIG_PF(terminateServer));

    // start the server
    if (!serverStart())
    {
        bzUPnP.stop();
#ifdef BZ_PLUGINS
        unloadPlugins();
#endif
#if defined(_WIN32)
        WSACleanup();
#endif /* defined(_WIN32) */
        std::cerr << "ERROR: Unable to start the server, perhaps one is already running?" << std::endl;
        return 2;
    }


    /* MAIN SERVER RUN LOOP
     *
     * the main loop runs at approximately 2 iterations per 5 seconds
     * when there are no players on the field.  this can increase to
     * about 100 iterations per 5 seconds with a single player, though
     * average is about 20-40 iterations per five seconds.  Adding
     * world weapons will increase the number of iterations
     * substantially (about x10)
     **/

    int i;
    while (!done)
    {

        // see if the octree needs to be reloaded
        world->checkCollisionManager();

        maxFileDescriptor = 0;
        // prepare select set
        fd_set read_set, write_set;
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        NetHandler::setFd(&read_set, &write_set, maxFileDescriptor);
        // always listen for connections
        FD_SET((unsigned int)wksSocket, &read_set);
        if (wksSocket > maxFileDescriptor)
            maxFileDescriptor = wksSocket;

        // Check for cURL needed activity
        int cURLmaxFile = cURLManager::fdset(read_set, write_set);
        if (cURLmaxFile > maxFileDescriptor)
            maxFileDescriptor = cURLmaxFile;

        // find timeout when next flag would hit ground
        TimeKeeper tm = TimeKeeper::getCurrent();
        // lets start by waiting 3 sec
        float waitTime = 3.0f;

        if ((countdownDelay >= 0) || (countdownResumeDelay >= 0))
        {
            // 3 seconds too slow for match countdowns
            waitTime = 0.5f;
        }
        else if (countdownActive && clOptions->timeLimit > 0.0f)
            waitTime = 1.0f;

        // get time for next flag drop
        float dropTime;
        while ((dropTime = FlagInfo::getNextDrop(tm)) <= 0.0f)
        {
            // if any flags were in the air, see if they've landed
            for (i = 0; i < numFlags; i++)
            {
                FlagInfo *flag = FlagInfo::get(i);
                if (!flag)
                    continue;
                if (flag->landing(tm))
                {
                    if (flag->flag.status == FlagOnGround)
                        sendFlagUpdate(*flag);
                    else
                        resetFlag(*flag);
                }
            }
        }
        if (dropTime < waitTime)
            waitTime = dropTime;

        // get time for next Player internal action
        GameKeeper::Player::updateLatency(waitTime);

        // get time for the next world weapons shot
        if (world->getWorldWeapons().count() > 0)
        {
            float nextTime = world->getWorldWeapons().nextTime ();
            if (nextTime < waitTime)
                waitTime = nextTime;
        }

        // get time for the next replay packet (if active)
        if (Replay::enabled())
        {
            float nextTime = Replay::nextTime ();
            if (nextTime < waitTime)
                waitTime = nextTime;
        }
        else
        {
            // game time updates
            const float nextGT = nextGameTime();
            if (nextGT < waitTime)
                waitTime = nextGT;
        }

        // minmal waitTime
        if (waitTime < 0.0f)
            waitTime = 0.0f;

        // if there are buffered UDP, no wait at all
        if (NetHandler::anyUDPPending())
            waitTime = 0.0f;

        // see if we are within the plug requested max wait time
#ifdef BZ_PLUGINS
        const float pluginMaxWait = getPluginMinWaitTime();
        if (waitTime > pluginMaxWait)
            waitTime = pluginMaxWait;
#endif

        if (!netConnectedPeers.empty())
        {
            std::map<int,NetConnectedPeer>::iterator itr = netConnectedPeers.begin();
            while (itr != netConnectedPeers.end())
            {
                // don't wait if we have data to send out
                if (itr->second.sendChunks.size())
                {
                    waitTime = 0;
                    break;
                }
                ++itr;
            }
            if (waitTime > 0.1f)
                waitTime = 0.1f;
        }

        // don't wait (used by CURL and MsgEnter)
        if (dontWait)
        {
            waitTime = 0.0f;
            dontWait = false;
        }

        /**************
         *  SELECT()  *
         **************/

        // wait for an incoming communication, a flag to hit the ground,
        // a game countdown to end, a world weapon needed to be fired,
        // or a replay packet waiting to be sent.
        struct timeval timeout;
        timeout.tv_sec = long(floorf(waitTime));
        timeout.tv_usec = long(1.0e+6f * (waitTime - floorf(waitTime)));
        nfound = select(maxFileDescriptor+1, (fd_set*)&read_set, (fd_set*)&write_set, 0, &timeout);
        //if (nfound)
        //  logDebugMessage(1,"nfound,read,write %i,%08lx,%08lx\n", nfound, read_set, write_set);

        // send replay packets
        // (this check and response should follow immediately after the select() call)
        if (Replay::playing())
            Replay::sendPackets ();

        // game time updates
        if (!Replay::enabled())
            sendPendingGameTime();


        // synchronize PlayerInfo
        tm = TimeKeeper::getCurrent();
        PlayerInfo::setCurrentTime(tm);

        // players see a countdown
        if (countdownDelay >= 0)
        {
            static TimeKeeper timePrevious = tm;
            if (readySetGo == -1)
                readySetGo = countdownDelay;

            if (tm - timePrevious > 0.9f)
            {
                timePrevious = tm;
                if (readySetGo == 0)
                {
                    sendMessage(ServerPlayer, AllPlayers, "The match has started!...Good Luck Teams!");
                    countdownDelay = -1; // reset back to "unset"
                    countdownResumeDelay = -1; // reset back to "unset"
                    readySetGo = -1; // reset back to "unset"
                    countdownActive = true;
                    gameOver = false;

                    // start server's clock
                    gameStartTime = tm;
                    clOptions->timeElapsed = 0.0f;

                    // start client's clock
                    void *msg = getDirectMessageBuffer();
                    nboPackInt(msg, (int32_t)(int)clOptions->timeLimit);
                    broadcastMessage(MsgTimeUpdate, sizeof(int32_t), msg);

                    // kill any players that are playing already
                    GameKeeper::Player *player;
                    if (clOptions->gameType == ClassicCTF)
                    {
                        for (int j = 0; j < curMaxPlayers; j++)
                        {
                            void *buf, *bufStart = getDirectMessageBuffer();
                            player = GameKeeper::Player::getPlayerByIndex(j);
                            if (!player || player->player.isObserver() || !player->player.isPlaying())
                                continue;

                            // the server gets to capture the flag -- send some
                            // bogus player id

                            // curMaxPlayers should never exceed 255, so this should
                            // be a safe cast
                            TeamColor vteam = player->player.getTeam();

                            buf = nboPackUByte(bufStart, (uint8_t)curMaxPlayers);
                            buf = nboPackUShort(buf, uint16_t(FlagInfo::lookupFirstTeamFlag(vteam)));
                            buf = nboPackUShort(buf, uint16_t(1 + (int(vteam) % 4)));
                            directMessage(j, MsgCaptureFlag, (char*)buf - (char*)bufStart, bufStart);

                            // kick 'em while they're down
                            playerKilled(j, curMaxPlayers, 0, -1, Flags::Null, -1, true);

                            // be sure to reset the player!
                            player->player.setDead();
                            zapFlagByPlayer(j);
                            player->player.setPlayedEarly(false);
                            player->player.setRestartOnBase(true);
                        }
                    }

                    // reset all flags
                    for (int j = 0; j < numFlags; j++)
                        zapFlag(*FlagInfo::get(j));

                    // quietly reset team scores in case of a capture during the countdown
                    resetTeamScores();

                    // reset player scores
                    resetPlayerScores();

                    // fire off a game start event
                    bz_GameStartEndEventData_V2   gameData;
                    gameData.eventType = bz_eGameStartEvent;
                    gameData.duration = clOptions->timeLimit;
                    gameData.playerID = clOptions->countdownStarter;
                    worldEventManager.callEvents(bz_eGameStartEvent,&gameData);

                }
                else
                {
                    if ((readySetGo == countdownDelay) && (countdownDelay > 0))
                        sendMessage(ServerPlayer, AllPlayers, "Start your engines!......");

                    sendMessage(ServerPlayer, AllPlayers, TextUtils::format("%i...", readySetGo).c_str());
                    --readySetGo;
                }
            } // end check if second has elapsed
        } // end check if countdown delay is active

        // players see the announce of resuming the countdown
        if (countdownResumeDelay >= 0)
        {
            static TimeKeeper timePrevious = tm;
            if (tm - timePrevious > 0.9f)
            {
                timePrevious = tm;
                if (gameOver)
                    countdownResumeDelay = -1; // reset back to "unset"
                else if (countdownResumeDelay == 0)
                {
                    countdownResumeDelay = -1; // reset back to "unset"
                    clOptions->countdownPaused = false;
                    sendMessage(ServerPlayer, AllPlayers, "Countdown resumed");

                    // fire off a game resume event
                    bz_GamePauseResumeEventData_V1 resumeEventData;
                    resumeEventData.eventType = bz_eGameResumeEvent;
                    worldEventManager.callEvents(bz_eGameResumeEvent, &resumeEventData);
                }
                else
                {
                    sendMessage(ServerPlayer, AllPlayers,TextUtils::format("%i...", countdownResumeDelay).c_str());
                    --countdownResumeDelay;
                }
            } // end check if second has elapsed
        } // end check if countdown resuming delay is active

        // see if game time ran out or if we are paused
        if (!gameOver && countdownActive && clOptions->timeLimit > 0.0f)
        {
            float newTimeElapsed = (float)(tm - gameStartTime);
            float timeLeft = clOptions->timeLimit - newTimeElapsed;
            if (timeLeft <= 0.0f && !countdownPauseStart)
            {
                timeLeft = 0.0f;
                gameOver = true;
                countdownActive = false;
                countdownPauseStart = TimeKeeper::getNullTime ();
                clOptions->countdownPaused = false;

                // fire off a game end event
                bz_GameStartEndEventData_V2 gameData;
                gameData.eventType = bz_eGameEndEvent;
                gameData.duration = clOptions->timeLimit;
                worldEventManager.callEvents(bz_eGameEndEvent,&gameData);

                cleanupGameOver();
            }

            if (countdownActive && clOptions->countdownPaused && !countdownPauseStart)
            {
                // we have a new pause
                countdownPauseStart = tm;
                void *buf, *bufStart = getDirectMessageBuffer ();
                buf = nboPackInt (bufStart, -1);
                broadcastMessage (MsgTimeUpdate, (char *) buf - (char *) bufStart, bufStart);
            }

            if (countdownActive && !clOptions->countdownPaused && (countdownResumeDelay < 0) && countdownPauseStart)
            {
                // resumed
                gameStartTime += (tm - countdownPauseStart);
                countdownPauseStart = TimeKeeper::getNullTime ();
                newTimeElapsed = (float)(tm - gameStartTime);
                timeLeft = clOptions->timeLimit - newTimeElapsed;
                void *buf, *bufStart = getDirectMessageBuffer ();
                buf = nboPackInt (bufStart, (int32_t) timeLeft);
                broadcastMessage (MsgTimeUpdate, (char *) buf - (char *) bufStart, bufStart);
            }

            if ((timeLeft == 0.0f || newTimeElapsed - clOptions->timeElapsed >= 30.0f || clOptions->addedTime != 0.0f)
                    && !clOptions->countdownPaused && (countdownResumeDelay < 0))
            {
                // send update every 30 seconds, when the game is over, or when time adjusted
                if (clOptions->addedTime != 0.0f)
                {
                    (timeLeft + clOptions->addedTime <= 0.0f) ? timeLeft = 0.0f : clOptions->timeLimit += clOptions->addedTime;
                    if (timeLeft > 0.0f)
                        timeLeft += clOptions->addedTime;
                    // inform visitors about the change
                    sendMessage(ServerPlayer, AdminPlayers,TextUtils::format("Adjusting the countdown by %0.f seconds",
                                clOptions->addedTime).c_str());
                    clOptions->addedTime = 0.0f; //reset
                }

                void *buf, *bufStart = getDirectMessageBuffer ();
                buf = nboPackInt (bufStart, (int32_t) timeLeft);
                broadcastMessage (MsgTimeUpdate, (char *) buf - (char *) bufStart, bufStart);
                clOptions->timeElapsed = newTimeElapsed;
                if (clOptions->oneGameOnly && timeLeft == 0.0f)
                {
                    done = true;
                    exitCode = 0;
                }
            }
        }

        requestAuthentication = false;
        for (int p = 0; p < curMaxPlayers; ++p)
        {
            GameKeeper::Player* playerData = GameKeeper::Player::getPlayerByIndex(p);
            if (!playerData)
                continue;
            doStuffOnPlayer(*playerData);
        }
        if (requestAuthentication)
        {
            // Request the listserver authentication
            listServerLink->queueMessage(ListServerLink::ADD);
        }
        GameKeeper::Player::setAllNeedHostbanChecked(false);

        // manage voting poll for collective kicks/bans/sets
        if ((clOptions->voteTime > 0) && (votingarbiter != NULL))
        {
            if (votingarbiter->knowsPoll())
            {
                char message[MessageLen];

                std::string target = votingarbiter->getPollTarget();
                std::string action = votingarbiter->getPollAction();
                std::string realIP = votingarbiter->getPollTargetIP();

                static unsigned short int voteTime = 0;

                /* flags to only blather once */
                static bool announcedOpening = false;
                static bool announcedClosure = false;
                static bool announcedResults = false;
                static bool apiEventCalled = false;
                static bool calledCustomHandler = false;

                /* once a poll begins, announce its commencement */
                if (!announcedOpening)
                {
                    voteTime = votingarbiter->getVoteTime();
                    snprintf(message, MessageLen, "A poll to %s %s has begun.  Players have up to %d seconds to vote.", action.c_str(),
                             target.c_str(), voteTime);
                    sendMessage(ServerPlayer, AllPlayers, message);
                    announcedOpening = true;
                }

                static TimeKeeper lastAnnounce = TimeKeeper::getNullTime();

                /* make a heartbeat announcement every 15 seconds */
                if (((voteTime - (int)(tm - votingarbiter->getStartTime()) - 1) % 15 == 0) &&
                        ((int)(tm - lastAnnounce) != 0) &&
                        (votingarbiter->timeRemaining() > 0))
                {
                    snprintf(message, MessageLen, "%d seconds remain in the poll to %s %s.", votingarbiter->timeRemaining(), action.c_str(),
                             target.c_str());
                    sendMessage(ServerPlayer, AllPlayers, message);
                    lastAnnounce = tm;
                }

                if (votingarbiter->isPollClosed())
                {
                    if (!apiEventCalled)
                    {
                        bz_PollEndEventData_V1 pollEndData;
                        pollEndData.successful = votingarbiter->isPollSuccessful();
                        pollEndData.yesCount = votingarbiter->getYesCount();
                        pollEndData.noCount  = votingarbiter->getNoCount();
                        pollEndData.abstentionCount = votingarbiter->getAbstentionCount();

                        worldEventManager.callEvents(bz_ePollEndEvent, &pollEndData);

                        apiEventCalled = true;
                    }

                    if (!calledCustomHandler && customPollTypes.find(votingarbiter->getPollAction()) != customPollTypes.end())
                    {
                        customPollTypes[votingarbiter->getPollAction()].pollHandler->PollClose(votingarbiter->getPollAction().c_str(),
                                votingarbiter->getPollTarget().c_str(), votingarbiter->isPollSuccessful());
                        calledCustomHandler = true;
                    }

                    if (!announcedResults)
                    {
                        snprintf(message, MessageLen, "Poll Results: %ld in favor, %ld oppose, %ld abstain", votingarbiter->getYesCount(),
                                 votingarbiter->getNoCount(), votingarbiter->getAbstentionCount());
                        sendMessage(ServerPlayer, AllPlayers, message);
                        announcedResults = true;
                    }

                    if (votingarbiter->isPollSuccessful())
                    {
                        if (!announcedClosure)
                        {
                            std::string pollAction;
                            if (action == "ban")
                                pollAction = "temporarily banned";
                            else if (action == "kick")
                                pollAction = "kicked";
                            else if (action == "kill")
                                pollAction = "killed";
                            else
                                pollAction = action;
                            // a poll that exists and is closed has ended successfully
                            if (action != "flagreset")
                                snprintf(message, MessageLen, "The poll is now closed and was successful.  %s is scheduled to be %s.", target.c_str(),
                                         pollAction.c_str());
                            else
                                snprintf(message, MessageLen,
                                         "The poll is now closed and was successful.  Currently unused flags are scheduled to be reset.");
                            sendMessage(ServerPlayer, AllPlayers, message);
                            announcedClosure = true;
                        }
                    }
                    else
                    {
                        if (!announcedClosure)
                        {
                            snprintf(message, MessageLen, "The poll to %s %s was not successful", action.c_str(), target.c_str());
                            sendMessage(ServerPlayer, AllPlayers, message);
                            announcedClosure = true;

                            // go ahead and reset the poll (don't bother waiting for veto timeout)
                            votingarbiter->forgetPoll();

                            // reset all announcement flags
                            announcedOpening = false;
                            announcedClosure = false;
                            announcedResults = false;
                        }
                    }

                    /* the poll either terminates by itself or via a veto command */
                    if (votingarbiter->isPollExpired())
                    {

                        /* maybe successful, maybe not */
                        if (votingarbiter->isPollSuccessful())
                        {
                            // perform the action of the poll, if any
                            std::string pollAction;
                            if (action == "ban")
                            {
                                int hours = 0;
                                int minutes = clOptions->banTime % 60;
                                if (clOptions->banTime > 60)
                                    hours = clOptions->banTime / 60;
                                pollAction = std::string("banned for ");
                                if (hours > 0)
                                {
                                    pollAction += TextUtils::format("%d hour%s%s",
                                                                    hours,
                                                                    hours == 1 ? "." : "s",
                                                                    minutes > 0 ? " and " : "");
                                }
                                if (minutes > 0)
                                {
                                    pollAction += TextUtils::format("%d minute%s",
                                                                    minutes,
                                                                    minutes > 1 ? "s" : "");
                                }
                                pollAction += ".";
                            }
                            else if (action == "kick")
                                pollAction = std::string("kicked.");
                            else if (action == "kill")
                                pollAction = std::string("killed.");
                            else
                                pollAction = action;
                            if (action != "flagreset")
                                snprintf(message, MessageLen, "%s has been %s", target.c_str(), pollAction.c_str());
                            else
                                snprintf(message, MessageLen, "All unused flags have now been reset.");
                            sendMessage(ServerPlayer, AllPlayers, message);

                            /* regardless of whether or not the player was found, if the poll
                             * is a ban poll, ban the weenie
                             */

                            if (action == "ban")
                            {
                                // reload the banlist in case anyone else has added
                                clOptions->acl.load();

                                clOptions->acl.ban(realIP.c_str(), target.c_str(), clOptions->banTime);
                                clOptions->acl.save();

                            }

                            if ((action == "ban") || (action == "kick"))
                            {
                                // lookup the player id
                                bool foundPlayer = false;
                                int v;
                                for (v = 0; v < curMaxPlayers; v++)
                                {
                                    GameKeeper::Player *otherData
                                        = GameKeeper::Player::getPlayerByIndex(v);
                                    if (otherData && (strncmp(otherData->player.getCallSign(),
                                                              target.c_str(), 256) == 0))
                                    {
                                        foundPlayer = true;
                                        break;
                                    }
                                }
                                // show the delinquent no mercy; make sure he is kicked even if he changed
                                // his callsign by finding a corresponding IP and matching it to the saved one
                                if (!foundPlayer)
                                {
                                    v = NetHandler::whoIsAtIP(realIP);
                                    foundPlayer = (v >= 0);
                                }
                                if (foundPlayer)
                                {
                                    // notify the player
                                    snprintf(message, MessageLen, "You have been %s due to sufficient votes to have you removed",
                                             action == "ban" ? "temporarily banned" : "kicked");
                                    sendMessage(ServerPlayer, v, message);
                                    snprintf(message,  MessageLen, "/poll %s", action.c_str());
                                    removePlayer(v, message);
                                }
                            }
                            else if (action == "kill")
                            {
                                // lookup the player id
                                bool foundPlayer = false;
                                int v;
                                for (v = 0; v < curMaxPlayers; v++)
                                {
                                    GameKeeper::Player *otherData
                                        = GameKeeper::Player::getPlayerByIndex(v);
                                    if (otherData && (strncmp(otherData->player.getCallSign(),
                                                              target.c_str(), 256) == 0))
                                    {
                                        foundPlayer = true;
                                        break;
                                    }
                                }

                                if (foundPlayer)
                                {
                                    // notify the player
                                    sendMessage(ServerPlayer, v, "You have been killed due to sufficient votes");
                                    playerKilled(v, curMaxPlayers, 0, -1, Flags::Null, -1);
                                }
                            }

                            else if (action == "set")
                            {
                                std::vector<std::string> args = TextUtils::tokenize(target.c_str(), " ", 2, true);
                                if ( args.size() < 2 )
                                    logDebugMessage(1,"Poll set taking action: no action taken, not enough parameters (%s).\n",
                                                    (!args.empty() ? args[0].c_str() : "No parameters."));
                                else
                                {
                                    StateDatabase::Permission permission = BZDB.getPermission(args[0]);
                                    if (!(BZDB.isSet(args[0]) && (permission == StateDatabase::ReadWrite || permission == StateDatabase::Locked)))
                                        logDebugMessage(1,"Poll set taking action: no action taken, variable cannot be set\n");
                                    else
                                    {
                                        logDebugMessage(1,"Poll set taking action: setting %s to %s\n", args[0].c_str(), args[1].c_str());
                                        BZDB.set(args[0], args[1], StateDatabase::Server);
                                    }
                                }
                            }
                            else if (action == "reset")
                            {
                                logDebugMessage(1,"Poll flagreset taking action: resetting unused flags.\n");
                                for (int f = 0; f < numFlags; f++)
                                {
                                    FlagInfo &flag = *FlagInfo::get(f);
                                    if (flag.player == -1)
                                        resetFlag(flag);
                                }
                            }
                        } /* end if poll is successful */

                        // get ready for the next poll
                        votingarbiter->forgetPoll();

                        announcedClosure = false;
                        announcedOpening = false;
                        announcedResults = false;

                    } // the poll expired

                }
                else
                {
                    // the poll may get enough votes early
                    if (votingarbiter->isPollSuccessful())
                    {
                        if (action != "flagreset")
                            snprintf(message,  MessageLen, "Enough votes were collected to %s %s early.", action.c_str(), target.c_str());
                        else
                            snprintf(message,  MessageLen, "Enough votes were collected to reset all unused flags early.");

                        sendMessage(ServerPlayer, AllPlayers, message);

                        // close the poll since we have enough votes (next loop will kick off notification)
                        votingarbiter->closePoll();

                    } // the poll is over
                } // is the poll closed
            } // knows of a poll
        } // voting is allowed and an arbiter exists


        // periodic advertising broadcast
        static const std::vector<std::string>* adLines = clOptions->textChunker.getTextChunk("admsg");
        if ((clOptions->advertisemsg != "") || adLines != NULL)
        {
            static TimeKeeper lastbroadcast = tm;
            if (tm - lastbroadcast > 900)
            {
                // every 15 minutes
                char message[MessageLen];
                if (clOptions->advertisemsg != "")
                {
                    const std::string admsg = evaluateString(clOptions->advertisemsg);
                    // split the admsg into several lines if it contains '\n'
                    const char* c = admsg.c_str();
                    const char* j;
                    while ((j = strstr(c, "\\n")) != NULL)
                    {
                        int l = j - c < MessageLen - 1 ? j - c : MessageLen - 1;
                        strncpy(message, c, l);
                        message[l] = '\0';
                        sendMessage(ServerPlayer, AllPlayers, message);
                        c = j + 2;
                    }
                    strncpy(message, c, MessageLen - 1);
                    message[strlen(c) < MessageLen - 1 ? strlen(c) : MessageLen -1] = '\0';
                    sendMessage(ServerPlayer, AllPlayers, message);
                }
                // multi line from file advert
                if (adLines != NULL)
                {
                    for (int j = 0; j < (int)adLines->size(); j++)
                    {
                        const std::string admsg = evaluateString((*adLines)[j]);
                        sendMessage(ServerPlayer, AllPlayers, admsg.c_str());
                    }
                }
                lastbroadcast = tm;
            }
        }

        // check team flag timeouts
        if (clOptions->gameType == ClassicCTF)
        {
            for (i = RedTeam; i < CtfTeams; ++i)
            {
                if (team[i].flagTimeout - tm < 0 && team[i].team.size == 0)
                {
                    int flagid = FlagInfo::lookupFirstTeamFlag(i);
                    if (flagid >= 0)
                    {
                        for (int n = 0; n < clOptions->numTeamFlags[i]; n++)
                        {
                            FlagInfo &flag = *FlagInfo::get(flagid + n);
                            if (flag.exist() && flag.player == -1)
                            {
                                logDebugMessage(1,"Flag timeout for team %d\n", i);
                                zapFlag(flag);
                            }
                        }
                    }
                }
            }
        }

        // maybe add a super flag (only if game isn't over)
        if (!gameOver && clOptions->numExtraFlags > 0 && nextSuperFlagInsertion<=tm)
        {
            // randomly choose next flag respawn time; halflife distribution
            float r = float(bzfrand() + 0.01); // small offset, we do not want to wait forever
            nextSuperFlagInsertion += -logf(r) / flagExp;
            for (i = numFlags - clOptions->numExtraFlags; i < numFlags; i++)
            {
                FlagInfo &flag = *FlagInfo::get(i);
                if (flag.flag.type == Flags::Null)
                {
                    // flag in now entering game
                    flag.addFlag();
                    sendFlagUpdate(flag);
                    break;
                }
            }
        }

        // occasionally add ourselves to the list again (in case we were
        // dropped for some reason).
        if (clOptions->publicizeServer)
            if (tm - listServerLink->lastAddTime > ListServerReAddTime)
            {
                // if there are no list servers and nobody is playing then
                // try publicizing again because we probably failed to get
                // the list last time we published, and if we don't do it
                // here then unless somebody stumbles onto this server then
                // quits we'll never try publicizing ourself again.
                if (listServerLinksCount == 0)
                {
                    // if nobody playing then publicize
                    if (GameKeeper::Player::count() == 0)
                        publicize();
                }

                // send add request
                listServerLink->queueMessage(ListServerLink::ADD);
            }

        // check messages
        if (nfound > 0)
        {
            //logDebugMessage(1,"chkmsg nfound,read,write %i,%08lx,%08lx\n", nfound, read_set, write_set);
            // first check initial contacts
            if (FD_ISSET(wksSocket, &read_set))
                acceptClient();

            // check if we have any UDP packets pending
            if (NetHandler::isUdpFdSet(&read_set))
            {
                TimeKeeper receiveTime = TimeKeeper::getCurrent();
                while (true)
                {
                    struct sockaddr_in uaddr;
                    unsigned char ubuf[MaxPacketLen];
                    bool     udpLinkRequest;
                    // interface to the UDP Receive routines
                    int id = NetHandler::udpReceive((char *) ubuf, &uaddr,
                                                    udpLinkRequest);
                    if (id == -1)
                        break;
                    else if (id == -2)
                    {
                        // if I'm ignoring pings
                        // then ignore the ping.
                        if (handlePings)
                        {
                            respondToPing(Address(uaddr));
                            pingReply.write(NetHandler::getUdpSocket(), &uaddr);
                        }
                        continue;
                    }
                    else
                    {
                        if (udpLinkRequest)
                            // send client the message that we are ready for him
                            sendUDPupdate(id);

                        // handle the command for UDP
                        handleCommand(id, ubuf, true);

                        // don't spend more than 250ms receiving udp
                        if (TimeKeeper::getCurrent() - receiveTime > 0.25f)
                        {
                            logDebugMessage(2,"Too much UDP traffic, will hope to catch up later\n");
                            break;
                        }
                    }
                }
            }

            // process eventual resolver requests
            NetHandler::checkDNS(&read_set, &write_set);

            // now check messages from connected players and send queued messages
            GameKeeper::Player *playerData;
            NetHandler* netPlayer(0);
            for (int j = 0; j < curMaxPlayers; j++)
            {
                playerData = GameKeeper::Player::getPlayerByIndex(j);
                if (!playerData || !playerData->netHandler)
                    continue;
                netPlayer = playerData->netHandler;
                // send whatever we have ... if any
                if (netPlayer->pflush(&write_set) == -1)
                {
                    removePlayer(j, "ECONNRESET/EPIPE", false);
                    continue;
                }
                playerData->handleTcpPacket(&read_set);
            }
        }
        else if (nfound < 0)
        {
            if (getErrno() != EINTR)
            {
                // test code - do not uncomment, will cause big stuttering
                // TimeKeeper::sleep(1.0f);
            }
        }
        else
        {
            if (NetHandler::anyUDPPending())
                NetHandler::flushAllUDP();
        }


        // check net connected peers
        // see if we have any thing from people won arn't players yet
        std::map<int,NetConnectedPeer>::iterator peerItr;

        // get a list of connections to purge, then purge them
        std::vector<int> toKill;
        for (peerItr  = netConnectedPeers.begin(); peerItr != netConnectedPeers.end(); ++peerItr)
        {
            if (peerItr->second.deleteMe)
            {
                if (peerItr->second.netHandler)
                    peerItr->second.netHandler->closing();

                toKill.push_back(peerItr->first);
            }
        }

        for (unsigned int j = 0; j < toKill.size(); j++)
        {
            if (netConnectedPeers.find(toKill[j]) != netConnectedPeers.end())
            {
                NetConnectedPeer &peer = netConnectedPeers[toKill[j]];
                if (peer.netHandler)
                    delete(peer.netHandler);
                peer.netHandler = NULL;
                netConnectedPeers.erase(netConnectedPeers.find(toKill[j]));
            }
        }

        // process the connections
        for (peerItr = netConnectedPeers.begin(); peerItr != netConnectedPeers.end(); ++peerItr)
            processConnectedPeer(peerItr->second, peerItr->first, read_set, write_set);

        // remove anyone that became a player since they will be handled by the rest of the code
        // there net handler was transfered to the player class
        toKill.clear();
        for (peerItr = netConnectedPeers.begin(); peerItr != netConnectedPeers.end(); ++peerItr)
        {
            if (peerItr->second.player != -1)
                toKill.push_back(peerItr->first);
        }
        for (unsigned int j = 0; j < toKill.size(); j++)
        {
            if (netConnectedPeers.find(toKill[j]) != netConnectedPeers.end())
                netConnectedPeers.erase(netConnectedPeers.find(toKill[j]));
        }

        // remove anyone that hasn't done anything in a long time
        toKill.clear();
        double timeoutNow = TimeKeeper::getCurrent().getSeconds();

        for (peerItr = netConnectedPeers.begin(); peerItr != netConnectedPeers.end(); ++peerItr)
        {
            if (timeoutNow > (peerItr->second.lastActivity.getSeconds() + peerItr->second.inactivityTimeout))
            {
                peerItr->second.netHandler->closing();
                toKill.push_back(peerItr->first);
            }
        }
        for (unsigned int j = 0; j < toKill.size(); j++)
        {
            NetConnectedPeer &peer = netConnectedPeers[toKill[j]];
            if (peer.netHandler)
                delete(peer.netHandler);
            peer.netHandler = NULL;
            netConnectedPeers.erase(netConnectedPeers.find(toKill[j]));
        }


        // Fire world weapons
        world->getWorldWeapons().fire();

        // update all the shots we have tracked
        ShotManager.Update();

        // send out any pending chat messages
        std::list<PendingChatMessages>::iterator itr = pendingChatMessages.begin();
        while ( itr != pendingChatMessages.end() )
        {
            ::sendChatMessage(itr->from,itr->to,itr->text.c_str(),itr->type);
            ++itr;
        }
        pendingChatMessages.clear();

        // fire off a tick event
        bz_TickEventData_V1 tickData;
        worldEventManager.callEvents(bz_eTickEvent,&tickData);

        ApiTick();

        // Spawn waiting players
        doSpawns();

        // Clean pending players
        bool resetGame = GameKeeper::Player::clean();

        if (resetGame && playerHadWorld)
        {
            playerHadWorld = false;
            if ((clOptions->worldFile == "") && !Replay::enabled())
                defineWorld();
        }

        // cURLperform should be called in any case as we could incur in timeout
        dontWait = dontWait || cURLManager::perform();
    }

    bzUPnP.stop();
#ifdef BZ_PLUGINS
    unloadPlugins();
#endif

    // print uptime
    logDebugMessage(1,"Shutting down server: uptime %s\n",
                    TimeKeeper::printTime(TimeKeeper::getCurrent() - TimeKeeper::getStartTime()).c_str());

    serverStop();

    // remove from list server and disconnect
    delete listServerLink;

    // free misc stuff
    AresHandler::globalShutdown();

    // free misc stuff
    delete clOptions;
    clOptions = NULL;
    FlagInfo::setSize(0);
    delete world;
    world = NULL;
    delete[] worldDatabase;
    worldDatabase = NULL;
    delete votingarbiter;
    votingarbiter = NULL;

    Record::kill();
    Replay::kill();
    Flags::kill();
    netLogCB.kill();

#if defined(_WIN32)
    WSACleanup();
#endif /* defined(_WIN32) */

    // done
    return exitCode;
}

void playerStateToAPIState(bz_PlayerUpdateState &apiState, const PlayerState &playerState)
{
    apiState.status = eAlive;
    if (playerState.status == PlayerState::DeadStatus) // DeadStatus is 0
        apiState.status = eDead;
    //FIXME  else if (playerState.status & PlayerState::Paused)
    //FIXME    apiState.status = ePaused;
    else if (playerState.status & PlayerState::Exploding)
        apiState.status = eExploding;
    else if (playerState.status & PlayerState::Teleporting)
        apiState.status = eTeleporting;
//  else if (playerState.status & PlayerState::InBuilding)
//   apiState.status = eInBuilding;

    apiState.inPhantomZone = false;//(playerState.status & PlayerState::PhantomZoned) != 0;
    apiState.falling = (playerState.status & PlayerState::Falling) != 0;
    apiState.crossingWall = (playerState.status & PlayerState::CrossingWall) != 0;
    apiState.phydrv = (playerState.status & PlayerState::OnDriver) ? playerState.phydrv : -1;
    apiState.rotation = playerState.azimuth;
    apiState.angVel = playerState.angVel;
    apiState.pos[0]      = playerState.pos[0];
    apiState.pos[1]      = playerState.pos[1];
    apiState.pos[2]      = playerState.pos[2];
    apiState.velocity[0] = playerState.velocity[0];
    apiState.velocity[1] = playerState.velocity[1];
    apiState.velocity[2] = playerState.velocity[2];
}


void APIStateToplayerState(PlayerState &playerState, const bz_PlayerUpdateState &apiState)
{
    playerState.status = 0;
    switch (apiState.status)
    {
    case eDead:
        playerState.status = PlayerState::DeadStatus; // DeadStatus = 0
        break;
    case eAlive:
        playerState.status |= PlayerState::Alive;
        break;
    case ePaused:
        playerState.status |= PlayerState::Paused;
        break;
    case eExploding:
        playerState.status |= PlayerState::Exploding;
        break;
    case eTeleporting:
        playerState.status |= PlayerState::Teleporting;
        break;

    default: // FIXME
        break;
        //  case eInBuilding:
        //    playerState.status |= PlayerState::InBuilding;
        //    break;
    }

// if (apiState.inPhantomZone)
//   playerState.status |=  PlayerState::PhantomZoned;

    if (apiState.falling)
        playerState.status |=  PlayerState::Falling;

    if (apiState.crossingWall)
        playerState.status |=  PlayerState::CrossingWall;

    if (apiState.phydrv != -1)
    {
        playerState.status |=  PlayerState::OnDriver;
        playerState.phydrv = apiState.phydrv;
    }

    playerState.azimuth = apiState.rotation;
    playerState.angVel = apiState.angVel;
    playerState.pos[0]      = apiState.pos[0];
    playerState.pos[1]      = apiState.pos[1];
    playerState.pos[2]      = apiState.pos[2];
    playerState.velocity[0] = apiState.velocity[0];
    playerState.velocity[1] = apiState.velocity[1];
    playerState.velocity[2] = apiState.velocity[2];
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
