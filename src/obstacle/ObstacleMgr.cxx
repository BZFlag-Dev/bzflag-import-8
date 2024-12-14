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


#include "common.h"

// implementation header
#include "ObstacleMgr.h"

// system headers
#include <string.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <iostream>

// common headers
#include "Pack.h"
#include "MeshTransform.h"
#include "ObstacleModifier.h"
#include "StateDatabase.h"
#include "PhysicsDriver.h"
#include "BzMaterial.h"
#include "MeshDrawInfo.h"

// obstacle headers
#include "Obstacle.h"
#include "WallObstacle.h"
#include "BoxBuilding.h"
#include "PyramidBuilding.h"
#include "BaseBuilding.h"
#include "Teleporter.h"
#include "MeshObstacle.h"
#include "ArcObstacle.h"
#include "ConeObstacle.h"
#include "SphereObstacle.h"
#include "TetraBuilding.h"


//////////////////////////////////////////////////////////////////////////////
//
// Group Instance
// - uses a group definition and a transform to produce obstacles
//


void GroupInstance::init()
{
    modifyTeam = false;
    team = 0;
    modifyColor = false;
    tint[0] = tint[1] = tint[2] = tint[3] = 1.0f;
    modifyPhysicsDriver = false;
    phydrv = -1;
    modifyMaterial = false;
    material = NULL;
    driveThrough = false;
    shootThrough = false;
    ricochet     = false;

    return;
}


GroupInstance::GroupInstance(const std::string& _groupdef)
{
    groupdef = _groupdef;
    init();
    return;
}


GroupInstance::GroupInstance()
{
    init();
    return;
}


GroupInstance::~GroupInstance()
{
    return;
}


void GroupInstance::setTransform(const MeshTransform& _transform)
{
    transform = _transform;
    return;
}


void GroupInstance::setName(const std::string& _name)
{
    name = _name;
    return;
}


const std::string& GroupInstance::getName() const
{
    return name;
}


void GroupInstance::setTeam(int _team)
{
    team = _team;
    modifyTeam = true;
    return;
}


void GroupInstance::setTint(const float _tint[4])
{
    memcpy(tint, _tint, sizeof(float[4]));
    modifyColor = true;
    return;
}


void GroupInstance::setPhysicsDriver(int _phydrv)
{
    phydrv = _phydrv;
    modifyPhysicsDriver = true;
    return;
}


void GroupInstance::setMaterial(const BzMaterial* _material)
{
    material = _material;
    modifyMaterial = true;
    return;
}


void GroupInstance::setDriveThrough()
{
    driveThrough = true;
    return;
}


void GroupInstance::setShootThrough()
{
    shootThrough = true;
    return;
}

void GroupInstance::setCanRicochet()
{
    ricochet = true;
    return;
}


void GroupInstance::addMaterialSwap(const BzMaterial* srcMat,
                                    const BzMaterial* dstMat)
{
    matMap[srcMat] = dstMat;
    return;
}


const std::string& GroupInstance::getGroupDef() const
{
    return groupdef;
}


const MeshTransform& GroupInstance::getTransform() const
{
    return transform;
}


void* GroupInstance::pack(void* buf)
{
    buf = nboPackStdString(buf, groupdef);

    if (matMap.size() <= 0)
        buf = nboPackStdString(buf, name);
    else
    {
        // hack to stuff in material map data
        std::string fakeString = name;
        const unsigned int origSize = fakeString.size();
        const unsigned int fakeSize =
            (1 + sizeof(int32_t) + (matMap.size() * 2 * sizeof(int32_t)));
        fakeString.resize(origSize + fakeSize);
        char* buffer = new char[fakeSize];
        void* p;
        int count = matMap.size();
        p = nboPackUByte(buffer, 0); // terminate
        p = nboPackInt(p, count);
        MaterialMap::const_iterator it;
        for (it = matMap.begin(); it != matMap.end(); ++it)
        {
            int srcIndex = MATERIALMGR.getIndex(it->first);
            int dstIndex = MATERIALMGR.getIndex(it->second);
            p = nboPackInt(p, srcIndex);
            p = nboPackInt(p, dstIndex);
        }
        for (unsigned int i = 0; i < fakeSize; i++)
            fakeString[origSize + i] = buffer[i];
        delete[] buffer;
        buf = nboPackStdString(buf, fakeString);
    }

    buf = transform.pack(buf);

    uint8_t bits = 0;
    if (modifyTeam)      bits |= (1 << 0);
    if (modifyColor)     bits |= (1 << 1);
    if (modifyPhysicsDriver) bits |= (1 << 2);
    if (modifyMaterial)      bits |= (1 << 3);
    if (driveThrough)    bits |= (1 << 4);
    if (shootThrough)    bits |= (1 << 5);
    if (ricochet)     bits |= (1 << 6);
    buf = nboPackUByte(buf, bits);

    if (modifyTeam)
        buf = nboPackUShort(buf, team);
    if (modifyColor)
    {
        buf = nboPackVector(buf, tint);
        buf = nboPackFloat(buf, tint[3]);
    }
    if (modifyPhysicsDriver)
        buf = nboPackInt(buf, phydrv);
    if (modifyMaterial)
    {
        int matindex = MATERIALMGR.getIndex(material);
        buf = nboPackInt(buf, (int32_t) matindex);
    }

    return buf;
}


