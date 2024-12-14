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
#include "Pack.h"
#include <math.h>
#include <string.h>
#include <sstream>

#include "WorldFileObject.h"
#include "WorldFileLocation.h"
#include "MeshTransform.h"

WorldFileLocation::WorldFileLocation()
{
    pos[0] = pos[1] = pos[2] = 0.0f;
    rotation = 0.0f;
    size = glm::vec3(1.0f);
}


bool WorldFileLocation::read(const char *cmd, std::istream& input)
{
    //
    // Position, Size, and Rotation
    //
    if ((strcasecmp(cmd, "pos") == 0) ||
            (strcasecmp(cmd, "position") == 0))
    {
        if (!(input >> pos[0] >> pos[1] >> pos[2]))
            return false;
    }
    else if (strcasecmp(cmd, "size") == 0)
    {
        if (!(input >> size[0] >> size[1] >> size[2]))
            return false;
    }
    else if ((strcasecmp(cmd, "rot") == 0) ||
             (strcasecmp(cmd, "rotation") == 0))
    {
        if (!(input >> rotation))
            return false;
        // convert to radians
        rotation = (float)(rotation * (M_PI / 180.0));
    }

    //
    // Shift, Scale, Shear, Spin, and MatrixMult
    //
    else if (strcasecmp ("shift", cmd) == 0)
    {
        glm::vec3 data;
        if (!(input >> data[0] >> data[1] >> data[2]))
        {
            std::cout << "parameters errors " << std::endl;
            return false;
        }
        transform.addShift(data);
    }
    else if (strcasecmp ("scale", cmd) == 0)
    {
        glm::vec3 data;
        if (!(input >> data[0] >> data[1] >> data[2]))
        {
            std::cout << "parameters errors " << std::endl;
            return false;
        }
        transform.addScale(data);
    }
    else if (strcasecmp ("shear", cmd) == 0)
    {
        float data[3];
        if (!(input >> data[0] >> data[1] >> data[2]))
        {
            std::cout << "parameters errors " << std::endl;
            return false;
        }
        transform.addShear(data);
    }
    else if (strcasecmp ("spin", cmd) == 0)
    {
        float     angle;
        glm::vec3 normal;
        if (!(input >> angle >> normal[0] >> normal[1] >> normal[2]))
        {
            std::cout << "parameters errors " << std::endl;
            return false;
        }
        transform.addSpin(angle, normal);
    }
    else if (strcasecmp ("xform", cmd) == 0)
    {
        std::string _name;
        if (!(input >> _name))
        {
            std::cout << "parameters errors " << std::endl;
            return false;
        }
        int xform = TRANSFORMMGR.findTransform(_name);
        if (xform == -1)
            std::cout << "couldn't find Transform: " << _name << std::endl;
        else
            transform.addReference(xform);
    }
    else
        return WorldFileObject::read(cmd, input);

    return true;
}

void * WorldFileLocation::pack (void *buf) const
{
    buf = nboPackVector (buf, pos);
    buf = nboPackVector (buf, size);
    buf = nboPackFloat (buf, rotation);
    return buf;
}

// Local variables: ***
// mode: C++ ***
// tab-width: 4***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
