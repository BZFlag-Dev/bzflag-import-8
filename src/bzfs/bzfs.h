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

#ifndef __BZFS_H__
#define __BZFS_H__

// to dump score info to stdout
#define PRINTSCORE to include code to dump score info to stdout

#define SERVERLOGINMSG true

#include "common.h"

// must be before network.h because that defines a close() macro which
// messes up fstreams.  luckily, we don't need to call the close() method
// on any fstream.
#include <fstream>

// must be before windows.h
#include "network.h"

// common interface headers
#include "Address.h"
#include "Flag.h"
#include "Ping.h"

// bzfs specific headers
#include "TeamBases.h"
#include "CmdLineOptions.h"
#include "GameKeeper.h"
#include "FlagInfo.h"
#include "WorldInfo.h"
#include "VotingArbiter.h"
#include "ShotManager.h"

#include <list>
class PendingChatMessages
{
public:
    int to;
    int from;
    std::string text;
    MessageType type;

    PendingChatMessages ( int t, int f, const std::string &m, MessageType y )
    {
        to = t;
        from = f;
        text = m;
        type = y;
    }

    PendingChatMessages ( int t, int f, const char* m, MessageType y )
    {
        to = t;
        from = f;
        if (m)
            text = m;
        type = y;
    }

    PendingChatMessages ( const PendingChatMessages &m )
    {
        to = m.to;
        from = m.from;
        text = m.text;
        type = m.type;
    }
};

extern std::list<PendingChatMessages> pendingChatMessages;

extern void sendMessage(int     playerIndex,
                        PlayerId    dstPlayer,
                        const char  *message,
                        MessageType type = ChatMessage);
extern void removePlayer(int        playerIndex,
                         const char *reason,
                         bool       notify = true);
extern void playerKilled(int        victimIndex,
                         int        killerIndex,
                         int        reason,
                         int16_t    shotIndex,
                         const FlagType *flagType,
                         int        phydrv,
                         bool       respawnOnBase = false);
extern void doSpawns();
extern void grabFlag(int playerIndex, FlagInfo &flag, bool checkPos = true);
extern void sendPlayerMessage(GameKeeper::Player *playerData,
                              PlayerId dstPlayer,
                              const char *message);
extern char *getDirectMessageBuffer();
extern void  broadcastMessage(uint16_t code, int len, void *msg);
extern void  sendTeamUpdate(int playerIndex = -1,
                            int teamIndex1 = -1,
                            int teamIndex2 = -1);
extern void  sendFlagUpdate(FlagInfo &flag);
extern void  sendDrop(FlagInfo &flag);
extern void  sendIPUpdate(int targetPlayer = -1, int playerIndex = -1);
extern void  sendPlayerInfo(void);
extern void  directMessage(int playerIndex, uint16_t code,
                           int len, void *msg);
extern int   getCurMaxPlayers();
extern bool  areFoes(TeamColor team1, TeamColor team2);
extern PingPacket getTeamCounts();
extern void  zapFlagByPlayer(int playerIndex);
extern void  resetFlag(FlagInfo &flag);
extern void  dropFlag(FlagInfo& flag, const glm::vec3 &dropPos);
extern void  publicize();
extern TeamColor whoseBase(float x, float y, float z);
bool defineWorld(void);
bool saveWorldCache(const char* file = NULL);
float getMaxWorldHeight(void);

bool allowTeams ( void );
extern const std::string& getPublicOwner();
extern void setPublicOwner(const std::string& owner);

void loadBadwordsList();
void rescanForBans(bool isOperator = false, const char* callsign = NULL, int playerID = -1);

// initialize permission groups
extern void initGroups();

extern BasesList    bases;
extern CmdLineOptions   *clOptions;
extern uint16_t     curMaxPlayers;
extern bool     done;
extern bool     gameOver;
extern TeamInfo     team[NumTeams];
extern int      numFlags;
extern TimeKeeper   gameStartTime;
extern bool     countdownActive;
extern int      countdownDelay;
extern TimeKeeper   countdownPauseStart;
extern int      countdownResumeDelay;
extern std::string  hexDigest;
extern WorldInfo    *world;
extern char     *worldDatabase;
extern uint32_t     worldDatabaseSize;
extern char     worldSettings[4 + WorldSettingsSize];
extern uint8_t      rabbitIndex;
extern float        speedTolerance;
extern bool     handlePings;
extern uint16_t     maxPlayers;
extern uint16_t     maxRealPlayers;
extern float        pluginWorldSize;
extern float        pluginWorldHeight;
extern bool     checkShotMismatch;
extern bool     publiclyDisconnected;

extern Shots::Manager   ShotManager;

extern VotingArbiter    *votingarbiter;

bool captureFlag(int playerIndex, TeamColor teamCaptured, TeamColor teamCapped = NoTeam, bool checkCheat = true);

void resetTeamScores(void);
void pauseCountdown(int pausedBy = ServerPlayer);
void resumeCountdown(int resumedBy = ServerPlayer);
void startCountdown(int delay, float limit, int playerID = ServerPlayer);
void cancelCountdown( int playerID = ServerPlayer);

void dropPlayerFlag(GameKeeper::Player &playerData, const glm::vec3 &dropPos);
void playerAlive(int playerIndex);
void sendChatMessage(PlayerId srcPlayer, PlayerId dstPlayer, const char *message, MessageType type);

void makeWalls (void);

PlayerId getNewPlayerID();
void checkGameOn();
void cleanupGameOver();
void checkTeamScore(int playerIndex, int teamIndex);
void sendClosestFlagMessage(int playerIndex,FlagType *type, float pos[3]);

void ApiTick(void);

// peer list
struct NetConnectedPeer
{
    int socket;
    int player;

    NetHandler* netHandler;
    bz_NonPlayerConnectionHandler* apiHandler;

    std::list<std::string> sendChunks;
    std::string bufferedInput;

    TimeKeeper startTime;
    TimeKeeper lastActivity;

    TimeKeeper lastSend;
    double minSendTime;

    double inactivityTimeout;
    bool   sent;
    bool   deleteMe;
    bool   deleteWhenDoneSending;
};

extern std::map<int, NetConnectedPeer> netConnectedPeers;

extern unsigned int maxNonPlayerDataChunk;
extern void sendBufferedNetDataForPeer(NetConnectedPeer &peer);

// utils
void playerStateToAPIState(bz_PlayerUpdateState &apiState, const PlayerState &playerState);
void APIStateToplayerState(PlayerState &playerState, const bz_PlayerUpdateState &apiState);

void AddPlayer(int playerIndex, GameKeeper::Player *playerData);

void recalcAllHandicaps();
void broadcastHandicaps(int toPlayer = -1);

#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
