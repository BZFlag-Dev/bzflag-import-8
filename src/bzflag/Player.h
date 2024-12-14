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

#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "common.h"

/* system headers */
#include <string>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/* common interface headers */
#include "global.h"
#ifndef BUILDING_BZADMIN
#include "bzfgl.h"
#endif
#include "TimeKeeper.h"
#include "Address.h"
#include "Flag.h"
#include "PlayerState.h"
#include "ShotStatistics.h"


/* local interface headers */
class ShotPath;
class SceneDatabase;
class TankSceneNode;
class TankIDLSceneNode;
class SphereSceneNode;
class Obstacle;
class TankDeathOverride;

// 54 bytes
const int PlayerUpdatePLenMax =
    sizeof(float)     + // timestamp
    PlayerIdPLen      + // player id
    sizeof(int32_t)   + // order
    sizeof(int16_t)   + // status
    sizeof(float) * 3 + // position           (or int16_t * 3)
    sizeof(float) * 3 + // velocity           (or int16_t * 3)
    sizeof(float)     + // angle          (or int16_t)
    sizeof(float)     + // angular velocity       (or int16_t)
    sizeof(int16_t)   + // jump jets          (conditional)
    sizeof(int32_t)   + // physics driver     (conditional)
    sizeof(int16_t)   + // user speed         (conditional)
    sizeof(int16_t)   + // user angular velocity  (conditional)
    sizeof(uint8_t);    // sounds         (conditional)


class Player
{
public:
    Player(const PlayerId&, TeamColor,
           const char* callsign, const char* motto,
           const PlayerType);
    virtual   ~Player();

    PlayerId  getId() const;
    TeamColor getTeam() const;
    void      setTeam(TeamColor);
    void      updateTank(float dt, bool local);
    const char*   getCallSign() const;
    const char*   getMotto() const;
    void      setMotto(const char* _motto);
    PlayerType    getPlayerType() const;
    FlagType* getFlag() const;
    long      getOrder() const;
    short     getStatus() const;
    const glm::vec3 &getPosition() const;
    float     getAngle() const;
    const glm::vec3 &getForward() const;
    const glm::vec3 &getVelocity() const;

    float     getAngularVelocity() const;
    int       getPhysicsDriver() const;
    int       getDeathPhysicsDriver() const;
    float     getRadius() const;
    glm::vec3 getMuzzle() const;
    float     getMuzzleHeight() const;
    short     getWins() const;
    short     getLosses() const;
    short     getTeamKills() const;
    float     getTKRatio() const;
    float     getNormalizedScore() const;
    float     getLocalNormalizedScore() const;
    short     getScore() const;
    const float*  getDimensions() const;

    const glm::vec3 &getApparentVelocity() const;

#ifndef BUILDING_BZADMIN
    inline const glm::vec4 &getColor() const
    {
        return color;
    }
#endif
    short     getRabbitScore() const;
    short     getSelfKills() const;
    short     getLocalWins() const;
    short     getLocalLosses() const;
    short     getLocalTeamKills() const;
    const TimeKeeper& getExplodeTime() const;
    const TimeKeeper& getTeleportTime() const;
    short     getFromTeleporter() const;
    short     getToTeleporter() const;
    float     getTeleporterProximity() const;
    virtual int       getMaxShots() const;
    virtual ShotPath* getShot(int index) const = 0;

    const ShotStatistics* getShotStatistics() const;

    void      addToScene(SceneDatabase*, TeamColor effectiveTeam,
                         bool inCockpit, bool seerView,
                         bool showTreads, bool showIDL);

    bool      getIpAddress(Address&);
    void      setIpAddress(const Address& addr);

    virtual void  addShots(SceneDatabase*, bool colorblind) const;
    void      setLandingSpeed(float velocity);
    void      spawnEffect();
    void      fireJumpJets();

