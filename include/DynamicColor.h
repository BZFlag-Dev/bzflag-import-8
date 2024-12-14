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

#ifndef _DYNAMIC_COLOR_H_
#define _DYNAMIC_COLOR_H_

// 1st
#include "common.h"

/* system interface headers */
#include <string>
#include <vector>
#include <iostream>
#include <glm/vec4.hpp>


typedef struct sequenceList
{
    float period;
    float offset;
    signed char* list;
    unsigned int count;
} sequenceParams;

typedef struct
{
    float period;
    float offset;
    float weight;
} sinusoidParams;

typedef struct
{
    float period;
    float offset;
    float width;
} clampParams;

class DynamicColor
{
public:
    DynamicColor();
    ~DynamicColor();

    enum SequenceState
    {
        colorMin = 0,
        colorMid = 1,
        colorMax = 2
    };

    bool setName(const std::string& name);
    void setLimits(int channel, float min, float max);
    void setSequence(int channel, float period, float offset,
                     std::vector<char>& list);
    void addSinusoid(int channel, const float sinusoid[3]);
    void addClampUp(int channel, const float clampUp[3]);
    void addClampDown(int channel, const float clampDown[3]);

    void finalize();
    void update(double time);

    bool canHaveAlpha() const;
    const glm::vec4 *getColor() const;
    const std::string& getName() const;

    int packSize() const;
    void* pack(void*) const;
    const void* unpack(const void*);

    void print(std::ostream& out, const std::string& indent) const;

private:
    static const float minPeriod;

    std::string name;
    glm::vec4 color;

    typedef struct
    {
        float totalWeight; // tally of sinusoid weights
        sequenceParams sequence;
        std::vector<sinusoidParams> sinusoids;
        std::vector<clampParams> clampUps;
        std::vector<clampParams> clampDowns;
    } ChannelParams;

    glm::vec4 minValue, maxValue;
    ChannelParams channels[4];

    bool possibleAlpha;
};

inline bool DynamicColor::canHaveAlpha() const
{
    return possibleAlpha;
}

inline const glm::vec4 *DynamicColor::getColor() const
{
    return &color;
}


class DynamicColorManager
{
public:
    DynamicColorManager();
    ~DynamicColorManager();
    void update();
    void clear();
    int addColor(DynamicColor* dyncolor);
    int findColor(const std::string& name) const;
    const DynamicColor* getColor(int id) const;

    int packSize() const;
    void* pack(void*) const;
    const void* unpack(const void*);

    void print(std::ostream& out, const std::string& indent) const;

private:
    std::vector<DynamicColor*> colors;
};

extern DynamicColorManager DYNCOLORMGR;


#endif //_DYNAMIC_COLOR_H_

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
