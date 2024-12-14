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


// interface header
#include "MsgStrings.h"

// system headers
#include <string>
#include <vector>
#include <map>
#include <assert.h>
#include <stdarg.h>
#include <math.h>
#include <glm/vec3.hpp>

// common headers
#include "global.h"
#include "Protocol.h"
#include "Pack.h"
#include "AnsiCodes.h"
#include "TextUtils.h"
#include "Address.h"
#include "StateDatabase.h"
#include "PlayerState.h"
#include "Team.h"
#include "Flag.h"
#include "ShotUpdate.h"


typedef uint8_t  u8;
typedef uint16_t u16;

typedef std::map<u16, std::string> FlagListType;
typedef std::map<u16, std::string> PlayerListType;

typedef struct
{
    u16 len;
    u16 code;
    const void *data;
} PacketInfo;


static FlagListType   FlagList;
static PlayerListType PlayerList;
static bool UseDNS = true;
static bool Colorize = true;
static bool ShowMotto = true;
static bool TrackState = true;

static MsgStringList listMsgBasics (PacketInfo *pi);
static void listPush (MsgStringList &list, int level, const char *fmt, ...);
static std::string strFlag (u16 id);
static std::string strFlagStatus (FlagStatus status);
static std::string strTeam (u16 id);
static std::string strPlayer (u16 id);
static std::string strVector (const float *vector);
static std::string strKillReason (int16_t reason);
static std::string strAddress (Address& address);

static MsgStringList handleMsgNull(PacketInfo *pi); // fake packet type
static MsgStringList handleMsgAccept(PacketInfo *pi);
static MsgStringList handleMsgAlive(PacketInfo *pi);
static MsgStringList handleMsgAdminInfo(PacketInfo *pi);
static MsgStringList handleMsgAddPlayer(PacketInfo *pi);
static MsgStringList handleMsgCaptureFlag(PacketInfo *pi);
static MsgStringList handleMsgDropFlag(PacketInfo *pi);
static MsgStringList handleMsgEnter(PacketInfo *pi);
static MsgStringList handleMsgExit(PacketInfo *pi);
static MsgStringList handleMsgFlagUpdate(PacketInfo *pi);
static MsgStringList handleMsgGameTime(PacketInfo *pi);
static MsgStringList handleMsgGrabFlag(PacketInfo *pi);
static MsgStringList handleMsgGMUpdate(PacketInfo *pi);
static MsgStringList handleMsgGetWorld(PacketInfo *pi);
static MsgStringList handleMsgGameSettings(PacketInfo *pi);
static MsgStringList handleMsgKilled(PacketInfo *pi);
static MsgStringList handleMsgMessage(PacketInfo *pi);
static MsgStringList handleMsgNearFlag(PacketInfo *pi);
static MsgStringList handleMsgNewRabbit(PacketInfo *pi);
static MsgStringList handleMsgNegotiateFlags(PacketInfo *pi);
static MsgStringList handleMsgPause(PacketInfo *pi);
static MsgStringList handleMsgPlayerInfo(PacketInfo *pi);
static MsgStringList handleMsgPlayerUpdate(PacketInfo *pi);
static MsgStringList handleMsgPlayerUpdateSmall(PacketInfo *pi);
static MsgStringList handleMsgQueryGame(PacketInfo *pi);
static MsgStringList handleMsgQueryPlayers(PacketInfo *pi);
static MsgStringList handleMsgReject(PacketInfo *pi);
static MsgStringList handleMsgReplayReset(PacketInfo *pi);
static MsgStringList handleMsgRemovePlayer(PacketInfo *pi);
static MsgStringList handleMsgShotBegin(PacketInfo *pi);
static MsgStringList handleMsgScore(PacketInfo *pi);
static MsgStringList handleMsgScoreOver(PacketInfo *pi);
static MsgStringList handleMsgShotEnd(PacketInfo *pi);
static MsgStringList handleMsgSuperKill(PacketInfo *pi);
static MsgStringList handleMsgSetVar(PacketInfo *pi);
static MsgStringList handleMsgTimeUpdate(PacketInfo *pi);
static MsgStringList handleMsgTeleport(PacketInfo *pi);
static MsgStringList handleMsgTransferFlag(PacketInfo *pi);
static MsgStringList handleMsgTeamUpdate(PacketInfo *pi);
static MsgStringList handleMsgWantWHash(PacketInfo *pi);
static MsgStringList handleMsgWantSettings(PacketInfo *pi);
static MsgStringList handleMsgUDPLinkRequest(PacketInfo *pi);
static MsgStringList handleMsgUDPLinkEstablished(PacketInfo *pi);
static MsgStringList handleMsgLagPing(PacketInfo *pi);
static MsgStringList handleMsgPingCodeReply(PacketInfo *pi);
static MsgStringList handleMsgPingCodeRequest(PacketInfo *pi);


