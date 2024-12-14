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

#ifndef __LOCALPLAYER_H__
#define __LOCALPLAYER_H__

/* interface header */
#include "BaseLocalPlayer.h"

/* system interface headers */
#include <string>
#include <vector>
#include <glm/vec3.hpp>

/* common interface headers */
#include "Obstacle.h"
#include "TimeKeeper.h"

/* local interface headers */
#include "Player.h"
#include "ServerLink.h"

class FlagSceneNode;

class LocalPlayer : public BaseLocalPlayer
{
public:
    enum FiringStatus
    {
        Deceased,       // can't shoot cos I'm dead
        Ready,      // ready to shoot
        Loading,        // reloading
        Sealed,     // I'm inside a building
        Zoned       // I'm zoned
    };
    enum Location
    {
        Dead,       // dead, explosion over
        Exploding,      // dead and exploding
        OnGround,       // playing on ground
        InBuilding,     // playing in building
        OnBuilding,     // playing on building
        InAir       // playing in air
    };
    enum InputMethod      // what device am I using to move around
    {
        Keyboard = 0,
        Mouse,
        Joystick
    };

    LocalPlayer(const PlayerId&,
                const char* name, const char* motto);
    ~LocalPlayer();

    Location  getLocation() const;
    FiringStatus  getFiringStatus() const;
    float     getFlagShakingTime() const;
    int       getFlagShakingWins() const;
    const glm::vec3 *getAntidoteLocation() const;
    ShotPath* getShot(int index) const override;
    const Player* getTarget() const;
    int       getDeathPhysicsDriver() const;
    const std::vector<const Obstacle*>& getInsideBuildings() const;

    void      setTeam(TeamColor);
    void      setDesiredSpeed(float fracOfMaxSpeed);
    void      setDesiredAngVel(float fracOfMaxAngVel);
    void      setPause(bool = true);
    void      activateAutoPilot(bool = true);
    bool      fireShot();
    void      explodeTank() override;
    void      doJump();
    void      setJump();
    void      setJumpPressed(bool value);
    void      setTarget(const Player*);

    float     getReloadTime() const;


    void      setNemesis(const Player*);
    const Player* getNemesis() const;

    void      setRecipient(const Player*);
    const Player* getRecipient() const;

    void      restart(const glm::vec3 &pos, float azimuth);
    bool      checkHit(const Player* source, const ShotPath*& hit,
                       float& minTime) const override;
    void      setFlag(FlagType*) override;
    void      changeScore(short deltaWins, short deltaLosses, short deltaTeamKills) override;

    void      addAntidote(SceneDatabase*);

    InputMethod   getInputMethod() const;
    void      setInputMethod(InputMethod newInput);
    void      setInputMethod(std::string newInput);
    static std::string    getInputMethodName(InputMethod whatInput);
    bool      queryInputChange();
    void      setKey(int button, bool pressed);
    int       getRotation();
    int       getSpeed();
    bool      isSpawning();
    void      setSpawning( bool spawn );


    static LocalPlayer*   getMyTank();
    static void       setMyTank(LocalPlayer*);

    const Obstacle*   getHitBuilding(const glm::vec3 &oldPos, float oldAngle,
                                     const glm::vec3 &pos, float angle,
                                     bool phased, bool& expel);
    bool      getHitNormal(const Obstacle* o,
                           const glm::vec3 &pos1, float azimuth1,
                           const glm::vec3 &pos2, float azimuth2,
                           glm::vec3 &normal) const;

protected:
    bool      doEndShot(int index, bool isHit, glm::vec3 &pos) override;
    void      doUpdate(float dt) override;
    void      doUpdateMotion(float dt) override;
    void      doMomentum(float dt, float& speed, float& angVel);
    void      doFriction(float dt,
                         const glm::vec2 &oldVelocity,
                         glm::vec3 &newVelocity);
    void      doForces(float dt, glm::vec3 &velocity, float& angVel);
    LocalShotPath**   shots;
    bool    gettingSound;
    ServerLink*   server;

private:
    void      doSlideMotion(float dt, float slideTime,
                            float newAngVel, glm::vec3 &newVelocity);
    float     getNewAngVel(float old, float desired);
    void      collectInsideBuildings();

private:
    Location  location;
    FiringStatus  firingStatus;
    TimeKeeper    bounceTime;
    TimeKeeper    agilityTime;
    float     flagShakingTime;
    int       flagShakingWins;
    glm::vec3 flagAntidotePos;
    FlagSceneNode*    antidoteFlag;
    float     desiredSpeed;
    float     desiredAngVel;
    float     lastSpeed;
    glm::vec4 crossingPlane;
    bool      anyShotActive;
    const Player* target;
    const Player* nemesis;
    const Player* recipient;
    static LocalPlayer*   mainPlayer;
    InputMethod   inputMethod;
    bool      inputChanged;
    int       stuckFrameCount;
    bool      spawning;
    int       wingsFlapCount;
    bool      left;
    bool      right;
    bool      up;
    bool      down;
    bool      entryDrop; // first drop since entering
    bool      wantJump;
    bool      jumpPressed;
    int       deathPhyDrv;    // physics driver that caused death
    std::vector<const Obstacle*> insideBuildings;
};


inline LocalPlayer::Location LocalPlayer::getLocation() const
{
    return location;
}

inline LocalPlayer::FiringStatus LocalPlayer::getFiringStatus() const
{
    return firingStatus;
}

inline const Player* LocalPlayer::getTarget() const
{
    return target;
}

inline const Player* LocalPlayer::getNemesis() const
{
    return nemesis;
}

inline const Player* LocalPlayer::getRecipient() const
{
    return recipient;
}

inline int      LocalPlayer::getDeathPhysicsDriver() const
{
    return deathPhyDrv;
}

inline const std::vector<const Obstacle*>& LocalPlayer::getInsideBuildings() const
{
    return insideBuildings;
}

inline LocalPlayer::InputMethod LocalPlayer::getInputMethod() const
{
    return inputMethod;
}

inline void LocalPlayer::setInputMethod(InputMethod newInput)
{
    inputMethod = newInput;
    inputChanged = true;
}

inline void LocalPlayer::setInputMethod(std::string newInput)
{
    // FIXME - using hardcoded upper bound is ugly
    for (int i = 0; i < 3; i++)
    {
        if (newInput == getInputMethodName((InputMethod)i))
            setInputMethod((InputMethod)i);
    }
}

inline bool LocalPlayer::queryInputChange()
{
    const bool returnVal = inputChanged;
    inputChanged = false;
    return returnVal;
}

inline bool LocalPlayer::isSpawning()
{
    return spawning;
}

inline void LocalPlayer::setSpawning( bool spawn )
{
    spawning = spawn;
}

inline int LocalPlayer::getRotation()
{
    if (left && !right)
        return 1;
    else if (right && !left)
        return -1;
    else
        return 0;
}

inline int LocalPlayer::getSpeed()
{
    if (up && !down)
        return 1;
    else if (down && !up)
        return -1;
    else
        return 0;
}

inline LocalPlayer* LocalPlayer::getMyTank()
{
    return mainPlayer;
}

#endif /* __LOCALPLAYER_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
