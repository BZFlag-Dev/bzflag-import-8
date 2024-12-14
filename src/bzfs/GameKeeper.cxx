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
#include "GameKeeper.h"

/* system headers */
#include <vector>
#include <string>

/* common headers */
#include "GameTime.h"

GameKeeper::Player* GameKeeper::Player::playerList[PlayerSlot] = {0}; // this is suspect...
bool GameKeeper::Player::allNeedHostbanChecked = false;

void* PackPlayerInfo(void *buf, int playerIndex, uint8_t properties )
{
    buf = nboPackUByte(buf, playerIndex);
    buf = nboPackUByte(buf, properties);
    return buf;
}

GameKeeper::Player::Player(int _playerIndex,
                           const struct sockaddr_in &clientAddr, int fd,
                           tcpCallback _clientCallback)
    : _LSAState(start),
      player(_playerIndex), netHandler(new NetHandler(&player, clientAddr, _playerIndex, fd)),
      lagInfo(&player),
      stateTimeStamp(0.0f), serverTimeStamp(0.0),
      gameTimeRate(GameTime::startRate), gameTimeNext(TimeKeeper::getCurrent()),
      isParting(false), hasEntered(false),
      playerHandler(0),
      addWasDelayed(false), hadEnter(false), addDelayStartTime(0.0),
      playerIndex(_playerIndex), closed(false), clientCallback(_clientCallback),
      needThisHostbanChecked(false), idFlag(-1)
{
    playerList[playerIndex] = this;

    lastState.order  = 0;
    score.playerID = _playerIndex;
    lastHeldFlagID = -1;
}

GameKeeper::Player::Player(int _playerIndex,
                           NetHandler* handler,
                           tcpCallback _clientCallback)
    : _LSAState(start),
      player(_playerIndex), netHandler(handler),
      lagInfo(&player),
      stateTimeStamp(0.0f), serverTimeStamp(0.0),
      gameTimeRate(GameTime::startRate), gameTimeNext(TimeKeeper::getCurrent()),
      isParting(false), hasEntered(false),
      playerHandler(0),
      addWasDelayed(false), hadEnter(false), addDelayStartTime(0.0),
      playerIndex(_playerIndex), closed(false), clientCallback(_clientCallback),
      needThisHostbanChecked(false), idFlag(-1)
{
    playerList[playerIndex] = this;

    lastState.order  = 0;
    score.playerID = _playerIndex;

    netHandler->setPlayer(&player, _playerIndex);
    lastHeldFlagID = -1;
}

GameKeeper::Player::Player(int _playerIndex, bz_ServerSidePlayerHandler* handler)
    : _LSAState(start),
      player(_playerIndex), netHandler(0),
      lagInfo(&player),
      stateTimeStamp(0.0f), serverTimeStamp(0.0),
      gameTimeRate(GameTime::startRate), gameTimeNext(TimeKeeper::getCurrent()),
      isParting(false), hasEntered(false),
      playerHandler(handler),
      addWasDelayed(false), hadEnter(false), addDelayStartTime(0.0),
      playerIndex(_playerIndex), closed(false), clientCallback(0),
      needThisHostbanChecked(false), idFlag(0)
{
    playerList[playerIndex] = this;

    lastState.order  = 0;
    score.playerID = _playerIndex;
    lastHeldFlagID = -1;
}

GameKeeper::Player::~Player()
{
    flagHistory.clear();
    delete netHandler;
    playerList[playerIndex] = 0;
}

int GameKeeper::Player::count()
{
    Player* playerData(0);
    int     count = 0;

    for (int i = 0; i < PlayerSlot; i++)
        if ((playerData = playerList[i]) && !playerData->closed
                && playerData->player.isPlaying())
            count++;
    return count;
}

void GameKeeper::Player::updateLatency(float &waitTime)
{
    Player* playerData(0);

    for (int p = 0; p < PlayerSlot; ++p)
    {
        if ((playerData = playerList[p]) && !playerData->closed)
        {
            // get time for next lagping
            playerData->lagInfo.updateLatency(waitTime);
        }
    }
}

