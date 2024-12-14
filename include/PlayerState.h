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

#ifndef BZF_PLAYERSTATE_H
#define BZF_PLAYERSTATE_H

// 1st
#include "common.h"

// System headers
#include <glm/vec3.hpp>

class PlayerState
{
public:
    enum PStatus            // bit masks
    {
        DeadStatus =  0,      // not alive, not paused, etc.
        Alive =       (1 << 0),   // player is alive
        Paused =      (1 << 1),   // player is paused
        Exploding =   (1 << 2),   // currently blowing up
        Teleporting = (1 << 3),   // teleported recently
        FlagActive =  (1 << 4),   // flag special powers active
        CrossingWall =    (1 << 5),   // tank crossing building wall
        Falling =     (1 << 6),   // tank accel'd by gravity
        OnDriver =    (1 << 7),   // tank is on a physics driver
        UserInputs =  (1 << 8),   // user speed and angvel are sent
        JumpJets =    (1 << 9),   // tank has jump jets on
        PlaySound =   (1 << 10)   // play one or more sounds
    };

    enum PStatusSounds
    {
        NoSounds =    0,
        JumpSound =   (1 << 0),
        WingsSound =  (1 << 1),
        BounceSound = (1 << 2)
    };


    PlayerState();
    void*   pack(void*, uint16_t& code);
    const void* unpack(const void*, uint16_t code);

    long    order;      // packet ordering
    short   status;     // see PStatus enum
    glm::vec3 pos;     // position of tank
    glm::vec3 velocity;    // velocity of tank
    float   azimuth;    // orientation of tank
    float   angVel;     // angular velocity of tank
    int     phydrv;     // physics driver

    // the following are to be used only for drawing
    float   userSpeed;  // user's desired angular velocity
    float   userAngVel; // angular velocity of tank
    float   jumpJetsScale;  // angular velocity of tank

    // used to avoid awkward remote bouncy sounds
    uint8_t sounds;     // for playing sounds
};


#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
