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

#ifndef __WORLD_H__
#define __WORLD_H__

#include "common.h"

/* system interface headers */
#include <vector>
#include <string>

/* common interface headers */
#include "Team.h"
#include "FlagWarpSceneNode.h"
#include "BundleMgr.h"
#include "LinkManager.h"

/* local interface headers */
#include "RemotePlayer.h"
#include "WorldPlayer.h"
#include "Weapon.h"
#include "EntryZone.h"

class FlagSceneNode;
class MeshDrawInfo;

/**
 * World:
 *  Game database -- buildings, teleporters, game style
 */
class World
{
    friend class WorldBuilder;
public:
    World();
    ~World();

    bool        allowTeamFlags() const;
    bool        allowTeamKills() const;
    bool        allowSuperFlags() const;
    bool        allowJumping() const;
    bool        allShotsRicochet() const;
    bool        allowAntidote() const;
    bool        allowShakeTimeout() const;
    bool        allowShakeWins() const;
    bool        allowRabbit() const;
    bool        allowHandicap() const;
    bool        allowTeams() const;
    float       getWaterLevel() const;
    const BzMaterial*   getWaterMaterial() const;
    const BzMaterial*   getLinkMaterial() const;
    float       getLinearAcceleration() const;
    float       getAngularAcceleration() const;
    float       getFlagShakeTimeout() const;
    int         getFlagShakeWins() const;
    int         getMaxPlayers() const;
    int         getCurMaxPlayers() const;
    void        setCurMaxPlayers(int curMaxPlayers);
    int         getMaxShots() const;
    int         getMaxFlags() const;
    float       getShakeTimeout() const;
    int         getShakeWins() const;
    const Team*     getTeams() const;
    const Team&     getTeam(int index) const;
    Team*       getTeams();
    Team&       getTeam(int index);
    RemotePlayer**  getPlayers() const;
    RemotePlayer*&  getPlayer(int index) const;
    RemotePlayer*   getCurrentRabbit() const;
    WorldPlayer*    getWorldWeapons() const;
    Flag&       getFlag(int index) const;
    bool isBase(int, int=0) const;
    bool getBase(int _team, int base, glm::vec3 &pos,
                 float &rotation, float &w, float &b, float &h) const;
    const Teleporter*   getTeleporter(int source, int& face) const;
    int         getTeleporter(const Teleporter*, int face) const;
    int         getTeleportTarget(int source) const;
    int         getTeleportTarget(int source, unsigned int seed) const;

    TeamColor       whoseBase(const glm::vec3 &pos) const;
    const Obstacle* inBuilding(const glm::vec3 &pos, float radius,
                               float tankHeight) const;
    bool inBuilding(const glm::vec3 &pos, float angle,
                    float tankWidth, float tankBreadth) const;
    const Obstacle* hitBuilding(const glm::vec3 &pos, float angle,
                                float tankWidth, float tankBreadth,
                                float tankHeight) const;
    const Obstacle* hitBuilding(const glm::vec3 &oldPos, float oldAngle,
                                const glm::vec3 &pos, float angle,
                                float tankWidth, float tankBreadth,
                                float tankHeight, bool directional) const;
    bool        crossingTeleporter(const glm::vec3 &oldPos, float angle,
                                   float tankWidth, float tankBreadth,
                                   float tankHeight, glm::vec4 &plane) const;
    const Teleporter*   crossesTeleporter(const glm::vec3 &oldPos,
                                          const glm::vec3 &newPos, int& face) const;
    const Teleporter*   crossesTeleporter(const Ray& r, int& face) const;
    float       getProximity(const glm::vec3 &pos, float radius) const;

    void        initFlag(int index);
    void        updateFlag(int index, float dt);
    void        updateAnimations(float dt);
    void        addFlags(SceneDatabase*, bool seerView);
    void        updateWind(float dt);
    void        getWind(float wind[3], const glm::vec3 &pos) const;

    void        makeMeshDrawMgrs();

    static World*   getWorld();
    static void     setWorld(World*);

    static BundleMgr*   getBundleMgr();
    static void     setBundleMgr(BundleMgr *bundleMgr);

    static std::string  getLocale();
    static void     setLocale(const std::string &locale);

    static void     init();
    static void     done();
    static void     setFlagTexture(FlagSceneNode*);

    void        makeLinkMaterial();

    void        loadCollisionManager();
    void        checkCollisionManager();

    bool        writeWorld(const std::string& filename,
                           std::string& fullname);

    void        drawCollisionGrid() const;

    void        freeInsideNodes() const;

private:
    // disallow copy and assignment
    World(const World&);
    World&      operator=(const World&);

    void        freeFlags();
    void        freeMeshDrawMgrs();

private:
    short       gameType;
    short   gameOptions;
    float       linearAcceleration;
    float       angularAcceleration;
    int         maxPlayers;
    int         curMaxPlayers;
    int         maxShots;
    int         maxFlags;
    float       shakeTimeout;
    int         shakeWins;
    float       waterLevel;
    const BzMaterial*   waterMaterial;
    const BzMaterial*   linkMaterial;

