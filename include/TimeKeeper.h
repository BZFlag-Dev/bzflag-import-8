/* bzflag
 * Copyright (c) 1993-2021 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * TimeKeeper:
 *  Standard way to keep track of time in game.
 *
 * Generally, only the difference between TimeKeeper's is useful.
 * operator-() computes the difference in seconds as a float and
 * correctly handles wraparound.
 * operator+=() allows a time in seconds to be added to a TimeKeeper.
 */

#pragma once
#ifndef BZF_TIME_KEEPER_H
#define BZF_TIME_KEEPER_H

#include "common.h"

/* system interface headers */
#include <chrono>
#include <string>


/** TimeKeeper keeps time.  It's useful to determine how much time has
 * elapsed from some other point in time.  Use getCurrent() to return a
 * timekeeper object set to the current time.  You can then use subsequent
 * calls to getCurrent and subtract the second from the first to get an
 * elapsed float time value.
 */
class TimeKeeper
{
public:
    TimeKeeper() = default;
    using Seconds_t = std::chrono::duration<double>;

    explicit TimeKeeper(Seconds_t secs);

    operator bool() const;

    double      operator-(const TimeKeeper&) const;
    bool            operator<=(const TimeKeeper&) const;
    TimeKeeper&     operator+=(double);
    TimeKeeper&     operator+=(const TimeKeeper&);

    /** returns how many seconds have elapsed since epoch, Jan 1, 1970 */
    double       getSeconds() const;

    /** returns a timekeeper representing the current time */
    static const TimeKeeper&    getCurrent();

    /** returns a timekeeper representing the time of program execution */
    static const TimeKeeper&    getStartTime();

    /** sets the time to the current time (recalculates) */
    static void         setTick();
    /** returns a timekeeper that is updated periodically via setTick */
    static const TimeKeeper&    getTick();

    /** returns a timekeeper representing +Inf */
    static const TimeKeeper&    getSunExplodeTime();
    /** returns a timekeeper representing -Inf */
    static const TimeKeeper&    getSunGenesisTime();
    /** returns a timekeeper representing an unset timekeeper */
    static const TimeKeeper&    getNullTime();

    /** returns the local time */
    static void localTime(int *year = NULL, int *month = NULL, int* day = NULL, int* hour = NULL, int* min = NULL,
                          int* sec = NULL, bool* dst = NULL, long *tv_usec = nullptr);

    /** returns a string of the local time */
    static const char       *timestamp();

    static void localTime( int &day);

    /** returns the UTC time */
    static void UTCTime(int *year = NULL, int *month = NULL, int* day = NULL, int* dayOfWeek = NULL, int* hour = NULL,
                        int* min = NULL, int* sec = NULL, bool* dst = NULL, long *tv_usec = nullptr);


    /** converts a time difference into an array of integers
        representing days, hours, minutes, seconds */
    static void         convertTime(Seconds_t raw, long int convertedTimes[]);
    /** prints an integer-array time difference in human-readable form */
    static const std::string    printTime(long int timeValue[]);
    /** prints an float time difference in human-readable form */
    static const std::string    printTime(double diff);

    /** sleep for a given number of floating point seconds */
    static void           sleep(double secs); //const

protected:
    // not that we expect to subclass, but prefer separating methods from members
    void now();

private:
    std::chrono::time_point<std::chrono::steady_clock, Seconds_t> lastTime; // floating seconds
};

//
// TimeKeeper
//

inline double       TimeKeeper::operator-(const TimeKeeper& t) const
{
    return (lastTime - t.lastTime).count();
}

inline TimeKeeper&  TimeKeeper::operator+=(double dt)
{
    lastTime += Seconds_t(dt);
    return *this;
}
inline TimeKeeper&  TimeKeeper::operator+=(const TimeKeeper& t)
{
    lastTime += t.lastTime.time_since_epoch();
    return *this;
}

inline bool     TimeKeeper::operator<=(const TimeKeeper& t) const
{
    return lastTime <= t.lastTime;
}

inline double       TimeKeeper::getSeconds() const
{
    return lastTime.time_since_epoch().count();
}


#endif // BZF_TIME_KEEPER_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
