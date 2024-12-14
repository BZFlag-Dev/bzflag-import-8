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
#include "CustomMesh.h"

// System headers
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/* bzfs implementation headers */
#include "CustomMeshFace.h"
#include "ParseMaterial.h"

/* common implementation headers */
#include "PhysicsDriver.h"
#include "ObstacleMgr.h"
#include "MeshDrawInfo.h"


CustomMesh::CustomMesh()
{
    face = NULL;
    phydrv = -1;
    noclusters = false;
    smoothBounce = false;
    driveThrough = false;
    shootThrough = false;
    decorative = false;
    drawInfo = NULL;
    material.setTexture("mesh");

    return;
}


CustomMesh::~CustomMesh()
{
    if (face != NULL)
    {
        std::cout << "discarded incomplete mesh face" << std::endl;
        delete face;
    }

    std::vector<CustomMeshFace*>::iterator face_it;
    for (face_it = faces.begin(); face_it != faces.end(); ++face_it)
    {
        CustomMeshFace* _face = *face_it;
        delete _face;
    }

    return;
}


bool CustomMesh::read(const char *cmd, std::istream& input)
{
    bool materror;

    if (strncasecmp(cmd, "lod", 3) == 0)
    {
        std::string line, option;
        std::getline(input, line);
        input.putback('\n');
        option = cmd + line;
        std::cout << option << std::endl;
        lodOptions.push_back(option);
    }
    else if (strcasecmp(cmd, "endface") == 0)
    {
        if (face == NULL)
            std::cout << "extra 'endface' keyword found" << std::endl;
        else
        {
            faces.push_back(face);
            face = NULL;
        }
    }
    else if (face)
    {
        // currently processing a face
        return face->read(cmd, input);
    }
    else if (strcasecmp(cmd, "face") == 0)
    {
        if (face != NULL)
        {
            std::cout << "discarding incomplete mesh face" << std::endl;
            delete face;
        }
        face = new CustomMeshFace (material, phydrv, noclusters,
                                   smoothBounce, driveThrough, shootThrough);
    }
    else if (strcasecmp(cmd, "inside") == 0)
    {
        glm::vec3 inside;
        if (!(input >> inside[0] >> inside[1] >> inside[2]))
            return false;
        checkTypes.push_back(MeshObstacle::CheckInside);
        checkPoints.push_back(inside);
    }
    else if (strcasecmp(cmd, "outside") == 0)
    {
        glm::vec3 outside;
        if (!(input >> outside[0] >> outside[1] >> outside[2]))
            return false;
        checkTypes.push_back(MeshObstacle::CheckOutside);
        checkPoints.push_back(outside);
    }
    else if (strcasecmp(cmd, "vertex") == 0)
    {
        glm::vec3 vertex;
        if (!(input >> vertex[0] >> vertex[1] >> vertex[2]))
            return false;
        vertices.push_back(vertex);
    }
    else if (strcasecmp(cmd, "normal") == 0)
    {
        glm::vec3 normal;
        if (!(input >> normal[0] >> normal[1] >> normal[2]))
            return false;
        normals.push_back(normal);
    }
    else if (strcasecmp(cmd, "texcoord") == 0)
    {
        glm::vec2 texcoord;
        if (!(input >> texcoord[0] >> texcoord[1]))
            return false;
        texcoords.push_back(texcoord);
    }
    else if (strcasecmp(cmd, "phydrv") == 0)
    {
        std::string drvname;
        if (!(input >> drvname))
        {
            std::cout << "missing Physics Driver parameter" << std::endl;
            return false;
        }
        phydrv = PHYDRVMGR.findDriver(drvname);
        if ((phydrv == -1) && (drvname != "-1"))
            std::cout << "couldn't find PhysicsDriver: " << drvname << std::endl;
    }
    else if (strcasecmp(cmd, "smoothbounce") == 0)
        smoothBounce = true;
    else if (strcasecmp(cmd, "noclusters") == 0)
        noclusters = true;
    else if (strcasecmp(cmd, "decorative") == 0)
        decorative = true;
    else if (strcasecmp(cmd, "drawInfo") == 0)
    {
        if (drawInfo != NULL)
            std::cout << "WARNING: multiple drawInfo, using first" << std::endl;
        else
        {
            drawInfo = new MeshDrawInfo();

            bool parseSuccess = drawInfo->parse(input);
            lines += drawInfo->getLineCount();

            if (!parseSuccess)
            {
                std::cout << "WARNING: invalid drawInfo" << std::endl;
                delete drawInfo;
                drawInfo = NULL;
            }
        }
    }
    else if (parseMaterials(cmd, input, &material, 1, materror))
    {
        if (materror)
            return false;
    }
    else
    {
        // NOTE: the position, size, and rotation
        //       parameters are currently ignored
        return WorldFileObstacle::read(cmd, input);
    }

    return true;
}


void CustomMesh::writeToGroupDef(GroupDefinition *groupdef)
{
    // include the old style parameters
    MeshTransform xform;
    if ((size[0] != 1.0f) || (size[1] != 1.0f) || (size[2] != 1.0f))
        xform.addScale(size);
    if (rotation != 0.0f)
    {
        const auto zAxis = glm::vec3(0.0f, 0.0f, 1.0f);
        xform.addSpin((float)(rotation * (180.0 / M_PI)), zAxis);
    }
    if ((pos[0] != 0.0f) || (pos[1] != 0.0f) || (pos[2] != 0.0f))
        xform.addShift(pos);
    xform.append(transform);

    // hack to invalidate decorative meshes on older clients
    bool forcePassable = false;
    if (drawInfo)
    {
        glm::vec3 vert;
        if (decorative)
        {
            vert[0] = vert[1] = vert[2] = (Obstacle::maxExtent * 2.0f);
            if (!faces.empty() && !(driveThrough && shootThrough))
            {
                logDebugMessage(0,"WARNING: mesh is supposed to be decorative, setting to passable\n");
                forcePassable = true;
            }
        }
        else
            vert[0] = vert[1] = vert[2] = 0.0f;
        vertices.push_back(vert);
    }

    MeshObstacle* mesh =
        new MeshObstacle(xform, checkTypes, checkPoints,
                         vertices, normals, texcoords, faces.size(),
                         noclusters, smoothBounce,
                         driveThrough || forcePassable,
                         shootThrough || forcePassable,
                         ricochet);

    mesh->setName(name);

    // add the faces
    std::vector<CustomMeshFace*>::const_iterator face_it;
    for (face_it = faces.begin(); face_it != faces.end(); ++face_it)
    {
        const CustomMeshFace* customFace = *face_it;
        customFace->write(mesh);
    }

    mesh->finalize();

    if (drawInfo)
    {
        ((MeshDrawInfo*)drawInfo)->serverSetup(mesh);
        if (drawInfo->isValid())
            mesh->setDrawInfo(drawInfo);
        else
            delete ((MeshDrawInfo*)drawInfo);
    }

    groupdef->addObstacle(mesh);

    return;
}


// Local variables: ***
// mode: C++ ***
// tab-width: 4***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