void GameKeeper::Player::dumpScore()
{
    Player* playerData(0);

    std::cout << "\n#players\n";
    for (int p = 0; p < PlayerSlot; ++p)
    {
        if ((playerData = playerList[p]) && !playerData->closed
                && playerData->player.isPlaying())
        {
            playerData->score.dump();
            std::cout << ' ' << playerData->player.getCallSign() << std::endl;
        }
    }
}

int GameKeeper::Player::anointRabbit(int oldRabbit)
{
    float topRatio(   -100000.0f);
    int   rabbitIndex( NoPlayer);

    Player* playerData(0);
    bool    goodRabbitSelected(false);

    for (int i = 0; i < PlayerSlot; ++i)
    {
        if ((playerData = playerList[i]) && !playerData->closed
                && playerData->player.canBeRabbit(true))
        {
            bool  goodRabbit(i != oldRabbit && playerData->player.isAlive());
            float ratio(     playerData->score.ranking());
            bool  select(    false);
            if (goodRabbitSelected)
            {
                if (goodRabbit && (ratio > topRatio))
                    select = true;
            }
            else
            {
                if (goodRabbit)
                {
                    select         = true;
                    goodRabbitSelected = true;
                }
                else
                {
                    if (ratio > topRatio) select = true;
                }
            }
            if (select)
            {
                topRatio = ratio;
                rabbitIndex = i;
            }
        }
    }
    return rabbitIndex;
}

void GameKeeper::Player::updateNextGameTime()
{
    if (gameTimeRate < GameTime::startRate)
        gameTimeRate = GameTime::startRate;
    else if (gameTimeRate < GameTime::finalRate)
        gameTimeRate = gameTimeRate * 1.25f;
    else
        gameTimeRate = GameTime::finalRate;
    gameTimeNext = TimeKeeper::getCurrent();
    gameTimeNext += gameTimeRate;
    return;
}

void* GameKeeper::Player::packAdminInfo(void* buf)
{
    if (netHandler == 0)
    {
        buf = nboPackUByte(buf, 5);
        buf = nboPackUByte(buf, playerIndex);
        buf = nboPackUByte(buf, 127);
        buf = nboPackUByte(buf, 0);
        buf = nboPackUByte(buf, 0);
        buf = nboPackUByte(buf, 1);
    }
    else
    {
        buf = nboPackUByte(buf, netHandler->sizeOfIP());
        buf = nboPackUByte(buf, playerIndex);
        buf = netHandler->packAdminInfo(buf);
    }
    return buf;
}

void* GameKeeper::Player::packPlayerInfo(void* buf)
{
    buf = PackPlayerInfo(buf, playerIndex, accessInfo.getPlayerProperties());
    return buf;
}

void* GameKeeper::Player::packPlayerUpdate(void* buf)
{
    buf = nboPackUByte(buf, playerIndex);
    buf = player.packUpdate(buf);
    buf = score.pack(buf);
    buf = player.packId(buf);
    return buf;
}

void GameKeeper::Player::setPlayerAddMessage ( PlayerAddMessage& msg )
{
    msg.playerID = playerIndex;
    msg.team = player.getTeam();
    msg.type = player.getType();
    msg.wins = score.getWins();
    msg.losses = score.getLosses();
    msg.tks = score.getTKs();
    msg.callsign =  player.getCallSign();
    msg.motto =  player.getMotto();
}


std::vector<int> GameKeeper::Player::allowed(PlayerAccessInfo::AccessPerm right,
        int targetPlayer)
{
    std::vector<int> receivers;
    Player* playerData(0);

    if (targetPlayer != -1)
    {
        if ((playerData = playerList[targetPlayer]) && !playerData->closed
                && playerData->accessInfo.hasPerm(right))
            receivers.push_back(targetPlayer);
    }
    else
    {
        for (int i = 0; i < PlayerSlot; ++i)
        {
            if ((playerData = playerList[i]) && !playerData->closed
                    && playerData->accessInfo.hasPerm(right))
                receivers.push_back(i);
        }
    }

    return receivers;
}

