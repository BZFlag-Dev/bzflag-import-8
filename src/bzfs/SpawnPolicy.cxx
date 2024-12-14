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

/* interface header */
#include "SpawnPolicy.h"

/* common headers */
#include "GameKeeper.h"
#include "TimeKeeper.h"
#include "PlayerInfo.h"
#include "StateDatabase.h"
#include "BZDBCache.h"

/* server headers */
#include "bzfs.h"
#include "DropGeometry.h"


SpawnPolicy::SpawnPolicy(): testPos(), safeSWRadius(0.0), safeSRRadius(0.0), safeDistance(0.0)
{
}

SpawnPolicy::~SpawnPolicy()
{
}

void SpawnPolicy::getPosition(glm::vec3 &pos, int playerId, bool onGroundOnly, bool notNearEdges)
{
    /* the player is coming to life, depending on who they are an what
     * style map/configuration is being played determines how they will
     * spawn.
     */

    GameKeeper::Player *playerData
        = GameKeeper::Player::getPlayerByIndex(playerId);
    if (!playerData)
        return;

    const PlayerInfo& pi = playerData->player;
    team = pi.getTeam();

    if (!BZDB.isTrue("freeCtfSpawns") &&
            playerData->player.shouldRestartAtBase() &&
            (team >= RedTeam) && (team <= PurpleTeam) &&
            (bases.find(team) != bases.end()))
    {

        /* if the player needs to spawn on a base, select a random
         * position on one of their team's available bases.
         */

        TeamBases &teamBases = bases[team];
        const TeamBase &base = teamBases.getRandomBase((int)(bzfrand() * 100));
        base.getRandomPosition(pos[0], pos[1], pos[2]);

    }
    else
    {
        /* *** "random" spawn position selection occurs below here. ***
         *
         * The idea is to basically find a position that is the farthest
         * away as possible from your enemies within a given timeframe.
         * Since the random selection is occurring over a square that is
         * slightly smaller than 0.6 times the world size, this usually
         * results in players spawning near one of the four corners of a
         * map.
         *
         * TODO: this should be a circle instead of square to prevent such
         * predictability and tendency to select those potentially
         * dangerous corners.
         */

        const float tankRadius = BZDBCache::tankRadius;
        safeSWRadius = (float)((BZDB.eval(StateDatabase::BZDB_SHOCKOUTRADIUS) + BZDBCache::tankRadius) *
                               BZDB.eval("_spawnSafeSWMod"));
        safeSRRadius = tankRadius * BZDB.eval("_spawnSafeSRMod");
        safeDistance = tankRadius * BZDB.eval("_spawnSafeRadMod");
        const float size = BZDBCache::worldSize;
        const float maxHeight = world->getMaxWorldHeight();

        // keep track of how much time we spend searching for a location
        TimeKeeper start = TimeKeeper::getCurrent();

        int tries = 0;
        float minProximity = size / BZDB.eval("_spawnSafeSRMod");
        float bestDist = -1.0f;
        bool foundspot = false;
        while (!foundspot)
        {
            if (!world->getPlayerSpawnPoint(&pi, testPos))
            {
                if (notNearEdges)
                {
                    // don't spawn close to map edges in CTF mode
                    testPos[0] = ((float)bzfrand() - 0.5f) * size * 0.6f;
                    testPos[1] = ((float)bzfrand() - 0.5f) * size * 0.6f;
                }
                else
                {
                    testPos[0] = ((float)bzfrand() - 0.5f) * (size - 2.0f * tankRadius);
                    testPos[1] = ((float)bzfrand() - 0.5f) * (size - 2.0f * tankRadius);
                }
                testPos[2] = onGroundOnly ? 0.0f : ((float)bzfrand() * maxHeight);
            }
            tries++;

            const float waterLevel = world->getWaterLevel();
            float minZ = 0.0f;
            if (waterLevel > minZ)
                minZ = waterLevel;
            float maxZ = maxHeight;
            if (onGroundOnly)
                maxZ = 0.0f;

            if (DropGeometry::dropPlayer(testPos, minZ, maxZ))
                foundspot = true;

            // check every now and then if we have already used up 10ms of time
            if (tries >= 50)
            {
                tries = 0;
                if (TimeKeeper::getCurrent() - start > BZDB.eval("_spawnMaxCompTime"))
                {
                    if (bestDist < 0.0f)   // haven't found a single spot
                    {
                        //Just drop the sucka in, and pray
                        pos[0] = testPos[0];
                        pos[1] = testPos[1];
                        pos[2] = maxHeight;
                        logDebugMessage(1,"Warning: SpawnPolicy ran out of time, just dropping the sucker in\n");
                    }
                    break;
                }
            }

            // check if spot is safe enough
            bool dangerous = isImminentlyDangerous();
            if (foundspot && !dangerous)
            {
                float enemyAngle;
                float dist = enemyProximityCheck(enemyAngle);
                if (dist > bestDist)   // best so far
                {
                    bestDist = dist;
                    pos[0] = testPos[0];
                    pos[1] = testPos[1];
                    pos[2] = testPos[2];
                }
                if (bestDist < minProximity)   // not good enough, keep looking
                {
                    foundspot = false;
                    minProximity *= 0.99f; // relax requirements a little
                }
            }
            else if (dangerous)
                foundspot = false;
        }
    }
}

