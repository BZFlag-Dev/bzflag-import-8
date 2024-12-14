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

#include "EntryZones.h"
#include "CustomZone.h"

#include <string>
#include <vector>

#include "global.h"
#include "Protocol.h"
#include "Flag.h"
#include "Team.h"

#include "WorldInfo.h"


EntryZones::EntryZones()
{
}


const ZoneList& EntryZones::getZoneList() const
{
    return zones;
}


void EntryZones::addZone(const CustomZone *zone)
{
    zones.push_back(*zone);
}


void EntryZones::addZoneFlag(int zone, int flagId)
{
    if (zone >= (int)zones.size())
    {
        printf ("Internal error: EntryZones::addZoneFlag() unknown zone\n");
        exit(1);
    }
    const std::string& qualifier = CustomZone::getFlagIdQualifier(flagId);
    QPairList &qPairList = qmap[qualifier];
    if (!qPairList.empty())
    {
        printf ("Internal error: EntryZones::addZoneFlag() duplicate\n");
        exit(1);
    }
    qPairList.push_back(std::pair<int,float>(zone, 1.0f));
    return;
}


void EntryZones::calculateQualifierLists()
{
    // generate the qualifier lists
    for (unsigned int i = 0; i < zones.size(); i++)
    {
        QualifierList qualifiers = zones[i].getQualifiers();
        for (QualifierList::iterator it = qualifiers.begin(); it != qualifiers.end(); ++it)
        {
            std::string qualifier = *it;
            QPairList &qPairList = qmap[qualifier];
            qPairList.push_back(std::pair<int,float>(i, 0.0f));
        }
    }

    // calculate the qualifier weights
    for (QualifierMap::iterator mit = qmap.begin(); mit != qmap.end(); ++mit)
    {
        QPairList &qPairList = mit->second;
        float total = 0.0f;
        QPairList::iterator vit;
        for (vit = qPairList.begin(); vit != qPairList.end(); ++vit)
        {
            std::pair<int,float> &p = *vit;
            int zoneIndex = p.first;
            p.second = zones[zoneIndex].getArea();
            total += p.second;
        }
        for (vit = qPairList.begin(); vit != qPairList.end(); ++vit)
        {
            std::pair<int,float> &p = *vit;
            p.second /= total;
        }
    }
}


bool EntryZones::getRandomPoint(const std::string &qual, glm::vec3 &pt) const
{
    QualifierMap::const_iterator mit = qmap.find(qual);
    if (mit == qmap.end())
        return false;

    const QPairList &qPairList = mit->second;

    float rnd = (float)bzfrand();
    float total = 0.0f;
    QPairList::const_iterator vit;
    for (vit = qPairList.begin(); vit != qPairList.end(); ++vit)
    {
        total += vit->second;
        if (total > rnd)
            break;
    }

    if (vit == qPairList.end())
    {
        return false; // ??
    }

    zones[vit->first].getRandomPoint(pt);

    return true;
}


bool EntryZones::getClosePoint(const std::string &qual, const glm::vec3 &pos,
                               glm::vec3 &pt) const
{
    QualifierMap::const_iterator mit = qmap.find(qual);
    if (mit == qmap.end())
        return false;

    const QPairList &qPairList = mit->second;

    int closest = -1;
    float minDist = +Infinity;
    QPairList::const_iterator vit;
    for (vit = qPairList.begin(); vit != qPairList.end(); ++vit)
    {
        const int index = vit->first;
        float dist = zones[index].getDistToPoint(pos);
        if (dist < minDist)
        {
            closest = index;
            minDist = dist;
        }
    }

    if (closest == -1)
        return false;

    zones[closest].getRandomPoint(pt);

    return true;
}


bool EntryZones::getZonePoint(const std::string &qualifier, glm::vec3 &pt) const
{
    QualifierMap::const_iterator mit = qmap.find(qualifier);
    if (mit == qmap.end())
        return false;

    const QPairList &qPairList = mit->second;

    float rnd = (float)bzfrand();
    float total = 0.0f;
    QPairList::const_iterator vit;
    for (vit = qPairList.begin(); vit != qPairList.end(); ++vit)
    {
        total += vit->second;
        if (total > rnd)
            break;
    }

    if (vit == qPairList.end())
    {
        return false; // ??
    }

    zones[vit->first].getRandomPoint(pt);

    return true;
}


