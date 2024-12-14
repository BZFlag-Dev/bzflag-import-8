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

#ifndef __ENTRYZONES_H__
#define __ENTRYZONES_H__

/* common header */
#include "common.h"

/* common interface headers */
#include "Flag.h"

// bzfs-specific headers
#include "CustomZone.h"


typedef std::vector<CustomZone> ZoneList;
typedef std::vector<std::pair<int,float> > QPairList;
typedef std::map<std::string, QPairList> QualifierMap;


class EntryZones
{
public:
    EntryZones();

    void addZone( const CustomZone *zone );
    void addZoneFlag(int zone, int flagId);

    void calculateQualifierLists();

    bool getZonePoint(const std::string &qualifier, glm::vec3 &pt) const;
    bool getSafetyPoint(const std::string &qualifier,
                        const glm::vec3 &pos, glm::vec3 &pt) const;

    bool getRandomPoint(const std::string &qual, glm::vec3 &pt) const;
    bool getClosePoint(const std::string &qual, const glm::vec3 &pos,
                       glm::vec3 &pt) const;

    const ZoneList& getZoneList() const;

    int packSize() const;
    void *pack(void *buf) const;

private:
    ZoneList zones;
    QualifierMap qmap;

    void makeSplitLists (int zone,
                         std::vector<FlagType*> &flags,
                         std::vector<TeamColor> &teams,
                         std::vector<TeamColor> &safety) const;
};

#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
