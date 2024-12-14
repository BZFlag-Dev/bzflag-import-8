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

/* interface header */
#include "Roaming.h"

/* common headers */
#include "BZDBCache.h"
#include "AnsiCodes.h"
#include "Team.h"

/* local headers */
#include "ScoreboardRenderer.h"

Roaming::Roaming() : view(roamViewDisabled),
    targetManual(-1),
    targetWinner(-1),
    targetFlag(-1)
{
    resetCamera();
}


Player* getRoamTargetTank()
{
    return ROAM.getTargetTank();
}


void Roaming::resetCamera(void)
{
    camera.pos[0] = 0.0f;
    camera.pos[1] = 0.0f;
    camera.pos[2] = BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
    camera.theta = 0.0f;
    camera.zoom = 60.0f;
    camera.phi = -0.0f;
}

void Roaming::setCamera(RoamingCamera* newCam)
{
    memcpy(&camera, newCam, sizeof(RoamingCamera));
}

void Roaming::setMode(RoamingView newView)
{
    if (!(LocalPlayer::getMyTank() || devDriving))
        view = newView;
    else if (LocalPlayer::getMyTank()->getTeam() == ObserverTeam)
    {
        // disallow disabling roaming in observer mode
        if ((newView == roamViewDisabled) && !devDriving)
            view = (RoamingView)(roamViewDisabled + 1);
        else
            view = newView;
    }
    else
    {
        // don't allow roaming for non-observers
        if (newView != roamViewDisabled)
            view = roamViewDisabled;
        else
            view = newView;
    }
    // make sure we have a valid target
    changeTarget(next);
    changeTarget(previous);
}


static int findPlayerInVector(const std::vector<Player*>& vec, Player* p)
{
    for (size_t i = 0; i < vec.size(); i++)
    {
        if (p == vec[i])
            return int(i);
    }
    return -1;
}


static int findPlayerIndex(PlayerId pid)
{
    World* world = World::getWorld();
    for (int i = 0; i < world->getCurMaxPlayers(); i++)
    {
        Player* p = world->getPlayer(i);
        if (p && (p->getId() == pid))
            return i;
    }
    return -1;
}



bool Roaming::changePlayer(RoamingTarget target)
{
    World* world = World::getWorld();

    std::vector<Player*> players;
    ScoreboardRenderer::getPlayerList(players);

    const int pCount = int(players.size());
    if (!world || (pCount <= 0))
    {
        targetManual = targetWinner = -1;
        return false;
    }

    Player* current = NULL;
    if ((targetManual >= 0) && (targetManual < world->getCurMaxPlayers()))
        current = getPlayerByIndex(targetManual);

    const bool nextTarget = (target == next);

    if (!current)
    {
        Player* p = nextTarget ? players.back() : players.front();
        targetManual = targetWinner = findPlayerIndex(p->getId());
        return (targetManual >= 0);
    }

    int pIndex = findPlayerInVector(players, current);
    if (pIndex < 0)
        targetManual = targetWinner = -1;
    else
    {
        pIndex += (nextTarget ? -1 : +1);
        if (pIndex >= pCount)
            pIndex = -1;
        if ((pIndex >= 0) && (pIndex < pCount))
        {
            Player* p = players[pIndex];
            targetManual = targetWinner = findPlayerIndex(p->getId());
        }
        else
            targetManual = targetWinner = -1;
    }

    return true;
}