bool EntryZones::getSafetyPoint(const std::string &qualifier,
                                const glm::vec3 &pos, glm::vec3 &pt) const
{
    std::string safetyString = /*EntryZones::getSafetyPrefix() + */ qualifier;

    QualifierMap::const_iterator mit = qmap.find(safetyString);
    if (mit == qmap.end())
        return false;

    const QPairList &qPairList = mit->second;

    int closest = -1;
    float minDist = +Infinity;
    QPairList::const_iterator vit;
    for (vit = qPairList.begin(); vit != qPairList.end(); ++vit)
    {
        const int index = vit->first;
        float dist = zones[index].getDistToPoint(pos);
        if (dist < minDist)
        {
            closest = index;
            minDist = dist;
        }
    }

    if (closest == -1)
        return false;

    const CustomZone *zone = &zones[closest];
    zone->getRandomPoint(pt);

    return true;
}


void EntryZones::makeSplitLists (int zone,
                                 std::vector<FlagType*> &flags,
                                 std::vector<TeamColor> &teams,
                                 std::vector<TeamColor> &safety) const
{
    flags.clear();
    teams.clear();
    safety.clear();

    QualifierMap::const_iterator mit;
    for (mit = qmap.begin(); mit != qmap.end(); ++mit)
    {
        const QPairList &qPairList = mit->second;
        QPairList::const_iterator vit;
        for (vit = qPairList.begin(); vit != qPairList.end(); ++vit)
        {
            const std::pair<int,float> &p = *vit;
            int zoneIndex = p.first;
            if (zoneIndex == zone)
            {
                const std::string& qual = mit->first;
                int team;
                FlagType *type;
                if ((type = CustomZone::getFlagTypeFromQualifier(qual)) != Flags::Null)
                    flags.push_back(type);
                else if ((team = CustomZone::getPlayerTeamFromQualifier(qual)) >= 0)
                    teams.push_back((TeamColor)team);
                else if ((team = CustomZone::getFlagSafetyFromQualifier(qual)) >= 0)
                    safety.push_back((TeamColor)team);
                else
                    printf ("EntryZones::makeSplitLists() ERROR on (%s)\n", mit->first.c_str());
            }
        }
    }

    return;
}


void * EntryZones::pack(void *buf) const
{
    buf = nboPackUInt(buf, zones.size());

    for (unsigned int i = 0; i < zones.size(); i++)
    {
        const WorldFileLocation& z = (const WorldFileLocation) zones[i];
        std::vector<FlagType*> flags;
        std::vector<TeamColor> teams;
        std::vector<TeamColor> safety;
        makeSplitLists (i, flags, teams, safety);
        buf = z.pack (buf);
        buf = nboPackUShort(buf, (uint16_t)flags.size());
        buf = nboPackUShort(buf, (uint16_t)teams.size());
        buf = nboPackUShort(buf, (uint16_t)safety.size());
        unsigned int j;
        for (j = 0; j < flags.size(); j++)
            buf = flags[j]->pack(buf);
        for (j = 0; j < teams.size(); j++)
            buf = nboPackUShort(buf, teams[j]);
        for (j = 0; j < safety.size(); j++)
            buf = nboPackUShort(buf, safety[j]);
    }
    return buf;
}


int EntryZones::packSize() const
{
    int fullSize = 0;

    fullSize += sizeof(uint32_t); // zone count

    for (unsigned int i = 0; i < zones.size(); i++)
    {
        std::vector<FlagType*> flags;
        std::vector<TeamColor> teams;
        std::vector<TeamColor> safety;
        makeSplitLists (i, flags, teams, safety);
        fullSize += sizeof(float[3]); // pos
        fullSize += sizeof(float[3]); // size
        fullSize += sizeof(float);    // angle
        fullSize += sizeof(uint16_t); // flag count
        fullSize += sizeof(uint16_t); // team count
        fullSize += sizeof(uint16_t); // safety count
        unsigned int j;
        for (j = 0; j < flags.size(); j++)
            fullSize += FlagType::packSize;
        for (j = 0; j < teams.size(); j++)
            fullSize += sizeof(uint16_t);
        for (j = 0; j < safety.size(); j++)
            fullSize += sizeof(uint16_t);
    }

    return fullSize;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