bool GameKeeper::Player::loadEnterData(uint16_t& rejectCode,
                                       char* rejectMsg)
{
    // look if there is as name clash, we won't allow this
    for (int i = 0; i < PlayerSlot; i++)
    {
        Player* otherData(playerList[i]);
        if (i == playerIndex || !otherData || !otherData->player.isPlaying())
            continue;
        if (otherData->closed) continue;
        if (!strcasecmp(otherData->player.getCallSign(), player.getCallSign()))
        {
            rejectCode   = RejectRepeatCallsign;
            strcpy(rejectMsg, "The callsign specified is already in use.");
            return false;
        }
    }

    return true;
}

void GameKeeper::Player::signingOn(bool ctf)
{
    player.resetPlayer(ctf);
    player.signingOn();
    lagInfo.reset();
}


// Attempt to retrive a slot number for a player specified as EITHER "callsign" or "#<slot>"
int GameKeeper::Player::getPlayerIDByName(const std::string &name)
{
    Player* playerData(0);
    int slot(-1); // invalid

    if (sscanf (name.c_str(), "#%d", &slot) == 1)
    {
        if ( ! GameKeeper::Player::getPlayerByIndex(slot) ) return -1;
        return slot;
    }
    else
    {
        for (int i = 0; i < PlayerSlot; ++i)
        {
            if ((playerData = playerList[i]) && !playerData->closed
                    && (TextUtils::compare_nocase(playerData->player.getCallSign(), name) == 0))
                return i;
        }
    }
    return -1;
}


void GameKeeper::Player::reloadAccessDatabase()
{
    Player* playerData(0);
    for (int i = 0; i < PlayerSlot; ++i)
        if ((playerData = playerList[i]) && !playerData->closed)
            playerData->accessInfo.reloadInfo();
}

void GameKeeper::Player::close()
{
    closed = true;
}

bool GameKeeper::Player::clean()
{
    Player* playerData(0);
    // Trying to detect if this action cleaned the array of player
    bool empty(true);
    bool ICleaned(false);
    for (int i = 0; i < PlayerSlot; ++i)
    {
        if ((playerData = playerList[i]))
        {
            if (playerData->closed)
            {
                playerList[i] = 0;
                delete playerData;
                ICleaned = true;
            }
            else
                empty = false;
        }
    }
    return empty && ICleaned;
}

int GameKeeper::Player::getFreeIndex(int min, int max)
{
    for (int i = min; i < max; ++i)
        if (!playerList[i]) return i;
    return max;
}

void GameKeeper::Player::handleTcpPacket(fd_set* set)
{
    if (netHandler->isFdSet(set))
    {
        RxStatus const e(netHandler->tcpReceive());
        if (e == ReadPart) return;
        clientCallback(*netHandler, playerIndex, e);
    }
}

void GameKeeper::Player::setPlayerState(const glm::vec3 &pos, float azimuth)
{
    serverTimeStamp = (float)TimeKeeper::getCurrent().getSeconds();
    lastState.pos = pos;
    lastState.azimuth = azimuth;
    // Set Speeds to 0 too
    lastState.velocity = glm::vec3(0.0f);
    lastState.angVel = 0.0f;
    stateTimeStamp   = 0.0f;

    // player is alive.
    player.setAlive();
}

int GameKeeper::Player::maxShots(0);

void GameKeeper::Player::setMaxShots(int _maxShots)
{
    maxShots = _maxShots;
}

float GetShotLifetime(FlagType* flagType)
{
    float lifeTime(BZDB.eval(StateDatabase::BZDB_RELOADTIME));
    if (flagType == Flags::RapidFire)
        lifeTime *= BZDB.eval(StateDatabase::BZDB_RFIREADLIFE);
    else if (flagType == Flags::MachineGun)
        lifeTime *= BZDB.eval(StateDatabase::BZDB_MGUNADLIFE);
    else if (flagType == Flags::GuidedMissile)
        lifeTime *= BZDB.eval(StateDatabase::BZDB_GMADLIFE) + .01f;
    else if (flagType == Flags::Laser)
        lifeTime *= BZDB.eval(StateDatabase::BZDB_LASERADLIFE);
    else if (flagType == Flags::ShockWave)
        lifeTime *= BZDB.eval(StateDatabase::BZDB_SHOCKADLIFE);
    else if (flagType == Flags::Thief)
        lifeTime *= BZDB.eval(StateDatabase::BZDB_THIEFADLIFE);

    return lifeTime;
}