void Roaming::changeTarget(Roaming::RoamingTarget target, int explicitIndex)
{
    bool found = false;

    World* world = World::getWorld();
    if (!world)
    {
        logDebugMessage(4,"Roaming::changeTarget() no world, switching to free roaming\n");
        view = roamViewFree;
        buildRoamingLabel();
        return;
    }

    if (view == roamViewFree || view == roamViewDisabled)
    {
        // do nothing
        found = true;
    }
    else if (view == roamViewFlag)
    {
        if (target == explicitSet)
        {
            targetFlag = explicitIndex;
            found = true;
        }
        else
        {
            int i, j;
            const int maxFlags = world->getMaxFlags();
            for (i = 1; i <= maxFlags; ++i)
            {
                if (target == next)
                    j = (targetFlag + i) % maxFlags;
                else
                    j = (targetFlag - i + maxFlags) % maxFlags;
                const Flag& flag = world->getFlag(j);
                if (flag.type->flagTeam != NoTeam)
                {
                    targetFlag = j;
                    found = true;
                    break;
                }
            }
        }
    }
    else
    {
        if (target == explicitSet)
        {
            targetManual = targetWinner = explicitIndex;
            found = true;
        }
        else
            found = changePlayer(target);
    }

    if (!found)
        view = roamViewFree;

    buildRoamingLabel();

    Player* tracked = NULL;
    if (!devDriving)
    {
        if (world)
            tracked = world->getPlayer(targetWinner);
    }
    else
        tracked = LocalPlayer::getMyTank();

    if (tracked)
        tracked->reportedHits = tracked->computedHits = 0;
}

void Roaming::buildRoamingLabel(void)
{
    std::string playerString = "";

    World* world = World::getWorld();

    // follow the important tank
    if (targetManual == -1)
    {
        Player* top = NULL;
        if (world && world->allowRabbit())
        {
            // follow the white rabbit
            top = world->getCurrentRabbit();
            if (top != NULL)
            {
                playerString = "Rabbit ";
                targetWinner = top->getId();
            }
        }
        if (top == NULL)
        {
            // find the leader
            top = ScoreboardRenderer::getLeader(&playerString);
            if (top == NULL)
                targetWinner = 0;
            else
                targetWinner = top->getId();
        }
    }

    Player* tracked = NULL;
    if (!devDriving)
    {
        if (world)
            tracked = world->getPlayer(targetWinner);
    }
    else
        tracked = LocalPlayer::getMyTank();

    if (world && tracked)
    {
        if (BZDBCache::colorful)
        {
            TeamColor color = tracked->getTeam();
            playerString += Team::getAnsiCode(color);
        }
        playerString += tracked->getCallSign();

        const FlagType* flag = tracked->getFlag();
        if (flag != Flags::Null)
        {
            if (BZDBCache::colorful)
            {
                playerString += ColorStrings[CyanColor] + " / ";
                if (flag->flagTeam != NoTeam)
                    playerString += Team::getAnsiCode(flag->flagTeam);
                else
                    playerString += ColorStrings[WhiteColor];
            }
            else
                playerString += " / ";
            if (flag->endurance == FlagNormal)
                playerString += flag->flagName;
            else
                playerString += flag->flagAbbv;
        }

        switch (view)
        {
        case roamViewTrack:
        {
            roamingLabel = "Tracking " + playerString;
            break;
        }
        case roamViewFollow:
        {
            roamingLabel = "Following " + playerString;
            break;
        }
        case roamViewFP:
        {
            roamingLabel = "Driving with " + playerString;
            break;
        }
        case roamViewFlag:
        {
            roamingLabel = std::string("Tracking ")
                           + world->getFlag(targetFlag).type->flagName + " Flag";
            break;
        }
        default:
        {
            roamingLabel = "Roaming";
            break;
        }
        }
    }
    else
        roamingLabel = "Roaming";
}

