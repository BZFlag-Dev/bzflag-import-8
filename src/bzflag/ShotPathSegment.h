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

#ifndef __SHOTPATHSEGMENT_H__
#define __SHOTPATHSEGMENT_H__

/* common interface headers */
#include "TimeKeeper.h"
#include "Ray.h"


class ShotPathSegment
{
public:
    enum Reason     { Initial, Through, Ricochet, Teleport, Boundary };

    ShotPathSegment();
    ShotPathSegment(const TimeKeeper& start,
                    const TimeKeeper& end,
                    const Ray& r,
                    Reason = Initial);
    ShotPathSegment(const ShotPathSegment&);
    ~ShotPathSegment();
    ShotPathSegment&    operator=(const ShotPathSegment&);

public:
    TimeKeeper      start;
    TimeKeeper      end;
    Ray         ray;
    Reason      reason;
    glm::vec3   bbox[2];
};


#endif /* __SHOTPATHSEGMENT_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
