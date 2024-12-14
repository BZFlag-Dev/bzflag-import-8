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

#ifndef __GAMEKEEPER_H__
#define __GAMEKEEPER_H__

// bzflag global header
#include "common.h"

// system headers
#include <vector>
#include <string>

// common interface headers
#include "PlayerInfo.h"
#include "PlayerState.h"
#include "TimeKeeper.h"

// implementation-specific bzfs-specific headers
#include "CmdLineOptions.h"
#include "FlagHistory.h"
#include "Permissions.h"
#include "LagInfo.h"
#include "Score.h"
#include "RecordReplay.h"
#include "NetHandler.h"
#include "Authentication.h"
#include "messages.h"
#include "bzfsAPI.h"
#include "FlagInfo.h"
#include "ShotUpdate.h"

class ShotInfo
{
public:
    ShotInfo() : salt(0), expireTime(0.0), present(false), running(false) {};

    FiringInfo firingInfo;
    int   salt;
    float      expireTime;
    bool       present;
    bool       running;
};



const int PlayerSlot = MaxPlayers + ReplayObservers;

typedef void (*tcpCallback)(NetHandler &netPlayer, int i, const RxStatus e);

/** This class is meant to be the container of all the global entity that lives
    into the game and methods to act globally on those.
    Up to now it contain players. Flag class is only there as a TODO
*/
class GameKeeper
{
public:
    class Player
    {
    public:
        Player(int _playerIndex, const struct sockaddr_in &clientAddr, int fd,
               tcpCallback _clientCallback);
        Player(int _playerIndex, NetHandler *handler, tcpCallback _clientCallback);
        Player(int _playerIndex, bz_ServerSidePlayerHandler *handler);
        ~Player();

        int        getIndex();
        static int     getFreeIndex(int min, int max);
        static Player* getPlayerByIndex(int _playerIndex);
        static int     count();
        static void    updateLatency(float &waitTime);
        static void    dumpScore();
        static int     anointRabbit(int oldRabbit);
        static std::vector<int> allowed(PlayerAccessInfo::AccessPerm right,
                                        int targetPlayer = -1);
        static int     getPlayerIDByName(const std::string &name);
        static void    reloadAccessDatabase();

        bool       loadEnterData(uint16_t& rejectCode,
                                 char* rejectMsg);
        void*      packAdminInfo(void* buf);
        void*      packPlayerInfo(void* buf);
        void*      packPlayerUpdate(void* buf);

        void       setPlayerAddMessage ( PlayerAddMessage &msg );

        void       signingOn(bool ctf);
        void       close();
        static bool    clean();
        void       handleTcpPacket(fd_set *set);

        // For hostban checking, to avoid check and check again
        static void    setAllNeedHostbanChecked(bool set);
        void       setNeedThisHostbanChecked(bool set);
        bool       needsHostbanChecked();

        // To handle player State
        void       setPlayerState(const glm::vec3 &pos, float azimuth);
        void       getPlayerState(glm::vec3 &pos, float &azimuth);
        void       setPlayerState(PlayerState state, float timestamp);

        void       setBzIdentifier(const std::string& id);
        const std::string& getBzIdentifier() const;

        // When is the player's next GameTime?
        const TimeKeeper&   getNextGameTime() const;
        void        updateNextGameTime();

        // To handle Identify
        void       setLastIdFlag(int _idFlag);
        int     getLastIdFlag();

        // To handle shot
        static void    setMaxShots(int _maxShots);
        bool       addShot(int id, int salt, FiringInfo &firingInfo);
        bool       removeShot(int id, int salt);
        bool       updateShot(int id, int salt);


        enum LSAState
        {
            start,
            notRequired,
            required,
            requesting,
            checking,
            timedOut,
            failed,
            verified,
            done
        } _LSAState;

        // players
        PlayerInfo        player;
        // Net Handler
        NetHandler        *netHandler;
        // player lag info
        LagInfo       lagInfo;
        // player access
        PlayerAccessInfo  accessInfo;
        // Last known position, vel, etc
        PlayerState       lastState;
        float         stateTimeStamp;
        float         serverTimeStamp;
        // GameTime update
        float         gameTimeRate;
        TimeKeeper        gameTimeNext;
        // FlagHistory
        FlagHistory       flagHistory;
        // Score
        Score         score;
        // Authentication
        Authentication    authentication;

        std::map<std::string, std::string> extraData;

        // flag to let us know the player is on it's way out
        bool  isParting;

        bool hasEntered;

        // logic class for server side players
        bz_ServerSidePlayerHandler* playerHandler;

        bool addWasDelayed;
        bool hadEnter;
        double addDelayStartTime;

        int lastHeldFlagID;

    private:
        static Player*    playerList[PlayerSlot];
        int           playerIndex;
        bool          closed;
        tcpCallback       clientCallback;
        std::string       bzIdentifier;
        bool          needThisHostbanChecked;
        // In case you want recheck all condition on all players
        static bool       allNeedHostbanChecked;

        static int       maxShots;
        std::vector<ShotInfo> shotsInfo;

        int        idFlag;

    };

    class Flag
    {
    };
};

inline int GameKeeper::Player::getIndex()
{
    return playerIndex;
}

inline GameKeeper::Player* GameKeeper::Player::getPlayerByIndex(int
        _playerIndex)
{
    if (_playerIndex < 0 || _playerIndex >= PlayerSlot)
        return 0;
    if (!playerList[_playerIndex])
        return 0;
    if (playerList[_playerIndex]->closed)
        return 0;
    return playerList[_playerIndex];
}

void* PackPlayerInfo(void* buf, int playerIndex, uint8_t properties );

// For hostban checking, to avoid check and check again
inline void GameKeeper::Player::setAllNeedHostbanChecked(bool set)
{
    allNeedHostbanChecked = set;
}

inline void GameKeeper::Player::setNeedThisHostbanChecked(bool set)
{
    needThisHostbanChecked = set;
}

inline bool GameKeeper::Player::needsHostbanChecked()
{
    return (allNeedHostbanChecked || needThisHostbanChecked);
}


inline void GameKeeper::Player::setBzIdentifier(const std::string& id)
{
    bzIdentifier = id;
}

inline const std::string& GameKeeper::Player::getBzIdentifier() const
{
    return bzIdentifier;
}


inline const TimeKeeper& GameKeeper::Player::getNextGameTime() const
{
    return gameTimeNext;
}


#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
