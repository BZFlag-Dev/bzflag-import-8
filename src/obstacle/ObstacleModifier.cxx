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

// implementation header
#include "ObstacleModifier.h"

// system headers
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>

// common headers
#include "ObstacleMgr.h"
#include "BzMaterial.h"

// obstacle headers
#include "Obstacle.h"
#include "BaseBuilding.h"
#include "MeshObstacle.h"


void ObstacleModifier::init()
{
    // team
    modifyTeam = false;
    team = 0;
    // tint
    modifyColor = false;
    tint[0] = tint[1] = tint[2] = tint[3] = 1.0f;
    // phydrv
    modifyPhysicsDriver = false;
    phydrv = -1;
    // material;
    modifyMaterial = false;
    material = NULL;
    // passable bits
    driveThrough = false;
    shootThrough = false;
    ricochet     = false;

    return;
}


ObstacleModifier::ObstacleModifier()
{
    init();
    return;
}


ObstacleModifier::ObstacleModifier(const ObstacleModifier& obsMod,
                                   const GroupInstance& grpinst)
{
    init();

    if (grpinst.modifyTeam || obsMod.modifyTeam)
    {
        modifyTeam = true;
        if (obsMod.modifyTeam)
            team = obsMod.team;
        else
            team = grpinst.team;
    }

    if (grpinst.modifyColor || obsMod.modifyColor)
    {
        modifyColor = true;
        if (grpinst.modifyColor && obsMod.modifyColor)
        {
            tint[0] = grpinst.tint[0] * obsMod.tint[0];
            tint[1] = grpinst.tint[1] * obsMod.tint[1];
            tint[2] = grpinst.tint[2] * obsMod.tint[2];
            tint[3] = grpinst.tint[3] * obsMod.tint[3];
        }
        else if (obsMod.modifyColor)
            memcpy (tint, obsMod.tint, sizeof(float[4]));
        else
            memcpy (tint, grpinst.tint, sizeof(float[4]));
    }

    if (grpinst.modifyPhysicsDriver || obsMod.modifyPhysicsDriver)
    {
        modifyPhysicsDriver = true;
        if (obsMod.modifyPhysicsDriver)
            phydrv = obsMod.phydrv;
        else
            phydrv = grpinst.phydrv;
    }

    if (obsMod.modifyMaterial)
    {
        modifyMaterial = true;
        material = obsMod.material;
    }
    else if (obsMod.matMap.size() > 0)
    {
        if (grpinst.modifyMaterial)
        {
            modifyMaterial = true;
            MaterialMap::const_iterator find;
            find = obsMod.matMap.find(grpinst.material);
            if (find != obsMod.matMap.end())
                material = find->second;
            else
                material = grpinst.material;
        }
        else
        {
            matMap = obsMod.matMap;
            MaterialMap::const_iterator it;
            for (it = grpinst.matMap.begin(); it != grpinst.matMap.end(); ++it)
            {
                MaterialMap::const_iterator find_it;
                find_it = obsMod.matMap.find(it->second);
                if (find_it != obsMod.matMap.end())
                    matMap[it->first] = find_it->second;
                else
                    matMap[it->first] = it->second;
            }
        }
    }
    else if (grpinst.modifyMaterial)
    {
        modifyMaterial = true;
        material = grpinst.material;
    }
    else if (grpinst.matMap.size() > 0)
        matMap = grpinst.matMap;

    driveThrough = grpinst.driveThrough || obsMod.driveThrough;
    shootThrough = grpinst.shootThrough || obsMod.shootThrough;
    ricochet     = grpinst.ricochet     || obsMod.ricochet;

    return;
}


ObstacleModifier::~ObstacleModifier()
{
    return;
}


