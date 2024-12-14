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
 *
 */

#ifndef BZF_ROBOT_PLAYER_H
#define BZF_ROBOT_PLAYER_H

#include "common.h"

/* system interface headers */
#include <vector>

/* interface header */
#include "LocalPlayer.h"

/* local interface headers */
#include "Region.h"
#include "RegionPriorityQueue.h"
#include "ServerLink.h"


class RobotPlayer : public LocalPlayer
{
public:
    RobotPlayer(const PlayerId&,
                const char* name, ServerLink*,
                const char* _motto);

    float       getTargetPriority(const Player*) const;
    const Player*   getTarget() const;
    void        setTarget(const Player*);
    static void     setObstacleList(std::vector<BzfRegion*>*);

    void        restart(const glm::vec3 &pos, float azimuth);
    void        explodeTank();

private:
    void        doUpdate(float dt);
    void        doUpdateMotion(float dt);
    BzfRegion*      findRegion(const glm::vec2 &p, glm::vec2 &nearest) const;
    float       getRegionExitPoint(
        const glm::vec2 &p1, const glm::vec2 &p2,
        const glm::vec2 &a, const glm::vec2 &targetPoint,
        glm::vec2 &mid, float& priority);
    void       findPath(RegionPriorityQueue& queue,
                        BzfRegion* region, BzfRegion* targetRegion,
                        const glm::vec2 &targetPoint, int mailbox);

    void       projectPosition(const Player *targ, const float t, glm::vec3 &tPos) const;
    void       getProjectedPosition(const Player *targ, glm::vec3 &projpos) const;

private:
    const Player*   target;
    std::vector<RegionPoint>    path;
    int         pathIndex;
    float       timerForShot;
    bool        drivingForward;
    static std::vector<BzfRegion*>* obstacleList;
};

#endif // BZF_ROBOT_PLAYER_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
