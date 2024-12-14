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
#include "WorldBuilder.h"

/* common implementation headers */
#include "TextUtils.h"
#include "Protocol.h"
#include "DynamicColor.h"
#include "TextureMatrix.h"
#include "PhysicsDriver.h"
#include "FlagSceneNode.h"
#include "ObstacleMgr.h"
#include "BaseBuilding.h"

/* compression library header */
#include <zlib.h>


WorldBuilder::WorldBuilder()
{
    world = new World;
    owned = true;
}


WorldBuilder::~WorldBuilder()
{
    if (owned) delete world;
}


const void* WorldBuilder::unpack(const void* buf)
{
    TimeKeeper start = TimeKeeper::getCurrent();

    // unpack world database from network transfer
    // read style header
    uint16_t code, len;
    buf = nboUnpackUShort(buf, len);
    buf = nboUnpackUShort(buf, code);
    if (code != WorldCodeHeader) return NULL;

    // read style
    uint16_t serverMapVersion;
    buf = nboUnpackUShort(buf, serverMapVersion);
    if (serverMapVersion != mapVersion)
    {
        logDebugMessage(1,"WorldBuilder::unpack() bad map version\n");
        return NULL;
    }

    // decompress
    uint32_t compressedSize, uncompressedSize;
    buf = nboUnpackUInt (buf, uncompressedSize);
    buf = nboUnpackUInt (buf, compressedSize);
    uLongf destLen = uncompressedSize;
    char *uncompressedWorld = new char[destLen];
    const Bytef *compressedWorld = (const Bytef*) buf;

    if (uncompress ((Bytef*)uncompressedWorld, &destLen,
                    compressedWorld, compressedSize) != Z_OK)
    {
        delete[] uncompressedWorld;
        logDebugMessage(1,"WorldBuilder::unpack() could not decompress\n");
        return NULL;
    }
    const char* uncompressedEnd = uncompressedWorld + uncompressedSize;

    buf = uncompressedWorld;

    // setup the buffer overrun checking
    nboUseErrorChecking(true);
    nboSetBufferLength(uncompressedSize);
    nboClearBufferError();

    // unpack dynamic colors
    DYNCOLORMGR.clear();
    buf = DYNCOLORMGR.unpack(buf);

    // unpack texture matrices
    TEXMATRIXMGR.clear();
    buf = TEXMATRIXMGR.unpack(buf);

    // unpack materials
    MATERIALMGR.clear();
    buf = MATERIALMGR.unpack(buf);

    // unpack physics drivers
    PHYDRVMGR.clear();
    buf = PHYDRVMGR.unpack(buf);

    // unpack obstacle transforms
    TRANSFORMMGR.clear();
    buf = TRANSFORMMGR.unpack(buf);

    // unpack the obstacles
    OBSTACLEMGR.clear();
    buf = OBSTACLEMGR.unpack(buf);

    // unpack the teleporter links
    world->links.clear();
    buf = world->links.unpack(buf);

    // unpack water level
    buf = nboUnpackFloat(buf, world->waterLevel);
    if (world->waterLevel >= 0.0f)
    {
        int32_t matindex;
        buf = nboUnpackInt(buf, matindex);
        world->waterMaterial = MATERIALMGR.getMaterial(matindex);
    }

    uint32_t i, count;

    // unpack the weapons
    buf = nboUnpackUInt(buf, count);
    for (i = 0; i < count; i++)
    {
        Weapon weapon;
        buf = weapon.unpack(buf);
        world->weapons.push_back(weapon);
    }

    // unpack the entry zones
    buf = nboUnpackUInt(buf, count);
    for (i = 0; i < count; i++)
    {
        EntryZone zone;
        buf = zone.unpack(buf);
        world->entryZones.push_back(zone);
    }

    // check if the unpacking was successful
    nboUseErrorChecking(false);
    if (nboGetBufferError())
    {
        delete[] uncompressedWorld;
        logDebugMessage(1,"WorldBuilder::unpack() overrun\n");
        return NULL;
    }
    if ((const char*)buf != uncompressedEnd)
    {
        logDebugMessage(1,"WorldBuilder::unpack() ending mismatch (%i)\n",
                        (const char*)buf - uncompressedEnd);
        delete[] uncompressedWorld;
        return NULL;
    }

    // switch back to the original buffer
    buf = compressedWorld + compressedSize;
    buf = nboUnpackUShort(buf, len);
    buf = nboUnpackUShort(buf, code);
    if ((code != WorldCodeEnd) || (len != WorldCodeEndSize))
    {
        delete[] uncompressedWorld;
        logDebugMessage(1,"WorldBuilder::unpack() bad ending\n");
        return NULL;
    }

    // delete the buffer
    delete[] uncompressedWorld;

    // build the world obstacles
    OBSTACLEMGR.makeWorld();

    // link the teleporters
    world->links.doLinking();

    // make the team bases
    if (world->gameType == ClassicCTF)
    {
        const ObstacleList& bases = OBSTACLEMGR.getBases();
        for (i = 0; i < bases.size(); i++)
        {
            const BaseBuilding* base = (const BaseBuilding*) bases[i];
            setBase((TeamColor)base->getTeam(),
                    base->getPosition(), base->getRotation(),
                    base->getWidth(), base->getBreadth(), base->getHeight());
        }
    }
    else
        OBSTACLEMGR.replaceBasesWithBoxes();

    world->makeLinkMaterial();

    world->makeMeshDrawMgrs();

    // NOTE: relying on checkCollisionManager() to do the first loading
    //       of ColiisionManager, because the BZDB variables come in later,
    //       and would cause a double loading if we did it now.

    if (debugLevel >= 3)
    {
        TimeKeeper end = TimeKeeper::getCurrent();
        const float elapsed = (float)(end - start);
        logDebugMessage(0,"WorldBuilder::unpack() processed in %f seconds.\n", elapsed);
    }

    return buf;
}


