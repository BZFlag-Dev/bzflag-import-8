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
#include "Flag.h"

/* system implementation headers */
#include <math.h>
#include <string>
#include <assert.h>
#include <string.h>
#include <glm/vec3.hpp>

/* common implementation headers */
#include "Team.h"
#include "Pack.h"
#include "TextUtils.h"


FlagSet *FlagType::flagSets = NULL;
const int FlagType::packSize = FlagPackSize;
FlagSet FlagType::customFlags;

// Initialize flag description singletons in our Flags namespace
namespace Flags
{
FlagType *Null;
FlagType *RedTeam;
FlagType *GreenTeam;
FlagType *BlueTeam;
FlagType *PurpleTeam;
FlagType *Velocity;
FlagType *QuickTurn;
FlagType *OscillationOverthruster;
FlagType *RapidFire;
FlagType *MachineGun;
FlagType *GuidedMissile;
FlagType *Laser;
FlagType *Ricochet;
FlagType *SuperBullet;
FlagType *InvisibleBullet;
FlagType *Stealth;
FlagType *Tiny;
FlagType *Narrow;
FlagType *Shield;
FlagType *Steamroller;
FlagType *ShockWave;
FlagType *PhantomZone;
FlagType *Genocide;
FlagType *Jumping;
FlagType *Identify;
FlagType *Cloaking;
FlagType *Useless;
FlagType *Masquerade;
FlagType *Seer;
FlagType *Thief;
FlagType *Burrow;
FlagType *Wings;
FlagType *Agility;
FlagType *Colorblindness;
FlagType *Obesity;
FlagType *LeftTurnOnly;
FlagType *RightTurnOnly;
FlagType *ForwardOnly;
FlagType *ReverseOnly;
FlagType *Momentum;
FlagType *Blindness;
FlagType *Jamming;
FlagType *WideAngle;
FlagType *NoJumping;
FlagType *TriggerHappy;
FlagType *ReverseControls;
FlagType *Bouncy;

void init()
{
    Null    = new FlagType( "", "", FlagNormal, NormalShot, FlagGood, NoTeam, "" );

    RedTeam = new FlagType( "Red Team", "R*", FlagNormal, NormalShot, FlagGood, ::RedTeam,
                            "If it's yours, prevent other teams from taking it.  If it's not take it to your base to capture it!" );
    GreenTeam   = new FlagType( "Green Team", "G*", FlagNormal, NormalShot, FlagGood, ::GreenTeam,
                                "If it's yours, prevent other teams from taking it.  If it's not take it to your base to capture it!" );
    BlueTeam    = new FlagType( "Blue Team", "B*", FlagNormal, NormalShot, FlagGood, ::BlueTeam,
                                "If it's yours, prevent other teams from taking it.  If it's not take it to your base to capture it!" );
    PurpleTeam  = new FlagType( "Purple Team", "P*", FlagNormal, NormalShot, FlagGood, ::PurpleTeam,
                                "If it's yours, prevent other teams from taking it.  If it's not take it to your base to capture it!" );
    Velocity    = new FlagType( "High Speed", "V", FlagUnstable, NormalShot, FlagGood, NoTeam,
                                "Tank moves faster.  Outrun bad guys." );
    QuickTurn   = new FlagType( "Quick Turn", "QT", FlagUnstable, NormalShot, FlagGood, NoTeam,
                                "Tank turns faster.  Good for dodging." );
    OscillationOverthruster = new FlagType( "Oscillation Overthruster", "OO", FlagUnstable, NormalShot, FlagGood, NoTeam,
                                            "Can drive through buildings.  Can't back up or shoot while inside." );
    RapidFire   = new FlagType( "Rapid Fire", "F", FlagUnstable, SpecialShot, FlagGood, NoTeam,
                                "Shoots more often.  Shells go faster but not as far." );
    MachineGun  = new FlagType( "Machine Gun", "MG", FlagUnstable, SpecialShot, FlagGood, NoTeam,
                                "Very fast reload and very short range." );
    GuidedMissile   = new FlagType( "Guided Missile", "GM", FlagUnstable, SpecialShot, FlagGood, NoTeam,
                                    "Shots track a target.  Lock on with right button.  Can lock on or retarget after firing." );
    Laser   = new FlagType( "Laser", "L", FlagUnstable, SpecialShot, FlagGood, NoTeam,
                            "Shoots a laser.  Infinite speed and range but long reload time.");
    Ricochet    = new FlagType( "Ricochet", "R", FlagUnstable, SpecialShot, FlagGood, NoTeam,
                                "Shots bounce off walls.  Don't shoot yourself!" );
    SuperBullet = new FlagType( "Super Bullet", "SB", FlagUnstable, SpecialShot, FlagGood, NoTeam,
                                "Shoots through buildings.  Can kill Phantom Zone." );
    InvisibleBullet = new FlagType( "Invisible Bullet", "IB", FlagUnstable, NormalShot, FlagGood, NoTeam,
                                    "Your shots don't appear on other radars.  Can still see them out window.");
    Stealth = new FlagType( "Stealth", "ST", FlagUnstable, NormalShot, FlagGood, NoTeam,
                            "Tank is invisible on radar.  Shots are still visible.  Sneak up behind enemies!");
    Tiny    = new FlagType( "Tiny", "T", FlagUnstable, NormalShot, FlagGood, NoTeam,
                            "Tank is small and can get through small openings.  Very hard to hit." );
    Narrow  = new FlagType( "Narrow", "N", FlagUnstable, NormalShot, FlagGood, NoTeam,
                            "Tank is super thin.  Very hard to hit from front but is normal size from side.  Can get through small openings.");
    Shield  = new FlagType( "Shield", "SH", FlagUnstable, NormalShot, FlagGood, NoTeam,
                            "Getting hit only drops flag.  Flag flies an extra-long time.");
    Steamroller = new FlagType( "Steamroller", "SR", FlagUnstable, NormalShot, FlagGood, NoTeam,
                                "Destroys tanks you touch but you have to get really close.");
    ShockWave   = new FlagType( "Shock Wave", "SW", FlagUnstable, SpecialShot, FlagGood, NoTeam,
                                "Firing destroys all tanks nearby.  Don't kill teammates!  Can kill tanks on/in buildings.");
    PhantomZone = new FlagType( "Phantom Zone", "PZ", FlagUnstable, SpecialShot, FlagGood, NoTeam,
                                "Teleporting toggles Zoned effect.  Zoned tank can drive through buildings.  Zoned tank shoots Zoned bullets and can't be shot (except by superbullet, shock wave, and other Zoned tanks).");
    Genocide    = new FlagType( "Genocide", "G", FlagUnstable, NormalShot, FlagGood, NoTeam,
                                "Killing one tank kills that tank's whole team.");
    Jumping = new FlagType( "Jumping", "JP", FlagUnstable, NormalShot, FlagGood, NoTeam,
                            "Tank can jump.  Use Tab key.  Can't steer in the air.");
    Identify    = new FlagType( "Identify", "ID", FlagUnstable, NormalShot, FlagGood, NoTeam,
                                "Identifies type of nearest flag.");
    Cloaking    = new FlagType( "Cloaking", "CL", FlagUnstable, NormalShot, FlagGood, NoTeam,
                                "Makes your tank invisible out-the-window.  Still visible on radar.");
    Useless = new FlagType( "Useless", "US", FlagUnstable, NormalShot, FlagGood, NoTeam,
                            "You have found the useless flag. Use it wisely.");
    Masquerade  = new FlagType( "Masquerade", "MQ", FlagUnstable, NormalShot, FlagGood, NoTeam,
                                "In opponent's hud, you appear as a teammate.");
    Seer    = new FlagType( "Seer", "SE", FlagUnstable, NormalShot, FlagGood, NoTeam,
                            "See stealthed, cloaked and masquerading tanks as normal.");
    Thief   = new FlagType( "Thief", "TH", FlagUnstable, SpecialShot, FlagGood, NoTeam,
                            "Steal flags.  Small and fast but can't kill.");
    Burrow  = new FlagType( "Burrow", "BU", FlagUnstable, NormalShot, FlagGood, NoTeam,
                            "Tank burrows underground, impervious to normal shots, but can be steamrolled by anyone!");
    Wings   = new FlagType( "Wings", "WG", FlagUnstable, NormalShot, FlagGood, NoTeam,
                            "Tank can drive in air.");
    Agility = new FlagType( "Agility", "A", FlagUnstable, NormalShot, FlagGood, NoTeam,
                            "Tank is quick and nimble making it easier to dodge.");
    ReverseControls = new FlagType( "ReverseControls", "RC", FlagSticky, NormalShot, FlagBad, NoTeam,
                                    "Tank driving controls are reversed.");
    Colorblindness  = new FlagType( "Colorblindness", "CB", FlagSticky, NormalShot, FlagBad, NoTeam,
                                    "Can't tell team colors.  Don't shoot teammates!");
    Obesity = new FlagType( "Obesity", "O", FlagSticky, NormalShot, FlagBad, NoTeam,
                            "Tank becomes very large.  Can't fit through teleporters.");
    LeftTurnOnly    = new FlagType( "Left Turn Only", "LT", FlagSticky, NormalShot, FlagBad, NoTeam,
                                    "Can't turn right.");
    RightTurnOnly   = new FlagType( "Right Turn Only", "RT", FlagSticky, NormalShot, FlagBad, NoTeam,
                                    "Can't turn left.");
    ForwardOnly = new FlagType( "Forward Only", "FO", FlagSticky, NormalShot, FlagBad, NoTeam,
                                "Can't drive in reverse.");
    ReverseOnly = new FlagType( "ReverseOnly", "RO", FlagSticky, NormalShot, FlagBad, NoTeam,
                                "Can't drive forward.");
    Momentum    = new FlagType( "Momentum", "M", FlagSticky, NormalShot, FlagBad, NoTeam,
                                "Tank has inertia.  Acceleration is limited.");
    Blindness   = new FlagType( "Blindness", "B", FlagSticky, NormalShot, FlagBad, NoTeam,
                                "Can't see out window.  Radar still works.");
    Jamming = new FlagType( "Jamming", "JM", FlagSticky, NormalShot, FlagBad, NoTeam,
                            "Radar doesn't work.  Can still see.");
    WideAngle   = new FlagType( "Wide Angle", "WA", FlagSticky, NormalShot, FlagBad, NoTeam,
                                "Fish-eye lens distorts view.");
    NoJumping   = new FlagType( "No Jumping", "NJ", FlagSticky, NormalShot, FlagBad, NoTeam,
                                "Tank can't jump.");
    TriggerHappy    = new FlagType( "Trigger Happy", "TR", FlagSticky, NormalShot, FlagBad, NoTeam,
                                    "Tank can't stop firing.");
    Bouncy  = new FlagType( "Bouncy", "BY", FlagSticky, NormalShot, FlagBad, NoTeam,
                            "Tank can't stop bouncing.");
}

void kill()
{
    clearCustomFlags();

    delete RedTeam;
    delete GreenTeam;
    delete BlueTeam;
    delete PurpleTeam;
    delete Velocity;
    delete QuickTurn;
    delete OscillationOverthruster;
    delete RapidFire;
    delete MachineGun;
    delete GuidedMissile;
    delete Laser;
    delete Ricochet;
    delete SuperBullet;
    delete InvisibleBullet;
    delete Stealth;
    delete Tiny;
    delete Narrow;
    delete Shield;
    delete Steamroller;
    delete ShockWave;
    delete PhantomZone;
    delete Genocide;
    delete Jumping;
    delete Identify;
    delete Cloaking;
    delete Useless;
    delete Masquerade;
    delete Seer;
    delete Thief;
    delete Burrow;
    delete Wings;
    delete Agility;
    delete ReverseControls;
    delete Colorblindness;
    delete Obesity;
    delete LeftTurnOnly;
    delete RightTurnOnly;
    delete ForwardOnly;
    delete ReverseOnly;
    delete Momentum;
    delete Blindness;
    delete Jamming;
    delete WideAngle;
    delete NoJumping;
    delete TriggerHappy;
    delete Bouncy;

    delete[] FlagType::flagSets;
}

void clearCustomFlags()
{
    FlagSet::iterator itr;
    for (int q = 0; q < (int)NumQualities; ++q)
        for (itr = FlagType::flagSets[q].begin(); itr != FlagType::flagSets[q].end();)
            if ((*itr)->custom)
            {
                delete *itr;
                itr = FlagType::flagSets[q].erase(itr);
            }
            else
                itr++;
    FlagType::customFlags.clear();
}
}