const void* GroupInstance::unpack(const void* buf)
{
    buf = nboUnpackStdString(buf, groupdef);

    buf = nboUnpackStdStringRaw(buf, name);
    if (strlen(name.c_str()) != name.size())
    {
        // hack to extract material map data
        const void* p = name.c_str() + strlen(name.c_str()) + 1;
        nboUseErrorChecking(false);
        {
            int32_t count;
            p = nboUnpackInt(p, count);
            for (int i = 0; i < count; i++)
            {
                int32_t srcIndex, dstIndex;
                p = nboUnpackInt(p, srcIndex);
                p = nboUnpackInt(p, dstIndex);
                const BzMaterial* srcMat = MATERIALMGR.getMaterial(srcIndex);
                const BzMaterial* dstMat = MATERIALMGR.getMaterial(dstIndex);
                matMap[srcMat] = dstMat;
            }
        }
        nboUseErrorChecking(true);
        name.resize(strlen(name.c_str())); // clean up the name
    }

    buf = transform.unpack(buf);

    uint8_t bits;
    buf = nboUnpackUByte(buf, bits);
    modifyTeam =      ((bits & (1 << 0)) == 0) ? false : true;
    modifyColor =     ((bits & (1 << 1)) == 0) ? false : true;
    modifyPhysicsDriver = ((bits & (1 << 2)) == 0) ? false : true;
    modifyMaterial =  ((bits & (1 << 3)) == 0) ? false : true;
    driveThrough =    ((bits & (1 << 4)) == 0) ? false : true;
    shootThrough =    ((bits & (1 << 5)) == 0) ? false : true;
    ricochet =        ((bits & (1 << 6)) == 0) ? false : true;

    if (modifyTeam)
    {
        uint16_t tmpTeam;
        buf = nboUnpackUShort(buf, tmpTeam);
        team = (int)tmpTeam;
    }
    if (modifyColor)
    {
        buf = nboUnpackVector(buf, tint);
        buf = nboUnpackFloat(buf, tint[3]);
    }
    if (modifyPhysicsDriver)
    {
        int32_t inPhyDrv;
        buf = nboUnpackInt(buf, inPhyDrv);
        phydrv = int(inPhyDrv);
    }
    if (modifyMaterial)
    {
        int32_t matindex;
        buf = nboUnpackInt(buf, matindex);
        material = MATERIALMGR.getMaterial(matindex);
    }

    return buf;
}


int GroupInstance::packSize()
{
    int fullSize = 0;
    fullSize += nboStdStringPackSize(groupdef);

    fullSize += nboStdStringPackSize(name);
    if (matMap.size() > 0)
    {
        fullSize += 1; // terminator
        fullSize += sizeof(int32_t); // count;
        fullSize += matMap.size() * 2 * sizeof(int32_t);
    }

    fullSize += transform.packSize();
    fullSize += sizeof(uint8_t);
    if (modifyTeam)
        fullSize += sizeof(uint16_t);
    if (modifyColor)
        fullSize += sizeof(float[4]);
    if (modifyPhysicsDriver)
        fullSize += sizeof(int32_t);
    if (modifyMaterial)
        fullSize += sizeof(int32_t);
    return fullSize;
}