void SpawnPolicy::getAzimuth(float &azimuth)
{
    bool dangerous = isImminentlyDangerous();
    if (dangerous)
    {
        float enemyAngle;
        (void)enemyProximityCheck(enemyAngle);
        azimuth = fmod((float)(enemyAngle + M_PI), (float)(2.0 * M_PI));
    }
    else
        azimuth = (float)(bzfrand() * 2.0 * M_PI);
}


bool SpawnPolicy::isFacing(const glm::vec3 &enemyPos, const float enemyAzimuth,
                           const float deviation) const
{
    // vector points from test to enemy
    float dx = enemyPos[0] - testPos[0];
    float dy = enemyPos[1] - testPos[1];
    float dz = enemyPos[2] - testPos[2];
    float angActual = atan2f (dy, dx);
    float diff = fmodf(enemyAzimuth - angActual, (float)M_PI * 2.0f);

    // Ignore tanks that are above or below us
    if (fabs(dz) > 2.0f * BZDBCache::tankHeight )
        return false;

    // now diff is between {-PI*2 and +PI*2}, and we're looking for values around
    // -PI or +PI, because that's when the enemy is facing the source.
    diff = fabsf (diff); // between {0 and +PI*2}
    diff = fabsf ((float)(diff - M_PI));

    if (diff < (deviation / 2.0f))
        return true;
    else
        return false;
}


bool SpawnPolicy::isImminentlyDangerous() const
{
    GameKeeper::Player *playerData;
    float twentyDegrees = (float)(M_PI / 9.0); /* +- 10 degrees, i.e. 20 degree arc */
    int curmax = getCurMaxPlayers();
    for (int i = 0; i < curmax; i++)
    {
        playerData = GameKeeper::Player::getPlayerByIndex(i);
        if (!playerData)
            continue;
        if (playerData->player.isAlive())
        {
            const auto &enemyPos = playerData->lastState.pos;
            float  enemyAngle = playerData->lastState.azimuth;
            if (playerData->player.getFlag() >= 0)
            {
                // check for dangerous flags
                const FlagInfo *finfo = FlagInfo::get(playerData->player.getFlag());
                const FlagType *ftype = finfo->flag.type;
                // FIXME: any more?
                if (ftype == Flags::Laser)    // don't spawn in the line of sight of an L
                {
                    if (isFacing(enemyPos, enemyAngle, twentyDegrees))   // he's looking within 20 degrees of spawn point
                    {
                        return true;    // eek, don't spawn here
                    }
                }
                else if (ftype == Flags::ShockWave)      // don't spawn next to a SW
                {
                    if (distanceFrom(enemyPos) < safeSWRadius)   // too close to SW
                    {
                        return true;    // eek, don't spawn here
                    }
                }
                else if (ftype == Flags::Steamroller || ftype == Flags::Burrow)     // don't spawn if you'll squish or be squished
                {
                    if (distanceFrom(enemyPos) < safeSRRadius)   // too close to SR or BU
                    {
                        return true;    // eek, don't spawn here
                    }
                }
            }
            // don't spawn in the line of sight of a normal-shot tank within a certain distance
            if (distanceFrom(enemyPos) < safeDistance)   // within danger zone?
            {
                if (isFacing(enemyPos, enemyAngle, twentyDegrees))   //and he's looking at me
                    return true;
            }
        }
    }

    // TODO: should check world weapons also

    return false;
}


float SpawnPolicy::enemyProximityCheck(float &enemyAngle) const
{
    GameKeeper::Player *playerData;
    float worstDist = 1e12f; // huge number
    bool noEnemy    = true;
    int curmax = getCurMaxPlayers();
    for (int i = 0; i < curmax; i++)
    {
        playerData = GameKeeper::Player::getPlayerByIndex(i);
        if (!playerData)
            continue;
        if (playerData->player.isAlive()
                && areFoes(playerData->player.getTeam(), team))
        {
            const auto &enemyPos = playerData->lastState.pos;
            if (fabs(enemyPos[2] - testPos[2]) < 1.0f)
            {
                float x = enemyPos[0] - testPos[0];
                float y = enemyPos[1] - testPos[1];
                float distSq = x * x + y * y;
                if (distSq < worstDist)
                {
                    worstDist  = distSq;
                    enemyAngle = playerData->lastState.azimuth;
                    noEnemy    = false;
                }
            }
        }
    }
    if (noEnemy)
        enemyAngle = (float)(bzfrand() * 2.0 * M_PI);
    return sqrtf(worstDist);
}


float SpawnPolicy::distanceFrom(const glm::vec3 &farPos) const
{
    float dx = farPos[0] - testPos[0];
    float dy = farPos[1] - testPos[1];
    float dz = farPos[2] - testPos[2];
    return (float)sqrt(dx*dx + dy*dy + dz*dz);
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
