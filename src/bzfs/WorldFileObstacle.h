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
#ifndef __WORLDFILEOBSTACLE_H__
#define __WORLDFILEOBSTACLE_H__

// 1st
#include "common.h"

// system headers
#include <iostream>

// bzfs-specific headers
#include "WorldFileLocation.h"


class WorldFileObstacle : public WorldFileLocation
{
public:
    WorldFileObstacle();
    virtual bool read(const char *cmd, std::istream&);

protected:
    bool driveThrough;
    bool shootThrough;
    bool ricochet;
};

#endif /* __WORLDFILEOBSTACLE_H__ */

// Local variables: ***
// mode: C++ ***
// tab-width: 4***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