void GroupInstance::print(std::ostream& out, const std::string& indent) const
{
    out << indent << "group " << groupdef << std::endl;

    if (name.size() > 0)
        out << indent << "  name " << name << std::endl;

    transform.printTransforms(out, indent);
    if (modifyTeam)
        out << indent << "  team " << team << std::endl;
    if (modifyColor)
    {
        out << indent << "  tint " << tint[0] << " " << tint[1] << " "
            << tint[2] << " " << tint[3] << " "
            << std::endl;
    }
    if (modifyPhysicsDriver)
    {
        const PhysicsDriver* driver = PHYDRVMGR.getDriver(phydrv);
        if (driver != NULL)
        {
            out << indent << "  phydrv ";
            if (driver->getName().size() > 0)
                out << driver->getName();
            else
                out << phydrv;
            out << std::endl;
        }
    }
    if (modifyMaterial)
    {
        out << indent << "  matref ";
        MATERIALMGR.printReference(out, material);
        out << std::endl;
    }
    else if (matMap.size() > 0)
    {
        MaterialMap::const_iterator it;
        for (it = matMap.begin(); it != matMap.end(); ++it)
        {
            out << indent << "  matswap ";
            MATERIALMGR.printReference(out, it->first);
            out << " ";
            MATERIALMGR.printReference(out, it->second);
            out << std::endl;
        }
    }

    if (driveThrough)
        out << indent << "  driveThrough"  << std::endl;
    if (shootThrough)
        out << indent << "  shootThrough"  << std::endl;
    if (ricochet)
        out << indent << "  ricochet"      << std::endl;
    out << indent << "end" << std::endl;

    return;
}


//////////////////////////////////////////////////////////////////////////////
//
// Group Definition
// - defines an obstacle group
//

std::string GroupDefinition::depthName;


GroupDefinition::GroupDefinition(const std::string& _name)
{
    name = _name;
    active = false;
    return;
}


GroupDefinition::~GroupDefinition()
{
    return;
}


Obstacle* GroupDefinition::newObstacle(int type)
{
    Obstacle* obs = NULL;

    if (type == wallType)
        obs = new WallObstacle();
    else if (type == boxType)
        obs = new BoxBuilding();
    else if (type == pyrType)
        obs = new PyramidBuilding();
    else if (type == baseType)
        obs = new BaseBuilding();
    else if (type == teleType)
        obs = new Teleporter();
    else if (type == meshType)
        obs = new MeshObstacle();
    else if (type == arcType)
        obs = new ArcObstacle();
    else if (type == coneType)
        obs = new ConeObstacle();
    else if (type == sphereType)
        obs = new SphereObstacle();
    else if (type == tetraType)
        obs = new TetraBuilding();

    return obs;
}


void GroupDefinition::addObstacle(Obstacle* obstacle)
{
    const char* type = obstacle->getType();

    if (WallObstacle::getClassName() == type)
        lists[wallType].push_back(obstacle);
    else if (BoxBuilding::getClassName() == type)
        lists[boxType].push_back(obstacle);
    else if (BaseBuilding::getClassName() == type)
        lists[baseType].push_back(obstacle);
    else if (PyramidBuilding::getClassName() == type)
        lists[pyrType].push_back(obstacle);
    else if (Teleporter::getClassName() == type)
        lists[teleType].push_back(obstacle);
    else if (MeshObstacle::getClassName() == type)
        lists[meshType].push_back(obstacle);
    else if (ArcObstacle::getClassName() == type)
        lists[arcType].push_back(obstacle);
    else if (ConeObstacle::getClassName() == type)
        lists[coneType].push_back(obstacle);
    else if (SphereObstacle::getClassName() == type)
        lists[sphereType].push_back(obstacle);
    else if (TetraBuilding::getClassName() == type)
        lists[tetraType].push_back(obstacle);
    else
    {
        printf ("GroupDefinition::addObstacle() ERROR: type = %s\n", type);
        exit(1);
    }

    return;
}


void GroupDefinition::addGroupInstance(GroupInstance* group)
{
    groups.push_back(group);
    return;
}


static bool isContainer(int type)
{
    switch (type)
    {
    case GroupDefinition::arcType:
    case GroupDefinition::coneType:
    case GroupDefinition::sphereType:
    case GroupDefinition::tetraType:
        return true;
    default:
        return false;
    }
}