    bool      isAlive() const;
    bool      isPaused() const;
    bool      isFalling() const;
    bool      isFlagActive() const;
    bool      isTeleporting() const;
    bool      isExploding() const;
    bool      isPhantomZoned() const;
    bool      isCrossingWall() const;
    bool      isNotResponding() const;
    void      resetNotResponding();
    bool      isHunted() const;
    void      setHunted(bool _hunted);
    bool      isAutoPilot() const;
    void      setAutoPilot(bool = true);
    bool      isAdmin() const;
    void      setAdmin(bool = true);
    bool      isRegistered() const;
    void      setRegistered(bool = true);
    bool      isVerified() const;
    void      setVerified(bool = true);
    bool      hasPlayerList() const;
    void      setPlayerList(bool = true);

    bool      getPausedMessageState ( void ) const;
    void      setPausedMessageState ( bool set )
    {
        pauseMessageState = set;
    }

    bool      validTeamTarget(const Player *possibleTarget) const;

    // returns true iff dead reckoning is too different from the
    // current tank state.
    bool      isDeadReckoningWrong() const;

    // update state based on dead reckoning
    void      doDeadReckoning();

    // called to update state according to incoming packets
    void      move(const glm::vec3 &pos, float azimuth);
    void      setVelocity(const glm::vec3 &velocity);
    void      setAngularVelocity(float);
    void      setPhysicsDriver(int);
    void      setRelativeMotion();
    void      setUserSpeed(float speed);
    void      setUserAngVel(float angvel);
    void      changeTeam(TeamColor);
    virtual void  setFlag(FlagType*);
    virtual void  changeScore(short deltaWins, short deltaLosses, short deltaTeamKills);
    void      changeSelfKills(short delta);
    void      changeLocalScore(short deltaWins, short deltaLosses, short deltaTeamKills);
    void      setHandicap(float handicap);
    void      setStatus(short);
    void      setExplode(const TimeKeeper&);
    void      setTeleport(const TimeKeeper&, short from, short to);
    void      endShot(int index, bool isHit = false,
                      bool showExplosion = false);
    void      addHitToStats(FlagType* flag);

    void*     pack(void*, uint16_t& code);
    const void*   unpack(const void*, uint16_t code);

    void      setDeadReckoning();
    void      setDeadReckoning(float timestamp);

    void      setUserTexture ( const char *tex )
    {
        if (tex) userTexture = tex;
    }

    void      renderRadar() const;

    void      setExplodePos(const glm::vec3 &p);

    void setZpos (float z);

    float getMaxSpeed ( void ) const;

    void forceReload(float time);

    void setDeathEffect ( TankDeathOverride *e );
    TankDeathOverride* getDeathEffect ( void );

    int reportedHits;
    int computedHits;
    std::map<int,bool>    hitMap;
protected:
    void    clearRemoteSounds();
    void    addRemoteSound(int sound);

protected:
    // shot statistics
    ShotStatistics    shotStatistics;
    const Obstacle* lastObstacle; // last obstacle touched
    TimeKeeper    jamTime;

    // pause message
    bool pauseMessageState;

    std::vector<ShotPath*> shots;
    float       handicap;

private:
    // return true if the shot had to be terminated or false if it
    // was already terminated.  position must be set to the shot's
    // position if you return true (it's okay to return false if
    // there's no meaningful shot position).
    virtual bool doEndShot(int index, bool isHit, glm::vec3 &position) = 0;
    void getDeadReckoning(glm::vec3 &predictedPos, float* predictedAzimuth,
                          glm::vec3 &predictedVel, float time) const;
    void calcRelativeMotion(float vel[2], float& speed, float& angvel);
    void setVisualTeam (TeamColor team );
    void updateFlagEffect(FlagType* flag);
    void updateTranslucency(float dt);
    void updateDimensions(float dt, bool local);
    void updateTreads(float dt);
    void updateJumpJets(float dt);
    void updateTrackMarks();
    bool hitObstacleResizing();

private:
    // data not communicated with other players
    bool          notResponding;
    bool          hunted;

    // credentials
    PlayerId      id;
    bool          admin;
    bool          registered;
    bool          verified;
    bool          playerList;
    Address       ipAddr;
    bool          haveIpAddr;