typedef struct
{
    uint16_t code;
    const char *label;
    MsgStringList (*handler)(PacketInfo *pi);
} PacketListEntry;

// ick, a #define, but it cleans this up nicely
#define PACKET_LIST_ENTRY(x) {x, #x, handle ##x}
static PacketListEntry PacketList[] =
{
    PACKET_LIST_ENTRY (MsgNull),
    PACKET_LIST_ENTRY (MsgAccept),
    PACKET_LIST_ENTRY (MsgAlive),
    PACKET_LIST_ENTRY (MsgAdminInfo),
    PACKET_LIST_ENTRY (MsgAddPlayer),
    PACKET_LIST_ENTRY (MsgCaptureFlag),
    PACKET_LIST_ENTRY (MsgDropFlag),
    PACKET_LIST_ENTRY (MsgEnter),
    PACKET_LIST_ENTRY (MsgExit),
    PACKET_LIST_ENTRY (MsgFlagUpdate),
    PACKET_LIST_ENTRY (MsgGameTime),
    PACKET_LIST_ENTRY (MsgGrabFlag),
    PACKET_LIST_ENTRY (MsgGMUpdate),
    PACKET_LIST_ENTRY (MsgGetWorld),
    PACKET_LIST_ENTRY (MsgGameSettings),
    PACKET_LIST_ENTRY (MsgKilled),
    PACKET_LIST_ENTRY (MsgMessage),
    PACKET_LIST_ENTRY (MsgNearFlag),
    PACKET_LIST_ENTRY (MsgNewRabbit),
    PACKET_LIST_ENTRY (MsgNegotiateFlags),
    PACKET_LIST_ENTRY (MsgPause),
    PACKET_LIST_ENTRY (MsgPlayerInfo),
    PACKET_LIST_ENTRY (MsgPlayerUpdate),
    PACKET_LIST_ENTRY (MsgPlayerUpdateSmall),
    PACKET_LIST_ENTRY (MsgQueryGame),
    PACKET_LIST_ENTRY (MsgQueryPlayers),
    PACKET_LIST_ENTRY (MsgReject),
    PACKET_LIST_ENTRY (MsgReplayReset),
    PACKET_LIST_ENTRY (MsgRemovePlayer),
    PACKET_LIST_ENTRY (MsgShotBegin),
    PACKET_LIST_ENTRY (MsgScore),
    PACKET_LIST_ENTRY (MsgScoreOver),
    PACKET_LIST_ENTRY (MsgShotEnd),
    PACKET_LIST_ENTRY (MsgSuperKill),
    PACKET_LIST_ENTRY (MsgSetVar),
    PACKET_LIST_ENTRY (MsgTimeUpdate),
    PACKET_LIST_ENTRY (MsgTeleport),
    PACKET_LIST_ENTRY (MsgTransferFlag),
    PACKET_LIST_ENTRY (MsgTeamUpdate),
    PACKET_LIST_ENTRY (MsgWantWHash),
    PACKET_LIST_ENTRY (MsgWantSettings),
    PACKET_LIST_ENTRY (MsgUDPLinkRequest),
    PACKET_LIST_ENTRY (MsgUDPLinkEstablished),
    PACKET_LIST_ENTRY (MsgLagPing),
    PACKET_LIST_ENTRY (MsgPingCodeReply),
    PACKET_LIST_ENTRY (MsgPingCodeRequest)
};
static const int PacketListCount = sizeof (PacketList) / sizeof (PacketList[0]);


/******************************************************************************/

void MsgStrings::init ()
{
    Flags::init(); // Initialize the flags - FIXME - check if necessary
    MsgStrings::reset();
    return;
}