const void* WorldBuilder::unpackGameSettings(const void* buf)
{
    // read style
    uint16_t gameType, gameOptions, maxPlayers, maxShots, maxFlags;

    float worldSize;
    buf = nboUnpackFloat(buf, worldSize);
    BZDB.set(StateDatabase::BZDB_WORLDSIZE, TextUtils::format("%f", worldSize));
    buf = nboUnpackUShort(buf, gameType);
    setGameType(short(gameType));
    buf = nboUnpackUShort(buf, gameOptions);
    setGameOptions(short(gameOptions));
    buf = nboUnpackUShort(buf, maxPlayers);
    setMaxPlayers(int(maxPlayers));
    buf = nboUnpackUShort(buf, maxShots);
    setMaxShots(int(maxShots));
    buf = nboUnpackUShort(buf, maxFlags);
    setMaxFlags(int(maxFlags));
    buf = nboUnpackFloat(buf, world->linearAcceleration);
    buf = nboUnpackFloat(buf, world->angularAcceleration);
    uint16_t shakeTimeout = 0, shakeWins;
    buf = nboUnpackUShort(buf, shakeTimeout);
    setShakeTimeout(0.1f * float(shakeTimeout));
    buf = nboUnpackUShort(buf, shakeWins);
    setShakeWins(shakeWins);
    uint32_t UsedToBeSyncTime; // FIXME
    buf = nboUnpackUInt(buf, UsedToBeSyncTime);

    return buf;
}

void WorldBuilder::preGetWorld()
{
    // prepare players array
    if (world->players)
        delete[] world->players;
    world->players = new RemotePlayer*[maxRemotePlayers];
    int i;
    for (i = 0; i < maxRemotePlayers; i++)
        world->players[i] = NULL;

    // prepare flags array
    world->freeFlags();
    world->flags = new Flag[world->maxFlags];
    world->flagNodes = new FlagSceneNode*[world->maxFlags];
    world->flagWarpNodes = new FlagWarpSceneNode*[world->maxFlags];
    for (i = 0; i < world->maxFlags; i++)
    {
        world->flags[i].type = Flags::Null;
        world->flags[i].status = FlagNoExist;
        world->flags[i].position[0] = 0.0f;
        world->flags[i].position[1] = 0.0f;
        world->flags[i].position[2] = 0.0f;
        world->flagNodes[i] = new FlagSceneNode(world->flags[i].position);
        world->flagWarpNodes[i] = new FlagWarpSceneNode(world->flags[i].position);
        world->flagNodes[i]->setTexture(World::flagTexture);
    }

    return;
}


World* WorldBuilder::getWorld()
{
    owned = false;
    preGetWorld();
    return world;
}


World* WorldBuilder::peekWorld()
{
    preGetWorld();
    return world;
}

void WorldBuilder::setGameType(short gameType)
{
    world->gameType = gameType;
}

void WorldBuilder::setGameOptions(short gameOptions)
{
    world->gameOptions = gameOptions;
}

void WorldBuilder::setMaxPlayers(int maxPlayers)
{
    world->maxPlayers = maxPlayers;
}

void WorldBuilder::setMaxShots(int maxShots)
{
    world->maxShots = maxShots;
}

void WorldBuilder::setMaxFlags(int maxFlags)
{
    world->maxFlags = maxFlags;
}

void WorldBuilder::setShakeTimeout(float timeout) const
{
    world->shakeTimeout = timeout;
}

void WorldBuilder::setShakeWins(int wins) const
{
    world->shakeWins = wins;
}

void WorldBuilder::setBase(TeamColor team,
                           const glm::vec3 &pos, float rotation,
                           float w, float b, float h)
{
    int teamIndex = int(team);

    World::BaseParms bp;
    bp.pos      = pos;
    bp.rotation = rotation;
    bp.w        = w;
    bp.b        = b;
    bp.h        = h;
    world->bases[teamIndex].push_back( bp );
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