FlagType::~FlagType()
{
    getFlagMap().erase(flagAbbv);
}

void* FlagType::pack(void* buf) const
{
    buf = nboPackUByte(buf, (flagAbbv.size() > 0) ? flagAbbv[0] : 0);
    buf = nboPackUByte(buf, (flagAbbv.size() > 1) ? flagAbbv[1] : 0);
    return buf;
}

void* FlagType::fakePack(void* buf) const
{
    buf = nboPackUByte(buf, 'P');
    buf = nboPackUByte(buf, 'Z');
    return buf;
}

const void* FlagType::unpack(const void* buf, FlagType* &type)
{
    unsigned char abbv[3] = {0, 0, 0};
    buf = nboUnpackUByte(buf, abbv[0]);
    buf = nboUnpackUByte(buf, abbv[1]);
    type = Flag::getDescFromAbbreviation((const char *)abbv);
    return buf;
}

void* FlagType::packCustom(void* buf) const
{
    buf = pack(buf);
    buf = nboPackUByte(buf, uint8_t(flagQuality));
    buf = nboPackUByte(buf, uint8_t(flagShot));
    buf = nboPackStdString(buf, flagName);
    buf = nboPackStdString(buf, flagHelp);
    return buf;
}

const void* FlagType::unpackCustom(const void* buf, FlagType* &type)
{
    unsigned char abbv[3] = {0, 0, 0};
    buf = nboUnpackUByte(buf, abbv[0]);
    buf = nboUnpackUByte(buf, abbv[1]);

    uint8_t quality, shot;
    buf = nboUnpackUByte(buf, quality);
    buf = nboUnpackUByte(buf, shot);

    // make copies to keep - note that these will need to be deleted.
    std::string sName, sHelp;
    buf = nboUnpackStdString(buf, sName);
    buf = nboUnpackStdString(buf, sHelp);

    FlagEndurance e = FlagUnstable;
    switch ((FlagQuality)quality)
    {
    case FlagGood:
        e = FlagUnstable;
        break;
    case FlagBad:
        e = FlagSticky;
        break;
    default:
        assert(false); // shouldn't happen
    }

    type = new FlagType(sName, (char *)abbv,
                        e, (ShotType)shot, (FlagQuality)quality, NoTeam, sHelp, true);
    return buf;
}