void MsgStrings::reset ()
{
    unsigned int i;

    UseDNS = true;
    Colorize = true;
    ShowMotto = true;
    TrackState = true;

    FlagList.clear();
    PlayerList.clear();

    // setup default player names
    PlayerList[ServerPlayer] = "SERVER";
    PlayerList[AllPlayers] = "ALL";
    PlayerList[AdminPlayers] = "ADMIN";
    PlayerList[NoPlayer] = "NOPLAYER";
    for (i = LastRealPlayer + 1 ; i <= FirstTeam; i++)
        PlayerList[i] = Team::getName (TeamColor(FirstTeam - i));

    // set default DB entries
    for (i = 0; i < numGlobalDBItems; ++i)
    {
        assert(globalDBItems[i].name != NULL);
        if (globalDBItems[i].value != NULL)
        {
            BZDB.set(globalDBItems[i].name, globalDBItems[i].value);
            BZDB.setDefault(globalDBItems[i].name, globalDBItems[i].value);
        }
        BZDB.setPersistent(globalDBItems[i].name, globalDBItems[i].persistent);
        BZDB.setPermission(globalDBItems[i].name, globalDBItems[i].permission);
    }

    return;
}


void MsgStrings::useDNS (bool value)
{
    UseDNS = value;
    return;
}


void MsgStrings::showMotto (bool value)
{
    ShowMotto = value;
    return;
}


void MsgStrings::colorize (bool value)
{
    Colorize = value;
    return;
}


void MsgStrings::trackState (bool value)
{
    TrackState = value;
    return;
}


int MsgStrings::knownPacketTypes ()
{
    return (PacketListCount - 1);  // MsgNull doesn't count
}


const char * MsgStrings::strMsgCode (uint16_t code)
{
    int i;
    for (i = 0; i < PacketListCount; i++)
    {
        if (PacketList[i].code == code)
            break;
    }
    if (i < PacketListCount)
        return PacketList[i].label;
    else
    {
        static char buf[32];
        sprintf (buf, "MsgUnknown(0x%04X)", code);
        return buf;
    }
}


// a simple example of how to use this:
// MsgStringList details = MsgStrings::msgFromServer(len, code, msg);
// for (MsgStringList::iterator i = details.begin(); i != details.end(); i++)
//   std::cerr << i->text << std::endl;
MsgStringList MsgStrings::msgFromServer (u16 len, u16 code, const void *data)
{
    int i;
    for (i = 0; i < PacketListCount; i++)
    {
        if (PacketList[i].code == code)
            break;
    }
    if (i < PacketListCount)
    {
        PacketInfo pi = {len, code, data};
        return PacketList[i].handler (&pi);
    }
    else
    {
        MsgStringList badcode;
        listPush (badcode, 0, "Unknown message code: 0x%04X\n", code);
        return badcode;
    }
}


MsgStringList MsgStrings::msgFromClient (u16 len, u16 code, const void *data)
{
    int i;
    for (i = 0; i < PacketListCount; i++)
    {
        if (PacketList[i].code == code)
            break;
    }
    if (i < PacketListCount)
    {
        PacketInfo pi = {len, code, data};
        MsgStringList list = listMsgBasics (&pi);
        list[0].text += "  << client to server messages unimplemented >>";
        return list;
    }
    else
    {
        MsgStringList list;
        listPush (list, 0, "Unknown message code: 0x%04X\n", code);
        list[0].text += "  << client to server messages unimplemented >>";
        return list;
    }
}


/******************************************************************************/

static void listPush (MsgStringList &list, int level, const char *fmt, ...)
{
//  std::string str;
    MsgString mstr = { level, "", "" };
    char buffer[256];
    va_list args;
    va_start(args, fmt);
#ifdef HAVE_VSNPRINTF
    vsnprintf (buffer, 256, fmt, args);
#else
    vsprintf (buffer, fmt, args);
#endif
    va_end(args);
    mstr.text += buffer;
    if (Colorize)
    {
        switch (level)
        {
        case 0:
            mstr.color = ANSI_STR_FG_MAGENTA;
            break;
        case 1:
            mstr.color = ANSI_STR_FG_BLUE;
            break;
        case 2:
            mstr.color = ANSI_STR_FG_CYAN;
            break;
        case 3:
            mstr.color = ANSI_STR_FG_GREEN;
            break;
        }
    }

    list.push_back (mstr);
    return;
}


