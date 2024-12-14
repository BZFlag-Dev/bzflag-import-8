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

#ifndef BZF_TRACKS_H
#define BZF_TRACKS_H

// BZFlag common header
#include "common.h"

// System headers
#include <glm/fwd.hpp>

class SceneDatabase;

namespace TrackMarks
{

void init();
void kill();
void clear();
void update(float dt);
void addSceneNodes(SceneDatabase* scene);
void notifyStyleChange();
void renderGroundTracks();   // zbuffer is not used
void renderObstacleTracks(); // zbuffer is used

bool addMark(const glm::vec3 &pos, float scale, float angle, int phydrv);

void setUserFade(float);
float getUserFade();

enum AirCullStyle
{
    NoAirCull     = 0,
    InitAirCull   = (1 << 0), // cull for initial air mark conditions
    PhyDrvAirCull = (1 << 1), // cull for physics driver effects
    FullAirCull   = (InitAirCull | PhyDrvAirCull)
};

void setAirCulling(AirCullStyle style);
AirCullStyle getAirCulling();

const float updateTime = (1.0f / 20.0f);
}


#endif // BZF_TRACKS_H


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