void GroupDefinition::makeTeleName(Obstacle* obs, unsigned int pos) const
{
    Teleporter* tele = (Teleporter*) obs;
    std::string fullname = depthName;
    if (tele->getName().size() > 0)
        fullname += tele->getName();
    else
    {
        // make the default name
        fullname += "/t";
        char buffer[8];
        sprintf (buffer, "%i", pos);
        fullname += buffer;
    }
    tele->setName(fullname);
    return;
}


void GroupDefinition::appendGroupName(const GroupInstance* group) const
{
    std::string newName;
    if (group->getName().size() > 0)
        newName = group->getName();
    else
    {
        // make the default name
        int count = 0;
        for (unsigned int i = 0; i < groups.size(); i++)
        {
            const GroupInstance* g = groups[i];
            if (g == group)
                break;
            if (g->getGroupDef() == group->getGroupDef())
                count++;
        }
        newName = "/";
        newName += group->getGroupDef();
        newName += "/";
        std::stringstream buffer;
        buffer << count;
        newName += buffer.str();
    }
    depthName += newName;
    depthName += ":";
    return;
}


static MeshObstacle* makeContainedMesh(int type, Obstacle* obs)
{
    MeshObstacle* mesh = NULL;
    switch (type)
    {
    case GroupDefinition::arcType:
    {
        mesh = ((ArcObstacle*)obs)->makeMesh();
        break;
    }
    case GroupDefinition::coneType:
    {
        mesh = ((ConeObstacle*)obs)->makeMesh();
        break;
    }
    case GroupDefinition::sphereType:
    {
        mesh = ((SphereObstacle*)obs)->makeMesh();
        break;
    }
    case GroupDefinition::tetraType:
    {
        mesh = ((TetraBuilding*)obs)->makeMesh();
        break;
    }
    }
    return mesh;
}


void GroupDefinition::makeGroups(const MeshTransform& xform,
                                 const ObstacleModifier& obsMod) const
{
    if (active)
    {
        logDebugMessage(1,"warning: avoided recursion, groupdef \"%s\"\n", name.c_str());
        return; // avoid recursion
    }

    active = true;

    const bool isWorld = (this == OBSTACLEMGR.getWorld());
    char groupDefBit = isWorld ? Obstacle::WorldSource :
                       Obstacle::GroupDefSource;

    for (int type = 0; type < ObstacleTypeCount; type++)
    {
        const ObstacleList& list = lists[type];
        for (unsigned int i = 0; i < list.size(); i++)
        {
            Obstacle* obs;
            if (isWorld)
            {
                obs = list[i]; // no need to copy
            }
            else
                obs = list[i]->copyWithTransform(xform);

            // the tele names are setup with default names if
            // they are not named (even for those in the world
            // groupd def). invalid teleporters are also named
            if (type == teleType)
                makeTeleName(obs, i);

            if (obs->isValid())
            {
                if (!isWorld)
                {
                    // add it to the world
                    // (this will also add container obstacles into the world group)
                    obs->setSource(Obstacle::GroupDefSource);
                    obsMod.execute(obs);
                    OBSTACLEMGR.addWorldObstacle(obs);
                    // add a modified MeshDrawInfo to the new mesh, if applicable
                    if (type == meshType)
                    {
                        const MeshObstacle* source = (const MeshObstacle*) list[i];
                        const MeshDrawInfo* diSource = source->getDrawInfo();
                        if ((diSource != NULL) &&
                                (!diSource->isServerSide()) && diSource->isValid())
                        {
                            MaterialSet matSet;
                            MaterialMap matMap;
                            diSource->getMaterials(matSet);
                            obsMod.getMaterialMap(matSet, matMap);
                            MeshDrawInfo* diDest = new MeshDrawInfo(diSource, xform, matMap);
                            MeshObstacle* dest = (MeshObstacle*) obs;
                            dest->setDrawInfo(diDest);
                        }
                    }
                }
                // generate contained meshes
                // (always get placed into the world group)
                MeshObstacle* mesh = makeContainedMesh(type, obs);
                if ((mesh != NULL) && mesh->isValid())
                {
                    mesh->setSource(Obstacle::ContainerSource | groupDefBit);
                    obsMod.execute(mesh);
                    OBSTACLEMGR.addWorldObstacle(mesh);
                }
            }
        }
    }

    for (unsigned int i = 0; i < groups.size(); i++)
    {
        const GroupInstance* group = groups[i];
        const GroupDefinition* groupDef =
            OBSTACLEMGR.findGroupDef(group->getGroupDef());
        if (groupDef != NULL)
        {
            // make the new depth name
            std::string tmpDepthName = depthName;
            appendGroupName(group);

            // setup the transform and modifier
            ObstacleModifier newObsMod(obsMod, *group);
            MeshTransform tmpXform = xform;
            tmpXform.prepend(group->getTransform());

            // recurse and make plentiful
            groupDef->makeGroups(tmpXform, newObsMod);

            // revert to the old depth name
            depthName = tmpDepthName;
        }
        else
        {
            logDebugMessage(1,"warning: group definition \"%s\" is missing\n",
                            group->getGroupDef().c_str());
        }
    }

    active = false;

    return;
}