static MsgStringList listMsgBasics (PacketInfo *pi)
{
    MsgStringList list;
    // also include the length and code u16's for the total length
    listPush (list, 0, "%s <%i>", MsgStrings::strMsgCode (pi->code), pi->len + 4);
    return list;
}


static std::string strFlag (u16 id)
{
    FlagListType::iterator it = FlagList.find (id);
    std::string name = "Invalid";
    if (it != FlagList.end())
        name = (*it).second;
    return TextUtils::format ("%-2s [%i]", name.c_str(), id);
}


static std::string strFlagStatus (FlagStatus status)
{
    std::string str;
#define STRING_CASE(x)  case (x): str = #x; break
    switch (status)
    {
        STRING_CASE (FlagNoExist);
        STRING_CASE (FlagOnGround);
        STRING_CASE (FlagOnTank);
        STRING_CASE (FlagInAir);
        STRING_CASE (FlagComing);
        STRING_CASE (FlagGoing);
    default:
        str = TextUtils::format ("UNKNOWN: 0x04%X", status);
        break;
    }
    return str;
}

static std::string strTeam (u16 id)
{
    return std::string (Team::getName((TeamColor) id));
}


static std::string strPlayer (u16 id)
{
    PlayerListType::iterator it = PlayerList.find (id);
    std::string name;
    if (it != PlayerList.end())
        name = (*it).second;
    else
        name = "UnTracked";
    return TextUtils::format ("%s [%i]", name.c_str(), id);
}


static std::string strVector (const glm::vec3 &vector)
{
    std::string str = TextUtils::format ("(%8.3f, %8.3f, %8.3f)",
                                         vector[0], vector[1], vector[2]);
    return str;
}


static std::string strVector (const float *vector)
{
    std::string str = TextUtils::format ("(%8.3f, %8.3f, %8.3f)",
                                         vector[0], vector[1], vector[2]);
    return str;
}


static std::string strAddress (Address& address)
{
    return address.getDotNotation();
}


static std::string strKillReason (int16_t reason)
{
    switch (reason)
    {
    case 0:
        return std::string ("blowed up");
    case 1:
        return std::string ("shot");
    case 2:
        return std::string ("runover");
    case 3:
        return std::string ("captured");
    case 4:
        return std::string ("genocide");
    case 5:
        return std::string ("self destruct");
    default:
        return TextUtils::format ("unknown: %i", reason);
    }
}

/******************************************************************************/