static const BzMaterial* getTintedMaterial(const float tint[4],
        const BzMaterial* mat)
{
    BzMaterial tintmat(*mat);
    glm::vec4 newColor;

    // diffuse
    auto oldColor = mat->getDiffuse();
    newColor[0] = oldColor[0] * tint[0];
    newColor[1] = oldColor[1] * tint[1];
    newColor[2] = oldColor[2] * tint[2];
    newColor[3] = oldColor[3] * tint[3];
    tintmat.setDiffuse(newColor);
    // ambient, specular, and emission are intentionally unmodified

    const BzMaterial* newmat = MATERIALMGR.addMaterial(&tintmat);
    return newmat;
}


void ObstacleModifier::execute(Obstacle* obstacle) const
{
    if (modifyTeam)
    {
        if (obstacle->getType() == BaseBuilding::getClassName())
        {
            BaseBuilding* base = (BaseBuilding*) obstacle;
            base->team = team;
        }
    }
    if (modifyColor || modifyMaterial || (matMap.size() > 0))
    {
        if (obstacle->getType() == MeshObstacle::getClassName())
        {
            const MeshObstacle* mesh = (MeshObstacle*) obstacle;
            for (int i = 0; i < mesh->getFaceCount(); i++)
            {
                MeshFace* face = mesh->getFace(i);
                if (modifyMaterial)
                    face->bzMaterial = material;
                else if (matMap.size() > 0)
                {
                    MaterialMap::const_iterator it = matMap.find(face->bzMaterial);
                    if (it != matMap.end())
                        face->bzMaterial = it->second;
                }
                if (modifyColor)
                    face->bzMaterial = getTintedMaterial(tint, face->bzMaterial);
            }
        }
    }
    if (modifyPhysicsDriver)
    {
        if (obstacle->getType() == MeshObstacle::getClassName())
        {
            const MeshObstacle* mesh = (MeshObstacle*) obstacle;
            for (int i = 0; i < mesh->getFaceCount(); i++)
            {
                MeshFace* face = mesh->getFace(i);
                // only modify faces that already have a physics driver
                if (face->phydrv >= 0)
                    face->phydrv = phydrv;
            }
        }
    }
    if (driveThrough)
    {
        obstacle->driveThrough = true;
        if (obstacle->getType() == MeshObstacle::getClassName())
        {
            const MeshObstacle* mesh = (MeshObstacle*) obstacle;
            for (int i = 0; i < mesh->getFaceCount(); i++)
            {
                MeshFace* face = mesh->getFace(i);
                face->driveThrough = true;
            }
        }
    }
    if (shootThrough)
    {
        obstacle->shootThrough = true;
        if (obstacle->getType() == MeshObstacle::getClassName())
        {
            const MeshObstacle* mesh = (MeshObstacle*) obstacle;
            for (int i = 0; i < mesh->getFaceCount(); i++)
            {
                MeshFace* face = mesh->getFace(i);
                face->shootThrough = true;
            }
        }
    }
    if (ricochet)
    {
        obstacle->ricochet = true;
        if (obstacle->getType() == MeshObstacle::getClassName())
        {
            const MeshObstacle* mesh = (MeshObstacle*) obstacle;
            for (int i = 0; i < mesh->getFaceCount(); i++)
            {
                MeshFace* face = mesh->getFace(i);
                face->ricochet = true;
            }
        }
    }

    return;
}


void ObstacleModifier::getMaterialMap(const MaterialSet& matSet,
                                      MaterialMap& materialMap) const
{
    materialMap.clear();

    if (modifyColor || modifyMaterial || (matMap.size() > 0))
    {
        MaterialSet::const_iterator it;
        for (it = matSet.begin(); it != matSet.end(); ++it)
        {
            const BzMaterial* origMat = *it;
            const BzMaterial* convMat = *it;
            if (modifyMaterial)
                convMat = material;
            else if (matMap.size() > 0)
            {
                MaterialMap::const_iterator swap_it = matMap.find(origMat);
                if (swap_it != matMap.end())
                    convMat = swap_it->second;
            }
            if (modifyColor)
                convMat = getTintedMaterial(tint, convMat);
            if (convMat != origMat)
                materialMap[origMat] = convMat;
        }
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