void GroupDefinition::replaceBasesWithBoxes()
{
    ObstacleList& list = lists[baseType];
    for (unsigned int i = 0; i < list.size(); i++)
    {
        BaseBuilding* base = (BaseBuilding*) list[i];
        const auto &baseSize = base->getSize();
        BoxBuilding* box =
            new BoxBuilding(base->getPosition(), base->getRotation(),
                            baseSize[0], baseSize[1], baseSize[2],
                            base->isDriveThrough(), base->isShootThrough(),
                            base->canRicochet(), false);
        delete base;
        list.remove(i);
        i--;
        addObstacle(box);
    }
    return;
}


void GroupDefinition::clear()
{
    for (int type = 0; type < ObstacleTypeCount; type++)
    {
        ObstacleList& list = lists[type];
        for (unsigned int i = 0; i < list.size(); i++)
            delete list[i];
        list.clear();
    }

    for (unsigned int i = 0; i < groups.size(); i++)
        delete groups[i];
    groups.clear();

    return;
}


void GroupDefinition::tighten()
{
    for (int type = 0; type < ObstacleTypeCount; type++)
        lists[type].tighten();
    return;
}


void GroupDefinition::sort(int (*compare)(const void* a, const void* b))
{
    if (this != OBSTACLEMGR.getWorld())
    {
        return; // only sort the world groupdef
    }
    for (int type = 0; type < ObstacleTypeCount; type++)
        lists[type].sort(compare);
    return;
}


void GroupDefinition::deleteInvalidObstacles()
{
    if (this != OBSTACLEMGR.getWorld())
    {
        return; // only delete invalid obstacles in the world groupdef
    }
    for (int type = 0; type < ObstacleTypeCount; type++)
    {
        ObstacleList& list = lists[teleType];
        for (unsigned int i = 0; i < list.size(); i++)
        {
            Obstacle* obs = list[i];
            if (!obs->isValid())
            {
                logDebugMessage(1,"Deleted invalid %s obstacle\n", obs->getType());
                delete obs;
                list.remove(i);
                i--; // don't miss the substitute
            }
        }
    }
    return;
}


void GroupDefinition::getSourceMeshes(std::vector<MeshObstacle*>& meshes) const
{
    const bool isWorld = (this == OBSTACLEMGR.getWorld());

    const ObstacleList& list = lists[meshType];
    for (unsigned int i = 0; i < list.size(); i++)
    {
        MeshObstacle* mesh = (MeshObstacle*)list[i];
        if (!isWorld || mesh->isFromWorldFile())
        {
            const int listSize = (int)meshes.size();
            int j;
            for (j = 0; j < listSize; j++)
            {
                if (meshes[j] == mesh)
                    break;
            }
            if (j == listSize)
            {
                meshes.push_back(mesh); // a new entry
            }
        }
    }

    for (unsigned int i = 0; i < groups.size(); i++)
    {
        const GroupInstance* group = groups[i];
        const GroupDefinition* groupDef =
            OBSTACLEMGR.findGroupDef(group->getGroupDef());
        if (groupDef != NULL)
            groupDef->getSourceMeshes(meshes);
    }

    return;
}


void* GroupDefinition::pack(void* buf) const
{
    buf = nboPackStdString(buf, name);

    unsigned int i;
    for (int type = 0; type < ObstacleTypeCount; type++)
    {
        const ObstacleList& list = getList(type);
        int count = 0;
        for (i = 0; i < list.size(); i++)
        {
            if (list[i]->isFromWorldFile())
                count++;
        }
        buf = nboPackUInt(buf, count);
        for (i = 0; i < list.size(); i++)
        {
            if (list[i]->isFromWorldFile())
                buf = list[i]->pack(buf);
        }
    }

    buf = nboPackUInt(buf, groups.size());
    for (i = 0; i < groups.size(); i++)
        buf = groups[i]->pack(buf);

    return buf;
}


