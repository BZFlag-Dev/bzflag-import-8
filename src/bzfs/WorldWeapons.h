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

#ifndef __WORLDWEAPON_H__
#define __WORLDWEAPON_H__

/* common header */
#include "common.h"

/* system headers */
#include <vector>

/* common interface headers */
#include "Flag.h"
#include "TimeKeeper.h"

#include "WorldEventManager.h"
#include "PlayerInfo.h"

#define _MAX_WORLD_SHOTS 255

/** WorldWeapons is a container class that holds weapons
 */
class WorldWeapons
{
public:
    WorldWeapons();
    ~WorldWeapons();
    void fire();
    void add(const FlagType *type, const glm::vec3 &origin,
             float direction, float tilt, TeamColor teamColor,
             float initdelay, const std::vector<float> &delay,
             TimeKeeper &sync);
    float nextTime();
    void clear();
    unsigned int count(); // returns the number of world weapons
    int packSize() const;
    void *pack(void *buf) const;

    uint32_t fireShot(FlagType* type, const glm::vec3 &origin,
                      const float vector[3], int *shotID,
                      TeamColor teamColor = RogueTeam,
                      PlayerId targetPlayerID = -1);

private:
    struct Weapon
    {
        const FlagType  *type;
        TeamColor       teamColor;
        glm::vec3       origin;
        float       direction;
        float       tilt;
        float   initDelay;
        std::vector<float>  delay;
        TimeKeeper      nextTime;
        int         nextDelay;
    };

    std::vector<Weapon*> weapons;
    int worldShotId;

    int getNewWorldShotID(void);

    WorldWeapons( const WorldWeapons &w);
    WorldWeapons& operator=(const WorldWeapons &w) const;
};

class WorldWeaponGlobalEventHandler : public bz_EventHandler
{
public:
    WorldWeaponGlobalEventHandler(FlagType *type, const glm::vec3 &origin,
                                  float direction, float tilt,
                                  TeamColor teamColor);
    virtual ~WorldWeaponGlobalEventHandler();

    virtual void process ( bz_EventData *eventData );

protected:
    FlagType    *type;
    glm::vec3   origin;
    float       direction;
    float       tilt;
    bz_eTeamType    team;
};

#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
