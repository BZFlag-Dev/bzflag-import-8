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
#include "BZWReader.h"

// implementation-specific system headers
#include <fstream>
#include <sstream>
#include <ctype.h>

// implementation-specific bzflag headers
#include "BZDBCache.h"

// implementation-specific bzfs-specific headers
#include "TeamBases.h"
#include "WorldFileObject.h"
#include "CustomGroup.h"
#include "CustomBox.h"
#include "CustomPyramid.h"
#include "CustomGate.h"
#include "CustomLink.h"
#include "CustomBase.h"
#include "CustomWeapon.h"
#include "CustomWorld.h"
#include "CustomZone.h"
#include "CustomTetra.h"
#include "CustomMesh.h"
#include "CustomArc.h"
#include "CustomCone.h"
#include "CustomSphere.h"
#include "CustomWaterLevel.h"
#include "CustomDynamicColor.h"
#include "CustomTextureMatrix.h"
#include "CustomMaterial.h"
#include "CustomPhysicsDriver.h"
#include "CustomMeshTransform.h"

// common headers
#include "ObstacleMgr.h"
#include "BaseBuilding.h"
#include "TextUtils.h"
#include "StateDatabase.h"

// bzfs specific headers
#include "bzfs.h"
#include "bzfsPython.h"


BZWReader::BZWReader(std::string filename) : cURLManager(), location(filename),
    input(NULL)
{
    static const std::string httpProtocol("http://");
    static const std::string ftpProtocol("ftp://");
    static const std::string fileProtocol("file:/");

    errorHandler = new BZWError(location);

    if ((filename.substr(0, httpProtocol.size()) == httpProtocol)
            || (filename.substr(0, ftpProtocol.size()) == ftpProtocol)
            || (filename.substr(0, fileProtocol.size()) == fileProtocol))
    {
        setURL(location);
        performWait();
        input = new std::istringstream(httpData);
    }
    else
        input = new std::ifstream(filename.c_str(), std::ios::in);

    // .BZW is the official worldfile extension, warn for others
    if ((filename.length() < 4) ||
            (strcasecmp(filename.substr(filename.length() - 4, 4).c_str(),
                        ".bzw") != 0))
    {
        errorHandler->warning(std::string(
                                  "world file extension is not .bzw, trying to load anyway"), 0);
    }

    if (input->peek() == EOF)
        errorHandler->fatalError(std::string("could not find bzflag world file"), 0);
}


BZWReader::~BZWReader()
{
    // clean up
    delete errorHandler;
    delete input;
}


void BZWReader::finalization(char *data, unsigned int length, bool good)
{
    if (good)
        httpData = std::string(data, length);
    else
        httpData = "";
}

void BZWReader::readToken(char *buffer, int n)
{
    int c = -1;

    // skip whitespace
    while (input->good() && (c = input->get()) != -1 && isspace(c) && c != '\n')
        ;

    // read up to whitespace or n - 1 characters into buffer
    int i = 0;
    if (c != -1 && c != '\n')
    {
        buffer[i++] = c;
        while (input->good() && i < n - 1 && (c = input->get()) != -1 && !isspace(c))
            buffer[i++] = (char)c;
    }

    // terminate string
    buffer[i] = 0;

    // put back last character we didn't use
    if (c != -1 && isspace(c))
        input->putback(c);
}


static bool parseNormalObject(const char* token, WorldFileObject** object)
{
    WorldFileObject* tmpObj = NULL;

    if (strcasecmp(token, "box") == 0)
        tmpObj = new CustomBox;
    else if (strcasecmp(token, "pyramid") == 0)
        tmpObj = new CustomPyramid();
    else if (strcasecmp(token, "base") == 0)
        tmpObj = new CustomBase;
    else if (strcasecmp(token, "link") == 0)
        tmpObj = new CustomLink();
    else if (strcasecmp(token, "mesh") == 0)
        tmpObj = new CustomMesh;
    else if (strcasecmp(token, "arc") == 0)
        tmpObj = new CustomArc(false);
    else if (strcasecmp(token, "meshbox") == 0)
        tmpObj = new CustomArc(true);
    else if (strcasecmp(token, "cone") == 0)
        tmpObj = new CustomCone(false);
    else if (strcasecmp(token, "meshpyr") == 0)
        tmpObj = new CustomCone(true);
    else if (strcasecmp(token, "sphere") == 0)
        tmpObj = new CustomSphere;
    else if (strcasecmp(token, "tetra") == 0)
        tmpObj = new CustomTetra();
    else if (strcasecmp(token, "weapon") == 0)
        tmpObj = new CustomWeapon;
    else if (strcasecmp(token, "zone") == 0)
        tmpObj = new CustomZone;
    else if (strcasecmp(token, "waterLevel") == 0)
        tmpObj = new CustomWaterLevel;
    else if (strcasecmp(token, "dynamicColor") == 0)
        tmpObj = new CustomDynamicColor;
    else if (strcasecmp(token, "textureMatrix") == 0)
        tmpObj = new CustomTextureMatrix;
    else if (strcasecmp(token, "material") == 0)
        tmpObj = new CustomMaterial;
    else if (strcasecmp(token, "physics") == 0)
        tmpObj = new CustomPhysicsDriver;
    else if (strcasecmp(token, "transform") == 0)
        tmpObj = new CustomMeshTransform;

    if (tmpObj != NULL)
    {
        *object = tmpObj;
        return true;
    }
    else
        return false;
}