const void* GroupDefinition::unpack(const void* buf)
{
    buf = nboUnpackStdString(buf, name);

    uint32_t i, count;

    for (int type = 0; type < ObstacleTypeCount; type++)
    {
        buf = nboUnpackUInt(buf, count);
        for (i = 0; i < count; i++)
        {
            Obstacle* obs = newObstacle(type);
            if (obs != NULL)
            {
                buf = obs->unpack(buf);
                if (obs->isValid())
                    lists[type].push_back(obs);
            }
        }
    }

    buf = nboUnpackUInt(buf, count);
    for (i = 0; i < count; i++)
    {
        GroupInstance* group = new GroupInstance;
        buf = group->unpack(buf);
        addGroupInstance(group);
    }

    return buf;
}


int GroupDefinition::packSize() const
{
    int fullSize = 0;

    fullSize += nboStdStringPackSize(name);

    for (int type = 0; type < ObstacleTypeCount; type++)
    {
        fullSize += sizeof(uint32_t);
        const ObstacleList& list = getList(type);
        for (unsigned int i = 0; i < list.size(); i++)
        {
            if (list[i]->isFromWorldFile())
                fullSize += list[i]->packSize();
        }
    }
    fullSize += sizeof(uint32_t);
    for (unsigned int i = 0; i < groups.size(); i++)
        fullSize += groups[i]->packSize();

    return fullSize;
}


void GroupDefinition::printGrouped(std::ostream& out,
                                   const std::string& indent) const
{
    const bool isWorld = (this == OBSTACLEMGR.getWorld());
    const bool saveAsMeshes = BZDB.isTrue("saveAsMeshes");
    std::string myIndent = indent;

    // deal with indenting
    if (!isWorld)
    {
        myIndent += "  ";
        out << indent << "define " << name << std::endl;
    }

    // print the obstacles
    for (int type = 0; type < ObstacleTypeCount; type++)
    {
        const ObstacleList& list = getList(type);
        for (unsigned int i = 0; i < list.size(); i++)
        {
            Obstacle* obs = list[i];

            if (!obs->isFromGroupDef() && !obs->isFromContainer())
            {
                if (!saveAsMeshes)
                {
                    if (!obs->isFromContainer())
                        obs->print(out, myIndent);
                }
                else
                {
                    if (!isContainer(type))
                        obs->print(out, myIndent);
                    else
                    {
                        // rebuild the mesh  (ya, just to print it)
                        MeshObstacle* mesh = makeContainedMesh(type, obs);
                        if ((mesh != NULL) && (mesh->isValid()))
                            mesh->print(out, myIndent);
                        delete mesh;
                    }
                }
            }
        }
    }

    // print the groups
    for (unsigned int i = 0; i < groups.size(); i++)
        groups[i]->print(out, myIndent);

    // deal with indenting
    if (!isWorld)
        out << indent << "enddef" << std::endl << std::endl;

    return;
}


void GroupDefinition::printFlatFile(std::ostream& out,
                                    const std::string& indent) const
{
    const bool saveAsOBJ = BZDB.isTrue("saveAsOBJ");
    const bool saveAsMeshes = BZDB.isTrue("saveAsMeshes");

    // print the obstacles
    for (int type = 0; type < ObstacleTypeCount; type++)
    {
        const ObstacleList& list = getList(type);
        for (unsigned int i = 0; i < list.size(); i++)
        {
            const Obstacle* obs = list[i];

            if (!saveAsMeshes)
            {
                if (!obs->isFromContainer())
                    obs->print(out, indent);
            }
            else
            {
                if (!isContainer(type))
                {
                    if (!saveAsOBJ)
                        obs->print(out, indent);
                    else
                        obs->printOBJ(out, indent);
                }
            }
        }
    }

    return;
}


void GroupDefinition::clearDepthName()
{
    depthName = "";
    return;
}


//////////////////////////////////////////////////////////////////////////////
//
// Group Definition Manager
// - utility class to keep track of group definitions
//

GroupDefinitionMgr OBSTACLEMGR;


GroupDefinitionMgr::GroupDefinitionMgr() : world("")
{
    return;
}