bool GameKeeper::Player::addShot(int id, int salt, FiringInfo &firingInfo)
{
    float now((float)TimeKeeper::getCurrent().getSeconds());
    if (id < int(shotsInfo.size()) && shotsInfo[id].present
            && now < shotsInfo[id].expireTime)
    {
        logDebugMessage(2,"Player %s [%d] shot id %d duplicated\n",
                        player.getCallSign(), playerIndex, id);
        return false;
    }
    // verify shot number
    if (id > maxShots - 1)
    {
        logDebugMessage(2,"Player %s [%d] shot id %d out of range %d\n",
                        player.getCallSign(), playerIndex, id, maxShots);
        return false;
    }

    shotsInfo.resize(maxShots);

    float lifeTime = GetShotLifetime(firingInfo.flagType);

    ShotInfo myShot;
    myShot.firingInfo  = firingInfo;
    myShot.salt   = salt;
    myShot.expireTime  = now + lifeTime;
    myShot.present     = true;
    myShot.running     = true;

    shotsInfo[id] = myShot;
    return true;
}

bool GameKeeper::Player::removeShot(int id, int salt)
{
    float now = (float)TimeKeeper::getCurrent().getSeconds();
    if (id >= int(shotsInfo.size()) || !shotsInfo[id].present
            || now >= shotsInfo[id].expireTime)
    {
        logDebugMessage(2,"Player %s [%d] trying to stop the unexistent shot id %d\n",
                        player.getCallSign(), playerIndex, id);
        return false;
    }
    if (shotsInfo[id].salt != salt)
    {
        logDebugMessage(2,"Player %s [%d] trying to stop a mismatched shot id %d\n",
                        player.getCallSign(), playerIndex, id);
        return false;
    }
    if (!shotsInfo[id].running) return false;
    shotsInfo[id].running = false;
    return true;
}

bool GameKeeper::Player::updateShot(int id, int salt)
{
    float now((float)TimeKeeper::getCurrent().getSeconds());
    if (id >= (int)shotsInfo.size() || !shotsInfo[id].present
            || now >= shotsInfo[id].expireTime)
    {
        logDebugMessage(2,"Player %s [%d] trying to update an unexistent shot id %d\n",
                        player.getCallSign(), playerIndex, id);
        return false;
    }
    if (shotsInfo[id].salt != salt)
    {
        logDebugMessage(2,"Player %s [%d] trying to update a mismatched shot id %d\n",
                        player.getCallSign(), playerIndex, id);
        return false;
    }
    if (!shotsInfo[id].running) return false;
    // only GM can be updated
    if (shotsInfo[id].firingInfo.flagType != Flags::GuidedMissile)
    {
        logDebugMessage(2,"Player %s [%d] trying to update a non GM shot id %d\n",
                        player.getCallSign(), playerIndex, id);
        return false;
    }
    return true;
}

void GameKeeper::Player::setPlayerState(PlayerState state, float timestamp)
{
    lagInfo.updateLag(timestamp, state.order - lastState.order > 1);
    player.updateIdleTime();
    lastState      = state;
    stateTimeStamp = timestamp;
    serverTimeStamp = (float)TimeKeeper::getCurrent().getSeconds();
}

void GameKeeper::Player::getPlayerState(glm::vec3 &pos, float &azimuth)
{
    pos     = lastState.pos;
    azimuth = lastState.azimuth;
}

void GameKeeper::Player::setLastIdFlag(int _idFlag)
{
    idFlag = _idFlag;
}

int GameKeeper::Player::getLastIdFlag()
{
    return idFlag;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