bool BZWReader::readWorldStream(std::vector<WorldFileObject*>& wlist,
                                GroupDefinition* groupDef)
{
    // make sure input is valid
    if (input->peek() == EOF)
    {
        errorHandler->fatalError(std::string("unexpected EOF"), 0);
        return false;
    }

    int line = 1;
    char buffer[1024];
    WorldFileObject* object = NULL;
    WorldFileObject* newObject = NULL;
    WorldFileObject* const fakeObject = (WorldFileObject*)((char*)1);
    GroupDefinition* const worldDef = const_cast<GroupDefinition*>(OBSTACLEMGR.getWorld());
    GroupDefinition* const startDef = groupDef;

    std::string customObject;
    std::vector<std::string>  customLines;
    bool fromPython = false;

    bool gotWorld = false;

    while (!input->eof() && !input->fail() && input->good())
    {
        // watch out for starting a new object when one is already in progress
        if (newObject)
        {
            if (object)
            {
                errorHandler->warning(
                    std::string("discarding incomplete object"), line);
                if (object != fakeObject)
                    delete object;
                else if (not customObject.empty())
                {
                    customObject = "";
                    customLines.clear();
                }
            }
            object = newObject;
            newObject = NULL;
        }

        // read first token but do not skip newlines
        readToken(buffer, sizeof(buffer));
        if (strcmp(buffer, "") == 0)
        {
            // ignore blank line
        }
        else if (buffer[0] == '#')
        {
            // ignore comment

        }
        else if (strcasecmp(buffer, "end") == 0)
        {
            if (object)
            {
                if (object != fakeObject)
                {
                    line += object->getLineCount();

                    if (object->usesManager())
                    {
                        object->writeToManager();
                        delete object;
                    }
                    else if (object->usesGroupDef())
                    {
                        object->writeToGroupDef(groupDef);
                        delete object;
                    }
                    else
                        wlist.push_back(object);
                }
                else if (not customObject.empty())
                {
                    if (fromPython)
                        bzPython_customZoneMapObject(customObject, customLines);
                    else
                    {
                        bz_CustomMapObjectInfo data;
                        data.name = bz_ApiString(customObject);
                        for (unsigned int i = 0; i < customLines.size(); i++)
                            data.data.push_back(customLines[i]);
                        customObjectMap[customObject]->MapObject(bz_ApiString(customObject),&data);
                    }
                }
                object = NULL;
            }
            else
            {
                errorHandler->fatalError(
                    std::string("unexpected \"end\" token"), line);
                return false;
            }

        }
        else if (parseNormalObject(buffer, &newObject))
        {
            // newObject has already been assigned

        }
        else if (strcasecmp(buffer, "define") == 0)
        {
            if (groupDef != worldDef)
            {
                errorHandler->warning(
                    std::string("group definitions can not be nested \"") +
                    std::string(buffer) + std::string("\" - skipping"), line);
            }
            else
            {
                readToken(buffer, sizeof(buffer));
                if (strlen(buffer) > 0)
                {
                    if (OBSTACLEMGR.findGroupDef(buffer) != NULL)
                    {
                        errorHandler->warning(
                            std::string("duplicate group definition \"") +
                            std::string(buffer) + std::string("\" - using newest"), line);
                    }
                    groupDef = new GroupDefinition(buffer);
                }
                else
                {
                    errorHandler->warning(
                        std::string("missing group definition name"), line);
                }
            }

        }
        else if (strcasecmp(buffer, "enddef") == 0)
        {
            if (groupDef == worldDef)
            {
                errorHandler->warning(
                    std::string("enddef without define - skipping"), line);
            }
            else
            {
                OBSTACLEMGR.addGroupDef(groupDef);
                groupDef = worldDef;
            }

        }
        else if (strcasecmp(buffer, "group") == 0)
        {
            readToken(buffer, sizeof(buffer));
            if (strlen(buffer) <= 0)
            {
                errorHandler->warning(
                    std::string("missing group definition reference"), line);
            }
            newObject = new CustomGroup(buffer);

        }
        else if (strcasecmp(buffer, "teleporter") == 0)
        {
            readToken(buffer, sizeof(buffer));
            newObject = new CustomGate(buffer);

        }
        else if (strcasecmp(buffer, "options") == 0)
            newObject = fakeObject;

        else if (strcasecmp(buffer, "include") == 0)
        {
            // NOTE: intentionally undocumented  (at the moment)
            readToken(buffer, sizeof(buffer));
            std::string incName = buffer;
            if (object == NULL)
            {
                // FIXME - check for recursion
                //       - better filename handling ("", spaces, and / vs. \\)
                //       - make relative names work from the base file location
                logDebugMessage(1,"%s: (line %i): including \"%s\"\n",
                                location.c_str(), line, incName.c_str());
                BZWReader incFile(incName);
                std::vector<WorldFileObject*> incWlist;
                if (incFile.readWorldStream(incWlist, groupDef))
                {
                    // add the included objects
                    for (unsigned int i = 0; i < incWlist.size(); i++)
                        wlist.push_back(incWlist[i]);
                }
                else
                {
                    // empty the failed list
                    emptyWorldFileObjectList(incWlist);
                    errorHandler->fatalError(
                        TextUtils::format("including \"%s\"", incName.c_str()), line);
                    return false;
                }
            }
            else
            {
                errorHandler->warning(
                    TextUtils::format("including \"%s\" within an obstacle, skipping",
                                      incName.c_str()), line);
            }

        }
        else if (strcasecmp(buffer, "world") == 0)
        {
            if (!gotWorld)
            {
                newObject = new CustomWorld();
                gotWorld = true;
            }
            else
            {
                errorHandler->warning(
                    std::string("multiple \"world\" sections found"), line);
            }

        }
        else if (object)
        {
            if (object != fakeObject)
            {
                if (!object->read(buffer, *input))
                {
                    // unknown token
                    errorHandler->warning(
                        std::string("unknown object parameter \"") +
                        std::string(buffer) + std::string("\" - skipping"), line);
                    // delete object;
                    // return false;
                }
            }
            else if (not customObject.empty())
            {
                std::string thisline = buffer;
                thisline += " ";

                while (input->good() && input->peek() != '\n')
                {
                    input->get(buffer, sizeof(buffer));
                    thisline += buffer;
                }

                thisline = TextUtils::replace_all(thisline,std::string("\r"),std::string(""));
                thisline = TextUtils::replace_all(thisline,std::string("\n"),std::string(""));
                customLines.push_back(thisline);
            }

        }
        else     // filling the current object
        {
            std::string currObject = TextUtils::toupper(std::string(buffer));
            // unknown token
            if (customObjectMap.find(currObject) != customObjectMap.end())
            {
                customObject = currObject;
                object = fakeObject;
                customLines.clear();
                fromPython = false;
            }
            else if (bzPython_CheckIfCustomMap(currObject))
            {
                customObject = currObject;
                object = fakeObject;
                customLines.clear();
                fromPython = true;
            }
            else
            {
                errorHandler->warning(
                    std::string("invalid object type \"") +
                    std::string(buffer) + std::string("\" - skipping"), line);
                if (object != fakeObject)
                    delete object;
            }
            // return false;
        }

        // discard remainder of line
        while (input->good() && input->peek() != '\n')
            input->get(buffer, sizeof(buffer));
        input->getline(buffer, sizeof(buffer));
        ++line;
    }

    bool retval = true;
    if (object)
    {
        errorHandler->fatalError(std::string("missing \"end\" parameter"), line);
        if (object != fakeObject)
            delete object;
        retval = false;
    }
    if (groupDef != startDef)
    {
        errorHandler->fatalError(std::string("missing \"enddef\" parameter"), line);
        if (startDef == worldDef)
            delete groupDef;
        retval = false;
    }
    return retval;
}