static MsgStringList handleMsgNull (PacketInfo *pi)
{
    // not recorded, never seen?
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgAccept (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgAlive (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 player;
    float pos[3], azimuth;
    d = nboUnpackUByte (d, player);
    d = nboUnpackVector (d, pos);
    d = nboUnpackFloat (d, azimuth);
    listPush (list, 1, "player: %s", strPlayer(player).c_str());
    listPush (list, 2, "pos:    %s", strVector(pos).c_str());
    listPush (list, 2, "angle:  %-8.3f = %8.3f deg",
              azimuth, azimuth * (180.0f / M_PI));

    return list;
}


static MsgStringList handleMsgAdminInfo (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 count, ipsize, player;
    Address address;
    u16 i;
    d = nboUnpackUByte (d, count);
    listPush (list, 1, "count: %i", count);
    for (i=0 ; i < count; i++)
    {
        d = nboUnpackUByte (d, ipsize);
        d = nboUnpackUByte (d, player);
        d = address.unpack (d);

        listPush (list, 2, "player:     %s", strPlayer(player).c_str());
        listPush (list, 2, "ipsize:     %i", ipsize);
        listPush (list, 2, "address:    %s", strAddress(address).c_str());
    }

    return list;
}


static MsgStringList handleMsgAddPlayer (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 index;
    u16 type, team, wins, losses, tks;
    char callsign[CallSignLen];
    char motto[MottoLen];

    d = nboUnpackUByte (d, index);
    d = nboUnpackUShort (d, type);
    d = nboUnpackUShort (d, team);
    d = nboUnpackUShort (d, wins);
    d = nboUnpackUShort (d, losses);
    d = nboUnpackUShort (d, tks);
    d = nboUnpackString (d, callsign, CallSignLen);
    d = nboUnpackString (d, motto, MottoLen);

    if (TrackState)
        PlayerList[index] = callsign;
    listPush (list, 1, "player: %s", strPlayer(index).c_str());
    listPush (list, 1, "motto:  %s", motto);
    listPush (list, 1, "team:   %s", strTeam(team).c_str());
    listPush (list, 1, "type:   %i", type);
    listPush (list, 2, "wins:   %i", wins);
    listPush (list, 2, "losses: %i", losses);
    listPush (list, 2, "tks:    %i", tks);

    return list;
}


static MsgStringList handleMsgCaptureFlag (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    PlayerId id;
    u16 flag, team;
    d = nboUnpackUByte(d, id);
    d = nboUnpackUShort (d, flag);
    d = nboUnpackUShort (d, team);
    listPush (list, 1, "player: %s", strPlayer(id).c_str());
    listPush (list, 1, "team: %s", strTeam (team).c_str());

    return list;
}


static MsgStringList handleMsgDropFlag (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    Flag flag;
    u8 player;
    u16 flagid;
    d = nboUnpackUByte (d, player);
    d = nboUnpackUShort (d, flagid);
    d = flag.unpack (d);
    listPush (list, 1, "player: %s", strPlayer(player).c_str());
    listPush (list, 1, "flag: %s", strFlag (flagid).c_str());

    return list;
}


static MsgStringList handleMsgEnter (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgExit (PacketInfo *pi)
{
    // not recorded, but bzfs will send a MsgRemovePlayer
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgFlagUpdate (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u16 count, index;
    int i;
    d = nboUnpackUShort (d, count);
    listPush (list, 1, "count: %i", count);
    for (i = 0; i < (int) count; i++)
    {
        Flag flag;
        d = nboUnpackUShort (d, index);
        d = flag.unpack (d);
        if (TrackState)
            FlagList[index] = flag.type->flagAbbv;
        listPush (list, 2, "flag: %s", strFlag (index).c_str());
        listPush (list, 3, "owner:  %s", strPlayer (flag.owner).c_str());
        listPush (list, 3, "pos:    %s", strVector (flag.position).c_str());
        listPush (list, 3, "status: %s", strFlagStatus (flag.status).c_str());
    }

    return list;
}


static MsgStringList handleMsgGrabFlag (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    Flag flag;
    u8 player;
    u16 flagid;
    d = nboUnpackUByte (d, player);
    d = nboUnpackUShort (d, flagid);
    d = flag.unpack (d);
    listPush (list, 1, "player: %s", strPlayer(player).c_str());
    listPush (list, 1, "flag: %s", strFlag (flagid).c_str());

    return list;
}


static MsgStringList handleMsgGMUpdate (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 target;
    ShotUpdate shot;
    d = shot.unpack (d);
    d = nboUnpackUByte (d, target);
    listPush (list, 1, "player:   %s", strPlayer(shot.player).c_str());
    listPush (list, 1, "target:   %s", strPlayer(target).c_str());
    listPush (list, 2, "id:       %i", shot.id);
    listPush (list, 2, "team:     %s", strTeam(shot.team).c_str());
    listPush (list, 2, "pos:      %s", strVector(shot.pos).c_str());
    listPush (list, 2, "vel:      %s", strVector(shot.vel).c_str());

    return list;
}


static MsgStringList handleMsgGetWorld (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgGameSettings (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgKilled (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 victim, killer;
    int16_t reason, shot;
    FlagType* flagType;
    int32_t phydrv;
    d = nboUnpackUByte(d, victim);
    d = nboUnpackUByte(d, killer);
    d = nboUnpackShort(d, reason);
    d = nboUnpackShort(d, shot);
    d = FlagType::unpack(d, flagType);
    if (reason == PhysicsDriverDeath)
        d = nboUnpackInt(d, phydrv);
    listPush (list, 1, "victim: %s", strPlayer(victim).c_str());
    listPush (list, 1, "killer: %s", strPlayer(killer).c_str());
    listPush (list, 1, "reason: %s", strKillReason(reason).c_str());
    listPush (list, 1, "shotid: %i", shot);

    if (flagType != Flags::Null)
        listPush (list, 1, "flag:   %s", flagType->flagAbbv.c_str());
    else
        listPush (list, 1, "flag:   Null");
    if (reason == PhysicsDriverDeath)
        listPush (list, 1, "phydrv: %i", phydrv);

    return list;
}


static MsgStringList handleMsgMessage (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 src, dst;
    d = nboUnpackUByte (d, src);
    d = nboUnpackUByte (d, dst);
    listPush (list, 1, "src: %s", strPlayer(src).c_str());
    listPush (list, 1, "dst: %s", strPlayer(dst).c_str());
    listPush (list, 1, "message: \"%s\"", (const char*) d);

    return list;
}


static MsgStringList handleMsgNearFlag (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgNewRabbit (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 player, paused;
    d = nboUnpackUByte (d, player);
    d = nboUnpackUByte (d, paused);

    return list;
}


static MsgStringList handleMsgNegotiateFlags (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgPause (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 player, paused;
    d = nboUnpackUByte (d, player);
    d = nboUnpackUByte (d, paused);
    listPush (list, 1, "player: %s", strPlayer(player).c_str());
    listPush (list, 1, "paused: %i", paused);

    return list;
}


static MsgStringList handleMsgPlayerInfo (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 count, player, properties;
    Address address;
    u16 i;
    d = nboUnpackUByte (d, count);
    listPush (list, 1, "count: %i", count);
    for (i=0 ; i < count; i++)
    {
        d = nboUnpackUByte (d, player);
        d = nboUnpackUByte (d, properties);

        std::string props;
        if (properties & IsRegistered)
            props += "Registered ";
        if (properties & IsVerified)
            props += "Verified ";
        if (properties & IsAdmin)
            props += "Admin ";

        listPush (list, 2, "player:     %s", strPlayer(player).c_str());
        listPush (list, 2, "properties: %s(0x%02X)", props.c_str(), properties);
    }

    return list;
}


static MsgStringList handleMsgPlayerUpdate (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    float timestamp;
    u8 index;
    PlayerState state;
    d = nboUnpackFloat (d, timestamp);
    d = nboUnpackUByte (d, index);
    d = state.unpack (d, pi->code);

    listPush (list, 1, "player: %s", strPlayer(index).c_str());
    listPush (list, 2, "state: 0x%04X  order: %li", state.status, state.order);
    listPush (list, 3, "pos:    %s", strVector (state.pos).c_str());
    listPush (list, 3, "vel:    %s", strVector (state.velocity).c_str());
    listPush (list, 3, "angle:  %-8.3f = %8.3f deg",
              state.azimuth, state.azimuth * (180.0f / M_PI));
    listPush (list, 3, "angvel: %-8.3f = %8.3f deg/sec",
              state.angVel, state.angVel * (180.0f / M_PI));

    return list;
}


static MsgStringList handleMsgPlayerUpdateSmall (PacketInfo *pi)
{
    // call the normal function while maintaining the same 'code' value
    return handleMsgPlayerUpdate (pi);
}


static MsgStringList handleMsgQueryGame (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgQueryPlayers (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgReject (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgReplayReset (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgRemovePlayer (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 index;
    d = nboUnpackUByte (d, index);
    listPush (list, 1, "player: %s", strPlayer(index).c_str());
    if (TrackState)
        PlayerList.erase (index);

    return list;
}


static MsgStringList handleMsgShotBegin (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    FiringInfo finfo;
    d = finfo.unpack (d);
    const ShotUpdate& shot = finfo.shot;
    listPush (list, 1, "player:   %s", strPlayer(shot.player).c_str());
    listPush (list, 1, "type:     %.2s", finfo.flagType->flagAbbv.c_str()); // FIXME ?
    listPush (list, 2, "id:       %i", shot.id);
    listPush (list, 2, "team:     %s", strTeam(shot.team).c_str());
    listPush (list, 2, "pos:      %s", strVector(shot.pos).c_str());
    listPush (list, 2, "vel:      %s", strVector(shot.vel).c_str());
    listPush (list, 2, "lifetime: %-8.3f", finfo.lifetime);

    return list;
}


static MsgStringList handleMsgScore (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 count, player;
    u16 wins, losses, tks, i;
    d = nboUnpackUByte (d, count);
    listPush (list, 1, "count: %i", count);
    for (i=0; i < count; i++)
    {
        d = nboUnpackUByte (d, player);
        d = nboUnpackUShort (d, wins);
        d = nboUnpackUShort (d, losses);
        d = nboUnpackUShort (d, tks);
        listPush (list, 2, "player: %s", strPlayer(player).c_str());
        listPush (list, 3, "wins: %i  losses: %i  tks: %i", wins, losses, tks);
    }

    return list;
}


static MsgStringList handleMsgScoreOver (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 player;
    u16 team;
    d = nboUnpackUByte(d, player);
    d = nboUnpackUShort(d, team);
    listPush (list, 1, "player: %s", strPlayer(player).c_str());
    listPush (list, 1, "team:   %s", strTeam(team).c_str());

    return list;
}


static MsgStringList handleMsgShotEnd (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 player;
    u16 shotid;
    int16_t reason;
    d = nboUnpackUByte(d, player);
    d = nboUnpackUShort(d, shotid);
    d = nboUnpackShort(d, reason);
    listPush (list, 1, "player: %s", strPlayer(player).c_str());
    listPush (list, 1, "shotid: %i", shotid);
    listPush (list, 1, "reason: %i", reason); // FIXME

    return list;
}


static MsgStringList handleMsgSuperKill (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static std::string n, v;
static MsgStringList handleMsgSetVar (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u16 i;
    u16 count;
    u8 nameLen, valueLen;
    d = nboUnpackUShort(d, count);
    listPush (list, 1, "count: %i", count);

    char name[256] = {0}, value[256] = {0};
    for (i = 0; i < count; i++)
    {
        d = nboUnpackUByte(d, nameLen);
        d = nboUnpackString(d, name, nameLen);
        d = nboUnpackUByte(d, valueLen);
        d = nboUnpackString(d, value, valueLen);
        value[valueLen] = '\0';
        listPush (list, 2, "%-20s = \"%s\"", name, value);

        if (TrackState)
        {
            n = name;
            v = value;
            BZDB.set (n, v, StateDatabase::Locked);
        }

    }

    return list;
}


static MsgStringList handleMsgTimeUpdate (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);
    const void *d = pi->data;
    int32_t timeLeft;
    d = nboUnpackInt(d, timeLeft);
    listPush (list, 1, "timeLeft: %i", timeLeft);

    return list;
}


static MsgStringList handleMsgTeleport (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 player;
    u16 to, from;
    d = nboUnpackUByte(d, player);
    d = nboUnpackUShort(d, from);
    d = nboUnpackUShort(d, to);
    listPush (list, 1, "player: %s", strPlayer(player).c_str());
    listPush (list, 1, "from:   %i", from);
    listPush (list, 1, "to:     %i", to);

    return list;
}


static MsgStringList handleMsgTransferFlag (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 to, from;
    u16 flagid;
    Flag flag;
    d = nboUnpackUByte(d, from);
    d = nboUnpackUByte(d, to);
    d = nboUnpackUShort(d, flagid);
    d = flag.unpack (d);
    listPush (list, 1, "from: %s", strPlayer(from).c_str());
    listPush (list, 1, "to:   %s", strPlayer(to).c_str());
    listPush (list, 1, "flag: %s", strFlag(flagid).c_str());

    return list;
}


static MsgStringList handleMsgTeamUpdate (PacketInfo *pi)
{
    MsgStringList list = listMsgBasics (pi);

    const void *d = pi->data;
    u8 count;
    u16 i, team, size, won, lost;
    d = nboUnpackUByte(d, count);
    listPush (list, 1, "count: %i", count);
    for (i=0; i<count; i++)
    {
        d = nboUnpackUShort(d, team);
        d = nboUnpackUShort(d, size);
        d = nboUnpackUShort(d, won);
        d = nboUnpackUShort(d, lost);
        listPush (list, 2, "team: %s", strTeam(team).c_str());
        listPush (list, 3, "size = %i, won = %i, lost = %i", size, won, lost);
    }

    return list;
}


static MsgStringList handleMsgWantWHash (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgWantSettings (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgUDPLinkRequest (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgUDPLinkEstablished (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgLagPing (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgGameTime (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgPingCodeReply (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


static MsgStringList handleMsgPingCodeRequest (PacketInfo *pi)
{
    // not recorded
    MsgStringList list = listMsgBasics (pi);
    return list;
}


/******************************************************************************/

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