    // data use for drawing
    TankSceneNode*    tankNode;
    TankIDLSceneNode* tankIDLNode;
    SphereSceneNode*  pausedSphere;
#ifndef BUILDING_BZADMIN
    glm::vec4     color;
    GLfloat       teleAlpha;
#endif
    std::string       userTexture;
    static int        tankTexture;
    static int        tankOverideTexture;
    TeamColor     lastVisualTeam;
    TimeKeeper        lastTrackDraw;

    // permanent data
    TeamColor     team;           // my team

    char          callSign[CallSignLen];  // my pseudonym
    char          motto[MottoLen];    // my motto
    PlayerType        type;           // Human/Computer

    // relatively stable data
    FlagType*     flagType;       // flag type I'm holding
    float         dimensions[3];      // current tank dimensions
    glm::vec3     dimensionsScale; // use to scale the dimensions
    float         dimensionsRate[3];   // relative to scale
    glm::vec3     dimensionsTarget;    // relative to scale
    bool          useDimensions;      // use the varying dimensions for gfx
    float         alpha;          // current tank translucency
    float         alphaRate;      // current tank translucency
    float         alphaTarget;        // current tank translucency
    TimeKeeper        spawnTime;      // time I started spawning
    TimeKeeper        explodeTime;        // time I started exploding
    TimeKeeper        teleportTime;       // time I started teleporting
    short         fromTeleporter;     // teleporter I entered
    short         toTeleporter;       // teleporter I exited
    float         teleporterProximity;    // how close to a teleporter
    short         wins;           // number of kills
    short         losses;         // number of deaths
    short         tks;            // number of teamkills
    short         selfKills;          // number of self-destructions

    // score of local player against this player
    short         localWins;      // local player won this many
    short         localLosses;        // local player lost this many
    short         localTks;       // local player team killed this many

    // highly dynamic data
    PlayerState       state;

    // additional state
    bool          autoPilot;

    // computable highly dynamic data
    glm::vec3     forward;     // forward unit vector

    // relative motion information
    float         relativeSpeed;      // relative speed
    float         relativeAngVel;     // relative angular velocity

    glm::vec3     apparentVelocity;    // velocity of tank as derived from it's last positional update

    // dead reckoning stuff
    TimeKeeper inputTime;     // time of input
    float inputTimestamp;  // input timestamp of sender
    int   inputStatus;        // tank status
    glm::vec3 inputPos;        // tank position
    glm::vec3 inputVel;        // tank velocity
    float inputAzimuth;       // direction tank is pointing
    float inputAngVel;        // tank turn rate
    bool  inputTurning;       // tank is turning
    float inputRelVel[2];     // relative velocity
    float inputRelSpeed;      // relative speed
    float inputRelAngVel;     // relative angular velocity
    float inputTurnCenter[2]; // tank turn center
    float inputTurnVector[2]; // tank turn vector
    int   inputPhyDrv;        // physics driver

    // average difference between time source and time destination
    float         deltaTime;

    // time offset on last measurement
    float         offset;

    // 0 -> not received any sample
    // 1 -> 1 sample rx
    // 2 -> 2 or more sample rx
    int           deadReckoningState;

    int           oldStatus;      // old tank status bits
    float         oldZSpeed;      // old tank vertical speed
};

// shot data goes in LocalPlayer or RemotePlayer so shot type isn't lost.

//
// Player
//

inline PlayerId     Player::getId() const
{
    return id;
}

inline TeamColor    Player::getTeam() const
{
    return team;
}

inline void     Player::setTeam(TeamColor _team)
{
    team = _team;
}

inline const char*  Player::getCallSign() const
{
    return callSign;
}

inline const char*  Player::getMotto() const
{
    return motto;
}

inline PlayerType   Player::getPlayerType() const
{
    return type;
}

inline FlagType*    Player::getFlag() const
{
    return flagType;
}

inline long     Player::getOrder() const
{
    return state.order;
}

inline short        Player::getStatus() const
{
    return state.status;
}

inline const glm::vec3 &Player::getPosition() const
{
    return state.pos;
}

inline float        Player::getAngle() const
{
    return state.azimuth;
}