GroupDefinitionMgr::~GroupDefinitionMgr()
{
    clear();
    return;
}


void GroupDefinitionMgr::clear()
{
    world.clear();
    for (unsigned int i = 0; i < list.size(); i++)
    {
        list[i]->clear();
        delete list[i];
    }
    list.clear();
    return;
}


void GroupDefinitionMgr::tighten()
{
    for (unsigned int i = 0; i < list.size(); i++)
        list[i]->tighten();
    world.tighten();
    return;
}


static int compareHeights(const void* a, const void* b)
{
    const Obstacle* obsA = *((const Obstacle* const *)a);
    const Obstacle* obsB = *((const Obstacle* const *)b);
    const Extents& eA = obsA->getExtents();
    const Extents& eB = obsB->getExtents();

    if (eA.maxs[2] > eB.maxs[2])
        return -1;
    else
        return +1;
}


void GroupDefinitionMgr::makeWorld()
{
    GroupDefinition::clearDepthName();

    MeshTransform noXform;
    ObstacleModifier noMods;

    world.makeGroups(noXform, noMods);

    world.deleteInvalidObstacles();

    // sort from top to bottom for enhanced radar
    for (int type = 0; type < GroupDefinition::ObstacleTypeCount; type++)
        world.sort(compareHeights);

    tighten();

    return;
}


void GroupDefinitionMgr::replaceBasesWithBoxes()
{
    world.replaceBasesWithBoxes();
}


void GroupDefinitionMgr::addWorldObstacle(Obstacle* obstacle)
{
    world.addObstacle(obstacle);
    return;
}


void GroupDefinitionMgr::addGroupDef(GroupDefinition* groupdef)
{
    if (groupdef->getName().size() > 0)
        list.push_back(groupdef);
    else
        delete groupdef;
    return;
}


GroupDefinition* GroupDefinitionMgr::findGroupDef(const std::string& name) const
{
    if (name.size() <= 0)
        return NULL;
    for (unsigned int i = 0; i < list.size(); i++)
    {
        if (name == list[i]->getName())
            return list[i];
    }
    return NULL;
}


void GroupDefinitionMgr::getSourceMeshes(std::vector<MeshObstacle*>& meshes) const
{
    meshes.clear();
    world.getSourceMeshes(meshes);
    return;
}


void* GroupDefinitionMgr::pack(void* buf) const
{
    buf = world.pack(buf);
    buf = nboPackUInt(buf, list.size());
    for (unsigned int i = 0; i < list.size(); i++)
        buf = list[i]->pack(buf);
    return buf;
}


const void* GroupDefinitionMgr::unpack(const void* buf)
{
    buf = world.unpack(buf);
    uint32_t i, count;
    buf = nboUnpackUInt(buf, count);
    for (i = 0; i < count; i++)
    {
        GroupDefinition* groupdef = new GroupDefinition("");
        buf = groupdef->unpack(buf);
        addGroupDef(groupdef);
    }
    return buf;
}


int GroupDefinitionMgr::packSize() const
{
    int fullSize = 0;

    fullSize += world.packSize();
    fullSize += sizeof(uint32_t);
    for (unsigned int i = 0; i < list.size(); i++)
        fullSize += list[i]->packSize();
    return fullSize;
}


void GroupDefinitionMgr::print(std::ostream& out,
                               const std::string& indent) const
{
    const bool saveAsOBJ = BZDB.isTrue("saveAsOBJ");
    const bool saveFlatFile = BZDB.isTrue("saveFlatFile");
    const bool saveAsMeshes = BZDB.isTrue("saveAsMeshes");
    if (saveAsOBJ)
    {
        BZDB.set("saveAsMeshes", "1");
        BZDB.set("saveFlatFile", "1");
    }

    // for unique OBJ mesh ids
    Obstacle::resetObjCounter();

    if (!(saveFlatFile || saveAsOBJ))
    {
        // print the group definitions
        for (unsigned int i = 0; i < list.size(); i++)
            list[i]->printGrouped(out, indent);
        // print the world
        world.printGrouped(out, indent);
    }
    else
    {
        // print the world
        world.printFlatFile(out, indent);
    }

    if (saveAsOBJ)
    {
        BZDB.set("saveAsMeshes", saveAsMeshes ? "1" : "0");
        BZDB.set("saveFlatFile", saveFlatFile ? "1" : "0");
    }

    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
