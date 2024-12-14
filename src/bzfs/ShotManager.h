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

#ifndef __SHOTMANAGER_H__
#define __SHOTMANAGER_H__

#include "common.h"

// System headers
#include <string>
#include <vector>
#include <map>
#ifdef USE_TR1
#include <tr1/memory>
#include <tr1/functional>
#define shared_ptr  tr1::shared_ptr
#define function    tr1::function
#else
#include <memory>
#include <functional>
#endif
#include <glm/vec3.hpp>

/* common interface headers */
#include "global.h"  /* for TeamColor */
#include "ShotUpdate.h"

/** a ShotManager is used track shots fired by players and the server
 */

namespace Shots
{
class Shot;

class FlightLogic
{
public:
    virtual ~FlightLogic() {}

    virtual void Setup(Shot& UNUSED(shot) ) {}
    virtual bool Update ( Shot& UNUSED(shot) ); // call the base class for lifetime expire
    virtual void End ( Shot& UNUSED(shot) ) {}
    virtual void Retarget ( Shot& UNUSED(shot), PlayerId UNUSED(newTarget) ) {};

    virtual bool CollideBox (
        Shot& UNUSED(shot), glm::vec3 &UNUSED(center),
        glm::vec3 &UNUSED(size), float UNUSED(rotation))
    {
        return false;
    }
    virtual bool CollideSphere ( Shot& UNUSED(shot), glm::vec3 &UNUSED(center), float UNUSED(radius) )
    {
        return false;
    }
    virtual bool CollideCylinder (
        Shot& UNUSED(shot), glm::vec3 &UNUSED(center),
        float UNUSED(height), float UNUSED(radius) )
    {
        return false;
    }

protected:
    virtual glm::vec3 ProjectShotLocation( Shot& shot, double deltaT );
};

typedef std::map<std::string, FlightLogic*> FlightLogicMap;

class Shot
{
protected:
    uint32_t GUID;
    FlightLogic &Logic;

    double  LifeTime;

    class MetaDataItem
    {
    public:
        std::string Name;
        std::string DataS;
        uint32_t    DataI;
    };

    std::map<std::string, MetaDataItem> MetaData;

public:
    glm::vec3   StartPosition;
    glm::vec3   LastUpdatePosition;
    double      LastUpdateTime;
    double      StartTime;

    FiringInfo  Info;

    PlayerId    Target;

    void        *Pimple;

    Shot(uint32_t guid, const FiringInfo &info, FlightLogic& logic);
    virtual ~Shot();

    bool Update();
    void End();
    void Retarget(PlayerId target);

    uint32_t GetGUID()
    {
        return GUID;
    }
    uint16_t GetLocalShotID()
    {
        return Info.shot.player;
    }

    PlayerId GetPlayerID()
    {
        return Info.shot.player;
    }

    double GetLastUpdateTime()
    {
        return LastUpdateTime;
    }
    double GetStartTime()
    {
        return StartTime;
    }
    double GetLifeTime()
    {
        return LifeTime;
    }

    double GetLifeParam()
    {
        return (LastUpdateTime-StartTime)/LifeTime;
    }

    bool CollideBox (glm::vec3 &center, glm::vec3 size, float rotation)
    {
        return Logic.CollideBox(*this,center,size,rotation);
    }
    bool CollideSphere (glm::vec3 &center, float radius)
    {
        return Logic.CollideSphere(*this,center,radius);
    }
    bool CollideCylinder (glm::vec3 &center, float height, float radius)
    {
        return Logic.CollideCylinder(*this,center,height,radius);
    }

    // meta data API
    void SetMetaData(const std::string& name, const char* data);
    void SetMetaData(const std::string& name, uint32_t data);
    bool HasMetaData(const std::string& name);
    const char * GetMetaDataS(const std::string& name);
    uint32_t GetMetaDataI(const std::string& name);

};

typedef std::shared_ptr<Shot>   ShotRef;
typedef std::vector<std::shared_ptr<Shot>> ShotList;
typedef std::shared_ptr<std::function <void (Shot&)> > ShotEvent;

#ifdef USE_TR1
// limit the scope of possible side effects of these macro definitions
#undef  shared_ptr
#undef  function
#endif

#define INVALID_SHOT_GUID 0

class Manager
{
public:
    Manager();
    virtual ~Manager();

    void Init();

    void SetFlightLogic(const char* flagCode, FlightLogic* logic);

    uint32_t AddShot (const FiringInfo &info, PlayerId shooter);
    void RemoveShot (uint32_t shotID);

    void RemovePlayer (PlayerId player);

    void SetShotTarget( uint32_t shot, PlayerId target );

    uint32_t FindShotGUID (PlayerId shooter, uint16_t localShotID);
    ShotRef FindShot(uint32_t shotID)
    {
        return FindByID(shotID);
    }

    void Update();

    static double DeadShotCacheTime;

    ShotList  LiveShotsForPlayer(PlayerId player);
    ShotList  DeadShotsForPlayer(PlayerId player);

    ShotEvent ShotCreated;
    ShotEvent ShotEnded;

private:
    uint32_t NewGUID();
    ShotRef FindByID(uint32_t shotID);

    double Now();

    ShotList    LiveShots;
    ShotList    RecentlyDeadShots;

    FlightLogicMap Logics;

    uint32_t    LastGUID;

};

class ProjectileShotLogic: public FlightLogic
{
public:
    virtual ~ProjectileShotLogic() {}

    virtual bool Update ( Shot& shot );
};

class GuidedMissileLogic: public ProjectileShotLogic
{
public:
    virtual ~GuidedMissileLogic() {}

    virtual void End ( Shot& UNUSED(shot) );
};

class SuperBulletLogic: public ProjectileShotLogic
{
public:
};

class ShockwaveLogic: public FlightLogic
{
public:
    virtual void Setup(Shot& shot );
    virtual bool Update ( Shot& shot );

    virtual bool CollideBox (
        Shot& shot, glm::vec3 &center,
        glm::vec3& size, float rotation);
    virtual bool CollideSphere (
        Shot& shot, glm::vec3 &center, float radius );
    virtual bool CollideCylinder (
        Shot& shot, glm::vec3 &center, float height, float radius);

protected:
    bool PointInSphere (glm::vec3 &point, Shot& shot);
};
}
#endif  /* __SHOT_MANAGER_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