WorldInfo* BZWReader::defineWorldFromFile()
{
    // create world object
    WorldInfo *myWorld = new WorldInfo;
    if (!myWorld)
    {
        errorHandler->fatalError(std::string("WorldInfo failed to initialize"), 0);
        return NULL;
    }

    // read file
    std::vector<WorldFileObject*> list;
    GroupDefinition* worldDef = const_cast<GroupDefinition*>(OBSTACLEMGR.getWorld());
    if (!readWorldStream(list, worldDef))
    {
        emptyWorldFileObjectList(list);
        errorHandler->fatalError(std::string("world file failed to load."), 0);
        delete myWorld;
        return NULL;
    }

    if (!BZDB.isTrue("noWalls"))
        makeWalls();

    // generate group instances
    OBSTACLEMGR.makeWorld();

    // make local bases
    unsigned int i;
    const ObstacleList& baseList = OBSTACLEMGR.getBases();
    for (i = 0; i < baseList.size(); i++)
    {
        const BaseBuilding* base = (const BaseBuilding*) baseList[i];
        TeamColor color = (TeamColor)base->getTeam();
        if (bases.find(color) == bases.end())
            bases[color] = TeamBases((TeamColor)color);
        bases[color].addBase(base->getPosition(), base->getSize(),
                             base->getRotation());
    }

    // add objects
    const unsigned int n = list.size();
    for (i = 0; i < n; ++i)
        list[i]->writeToWorld(myWorld);

    // clean up
    emptyWorldFileObjectList(list);
    myWorld->finishWorld();

    return myWorld;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