FlagTypeMap& FlagType::getFlagMap()
{
    static FlagTypeMap flagMap;
    return flagMap;
}

void* Flag::pack(void* buf) const
{
    buf = type->pack(buf);
    buf = nboPackUShort(buf, uint16_t(status));
    buf = nboPackUShort(buf, uint16_t(endurance));
    buf = nboPackUByte(buf, owner);
    buf = nboPackVector(buf, position);
    buf = nboPackVector(buf, launchPosition);
    buf = nboPackVector(buf, landingPosition);
    buf = nboPackFloat(buf, flightTime);
    buf = nboPackFloat(buf, flightEnd);
    buf = nboPackFloat(buf, initialVelocity);
    return buf;
}

void* Flag::fakePack(void* buf) const
{
    buf = type->fakePack(buf);
    buf = nboPackUShort(buf, uint16_t(status));
    buf = nboPackUShort(buf, uint16_t(endurance));
    buf = nboPackUByte(buf, owner);
    buf = nboPackVector(buf, position);
    buf = nboPackVector(buf, launchPosition);
    buf = nboPackVector(buf, landingPosition);
    buf = nboPackFloat(buf, flightTime);
    buf = nboPackFloat(buf, flightEnd);
    buf = nboPackFloat(buf, initialVelocity);
    return buf;
}