    typedef struct
    {
        glm::vec3 pos;
        float rotation;
        float w;
        float b;
        float h;
    } BaseParms;
    typedef std::vector<BaseParms> TeamBases;
    TeamBases       bases[NumTeams];
    Team        team[NumTeams];

    std::vector<Weapon> weapons;
    std::vector<EntryZone> entryZones;

    RemotePlayer**  players;
    WorldPlayer*    worldWeapons;
    Flag*       flags;
    FlagSceneNode** flagNodes;
    FlagWarpSceneNode** flagWarpNodes;

    int         drawInfoCount;
    MeshDrawInfo**  drawInfoArray;

    float       wind[3];

    LinkManager     links;

    // required graphics settings
    int         oldFogEffect;
    bool        oldUseDrawInfo;

    static World*   playingField;
    static BundleMgr    *bundleMgr;
    static std::string  locale;
    static int flagTexture;
};


//
// World
//

inline  bool        World::allowTeams() const
{
    return gameType != OpenFFA;
}

inline bool     World::allowTeamFlags() const
{
    return gameType == ClassicCTF;
}

inline bool     World::allowTeamKills() const
{
    return (gameOptions & short(NoTeamKillsGameStyle)) == 0;
}

inline bool     World::allowSuperFlags() const
{
    return (gameOptions & short(SuperFlagGameStyle)) != 0;
}

inline bool     World::allowJumping() const
{
    return (gameOptions & short(JumpingGameStyle)) != 0;
}

inline bool     World::allShotsRicochet() const
{
    return (gameOptions & short(RicochetGameStyle)) != 0;
}

inline bool     World::allowAntidote() const
{
    return (gameOptions & short(AntidoteGameStyle)) != 0;
}

inline bool     World::allowShakeTimeout() const
{
    return (gameOptions & short(ShakableGameStyle)) != 0 && shakeTimeout != 0.0f;
}

inline bool     World::allowShakeWins() const
{
    return (gameOptions & short(ShakableGameStyle)) != 0 && shakeWins != 0;
}

inline bool     World::allowRabbit() const
{
    return gameType == RabbitChase;
}

inline bool     World::allowHandicap() const
{
    return (gameOptions & short(HandicapGameStyle)) != 0;
}

inline float        World::getWaterLevel() const
{
    return waterLevel;
}

inline const BzMaterial*    World::getWaterMaterial() const
{
    return waterMaterial;
}

inline const BzMaterial*    World::getLinkMaterial() const
{
    return linkMaterial;
}

inline float        World::getLinearAcceleration() const
{
    return linearAcceleration;
}

inline float        World::getAngularAcceleration() const
{
    return angularAcceleration;
}

inline float        World::getFlagShakeTimeout() const
{
    return shakeTimeout;
}

inline int      World::getFlagShakeWins() const
{
    return shakeWins;
}

inline int      World::getMaxPlayers() const
{
    return maxPlayers;
}

inline int      World::getCurMaxPlayers() const
{
    return curMaxPlayers;
}

inline void     World::setCurMaxPlayers(int _curMaxPlayers)
{
    curMaxPlayers = _curMaxPlayers;
}

inline int      World::getMaxShots() const
{
    return maxShots;
}

inline int      World::getMaxFlags() const
{
    return maxFlags;
}

inline float        World::getShakeTimeout() const
{
    return shakeTimeout;
}

inline int      World::getShakeWins() const
{
    return shakeWins;
}

inline const Team*  World::getTeams() const
{
    return team;
}

inline Team*        World::getTeams()
{
    return team;
}

inline const Team&  World::getTeam(int index) const
{
    return team[index];
}

inline Team&        World::getTeam(int index)
{
    return team[index];
}

inline RemotePlayer**   World::getPlayers() const
{
    return players;
}

inline RemotePlayer*&   World::getPlayer(int index) const
{
    return players[index];
}

inline WorldPlayer* World::getWorldWeapons() const
{
    return worldWeapons;
}

inline Flag&        World::getFlag(int index) const
{
    return flags[index];
}

inline bool World::isBase(int _team, int base) const
{
    if (base < 0)
        return false;

    const TeamBases &b = bases[_team];
    return base < (int)b.size();
}

inline bool World::getBase(
    int _team, int base,
    glm::vec3 &pos,
    float &rotation,
    float &w, float &b, float &h) const
{
    const TeamBases &base_ = bases[_team];
    if ((base < 0) || (base >= (int)base_.size()))
        return false;

    pos      = base_[base].pos;
    rotation = base_[base].rotation;
    w        = base_[base].w;
    b        = base_[base].b;
    h        = base_[base].h;
    return true;
}

inline World*       World::getWorld()
{
    return playingField;
}

inline BundleMgr*   World::getBundleMgr()
{
    return World::bundleMgr;
}

inline void     World::setBundleMgr(BundleMgr *_bundleMgr)
{
    bundleMgr = _bundleMgr;
}

inline std::string  World::getLocale()
{
    return locale;
}

inline void     World::setLocale(const std::string& _locale)
{
    locale = _locale;
}

inline void     World::getWind(float w[3], const glm::vec3 &) const
{
    // homogeneous, for now
    w[0] = wind[0];
    w[1] = wind[1];
    w[2] = wind[2];
    return;
}

#endif /* __WORLD_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
