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
#include "BzMaterial.h"
#include "ParseMaterial.h"

/* system headers */
#include <sstream>
#include <string.h>

/* common implementation headers */
#include "ParseColor.h"
#include "TextureMatrix.h"
#include "DynamicColor.h"


bool parseMaterials(const char* cmd, std::istream& input,
                    BzMaterial* materials, int materialCount, bool& error)
{
    int i;
    error = false;

    if (strcasecmp(cmd, "matref") == 0)
    {
        std::string name;
        if (!(input >> name))
        {
            std::cout << "missing " << cmd << " parameters" << std::endl;
            error = true;
        }
        const BzMaterial* matref = MATERIALMGR.findMaterial(name);
        if (matref == NULL)
        {
            std::cout << "couldn't find reference material: " << name << std::endl;
            error = true;
        }
        else
        {
            for (i = 0; i < materialCount; i++)
                materials[i] = *matref;
        }
    }
    else if (strcasecmp(cmd, "resetmat") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].reset();
    }
    else if (strcasecmp(cmd, "dyncol") == 0)
    {
        std::string dyncol;
        if (!(input >> dyncol))
        {
            std::cout << "missing " << cmd << " parameters" << std::endl;
            error = true;
        }
        int dynamicColor = DYNCOLORMGR.findColor(dyncol);
        if ((dynamicColor == -1) && (dyncol != "-1"))
        {
            std::cout << "couldn't find dynamicColor: " << dyncol << std::endl;
            error = true;
        }
        else
        {
            for (i = 0; i < materialCount; i++)
                materials[i].setDynamicColor(dynamicColor);
        }
    }
    else if (strcasecmp(cmd, "ambient") == 0)
    {
        float ambient[4];
        error = !parseColorStream(input, ambient);
        if (!error)
        {
            for (i = 0; i < materialCount; i++)
                materials[i].setAmbient(ambient);
        }
        else
            std::cout << "bad " << cmd << " specification" << std::endl;
    }
    else if ((strcasecmp(cmd, "diffuse") == 0) || // currently used by bzflag
             (strcasecmp(cmd, "color") == 0))
    {
        glm::vec4 diffuse;
        error = !parseColorStream(input, diffuse);
        if (!error)
        {
            for (i = 0; i < materialCount; i++)
                materials[i].setDiffuse(diffuse);
        }
        else
            std::cout << "bad " << cmd << " specification" << std::endl;
    }
    else if (strcasecmp(cmd, "specular") == 0)
    {
        glm::vec4 specular;
        error = !parseColorStream(input, specular);
        if (!error)
        {
            for (i = 0; i < materialCount; i++)
                materials[i].setSpecular(glm::vec3(specular));
        }
        else
            std::cout << "bad " << cmd << " specification" << std::endl;
    }
    else if (strcasecmp(cmd, "emission") == 0)
    {
        glm::vec4 emission;
        error = !parseColorStream(input, emission);
        if (!error)
        {
            for (i = 0; i < materialCount; i++)
                materials[i].setEmission(glm::vec3(emission));
        }
        else
            std::cout << "bad " << cmd << " specification" << std::endl;
    }
    else if (strcasecmp(cmd, "shininess") == 0)
    {
        float shininess;
        if (!(input >> shininess))
        {
            std::cout << "missing " << cmd << " parameters" << std::endl;
            error = true;
        }
        for (i = 0; i < materialCount; i++)
            materials[i].setShininess(shininess);
    }
    else if (strcasecmp(cmd, "alphathresh") == 0)
    {
        float alphaThresh;
        if (!(input >> alphaThresh))
        {
            std::cout << "missing " << cmd << " parameters" << std::endl;
            error = true;
        }
        for (i = 0; i < materialCount; i++)
            materials[i].setAlphaThreshold(alphaThresh);
    }
    else if (strcasecmp(cmd, "noculling") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].setNoCulling(true);
    }
    else if (strcasecmp(cmd, "nosorting") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].setNoSorting(true);
    }
    else if (strcasecmp(cmd, "noradar") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].setNoRadar(true);
    }
    else if (strcasecmp(cmd, "noshadow") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].setNoShadow(true);
    }
    else if (strcasecmp(cmd, "nolighting") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].setNoLighting(true);
    }
    else if (strcasecmp(cmd, "occluder") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].setOccluder(true);
    }
    else if (strcasecmp(cmd, "groupAlpha") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].setGroupAlpha(true);
    }
    else if (strcasecmp(cmd, "texture") == 0)
    {
        std::string name;
        if (!(input >> name))
        {
            std::cout << "missing " << cmd << " parameters" << std::endl;
            error = true;
        }
        else
        {
            for (i = 0; i < materialCount; i++)
                materials[i].setTexture(name);
        }
    }
    else if (strcasecmp(cmd, "notextures") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].clearTextures();
    }
    else if (strcasecmp(cmd, "addtexture") == 0)
    {
        std::string name;
        if (!(input >> name))
        {
            std::cout << "missing " << cmd << " parameters" << std::endl;
            error = true;
        }
        for (i = 0; i < materialCount; i++)
            materials[i].addTexture(name);
    }
    else if (strcasecmp(cmd, "texmat") == 0)
    {
        std::string texmat;
        if (!(input >> texmat))
        {
            std::cout << "missing " << cmd << " parameters" << std::endl;
            error = true;
        }
        int textureMatrix = TEXMATRIXMGR.findMatrix(texmat);
        if ((textureMatrix == -1) && (texmat != "-1"))
            std::cout << "couldn't find textureMatrix: " << texmat << std::endl;
        for (i = 0; i < materialCount; i++)
            materials[i].setTextureMatrix(textureMatrix);
    }
    else if (strcasecmp(cmd, "notexalpha") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].setUseTextureAlpha(false);
    }
    else if (strcasecmp(cmd, "notexcolor") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].setUseColorOnTexture(false);
    }
    else if (strcasecmp(cmd, "spheremap") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].setUseSphereMap(true);
    }
    else if (strcasecmp(cmd, "shader") == 0)
    {
        std::string name;
        if (!(input >> name))
        {
            std::cout << "missing " << cmd << " parameters" << std::endl;
            error = true;
        }
        for (i = 0; i < materialCount; i++)
            materials[i].setShader(name);
    }
    else if (strcasecmp(cmd, "addshader") == 0)
    {
        std::string name;
        if (!(input >> name))
        {
            std::cout << "missing " << cmd << " parameters" << std::endl;
            error = true;
        }
        for (i = 0; i < materialCount; i++)
            materials[i].addShader(name);
    }
    else if (strcasecmp(cmd, "noshaders") == 0)
    {
        for (i = 0; i < materialCount; i++)
            materials[i].clearShaders();
    }
    else
        return false;

    return true;
}


bool parseMaterialsByName(const char* cmd, std::istream& input,
                          BzMaterial* materials, const char** names,
                          int materialCount, bool& error)
{
    error = false;

    for (int n = 0; n < materialCount; n++)
    {
        if (strcasecmp (cmd, names[n]) == 0)
        {
            std::string line, matcmd;
            std::getline(input, line);
            std::istringstream parms(line);
            input.putback('\n');
            if (!(parms >> matcmd))
                error = true;
            else
            {
                if (!parseMaterials(matcmd.c_str(), parms, &materials[n], 1, error))
                    error = true;
            }
            return true;
        }
    }

    return false;
}


// Local variables: ***
// mode: C++ ***
// tab-width: 4***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
