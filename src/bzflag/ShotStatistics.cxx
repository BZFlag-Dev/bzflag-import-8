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

// Interface header
#include "ShotStatistics.h"

// System headers
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

// Common headers
#include "TimeKeeper.h"
#include "playing.h"
#include "mathRoutine.h"

ShotStatistics::ShotStatistics() :
    totalFired(0), totalHit(0)
{
    lastShotTimeDelta = 0;
    lastShotTime = 0;
    lastShotDeviation = 0;
}

ShotStatistics::~ShotStatistics()
{
}

int ShotStatistics::getTotalPerc() const
{
    if (getTotalFired() == 0)
        return 0;
    return (int)(100 * ((float)getTotalHit() / (float)getTotalFired()));
}

void ShotStatistics::recordFire(
    FlagType* flag, const glm::vec3 &pVec, const glm::vec3 &shotVec)
{
    fired[flag]++;
    totalFired++;

    double currentTime = TimeKeeper::getCurrent().getSeconds();
    if (lastShotTime > 0)
        lastShotTimeDelta = currentTime-lastShotTime;
    lastShotTime = currentTime;

    const auto playerNorm = glm::normalize(pVec);
    const auto shotNorm = shotVec * bzInverseSqrt(glm::length2(shotVec));

    float dot = glm::dot(shotNorm, playerNorm);

    lastShotDeviation = acosf(dot) * RAD2DEGf;

    if (getShotStats())
        getShotStats()->refresh();
}

void ShotStatistics::recordHit(FlagType* flag)
{
    hit[flag]++;
    totalHit++;

    if (getShotStats())
        getShotStats()->refresh();
}

typedef std::pair<FlagType*, float> FlagStat;

FlagType* ShotStatistics::getFavoriteFlag() const
{
    /* return the flag the player fired most */
    FlagType* greatest = Flags::Null;

    // we don't deal with the case where there are two "equally favorite"
    // flags; doesn't really matter
    for (auto it = fired.begin(); it != fired.end(); it++)
    {
        // ignore none/null - looking for favorite *flags*
        if (it->second > getFired(greatest) && it->first != Flags::Null)
            greatest = it->first;
    }

    return greatest;
}

FlagType* ShotStatistics::getBestFlag() const
{
    /* return the flag with the best hits/fired ratio */
    FlagType* greatest = Flags::Null;
    float greatestRatio = getHit(greatest)/float(getFired(greatest));

    // we don't deal with the case where there are two "equally best"
    // flags; doesn't really matter
    for (auto it = fired.begin(); it != fired.end(); it++)
    {
        float ratio = getHit(it->first)/float(it->second);

        // normal shots have the opportunity to be best
        if (ratio > greatestRatio)
        {
            greatest = it->first;
            greatestRatio = getHit(greatest)/float(getFired(greatest));
        }
    }

    return greatest;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
