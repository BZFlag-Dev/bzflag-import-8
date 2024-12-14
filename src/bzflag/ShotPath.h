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
 * ShotPath:
 *  Encapsulates the path a shot follows.  Most paths can
 *  be computed at the instant of firing (though they may
 *  terminate early because of a hit).  Some paths need
 *  to be updated continuously during flight.
 *
 * RemoteShotPath:
 *  A ShotPath acting as a proxy for a remote ShotPath.
 *  Created by a LocalPlayer on behalf of a RemotePlayer.
 */

#ifndef __SHOTPATH_H__
#define __SHOTPATH_H__

#include "common.h"

/* common interface headers */
#include "TimeKeeper.h"
#include "Flag.h"
#include "ShotUpdate.h"

/* local interface headers */
#include "BaseLocalPlayer.h"
#include "ShotStrategy.h"
#include "SceneDatabase.h"

class ShotStrategy;

class ShotPath
{
public:
    virtual     ~ShotPath();

    bool        isExpiring() const;
    bool        isExpired() const;
    bool        isReloaded() const;
    const PlayerId& getPlayer() const;
    uint16_t        getShotId() const;
    FlagType*       getFlag() const;
    float       getLifetime() const;
    float       getReloadTime() const;
    const TimeKeeper&   getStartTime() const;
    const TimeKeeper&   getCurrentTime() const;
    const glm::vec3  &getPosition() const;
    const glm::vec3   getVelocity() const;

    float       checkHit(const BaseLocalPlayer*, glm::vec3 &position) const;
    void        setExpiring();
    void        setExpired();
    bool        isStoppedByHit() const;
    void        boostReloadTime(float dt);

    void        addShot(SceneDatabase*, bool colorblind);

    void        radarRender() const;
    FiringInfo&     getFiringInfo();
    TeamColor       getTeam() const;
protected:
    ShotPath(const FiringInfo&);
    void        updateShot(float dt);
    const ShotStrategy* getStrategy() const;
    ShotStrategy*   getStrategy();

    friend class ShotStrategy;
    void        setReloadTime(float);
    void        setPosition(const glm::vec3 &p);
    void        setVelocity(const glm::vec3 &v);

private:
    ShotStrategy*   strategy;       // strategy for moving shell
    FiringInfo      firingInfo;     // shell information
    float       reloadTime;     // time to reload
    TimeKeeper      startTime;      // time of firing
    TimeKeeper      currentTime;        // current time
    bool        expiring;       // shot has almost terminated
    bool        expired;        // shot has terminated
};

class LocalShotPath : public ShotPath
{
public:
    LocalShotPath(const FiringInfo&);
    ~LocalShotPath();

    void        update(float dt);
};

class RemoteShotPath : public ShotPath
{
public:
    RemoteShotPath(const FiringInfo&);
    ~RemoteShotPath();

    void        update(float dt);
    void        update(const ShotUpdate& shot,
                       uint16_t code, const void* msg);
};

//
// ShotPath
//

inline bool     ShotPath::isExpiring() const
{
    return expiring;
}

inline bool     ShotPath::isExpired() const
{
    return expired;
}

inline bool     ShotPath::isReloaded() const
{
    return (currentTime - startTime >= reloadTime);
}

inline const PlayerId&  ShotPath::getPlayer() const
{
    return firingInfo.shot.player;
}

inline uint16_t     ShotPath::getShotId() const
{
    return firingInfo.shot.id;
}

inline FlagType*    ShotPath::getFlag() const
{
    return firingInfo.flagType;
}

inline float        ShotPath::getLifetime() const
{
    return firingInfo.lifetime;
}

inline float        ShotPath::getReloadTime() const
{
    return reloadTime;
}

inline const TimeKeeper &ShotPath::getStartTime() const
{
    return startTime;
}

inline const TimeKeeper &ShotPath::getCurrentTime() const
{
    return currentTime;
}

inline const glm::vec3 &ShotPath::getPosition() const
{
    return firingInfo.shot.pos;
}

inline const glm::vec3 ShotPath::getVelocity() const
{
    return firingInfo.shot.vel;
}

inline FiringInfo&  ShotPath::getFiringInfo()
{
    return firingInfo;
}

inline  TeamColor   ShotPath::getTeam() const
{
    return firingInfo.shot.team;
}

inline const ShotStrategy*  ShotPath::getStrategy() const
{
    return strategy;
}

inline ShotStrategy*    ShotPath::getStrategy()
{
    return strategy;
}

#endif /* __SHOTPATH_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
