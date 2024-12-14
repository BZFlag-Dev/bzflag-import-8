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

#ifndef __WORLDBUILDER_H__
#define __WORLDBUILDER_H__

#include "common.h"

/* local interface headers */
#include "World.h"


/** builds a bzlfag world
 */
class WorldBuilder
{
public:
    WorldBuilder();
    ~WorldBuilder();

    const void*     unpack(const void*);
    const void*     unpackGameSettings(const void*);

    World*      getWorld();
    World*      peekWorld();    // doesn't give up ownership

    void        setGameType(short gameType);
    void        setGameOptions(short gameOptions);
    void        setMaxPlayers(int maxPlayers);
    void        setMaxShots(int maxSimultaneousShots);
    void        setMaxFlags(int maxFlags);
    void        setShakeTimeout(float timeout) const;
    void        setShakeWins(int wins) const;
    void        setBase(TeamColor team,
                        const glm::vec3 &pos, float rotation,
                        float w, float b, float h);

private:
    void        preGetWorld();

private:
    bool        owned;
    World*      world;
};


#endif /* __WORLDBUILDER_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