void Roaming::updatePosition(RoamingCamera* dc, float dt)
{
    World* world = World::getWorld();

    // are we tracking?
    bool tracking = false;
    glm::vec3 trackPos;
    if (view == roamViewTrack)
    {
        Player *target;
        if (!devDriving)
        {
            if (world && (targetWinner < world->getCurMaxPlayers()))
                target = world->getPlayer(targetWinner);
            else
                target = NULL;
        }
        else
            target = LocalPlayer::getMyTank();
        if (target != NULL)
        {
            trackPos = target->getPosition();
            tracking = true;
        }
    }
    else if (view == roamViewFlag)
    {
        if ((world != NULL) && (targetFlag < world->getMaxFlags()))
        {
            Flag &flag = world->getFlag(targetFlag);
            trackPos = flag.position;
            tracking = true;
        }
    }

    // modify Z coordinate
    camera.pos[2] += dt * dc->pos[2];
    float muzzleHeight = BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
    if (camera.pos[2] < muzzleHeight)
    {
        camera.pos[2] = muzzleHeight;
        dc->pos[2] = 0.0f;
    }

    // modify X and Y coords
    if (!tracking)
    {
        const float c = cosf(camera.theta * (float)(M_PI / 180.0f));
        const float s = sinf(camera.theta * (float)(M_PI / 180.0f));
        camera.pos[0] += dt * (c * dc->pos[0] - s * dc->pos[1]);
        camera.pos[1] += dt * (c * dc->pos[1] + s * dc->pos[0]);
        camera.theta  += dt * dc->theta;
        camera.phi    += dt * dc->phi;
    }
    else
    {
        float dx = camera.pos[0] - trackPos[0];
        float dy = camera.pos[1] - trackPos[1];
        float dist = sqrtf((dx * dx) + (dy * dy));

        float nomDist = 4.0f * BZDBCache::tankSpeed;
        if (nomDist == 0.0f)
            nomDist = 100.0f;
        float distFactor =  (dist / nomDist);
        if (distFactor < 0.25f)
            distFactor = 0.25f;
        float newDist = dist - (dt * distFactor * dc->pos[0]);

        const float minDist = BZDBCache::tankLength * 0.5f;
        if (newDist < minDist)
        {
            if (dist >= minDist)
                newDist = minDist;
            else
                newDist = dist;
        }
        float scale = 0.0f;
        if (dist > 0.0f)
            scale = newDist / dist;
        dx = dx * scale;
        dy = dy * scale;
        if (fabsf(dx) < 0.001f) dx = 0.001f;
        if (fabsf(dy) < 0.001f) dy = 0.001f;
        const float dtheta = -(dt * dc->theta * (float)(M_PI / 180.0f));
        const float c = cosf(dtheta);
        const float s = sinf(dtheta);
        camera.pos[0] = trackPos[0] + ((c * dx) - (s * dy));
        camera.pos[1] = trackPos[1] + ((c * dy) + (s * dx));
        // setup so that free roam stays in the last state
        camera.theta = atan2f(trackPos[1] - camera.pos[1], trackPos[0] - camera.pos[0]);
        camera.theta *= (float)(180.0f / M_PI);
        camera.phi = atan2f(trackPos[2] - camera.pos[2], newDist);
        camera.phi *= (float)(180.0f / M_PI);
    }

    // clamp phi
    const float phiLimit = 90.0f - 1.0e-3f;
    if (camera.phi > phiLimit)
        camera.phi = phiLimit;
    else if (camera.phi < -phiLimit)
        camera.phi = -phiLimit;

    // adjust zoom
    camera.zoom += dt * dc->zoom;
    if (camera.zoom < BZDB.eval("roamZoomMin"))
        camera.zoom = BZDB.eval("roamZoomMin");
    else if (camera.zoom > BZDB.eval("roamZoomMax"))
        camera.zoom = BZDB.eval("roamZoomMax");
}


Roaming::RoamingView Roaming::parseView(const std::string& str) const
{
    const char* s = str.c_str();
    if (strcasecmp(s, "disabled") == 0)
        return roamViewDisabled;
    else if (strcasecmp(s, "free")     == 0)
        return roamViewFree;
    else if (strcasecmp(s, "track")    == 0)
        return roamViewTrack;
    else if (strcasecmp(s, "follow")   == 0)
        return roamViewFollow;
    else if (strcasecmp(s, "fps")      == 0)
        return roamViewFP;
    else if (strcasecmp(s, "flag")     == 0)
        return roamViewFlag;
    else
        return roamViewDisabled;
}


const char* Roaming::getViewName(RoamingView roamView) const
{
    switch (roamView)
    {
    case roamViewDisabled:
    {
        return "disabled";
    }
    case roamViewFree:
    {
        return "free";
    }
    case roamViewTrack:
    {
        return "track";
    }
    case roamViewFollow:
    {
        return "follow";
    }
    case roamViewFP:
    {
        return "fps";
    }
    case roamViewFlag:
    {
        return "flag";
    }
    default:
    {
        return "unknown";
    }
    }
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
