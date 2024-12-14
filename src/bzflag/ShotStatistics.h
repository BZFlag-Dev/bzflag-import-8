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

#ifndef __SHOTSTATISTICS_H__
#define __SHOTSTATISTICS_H__

#include "common.h"

#include "Flag.h"

/** ShotStatistics stores and calculates a set of statistics on player's shots
 *  and accuracy
 */

// FIXME - this looks bad, make the FlagType an enum
typedef std::map<FlagType*, int> FlagMap;

class ShotStatistics
{
public:

    ShotStatistics();
    ~ShotStatistics();

    // raw stats returns
    int    getFired(FlagType* flag) const;
    int    getHit(FlagType* flag) const;

    // stats processing
    int    getTotalFired() const;
    int    getTotalHit() const;
    int    getTotalPerc() const;
    FlagType*   getFavoriteFlag() const;
    FlagType*   getBestFlag() const;

    // tally functions
    void        recordFire(FlagType* flag, const glm::vec3 &pVec, const glm::vec3 &shotVec);
    void        recordHit(FlagType* flag);

    double getLastShotTimeDelta ( void ) const
    {
        return lastShotTimeDelta;
    }
    double getLastShotDeviation ( void ) const
    {
        return lastShotDeviation;
    }

private:
    FlagMap     fired;
    FlagMap     hit;

    int         totalFired;
    int         totalHit;

    double      lastShotTimeDelta;
    double      lastShotTime;

    float       lastShotDeviation;
};


inline int ShotStatistics::getFired(FlagType* flag) const
{
    if (fired.find(flag) != fired.end())
        return fired.at(flag);
    return 0;
}

inline int ShotStatistics::getHit(FlagType* flag) const
{
    if (hit.find(flag) != hit.end())
        return hit.at(flag);
    return 0;
}

inline int ShotStatistics::getTotalFired() const
{
    return totalFired;
}

inline int ShotStatistics::getTotalHit() const
{
    return totalHit;
}

#endif // __SHOTSTATISTICS_H__

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