inline const float* Player::getDimensions() const
{
    return dimensions;
}

inline const glm::vec3 &Player::getForward() const
{
    return forward;
}

inline const glm::vec3 &Player::getVelocity() const
{
    return state.velocity;
}

inline const glm::vec3 &Player::getApparentVelocity() const
{
    return apparentVelocity;
}

inline float        Player::getAngularVelocity() const
{
    return state.angVel;
}

inline int      Player::getPhysicsDriver() const
{
    return state.phydrv;
}

inline short        Player::getWins() const
{
    return wins;
}

inline short        Player::getLosses() const
{
    return losses;
}

inline short        Player::getTeamKills() const
{
    return tks;
}

inline short        Player::getSelfKills() const
{
    return selfKills;
}

inline short        Player::getLocalWins() const
{
    return localWins;
}

inline short        Player::getLocalLosses() const
{
    return localLosses;
}

inline short        Player::getLocalTeamKills() const
{
    return localTks;
}

inline short        Player::getScore() const
{
    return wins - losses;
}

inline const TimeKeeper &Player::getExplodeTime() const
{
    return explodeTime;
}

inline const TimeKeeper &Player::getTeleportTime() const
{
    return teleportTime;
}

inline short        Player::getFromTeleporter() const
{
    return fromTeleporter;
}

inline short        Player::getToTeleporter() const
{
    return toTeleporter;
}

inline float        Player::getTeleporterProximity() const
{
    return teleporterProximity;
}

inline const ShotStatistics*  Player::getShotStatistics() const
{
    return &shotStatistics;
}

inline bool     Player::isAlive() const
{
    return (state.status & short(PlayerState::Alive)) != 0;
}

inline bool     Player::isPaused() const
{
    return (state.status & short(PlayerState::Paused)) != 0;
}

inline bool Player::getPausedMessageState ( void ) const
{
    return pauseMessageState;
}

inline bool     Player::isAutoPilot() const
{
    return autoPilot;
}

inline void     Player::setAutoPilot(bool autopilot)
{
    autoPilot = autopilot;
}

inline bool     Player::isFalling() const
{
    return (state.status & short(PlayerState::Falling)) != 0;
}

inline bool     Player::isFlagActive() const
{
    return (state.status & short(PlayerState::FlagActive)) != 0;
}

inline bool     Player::isTeleporting() const
{
    return (state.status & short(PlayerState::Teleporting)) != 0;
}

inline bool     Player::isExploding() const
{
    return (state.status & short(PlayerState::Exploding)) != 0;
}

inline bool     Player::isPhantomZoned() const
{
    return (isFlagActive() && (getFlag() == Flags::PhantomZone));
}

inline bool     Player::isCrossingWall() const
{
    return (state.status & short(PlayerState::CrossingWall)) != 0;
}

inline bool     Player::isNotResponding() const
{
    return notResponding;
}

inline void     Player::resetNotResponding()
{
    notResponding = false;
}

inline bool     Player::isHunted() const
{
    return hunted;
}

inline void     Player::setHunted(bool _hunted)
{
    hunted = _hunted;
}

inline bool     Player::isAdmin() const
{
    return admin;
}

inline void     Player::setAdmin(bool _admin)
{
    admin = _admin;
}

inline bool     Player::isRegistered() const
{
    return registered;
}

inline void     Player::setRegistered(bool _registered)
{
    registered = _registered;
}

inline bool     Player::isVerified() const
{
    return verified;
}

inline void     Player::setVerified(bool _verified)
{
    verified = _verified;
}

inline bool     Player::hasPlayerList() const
{
    return playerList;
}

inline void     Player::setPlayerList(bool _playerList)
{
    playerList = _playerList;
}

inline void     Player::addHitToStats(FlagType* flag)
{
    shotStatistics.recordHit(flag);
}

inline void*        Player::pack(void* buf, uint16_t& code)
{
    setDeadReckoning();
    return state.pack(buf, code);
}

inline void Player::setZpos (float z)
{
    state.pos[2] = z;
}

#endif /* __PLAYER_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
