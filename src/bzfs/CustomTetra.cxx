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

/* interface header */
#include "CustomTetra.h"

/* common implementation headers */
#include "TetraBuilding.h"
#include "TextureMatrix.h"
#include "ObstacleMgr.h"

/* bzfs implementation headers */
#include "ParseMaterial.h"


CustomTetra::CustomTetra(): vertices(), normals(), texcoords()
{
    vertexCount = 0; // no vertices have yet been defined

    // reset the secondary coordinate states
    for (int i = 0; i < 4; i++)
    {
        useNormals[i] = false;
        useTexcoords[i] = false;
        materials[i].setTexture("mesh");
    }
    return;
}


bool CustomTetra::read(const char *cmd, std::istream& input)
{
    if (vertexCount > 4)
    {
        std::cout << "Extra tetrahedron vertex" << std::endl;
        // keep on chugging
        return true;
    }

    bool materror;
    if (vertexCount == 0)
    {
        // try to parse all 4 materials
        if (parseMaterials(cmd, input, materials, 4, materror))
            return !materror;
    }
    else
    {
        // try to parse the specific vertex's material
        int vc = vertexCount - 1;
        if (vc > 3)
            vc = 3;
        if (parseMaterials(cmd, input, &materials[vc], 1, materror))
            return !materror;
    }

    if (strcasecmp(cmd, "vertex") == 0)
    {
        if (vertexCount >= 4)
        {
            std::cout << "Extra tetrahedron vertex" << std::endl;
            // keep on chugging
        }
        else
        {
            auto &vertex = vertices[vertexCount];
            input >> vertex[0] >> vertex[1] >> vertex[2];
            vertexCount++;
        }
    }
    else if (strcasecmp(cmd, "normals") == 0)
    {
        if (vertexCount < 1)
        {
            std::cout << "Normals defined before any vertex" << std::endl;
            // keep on chugging
        }
        else if (vertexCount > 4)
        {
            std::cout << "Extra tetrahedron normals" << std::endl;
            // keep on chugging
        }
        else
        {
            useNormals[vertexCount - 1] = true;
            for (int v = 0; v < 3; v++)
            {
                float* normal = normals[vertexCount - 1][v];
                input >> normal[0] >> normal[1] >> normal[2];
            }
        }
    }
    else if (strcasecmp(cmd, "texcoords") == 0)
    {
        if (vertexCount < 1)
        {
            std::cout << "Texcoords defined before any vertex" << std::endl;
            // keep on chugging
        }
        else if (vertexCount > 4)
        {
            std::cout << "Extra tetrahedron texcoords" << std::endl;
            // keep on chugging
        }
        else
        {
            useTexcoords[vertexCount - 1] = true;
            for (int v = 0; v < 3; v++)
            {
                float* texcoord = texcoords[vertexCount - 1][v];
                input >> texcoord[0] >> texcoord[1];
            }
        }
    }
    else
        return WorldFileObstacle::read(cmd, input);

    return true;
}


void CustomTetra::writeToGroupDef(GroupDefinition *groupdef)
{
    if (vertexCount < 4)
    {
        std::cout << "Not creating tetrahedron, not enough vertices ("
                  << vertexCount << ")" << std::endl;
        return;
    }

    const BzMaterial* mats[4];
    for (int i = 0; i < 4; i++)
        mats[i] = MATERIALMGR.addMaterial(&materials[i]);
    TetraBuilding* tetra = new TetraBuilding(transform, vertices, normals, texcoords,
            useNormals, useTexcoords,
            mats, driveThrough, shootThrough, ricochet);
    if (tetra->isValid())
        groupdef->addObstacle(tetra);
    else
    {
        std::cout << "Error generating tetra obstacle." << std::endl;
        delete tetra;
    }

    return;
}


// Local variables: ***
// mode: C++ ***
// tab-width: 4***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