const void* Flag::unpack(const void* buf)
{
    uint16_t data;

    buf = FlagType::unpack(buf, type);
    buf = nboUnpackUShort(buf, data);
    status = FlagStatus(data);
    buf = nboUnpackUShort(buf, data);
    endurance = FlagEndurance(data);
    buf = nboUnpackUByte(buf, owner);
    buf = nboUnpackVector(buf, position);
    buf = nboUnpackVector(buf, launchPosition);
    buf = nboUnpackVector(buf, landingPosition);
    buf = nboUnpackFloat(buf, flightTime);
    buf = nboUnpackFloat(buf, flightEnd);
    buf = nboUnpackFloat(buf, initialVelocity);
    return buf;
}

FlagType* Flag::getDescFromAbbreviation(const char* abbreviation)
{
    FlagTypeMap::iterator i;
    std::string abbvString;

    /* Uppercase the abbreviation */
    while (*abbreviation)
    {
        abbvString += toupper(*abbreviation);
        abbreviation++;
    }

    i = FlagType::getFlagMap().find(abbvString);
    if (i == FlagType::getFlagMap().end())
        /* Not found, return the Null flag */
        return Flags::Null;
    else
        return i->second;
}

FlagSet& Flag::getGoodFlags()
{
    return FlagType::flagSets[FlagGood];
}

