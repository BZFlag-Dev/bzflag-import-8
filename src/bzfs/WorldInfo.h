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

#ifndef __WORLDINFO_H__
#define __WORLDINFO_H__

#include "common.h"

/* system interface headers */
#include <vector>
#include <string>

/* bzfs implementation headers */
#include "EntryZones.h"
#include "WorldWeapons.h"
#include "TeamBases.h"
#include "LinkManager.h"

/* common implementation headers */

class BzMaterial;
class Obstacle;
class BoxBuilding;
class PyramidBuilding;
class BaseBuilding;
class TetraBuilding;
class Teleporter;
class WallObstacle;
class MeshObstacle;
class ArcObstacle;
class ConeObstacle;
class SphereObstacle;

class WorldFileLocation;
class CustomZone;

class FlagType;
class FlagInfo;
class PlayerInfo;

typedef enum
{
    NOT_IN_BUILDING,
    IN_BASE,
    IN_BOX_NOTDRIVETHROUGH,
    IN_BOX_DRIVETHROUGH,
    IN_MESH,
    IN_MESHFACE,
    IN_PYRAMID,
    IN_TETRA,
    IN_TELEPORTER
} InBuildingType;


class WorldInfo
{

public:

    WorldInfo();
    ~WorldInfo();

    void setSize ( float x, float y );
    void setGravity ( float g );

    void addLink(int from, int to);
    void addLink(const std::string& from, const std::string& to);

    void addZone(const CustomZone *zone);
    void addWeapon(const FlagType *type, const glm::vec3 &origin,
                   float direction, float tilt, TeamColor teamColor,
                   float initdelay, const std::vector<float> &delay, TimeKeeper &sync);
    void addWaterLevel (float level, const BzMaterial* matref);

    void addBox(float x, float y, float z, float r,
                float w, float d, float h,
                bool drive = false, bool shoot = false, bool rico = false);
    void addPyramid(float x, float y, float z, float r,
                    float w, float d, float h,
                    bool drive = false, bool shoot = false, bool rico = false, bool flipZ = false);
    void addTeleporter(float x, float y, float z, float r,
                       float w, float d, float h, float b,
                       bool horizontal, bool drive = false, bool shoot = false, bool rico = false);
    void addBase(const glm::vec3 &pos, float r, const glm::vec3 &size,
                 int color, bool drive = false, bool shoot = false, bool rico = false);

    float getWaterLevel() const;
    float getMaxWorldHeight() const;

    bool getFlagDropPoint(const FlagInfo* fi, const glm::vec3 &pos, glm::vec3 &pt) const;
    bool getFlagSpawnPoint(const FlagInfo* fi, glm::vec3 &pt) const;
    bool getPlayerSpawnPoint(const PlayerInfo* pi, glm::vec3 &pt) const;

    void *getDatabase() const;
    int getDatabaseSize() const;
    int getUncompressedSize() const;

    WorldWeapons& getWorldWeapons();
    EntryZones& getEntryZones();

    void finishWorld();
    int packDatabase();

    bool isFinisihed()
    {
        return finished;
    }

private:

    bool finished;

    bool rectHitCirc(float dx, float dy, const float *p, float r) const;
    void loadCollisionManager();
    InBuildingType classifyHit (const Obstacle* obstacle) const;
    void makeWaterMaterial();

public:

    /** check collision between world object and a cylinder.
     * return value is kind of collision.
     * location will return a pointer to the world colliding object
     * Checking is quite raw. Does not use the CollisionManager and
     * can therefore be used before it has been setup.
     */
    InBuildingType inCylinderNoOctree(Obstacle **location,
                                      float x, float y, float z,
                                      float r, float height) const;

    /** check collision between world object and a cylinder.
     * return value is kind of collision.
     * location will return a pointer to the world colliding object
     * Checking is quite raw
     */
    InBuildingType cylinderInBuilding(const Obstacle **obstacle,
                                      float x, float y, float z,
                                      float radius, float height = 0.0f) const;

    /** check collision between world object and a cylinder.
     * return value is kind of collision.
     * location will return a pointer to the world colliding object
     * Checking is quite raw
     */
    InBuildingType cylinderInBuilding(const Obstacle **obstacle,
                                      const glm::vec3 &pos,
                                      float radius, float height = 0.0f) const;

    /** check collision between world object and a Z-axis aligned box.
     * return value is kind of collision.
     * location will return a pointer to the world colliding object
     */
    InBuildingType boxInBuilding(const Obstacle **obstacle,
                                 const glm::vec3 &pos, float angle,
                                 float width, float breadth, float height) const;


    /** see if the CollisionManager's view of the world size
     * matches that of BZDB. if not, reload the CollisionManager
     */
    void checkCollisionManager();

    bool inRect(const float *p1, float angle, const float *size,
                float x, float y, float r) const;

private:

    float size[2];
    float gravity;
    float maxHeight;
    float waterLevel;
    const BzMaterial* waterMatRef;

    EntryZones entryZones;
    LinkManager links;
    WorldWeapons worldWeapons;

    char *database;
    int databaseSize;
    int uncompressedSize;
};


#endif /* __WORLDINFO_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
