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

/*
 * Sound effect stuff
 */

#ifndef BZF_SOUND_H
#define BZF_SOUND_H

// 1st
#include "common.h"

// System headers
#include <string>
#include <glm/fwd.hpp>

#define SFX_FIRE    0       /* shell fired */
#define SFX_EXPLOSION   1       /* something other than me blew up */
#define SFX_RICOCHET    2       /* shot bounced off building */
#define SFX_GRAB_FLAG   3       /* grabbed a good flag */
#define SFX_DROP_FLAG   4       /* dropped a flag */
#define SFX_CAPTURE 5       /* my team captured enemy flag */
#define SFX_LOSE    6       /* my flag captured */
#define SFX_ALERT   7       /* my team flag grabbed by enemy */
#define SFX_JUMP    8       /* jumping sound */
#define SFX_LAND    9       /* landing sound */
#define SFX_TELEPORT    10      /* teleporting sound */
#define SFX_LASER   11      /* laser fired sound */
#define SFX_SHOCK   12      /* shockwave fired sound */
#define SFX_POP     13      /* tank appeared sound */
#define SFX_DIE     14      /* my tank exploded */
#define SFX_GRAB_BAD    15      /* grabbed a bad flag */
#define SFX_SHOT_BOOM   16      /* shot exploded */
#define SFX_KILL_TEAM   17      /* shot a teammate */
#define SFX_PHANTOM 18      /* Went into Phantom zone */
#define SFX_MISSILE 19      /* guided missile fired */
#define SFX_LOCK    20      /* missile locked on me */
#define SFX_TEAMGRAB    21      /* grabbed an opponents team flag */
#define SFX_HUNT    22      /* hunting sound */
#define SFX_HUNT_SELECT 23      /* hunt target selected */
#define SFX_RUNOVER     24        /* steamroller sound */
#define SFX_THIEF       25      /* thief sound */
#define SFX_BURROW  26      /* burrow sound */
#define SFX_MESSAGE_PRIVATE 27  /* private message received */
#define SFX_MESSAGE_TEAM    28  /* team message received */
#define SFX_MESSAGE_ADMIN   29  /* admin message received */
#define SFX_FLAP    30      /* wings flapping sound  */
#define SFX_BOUNCE  31      /* bouncing sound */

/* prepare sound effects generator and shut it down */
void            openSound(const char* pname);
void            closeSound(void);
bool            isSoundOpen();

/* reposition sound receiver (no Doppler) or move it (w/Doppler effect) */
void            moveSoundReceiver(float x, float y, float z, float t,
                                  int discontinuity);
void            speedSoundReceiver(const glm::vec3 &v);

/* sound effect event at given position in world, or possible locally */
void            playSound(int soundCode, const glm::vec3 &pos,
                          bool important, bool localSound);

/* sound effect event at given position in world */
void            playWorldSound(int soundCode, const glm::vec3 &pos,
                               bool important = false);

/* sound effect positioned at receiver */
void            playLocalSound(int soundCode);
void            playLocalSound(std::string sound);

/* start playing a sound effect repeatedly at world position */
void            playFixedSound(int soundCode,
                               float x, float y, float z);

/* change volume;  0 is mute, else 1-10 (min-max) */
void            setSoundVolume(int newLevel);

/* get current volume */
int         getSoundVolume();

/* update sound stuff (only does something when running sound in same process */
void            updateSound();

#endif // BZF_SOUND_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