FlagSet& Flag::getBadFlags()
{
    return FlagType::flagSets[FlagBad];
}

const glm::vec3 &FlagType::getColor() const
{
    static const auto superColor = glm::vec3(1.0f);

    if (flagTeam == NoTeam)
        return superColor;
    else
        return Team::getTankColor(flagTeam);
}

const glm::vec3 &FlagType::getRadarColor() const
{
    static const auto superColor = glm::vec3(1.0f);

    if (flagTeam == NoTeam)
        return superColor;
    else
        return Team::getRadarColor(flagTeam);
}


const std::string FlagType::label() const
{
    unsigned int i;

    /* convert to lowercase so we can uppercase the abbreviation later */
    std::string caseName = "";
    for (i = 0; i < flagName.size(); i++)
        caseName += tolower(flagName.c_str()[i]);

    /* modify the flag name to exemplify the abbreviation */
    int charPosition;
    for (i = 0; i < flagAbbv.size(); i++)
    {

        charPosition = caseName.find_first_of(tolower(flagAbbv[i]), 0);

        if (charPosition > 0)
        {
            /* if we can match an abbreviation on a word boundary -- prefer it */
            int alternateCharPosition = 1;
            while (alternateCharPosition > 0)
            {
                if (caseName[alternateCharPosition - 1] == ' ')
                {
                    charPosition = alternateCharPosition;
                    break;
                }
                alternateCharPosition = caseName.find_first_of(tolower(flagAbbv[i]), alternateCharPosition+1);
            }
        }

        if (charPosition >= 0)
            caseName[charPosition] = toupper(caseName[charPosition]);
    }

    if (flagTeam != NoTeam)
    {
        /* team flag info is more simple than non-team flag info */
        caseName += " flag";
    }
    else
    {
        /* non-team flags */
        caseName += TextUtils::format(" (%c%s)",
                                      flagQuality==FlagGood?'+':'-',
                                      flagAbbv.c_str());
    }

    return caseName;
}


const std::string FlagType::information() const
{
    return TextUtils::format("%s: %s",
                             label().c_str(),
                             flagHelp.c_str());
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
