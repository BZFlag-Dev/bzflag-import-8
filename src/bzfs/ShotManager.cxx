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
#include "ShotManager.h"

// System headers
#include <glm/trigonometric.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

// Common headers
#include "TimeKeeper.h"


namespace Shots
{
//----------------Manager

double Manager::DeadShotCacheTime = 10.0;

Manager::Manager()
{
    Logics[std::string("")] = new FlightLogic();
    LastGUID = INVALID_SHOT_GUID;
}

Manager::~Manager()
{
    for (FlightLogicMap::iterator itr = Logics.begin(); itr != Logics.end(); itr++)
        delete(itr->second);

    Logics.clear();

    LiveShots.clear();

    RecentlyDeadShots.clear();
}

void Manager::Init()
{
    Logics[Flags::GuidedMissile->flagAbbv] = new GuidedMissileLogic();
    Logics[Flags::SuperBullet->flagAbbv] = new SuperBulletLogic();
    Logics[Flags::ShockWave->flagAbbv] = new ShockwaveLogic();
}

void Manager::SetFlightLogic(const char* flagCode, FlightLogic* logic)
{
    std::string code;
    if (flagCode)
        code = flagCode;

    if (Logics.find(code) != Logics.end())
        delete(Logics.find(code)->second);

    Logics[code] = logic;
}

uint32_t Manager::AddShot( const FiringInfo &info, PlayerId UNUSED(shooter) )
{
    FlightLogic* logic = NULL;
    if (Logics.find(info.flagType->flagAbbv) != Logics.end())
        logic = Logics[info.flagType->flagAbbv];

    if (!logic)
        logic = Logics[""];

    ShotRef shot(new Shot(NewGUID(),info,*logic));

    shot->LastUpdateTime = shot->StartTime = Now();
    logic->Setup(*shot);
    shot->Update(); // to get the initial position
    shot->StartPosition = shot->LastUpdatePosition;

    LiveShots.push_back(shot);

    if (ShotCreated)
        (*ShotCreated)(*shot);
    return shot->GetGUID();
}

void Manager::RemoveShot (uint32_t shotID)
{
    ShotList::iterator itr = LiveShots.begin();
    while ( itr != LiveShots.end())
    {
        if ((*itr)->GetGUID() == shotID)
        {
            (*itr)->End();
            RecentlyDeadShots.push_back((*itr));
            itr = LiveShots.erase(itr);
            if (ShotEnded)
                (*ShotEnded)(*(*itr));
            return;
        }
        else
            itr++;
    }
}

void Manager::RemovePlayer( PlayerId player )
{
    ShotList::iterator itr = LiveShots.begin();
    while ( itr != LiveShots.end())
    {
        if ((*itr)->GetPlayerID() == player)
        {
            (*itr)->End();
            itr = LiveShots.erase(itr);
        }
        else
            itr++;
    }
}

void Manager::SetShotTarget( uint32_t shotID, PlayerId target )
{
    ShotRef shot = FindByID(shotID);
    if (shot)
        shot->Retarget(target);
}

uint32_t Manager::FindShotGUID (PlayerId shooter, uint16_t localShotID)
{
    for (ShotList::iterator itr = LiveShots.begin(); itr != LiveShots.end(); itr++)
    {
        if ((*itr)->GetPlayerID() == shooter && (*itr)->Info.shot.id == localShotID)
            return (*itr)->GetGUID();
    }

    for (ShotList::iterator itr = RecentlyDeadShots.begin(); itr != RecentlyDeadShots.end(); itr++)
    {
        if ((*itr)->GetPlayerID() == shooter && (*itr)->Info.shot.id == localShotID)
            return (*itr)->GetGUID();
    }

    return 0;
}

uint32_t Manager::NewGUID()
{
    LastGUID++;

    if (LastGUID == INVALID_SHOT_GUID) // handle the rollover
        LastGUID = INVALID_SHOT_GUID + 1;

    return LastGUID;
}

ShotRef Manager::FindByID (uint32_t shotID)
{
    for (ShotList::iterator itr = LiveShots.begin(); itr != LiveShots.end(); itr++)
    {
        if ((*itr)->GetGUID() == shotID)
            return *itr;
    }

    for (ShotList::iterator itr = RecentlyDeadShots.begin(); itr != RecentlyDeadShots.end(); itr++)
    {
        if ((*itr)->GetGUID() == shotID)
            return *itr;
    }

    return ShotRef();
}

double Manager::Now()
{
    return TimeKeeper::getCurrent().getSeconds();
}

void Manager::Update()
{
    double now = Now();

    ShotList::iterator itr = LiveShots.begin();
    while ( itr != LiveShots.end())
    {
        (*itr)->LastUpdateTime = now;
        if ((*itr)->Update())
        {
            ShotRef shot = *itr;
            itr = LiveShots.erase(itr);
            shot->End();
            RecentlyDeadShots.push_back(shot);
        }
        else
            itr++;
    }

    itr = RecentlyDeadShots.begin();

    while ( itr != RecentlyDeadShots.end())
    {
        if (now - (*itr)->GetLastUpdateTime() >= Manager::DeadShotCacheTime)
            itr = RecentlyDeadShots.erase(itr);
        else
            itr++;
    }
}

ShotList Manager::LiveShotsForPlayer( PlayerId player )
{
    ShotList list;

    for (ShotList::iterator itr = LiveShots.begin(); itr != LiveShots.end(); itr++)
    {
        if ((*itr)->GetPlayerID() == player)
            list.push_back(*itr);
    }

    return list;
}

ShotList Manager::DeadShotsForPlayer( PlayerId player )
{
    ShotList list;

    for (ShotList::iterator itr = RecentlyDeadShots.begin(); itr != RecentlyDeadShots.end(); itr++)
    {
        if ((*itr)->GetPlayerID() == player)
            list.push_back(*itr);
    }

    return list;
}

//----------------Shot

Shot::Shot(uint32_t guid, const FiringInfo &info, FlightLogic& logic): GUID(guid), Logic(logic), LastUpdateTime(-1.0),
    Info(info), Pimple(NULL)
{
    StartTime = -1;
    LifeTime = info.lifetime;
    Target = NoPlayer;
}

Shot::~Shot()
{
}

void Shot::SetMetaData(const std::string& name, const char* data)
{
    if (MetaData.find(name) == MetaData.end())
        MetaData[name] = MetaDataItem();
    MetaData[name].DataS = data;
}

void Shot::SetMetaData(const std::string& name, uint32_t data)
{
    if (MetaData.find(name) == MetaData.end())
        MetaData[name] = MetaDataItem();
    MetaData[name].DataI = data;
}

bool Shot::HasMetaData(const std::string& name)
{
    return MetaData.find(name) != MetaData.end();
}

const char * Shot::GetMetaDataS(const std::string& name)
{
    auto itr = MetaData.find(name);
    return itr == MetaData.end() ? nullptr : itr->second.DataS.c_str();
}

uint32_t Shot::GetMetaDataI(const std::string& name)
{
    auto itr = MetaData.find(name);
    return itr == MetaData.end() ? 0 : itr->second.DataI;
}

bool Shot::Update()
{
    if (Logic.Update(*this))
        return true;
    return false;
}

void Shot::End()
{
    Logic.End(*this);
    LastUpdateTime = TimeKeeper::getCurrent().getSeconds();
}

void Shot::Retarget(PlayerId target)
{
    Logic.Retarget(*this,target);
    Target = target;
}


//----------------ProjectileShotLogic
bool FlightLogic::Update ( Shot& shot)
{
    return shot.GetLastUpdateTime() - shot.GetStartTime() >= shot.GetLifeTime();
}

glm::vec3 FlightLogic::ProjectShotLocation(Shot& shot, double deltaT)
{
    const auto vec = shot.LastUpdatePosition + shot.Info.shot.vel * float(deltaT);

    return vec;
}

//----------------ProjectileShotLogic
bool ProjectileShotLogic::Update ( Shot& shot )
{
    shot.LastUpdatePosition = ProjectShotLocation(shot,shot.GetLastUpdateTime() - shot.GetStartTime());
    return FlightLogic::Update(shot );
}

//----------------GuidedMissileLogic
void GuidedMissileLogic::End (  Shot& UNUSED(shot) )
{

}

//----------------ShockwaveLogic
void ShockwaveLogic::Setup( Shot& shot )
{
    shot.LastUpdatePosition.x = BZDB.eval(StateDatabase::BZDB_SHOCKINRADIUS);
    shot.LastUpdatePosition.y = 0;
    shot.LastUpdatePosition.z = 0;
}

bool ShockwaveLogic::Update ( Shot& shot )
{
    float delta = BZDB.eval(StateDatabase::BZDB_SHOCKOUTRADIUS) - BZDB.eval(StateDatabase::BZDB_SHOCKINRADIUS);
    shot.LastUpdatePosition.x =  BZDB.eval(StateDatabase::BZDB_SHOCKINRADIUS) + (float)(delta * shot.GetLifeParam());
    shot.LastUpdatePosition.y = 0;
    shot.LastUpdatePosition.z = 0;
    return FlightLogic::Update(shot);
}

bool ShockwaveLogic::CollideBox (
    Shot& shot, glm::vec3& center, glm::vec3& size, float rotation)
{
    // check the top locations
    glm::vec3 xyPlus = size;
    glm::vec3 xyNeg(-size.x,-size.y,size.z);
    glm::vec3 xPlusYNeg(size.x,-size.y,size.z);
    glm::vec3 xNegYPlus(-size.x,size.y,size.z);

    float rotRads = glm::radians(rotation);

    // rotate them all into orientation
    xyPlus    = glm::rotateZ(xyPlus,    rotRads);
    xyNeg     = glm::rotateZ(xyNeg,     rotRads);
    xPlusYNeg = glm::rotateZ(xPlusYNeg, rotRads);
    xNegYPlus = glm::rotateZ(xNegYPlus, rotRads);

    // attach them to the center
    xyPlus += center;
    xyNeg += center;
    xPlusYNeg += center;
    xNegYPlus += center;

    // check the top
    if (PointInSphere(xyPlus,shot) || PointInSphere(xyNeg,shot) || PointInSphere(xPlusYNeg,shot)
            || PointInSphere(xNegYPlus,shot))
        return true;

    // check the bottom
    xyPlus.z = center.z;
    xyNeg.z = center.z;
    xPlusYNeg.z = center.z;
    xNegYPlus.z = center.z;

    if (PointInSphere(xyPlus,shot) || PointInSphere(xyNeg,shot) || PointInSphere(xPlusYNeg,shot)
            || PointInSphere(xNegYPlus,shot))
        return true;

    return false;
}

bool ShockwaveLogic::CollideSphere (
    Shot& shot, glm::vec3 &center, float radius)
{
    glm::vec3 vecToPoint = center - shot.StartPosition;
    return vecToPoint.length() <= shot.LastUpdatePosition.x - radius;
}

bool ShockwaveLogic::CollideCylinder (
    Shot& shot, glm::vec3 &center, float height, float radius)
{
    if (center.z > shot.StartPosition.z + shot.LastUpdatePosition.x)
        return false; // too high

    if (center.z + height < shot.StartPosition.z - shot.LastUpdatePosition.x)
        return false; // too low

    glm::vec3 vecToPoint = center - shot.StartPosition;
    vecToPoint.z = 0;

    return vecToPoint.length() <= shot.LastUpdatePosition.x - radius;
}

bool ShockwaveLogic::PointInSphere (glm::vec3 &point, Shot& shot)
{
    glm::vec3 vecToPoint = point - shot.StartPosition;
    return vecToPoint.length() <= shot.LastUpdatePosition.x;
}
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
