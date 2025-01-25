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

#include "common.h"

// interface header
#include "commands.h"


// system implementation headers
#include <vector>
#include <string>
#include <sstream>
#ifdef HAVE_CSTDIO
#include <cstdio>
#else
#include <stdio.h>
#endif
#ifdef HAVE_CSTRING
#include <cstring>
#else
#include <string.h>
#endif
#include <time.h>
#include <ctype.h>

// common implementation headers
#include "bzglob.h"
#include "TextUtils.h"
#include "CommandManager.h"
#include "LagInfo.h"
#include "NetHandler.h"
#include "PlayerInfo.h"
#include "TimeKeeper.h"
#include "VotingArbiter.h"
#include "global.h"
#include "version.h"
#include "WorldEventManager.h"

// local implementation headers
#include "FlagHistory.h"
#include "Permissions.h"
#include "RecordReplay.h"
#include "bzfs.h"
#include "PackVars.h"   // uses directMessage() from bzfs.h
#include "bzfsPython.h"


#if defined(_WIN32)
#define popen _popen
#define pclose _pclose
#endif

#include "PlayerInfo.h"

tmCustomSlashCommandMap customCommands;

class MsgCommand : ServerCommand
{
public:
    MsgCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class ServerQueryCommand : ServerCommand
{
public:
    ServerQueryCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class PartCommand : ServerCommand
{
public:
    PartCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class QuitCommand : ServerCommand
{
public:
    QuitCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class UpTimeCommand : ServerCommand
{
public:
    UpTimeCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class PasswordCommand : ServerCommand
{
public:
    PasswordCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class SetCommand : ServerCommand
{
public:
    SetCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class ResetCommand : ServerCommand
{
public:
    ResetCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class GameOverCommand : ServerCommand
{
public:
    GameOverCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class CountdownCommand : ServerCommand
{
public:
    CountdownCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class FlagCommand : ServerCommand
{
public:
    FlagCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class JitterWarnCommand : ServerCommand
{
public:
    JitterWarnCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class JitterDropCommand : ServerCommand
{
public:
    JitterDropCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class LagWarnCommand : ServerCommand
{
public:
    LagWarnCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class LagDropCommand : ServerCommand
{
public:
    LagDropCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class LagStatCommand : ServerCommand
{
public:
    LagStatCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class IdleStatCommand : ServerCommand
{
public:
    IdleStatCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class IdleTimeCommand : ServerCommand
{
public:
    IdleTimeCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class HandicapCommand : ServerCommand
{
public:
    HandicapCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class FlagHistoryCommand : ServerCommand
{
public:
    FlagHistoryCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class IdListCommand : ServerCommand
{
public:
    IdListCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class PacketLossWarnCommand : public ServerCommand
{
public:
    PacketLossWarnCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};


class PacketLossDropCommand : public ServerCommand
{
public:
    PacketLossDropCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class PlayerListCommand : ServerCommand
{
public:
    PlayerListCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class ReportCommand : ServerCommand
{
public:
    ReportCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class HelpCommand : ServerCommand
{
public:
    HelpCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class SendHelpCommand : ServerCommand
{
public:
    SendHelpCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class GroupListCommand : ServerCommand
{
public:
    GroupListCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class ShowGroupCommand : ServerCommand
{
public:
    ShowGroupCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class ShowPermsCommand : ServerCommand
{
public:
    ShowPermsCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class GroupPermsCommand : ServerCommand
{
public:
    GroupPermsCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class SetGroupCommand : ServerCommand
{
public:
    SetGroupCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class RemoveGroupCommand : ServerCommand
{
public:
    RemoveGroupCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class ReloadCommand : ServerCommand
{
public:
    ReloadCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class PollCommand : ServerCommand
{
public:
    PollCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class VoteCommand : ServerCommand
{
public:
    VoteCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class VetoCommand : ServerCommand
{
public:
    VetoCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class ViewReportCommand : ServerCommand
{
public:
    ViewReportCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class ClientQueryCommand : ServerCommand
{
public:
    ClientQueryCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class DateTimeCommand : ServerCommand
{
public:
    DateTimeCommand();
    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
protected:
    DateTimeCommand(std::string _commandName) : ServerCommand(_commandName, "- display current server time") {};
};

class DateCommand : DateTimeCommand
{
public:
    DateCommand();
};

class TimeCommand : DateTimeCommand
{
public:
    TimeCommand();
};

class RecordCommand : ServerCommand
{
public:
    RecordCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class ReplayCommand : ServerCommand
{
public:
    ReplayCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class SayCommand : ServerCommand
{
public:
    SayCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class CmdList : ServerCommand
{
public:
    CmdList();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class CmdHelp : ServerCommand
{
public:
    CmdHelp();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class ModCountCommand : ServerCommand
{
public:
    ModCountCommand();

    virtual bool operator() (const char *commandLine,
                             GameKeeper::Player *playerData);
};

class DebugCommand : ServerCommand
{
public:
    DebugCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};


class OwnerCommand : ServerCommand
{
public:
    OwnerCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class LoadPyCommand : ServerCommand
{
public:
    LoadPyCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

class UnloadPyCommand : ServerCommand
{
public:
    UnloadPyCommand();

    virtual bool operator() (const char    *commandLine,
                             GameKeeper::Player *playerData);
};

static MsgCommand     msgCommand;
static ServerQueryCommand serverQueryCommand;
static PartCommand    partCommand;
static QuitCommand    quitCommand;
static UpTimeCommand      upTimeCommand;
static PasswordCommand    passwordCommand;
static SetCommand     setCommand;
static ResetCommand       resetCommand;
ShutdownCommand       shutdownCommand;  // used by the API
SuperkillCommand      superkillCommand; // used by the API
static GameOverCommand    gameOverCommand;
static CountdownCommand   countdownCommand;
static FlagCommand    flagCommand;
static LagWarnCommand     lagWarnCommand;
static LagDropCommand     lagDropCommand;
static JitterWarnCommand  jitterWarnCommand;
static JitterDropCommand  jitterDropCommand;
static PacketLossWarnCommand  packetLossWarnCommand;
static PacketLossDropCommand  packetLossDropCommand;
static LagStatCommand     lagStatCommand;
static IdleStatCommand    idleStatCommand;
static IdleTimeCommand    idleTimeCommand;
static HandicapCommand    handicapCommand;
static FlagHistoryCommand flagHistoryCommand;
static IdListCommand      idListCommand;
static PlayerListCommand  playerListCommand;
static ReportCommand      ReportCommand;
static HelpCommand    helpCommand;
static SendHelpCommand    sendHelpCommand;
static GroupListCommand   groupListCommand;
static ShowGroupCommand   showGroupCommand;
static ShowPermsCommand   showPermsCommand;
static GroupPermsCommand  groupPermsCommand;
static SetGroupCommand    setGroupCommand;
static RemoveGroupCommand removeGroupCommand;
static ReloadCommand      reloadCommand;
static PollCommand    pollCommand;
static VoteCommand    voteCommand;
static VetoCommand    vetoCommand;
static ViewReportCommand  viewReportCommand;
static ClientQueryCommand clientQueryCommand;
static DateCommand    dateCommand;
static TimeCommand    timeCommand;
static RecordCommand      recordCommand;
static ReplayCommand      replayCommand;
static SayCommand     sayCommand;
static CmdList        cmdList;
static CmdHelp        cmdHelp;
static ModCountCommand    modCountCommand;
static DebugCommand       debugCommand;
static OwnerCommand       ownerCommand;
static LoadPyCommand      loadPyCommand;
static UnloadPyCommand    unloadPyCommand;

CmdHelp::CmdHelp()           : ServerCommand("") {} // fake entry
CmdList::CmdList()           : ServerCommand("/?",
            "- display the list of server-side commands") {}
MsgCommand::MsgCommand()         : ServerCommand("/msg",
            "<nick> text - Send text message to nick") {}
ServerQueryCommand::ServerQueryCommand() : ServerCommand("/serverquery",
            "- show the server version") {}
PartCommand::PartCommand()       : ServerCommand("/part",
            "[message] - leave the game with a parting message") {}
QuitCommand::QuitCommand()       : ServerCommand("/quit",
            "[message] - leave the game with a parting message, and close the client") {}
UpTimeCommand::UpTimeCommand()       : ServerCommand("/uptime",
            "- show the server's uptime") {}
PasswordCommand::PasswordCommand()       : ServerCommand("/password",
            "<passwd> - become an administrator with <passwd>") {}
SetCommand::SetCommand()         : ServerCommand("/set",
            "[ var [ value ] ] - set BZDB variable to value, or display variables") {}
ResetCommand::ResetCommand()         : ServerCommand("/reset",
            "- reset the BZDB variables") {}
ShutdownCommand::ShutdownCommand()       : ServerCommand("/shutdownserver",
            "- kill the server") {}
SuperkillCommand::SuperkillCommand()     : ServerCommand("/superkill",
            "- kick all of the players") {}
GameOverCommand::GameOverCommand()       : ServerCommand("/gameover",
            "- end the current game") {}
CountdownCommand::CountdownCommand()     : ServerCommand("/countdown",
            "- start the countdown sequence for a timed game") {}
FlagCommand::FlagCommand()       : ServerCommand("/flag",
            "<reset|up|show> - reset, remove or show the flags") {}
LagWarnCommand::LagWarnCommand()     : ServerCommand("/lagwarn",
            "[milliseconds] - display or set the maximum allowed lag time") {}
LagDropCommand::LagDropCommand()     : ServerCommand("/lagdrop",
            "[count] - display or set the number of lag warnings before a player is kicked") {}
JitterWarnCommand::JitterWarnCommand()   : ServerCommand("/jitterwarn",
            "[milliseconds] - display or set the maximum allowed jitter time") {}
JitterDropCommand::JitterDropCommand()   : ServerCommand("/jitterdrop",
            "[count] - display or set the number of jitter warnings before a player is kicked") {}
PacketLossWarnCommand::PacketLossWarnCommand()     : ServerCommand("/packetlosswarn",
            "<%> - change the maximum allowed packetloss") {}
PacketLossDropCommand::PacketLossDropCommand()   : ServerCommand("/packetlossdrop",
            "<count> - display or set the number of packetloss warnings before a player is kicked") {}
LagStatCommand::LagStatCommand()     : ServerCommand("/lagstats",
            "- list network delays, jitter and number of lost resp. out of order packets by player") {}
IdleStatCommand::IdleStatCommand()       : ServerCommand("/idlestats",
            "- display the idle time in seconds for each player") {}
IdleTimeCommand::IdleTimeCommand()       : ServerCommand("/idletime",
            "[seconds] - display or set the idle time") {}
HandicapCommand::HandicapCommand()   : ServerCommand("/handicap",
            "- list handicap values by player") {}
FlagHistoryCommand::FlagHistoryCommand() : ServerCommand("/flaghistory",
            "- list what flags players have grabbed in the past") {}
IdListCommand::IdListCommand()       : ServerCommand("/idlist",
            "- list player BZIDs") {}
PlayerListCommand::PlayerListCommand()   : ServerCommand("/playerlist",
            "- list player slots, names and IP addresses") {}
ReportCommand::ReportCommand()       : ServerCommand("/report",
            "<message> - write a message to the server administrator") {}
HelpCommand::HelpCommand()       : ServerCommand("/help",
            "<help page> - display the specified help page") {}
SendHelpCommand::SendHelpCommand()       : ServerCommand("/sendhelp",
            "<#slot|PlayerName|\"Player Name\"> <help page> - send the specified help page to a user") {}
GroupListCommand::GroupListCommand()     : ServerCommand("/grouplist",
            "- list the available user groups") {}
ShowGroupCommand::ShowGroupCommand()     : ServerCommand("/showgroup",
            "[callsign] - list the groups that a registered user is a member of") {}
ShowPermsCommand::ShowPermsCommand()     : ServerCommand("/showperms",
            "[callsign] - list the permissions that a user has been granted") {}
GroupPermsCommand::GroupPermsCommand()   : ServerCommand("/groupperms",
            "[group] - list the permissions for each group") {}
SetGroupCommand::SetGroupCommand()       : ServerCommand("/setgroup",
            "<callsign> <group> - add the user to the specified group") {}
RemoveGroupCommand::RemoveGroupCommand() : ServerCommand("/removegroup",
            "<callsign> <group> - remove a user from a group") {}
ReloadCommand::ReloadCommand()       : ServerCommand("/reload",
            "[all|groups|users|bans|helpfiles|badwords] - reload groups, users, help files, and/or bad words") {}
PollCommand::PollCommand()       : ServerCommand("/poll",
            "<ban|kick|kill|vote|veto> <callsign> - interact and make requests of the bzflag voting system") {}
VoteCommand::VoteCommand()       : ServerCommand("/vote",
            "<yes|no> - place a vote in favor or in opposition to the poll") {}
VetoCommand::VetoCommand()       : ServerCommand("/veto",
            "- will cancel the poll if there is one active") {}
ViewReportCommand::ViewReportCommand()   : ServerCommand("/viewreports",
            "[pattern] - view the server's report file") {}
ClientQueryCommand::ClientQueryCommand() : ServerCommand("/clientquery",
            "[callsign] - retrieve client version info from all users, or just CALLSIGN if given") {}
RecordCommand::RecordCommand()       : ServerCommand("/record",
            "[start|stop|size|list|rate..] - manage the bzflag record system") {}
ReplayCommand::ReplayCommand()       : ServerCommand("/replay",
            "[ list [-t|-n] | load <filename|#index> | loop | play | skip [+/-seconds] | stats ] - interact with recorded files") {}
SayCommand::SayCommand()         : ServerCommand("/say",
            "[message] - generate a public message sent by the server") {}
DateCommand::DateCommand()       : DateTimeCommand("/date") {}
TimeCommand::TimeCommand()       : DateTimeCommand("/time") {}
ModCountCommand::ModCountCommand() : ServerCommand("/modcount",
            "[+-seconds] - adjust countdown (if any)") {}
DebugCommand::DebugCommand()         : ServerCommand("/serverdebug",
            "[value] - set debug level or display the current setting") {}
OwnerCommand::OwnerCommand()         : ServerCommand("/owner",
            "display the server owner's BZBB name") {}
LoadPyCommand::LoadPyCommand()       : ServerCommand("/loadpython",
            "[arg] - load python file") {}
UnloadPyCommand::UnloadPyCommand()   : ServerCommand("/unloadpython",
            "[arg] - unload python file") {}


class NoDigit
{
public:
    bool operator() (char c)
    {
        return !isdigit(c);
    }
};



bool CmdList::operator() (const char*, GameKeeper::Player *playerData)
{
    int i;
    const int maxLineLen = 64;
    const int playerId = playerData->getIndex();

    if (!mapOfCommands)
    {
        sendMessage(ServerPlayer, playerId,
                    "No server commands are defined");  // should not happen
        return false;
    }

    // build a std::vector<> from the std::map<> of command names
    std::vector<const std::string*> commands;
    for (MapOfCommands::iterator it = mapOfCommands->begin(); it != mapOfCommands->end(); ++it)
    {
        const std::string& cmd = it->first;
        if (cmd[0] != '/')
        {
            continue; // ignore any fake entries (ex: CmdHelp)
        }
        else
            commands.push_back(&cmd);
    }
    const int cmdCount = (int)commands.size();

    // get the maximum length
    unsigned int maxCmdLen = 0;
    for (i = 0; i < cmdCount; i++)
    {
        if (commands[i]->size() > maxCmdLen)
            maxCmdLen = commands[i]->size();
    }
    maxCmdLen += 2; // add some padding

    // message generation variables
    char buffer[MessageLen];
    char* cptr = buffer;
    char format[8];
    snprintf(format, 8, "%%-%is", maxCmdLen);

    // formatting parameters
    const int cols = (maxLineLen / maxCmdLen);
    const int rows = ((cmdCount + (cols - 1)) / cols);

    for (int row = 0; row < rows; row++)
    {
        cptr = buffer;
        for (int col = 0; col < cols; col++)
        {
            const int index = (col * rows) + row;
            if (index >= cmdCount)
                break;
            sprintf(cptr, format, commands[index]->c_str());
            cptr += maxCmdLen;
        }
        sendMessage(ServerPlayer, playerId, buffer);
    }

    return true;
}


bool CmdHelp::operator() (const char     *message,
                          GameKeeper::Player *playerData)
{
    const int t = playerData->getIndex();
    if (!mapOfCommands)
    {
        sendMessage(ServerPlayer, t,
                    "No server commands are defined");  // should not happen
        return false;
    }

    int i;
    for (i = 0; message[i] && !isspace(message[i]); i++)
        ;
    if (!i)
        return false;
    i--;
    bool listOnly;
    if (message[i] == '?')
        listOnly = true;
    else if (message[i] == '*')
        listOnly = false;
    else
        return false;

    std::string commandToken(message, i);

    bool none = true;
    unsigned int matching = 0;
    for (MapOfCommands::iterator it = mapOfCommands->begin(); it != mapOfCommands->end(); ++it)
    {
        std::string master = it->first;
        master.resize(i);
        if (master == commandToken)
        {
            matching++;
            none = false;
        }
    }
    if (none)
        sendMessage(ServerPlayer, t,
                    ("No command starting with " + commandToken).c_str());
    else
        for (MapOfCommands::iterator it = mapOfCommands->begin(); it != mapOfCommands->end(); ++it)
        {
            std::string master = it->first;
            master.resize(i);
            if (master == commandToken)
            {
                if (matching > 1 || listOnly)
                    sendMessage(ServerPlayer, t, it->second->getHelp().c_str());
                else
                {
                    std::string commandLine = it->first + (message + i + 1);
                    return (*(it->second))(commandLine.c_str(), playerData);
                }
            }
        }
    return true;
}


bool UpTimeCommand::operator() (const char   *,
                                GameKeeper::Player *playerData)
{
    float rawTime;
    int t = playerData->getIndex();
    char reply[MessageLen] = {0};

    rawTime = float(TimeKeeper::getCurrent() - TimeKeeper::getStartTime());
    snprintf(reply, MessageLen, "%s.", TimeKeeper::printTime(rawTime).c_str());
    sendMessage(ServerPlayer, t, reply);
    return true;
}

bool ServerQueryCommand::operator() (const char  *,
                                     GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    logDebugMessage(2,"Server query requested by %s [%d]\n",
                    playerData->player.getCallSign(), t);

    sendMessage(ServerPlayer, t,
                TextUtils::format("BZFS Version: %s", getAppVersion()).c_str());
    return true;
}


bool PartCommand::operator() (const char     *message,
                              GameKeeper::Player *playerData)
{
    std::string byeStatement = "";

    if (strlen(message) > 5)
    {
        if (!TextUtils::isWhitespace(*(message+5)))
        {
            char reply[MessageLen] = {0};
            snprintf(reply, MessageLen, "Unknown command [%s]", message);
            sendMessage(ServerPlayer, playerData->getIndex(), reply);
            return true;
        }
        byeStatement = message + 6;
    }

    if (byeStatement[0] != '\0')
    {
        // check talk permission for the bye message
        if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::talk))
        {
            sendMessage(ServerPlayer, playerData->getIndex(),
                        "You do not have permission to send a part message");
        }
        else
        {
            std::string message2;
            message2 = TextUtils::format("%s has left (\"%s\") ",
                                         playerData->player.getCallSign(),  byeStatement.c_str());

            logDebugMessage(2,"%s has quit with the message \"%s\"\n", playerData->player.getCallSign(), byeStatement.c_str());
            sendMessage(ServerPlayer, AllPlayers, message2.c_str());
        }
    }

    // now to kick the player
    int t = playerData->getIndex();
    removePlayer(t, byeStatement.c_str());
    return true;
}


bool QuitCommand::operator() (const char     *message,
                              GameKeeper::Player *playerData)
{
    std::string byeStatement = "";

    if (strlen(message) > 5)
    {
        if (!TextUtils::isWhitespace(*(message+5)))
        {
            char reply[MessageLen] = {0};
            snprintf(reply, MessageLen, "Unknown command [%s]", message);
            sendMessage(ServerPlayer, playerData->getIndex(), reply);
            return true;
        }
        byeStatement = message + 6;
    }

    if (byeStatement[0] != '\0')
    {
        // check talk permission for the bye message
        if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::talk))
        {
            sendMessage(ServerPlayer, playerData->getIndex(),
                        "You do not have permission to send a quit message");
        }
        else
        {
            std::string message2;
            message2 = TextUtils::format("%s has quit (\"%s\") ",
                                         playerData->player.getCallSign(),  byeStatement.c_str());

            logDebugMessage(2,"%s has quit with the message \"%s\"\n", playerData->player.getCallSign(), byeStatement.c_str());
            sendMessage(ServerPlayer, AllPlayers, message2.c_str());
        }
    }

    // now to kick the player
    int t = playerData->getIndex();
    removePlayer(t, byeStatement.c_str());
    return true;
}


bool MsgCommand::operator() (const char  *message,
                             GameKeeper::Player *playerData)
{
    int from = playerData->getIndex();
    int to= -1;

    std::string message2;
    size_t callsignStart=0, callsignEnd=0, messageStart=0;

    // start from after "/msg"
    std::string arguments = &message[4];
    std::string recipient = std::string("");

    // skip any leading whitespace
    callsignStart = 0;
    while ((callsignStart < arguments.size()) &&
            (isspace(arguments[callsignStart])))
        callsignStart++;

    // make sure there was _some_ whitespace after /msg
    if (callsignStart == 0)
    {
        sendMessage(ServerPlayer, from, "Usage: /msg \"some callsign\" some message");
        return true;
    }

    // find the player name, optionally quoted
    if (arguments[callsignStart] == '"')
    {
        callsignStart++;

        // find the trailing quote
        bool foundQuote = false;
        callsignEnd = callsignStart;
        while ((callsignEnd + 1 < arguments.size()) &&
                (!foundQuote))
        {
            callsignEnd++;
            if (arguments[callsignEnd] == '"')
            {
                foundQuote = true;
                messageStart = callsignEnd + 1;
                callsignEnd--;
            }
        }

        // no quote means a mismatch
        if (!foundQuote)
        {
            sendMessage(ServerPlayer, from, "Quote mismatch?");
            sendMessage(ServerPlayer, from, "Usage: /msg \"some callsign\" some message");
            return true;
        }

    }
    else
    {
        // unquoted callsign

        // find the first matching name (not the longest for sake of performance)
        bool foundCallsign = false;
        callsignEnd = callsignStart;
        while ((callsignEnd + 1 < arguments.size()) &&
                (!foundCallsign))
        {
            callsignEnd++;
            if (!isspace(arguments[callsignEnd]))
                continue;

            // we have a space
            recipient = arguments.substr(callsignStart, callsignEnd - callsignStart);
            messageStart = callsignEnd;

            to = GameKeeper::Player::getPlayerIDByName(recipient);
            if (to < curMaxPlayers)
            {
                callsignEnd--;
                foundCallsign = true;
            }
        }
    }

    recipient = arguments.substr(callsignStart, callsignEnd - callsignStart + 1);

    if (recipient[0] == '>')
    {
        // /msg >admin sends on admin channel, /msg >team on team channel
        recipient.erase(0,1);
        if (TextUtils::toupper(recipient) == "ADMIN")
            to = AdminPlayers;
        else if (TextUtils::toupper(recipient) == "TEAM")
            to = FirstTeam - (int)playerData->player.getTeam();
    }
    else
    {
        to = GameKeeper::Player::getPlayerIDByName(recipient);

        // valid callsign
        if ((to < 0) || (to >= curMaxPlayers))
        {
            message2 = TextUtils::format("\"%s\" is not here.  No such callsign.", recipient.c_str());
            sendMessage(ServerPlayer, from, message2.c_str());
            return true;
        }
    }

    // make sure there is something to send
    if ((messageStart >= arguments.size() - 1) || (messageStart == 0))
    {
        // found player, but nothing to send
        message2 = TextUtils::format("No text to send to \"%s\".", recipient.c_str());
        sendMessage(ServerPlayer, from, message2.c_str());
        return true;
    }

    // send the message
    sendPlayerMessage(playerData, to, arguments.c_str() + messageStart + 1);

    // event handler goodness
    bz_ChatEventData_V2 chatData;
    chatData.from = playerData->getIndex();
    chatData.to = to;

    chatData.messageType = eChatMessage;
    chatData.message = arguments.c_str() + messageStart + 1;

    // send any events that want to watch the chat
    // everyone
    worldEventManager.callEvents(bz_eFilteredChatMessageEvent,&chatData);

    return true;
}


bool PasswordCommand::operator() (const char     *message,
                                  GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (playerData->accessInfo.passwordAttemptsMax())
    {
        logDebugMessage(1,"\"%s\" (%s) has attempted too many /password tries\n",
                        playerData->player.getCallSign(),
                        playerData->netHandler->getTargetIP());
        sendMessage(ServerPlayer, t, "Too many attempts");
    }
    else
    {
        if (clOptions->password.length() != 0 && strlen(message) > 10
                && strcmp(message + 10, clOptions->password.c_str()) == 0)    // skip past "/password "
        {
            playerData->accessInfo.setOperator();
            sendPlayerInfo();
            sendMessage(ServerPlayer, t, "You are now an administrator!");
            // Notify plugins of player authentication change
            bz_PlayerAuthEventData_V1 commandData;
            commandData.playerID = t;
            worldEventManager.callEvents(bz_ePlayerAuthEvent, &commandData);
        }
        else
        {
            sendMessage(ServerPlayer, t, "Wrong Password!");
            std::string temp;
            temp = playerData->player.getCallSign()
                   + std::string(" has tried to become administrator with bad password");
            sendMessage(ServerPlayer, AdminPlayers, temp.c_str());
        }
    }
    return true;
}


bool SetCommand::operator() (const char  *message,
                             GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    int setvar = playerData->accessInfo.hasPerm(PlayerAccessInfo::setVar) ? 1 : 0;
    int setall = playerData->accessInfo.hasPerm(PlayerAccessInfo::setAll) ? 1 : 0;
    char message2[MessageLen];
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::setVar)
            && !playerData->accessInfo.hasPerm(PlayerAccessInfo::setAll))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the set command");
        logDebugMessage(3,"set failed by %s, setvar=%d, setall=%d\n",playerData->player.getCallSign(),setvar,setall);
        return true;
    }
    if (Replay::enabled())
    {
        sendMessage(ServerPlayer, t, "You can't /set variables in replay mode");
        return true;
    }
    logDebugMessage(3,"set executed by %s, setvar=%d, setall=%d\n",playerData->player.getCallSign(),setvar,setall);
    std::string command = (message + 1);
    // we aren't case sensitive but CMDMGR is
    for (int i = 0; i < 3 /*"set"*/; ++i)
        command[i] = tolower(command[i]);

    bool  cmdError = false;

    std::string cmdReturn = CMDMGR.run(command,&cmdError);
    if (!cmdError)
    {
        std::string errMsg = "/set failed, reason: ";
        errMsg += cmdReturn;

        sendMessage(ServerPlayer, t, errMsg.c_str());
        return true;
    }

    sendMessage(ServerPlayer, t, cmdReturn.c_str());
    snprintf(message2, MessageLen, "Variable Modification Notice by %s of %s",
             playerData->player.getCallSign(), command.c_str());
    sendMessage(ServerPlayer, AllPlayers, message2);
    return true;
}


bool ResetCommand::operator() (const char    *message,
                               GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::setVar)
            && !playerData->accessInfo.hasPerm(PlayerAccessInfo::setAll))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the reset command");
        return true;
    }
    if (Replay::enabled())
    {
        sendMessage(ServerPlayer, t, "You can't /reset variables in replay mode");
        return true;
    }
    std::string command = (message + 1);
    bool cmdError = false;
    // we aren't case sensitive but CMDMGR is
    for (int i = 0; i < 5 /*"reset"*/; ++i)
        command[i] = tolower(command[i]);
    sendMessage(ServerPlayer, t, CMDMGR.run(command, &cmdError).c_str());

    if (!cmdError)
    {
        char message2[MessageLen];
        snprintf(message2, MessageLen, "Variable Reset Notice by %s of %s",
                 playerData->player.getCallSign(), command.c_str());
        sendMessage(ServerPlayer, AdminPlayers, message2);
    }

    return true;
}


bool ShutdownCommand::operator() (const char     *,
                                  GameKeeper::Player *playerData)
{
    // If no playerData - don't perform permission check, since it is probably the API
    if (playerData)
    {
        int t = playerData->getIndex();
        if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::shutdownServer))
        {
            sendMessage(ServerPlayer, t, "You do not have permission to run the shutdown command");
            return true;
        }
    }
    done = true;
    return true;
}


bool SuperkillCommand::operator() (const char    *,
                                   GameKeeper::Player *playerData)
{
    // If no playerData - dont perfom permission check, since it is probably the API
    int t = ServerPlayer;

    if (playerData)
    {
        t = playerData->getIndex();
        if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::superKill))
        {
            sendMessage(ServerPlayer, t, "You do not have permission to run the superkill command");
            return true;
        }
    }
    for (int i = 0; i < curMaxPlayers; i++)
        removePlayer(i, "/superkill");
    if (!gameOver)
    {
        gameOver = true;
        // fire off a game end event
        bz_GameStartEndEventData_V2   gameData;
        gameData.eventType = bz_eGameEndEvent;
        gameData.duration = clOptions->timeLimit;
        gameData.playerID = t;
        gameData.gameOver = true;
        worldEventManager.callEvents(bz_eGameEndEvent,&gameData);
    }
    if (clOptions->timeManualStart)
        countdownActive = false;
    return true;
}


bool GameOverCommand::operator() (const char     *,
                                  GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::endGame))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the gameover command");
        return true;
    }

    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, t);
    buf = nboPackUShort(buf, uint16_t(NoTeam));
    broadcastMessage(MsgScoreOver, (char*)buf - (char*)bufStart, bufStart);
    gameOver = true;
    if (clOptions->timeManualStart)
    {
        countdownActive = false;
        countdownPauseStart = TimeKeeper::getNullTime();
        clOptions->countdownPaused = false;
    }

    // fire off a game end event
    bz_GameStartEndEventData_V2   gameData;
    gameData.eventType = bz_eGameEndEvent;
    gameData.duration = clOptions->timeLimit;
    gameData.playerID = t;
    gameData.gameOver = true;
    worldEventManager.callEvents(bz_eGameEndEvent,&gameData);

    return true;
}


bool CountdownCommand::operator() (const char    * message,
                                   GameKeeper::Player *playerData)
{
    // /countdown starts timed game, if start is manual, everyone is allowed to
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::countdown))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the countdown command");
        return true;
    }
    else if (!clOptions->timeManualStart)
    {
        sendMessage(ServerPlayer, t, "This server was not configured for manual clock countdowns");
        return true;
    }

    // if the timelimit is not set .. don't countdown
    if (clOptions->timeLimit > 1.0f)
    {
        std::vector<std::string> parts = TextUtils::tokenize(message, " \t",2);

        if (parts.size() > 1)
        {
            // we have an argument
            if (parts[1] == "pause")
            {
                // pause the countdown
                if (!countdownActive)
                {
                    sendMessage(ServerPlayer, t, "There is no active game to pause");
                    return true;
                }
                else if (clOptions->countdownPaused)
                {
                    sendMessage(ServerPlayer, t, "The game is already paused");
                    return true;
                }

                pauseCountdown(t);
                return true;
            }
            else if (parts[1] == "resume")
            {
                // resume countdown if it was paused before
                if (!clOptions->countdownPaused)
                {
                    sendMessage(ServerPlayer, t, "The game is not paused");
                    return true;
                }
                resumeCountdown(t);
                return true;
            }
            else if (parts[1] == "cancel")
            {
                if (countdownDelay <= 0)
                    sendMessage(ServerPlayer, t, "There is no running countdown to cancel");
                else
                    cancelCountdown(t);

                return true;
            }
            else
            {
                if (countdownDelay > 0)
                {
                    sendMessage(ServerPlayer, t, "There is a countdown already in progress");
                    return true;
                }

                // so it's the countdown delay? else tell the player how to use /countdown
                std::istringstream timespec(message+10);
                timespec >> countdownDelay;
                if (timespec.fail())
                {
                    countdownDelay = -1;
                    sendMessage(ServerPlayer, t, "Usage: /countdown [<seconds>|pause|resume|cancel]");
                    return true;
                }
            }
        }
        else
        {
            if (countdownDelay > 0)
            {
                sendMessage(ServerPlayer, t, "There is a countdown already in progress");
                return true;
            }

            countdownDelay = 10;
        }

        // cancel here if a game is already running
        if (countdownActive)
        {
            sendMessage(ServerPlayer, t, "A game is already in progress");
            countdownDelay = -1;
            return true;
        }

        // limit/sanity check
        const int max_delay = 120;
        if (countdownDelay > max_delay)
        {
            sendMessage(ServerPlayer, t, TextUtils::format("Countdown set to %d instead of %d", max_delay, countdownDelay).c_str());
            countdownDelay = max_delay;
        }
        else if (countdownDelay < 0)
        {
            sendMessage(ServerPlayer, t, TextUtils::format("Countdown set to 0 instead of %d", countdownDelay).c_str());
            countdownDelay = 0;
        }

        startCountdown(countdownDelay, clOptions->timeLimit, t);
    }
    else
    {
        sendMessage(ServerPlayer, AllPlayers, "Team scores reset.");
        sendMessage(ServerPlayer, t, "The server is not configured for timed matches.");
    }

    resetTeamScores();

    return true;
}


static void flagCommandHelp(int t)
{
    sendMessage(ServerPlayer, t, "/flag up");
    sendMessage(ServerPlayer, t, "/flag show");
    sendMessage(ServerPlayer, t, "/flag reset <all|unused|team|#flagId|FlagAbbv> [noteam]");
    sendMessage(ServerPlayer, t, "/flag take <#slot|PlayerName|\"PlayerName\">");
    sendMessage(ServerPlayer, t,
                "/flag give <#slot|PlayerName|\"PlayerName\"> <#flagId|FlagAbbr> [force]");
    return;
}


static bool checkFlagMod(GameKeeper::Player* playerData)
{
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::flagMod) &&
            !playerData->accessInfo.hasPerm(PlayerAccessInfo::flagMaster))
    {
        sendMessage(ServerPlayer, playerData->getIndex(),
                    "You do not have the FlagMod permission");
        return false;
    }
    return true;
}


static bool checkFlagMaster(GameKeeper::Player* playerData)
{
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::flagMaster))
    {
        sendMessage(ServerPlayer, playerData->getIndex(),
                    "You do not have the FlagMaster permission");
        return false;
    }
    return true;
}


bool FlagCommand::operator() (const char     *message,
                              GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!checkFlagMod(playerData))
        return true;

    const char* msg = message + 6;
    while ((*msg != '\0') && isspace(*msg)) msg++; // eat whitespace

    if (strncasecmp(msg, "up", 2) == 0)
    {
        for (int i = 0; i < numFlags; i++)
        {
            FlagInfo &flag = *FlagInfo::get(i);
            if (flag.flag.type->flagTeam == ::NoTeam)
            {
                if (flag.flag.status == FlagOnTank)
                {
                    int player = flag.player;

                    sendDrop(flag);

                    // trigger the API event
                    bz_FlagDroppedEventData_V1 data;
                    data.playerID = player;
                    data.flagID = flag.getIndex();
                    data.flagType = flag.flag.type->flagAbbv.c_str();
                    memcpy(data.pos, flag.flag.position, sizeof(float)*3);

                    worldEventManager.callEvents(bz_eFlagDroppedEvent,&data);
                }
                flag.flag.status = FlagGoing;
                if (!flag.required)
                    flag.flag.type = Flags::Null;
                sendFlagUpdate(flag);
            }
        }
    }
    else if (strncasecmp(msg, "show", 4) == 0)
    {
        for (int i = 0; i < numFlags; i++)
        {
            char showMessage[MessageLen];
            FlagInfo::get(i)->getTextualInfo(showMessage);
            sendMessage(ServerPlayer, t, showMessage);
        }
    }
    else if (strncasecmp(msg, "reset", 5) == 0)
    {
        msg += 5;

        std::vector<std::string> argv = TextUtils::tokenize(msg, " \t", 0, true);
        if (argv.size() < 1)
        {
            flagCommandHelp(t);
            return true;
        }

        const char* command = argv[0].c_str();

        const bool keepTeamFlags = ((argv.size() > 1) &&
                                    strncasecmp(argv[1].c_str(), "noteam", 6) == 0);

        FlagType* ft = Flag::getDescFromAbbreviation(command);

        if (strncasecmp(command, "all", 3) == 0)
            bz_resetFlags(false, keepTeamFlags);
        else if (strncasecmp(command, "unused", 6) == 0)
            bz_resetFlags(true, keepTeamFlags);
        else if (strncasecmp(command, "team", 4) == 0)
        {
            // team flags
            for (int i = 0; i < numFlags; i++)
            {
                FlagInfo* fi = FlagInfo::get(i);
                if ((fi != NULL) && (fi->flag.type->flagTeam != NoTeam))
                {
                    const int playerIndex = fi->player;
                    if (playerIndex != -1)
                        sendDrop(*fi);
                    resetFlag(*fi);
                }
            }
        }
        else if (command[0] == '#')
        {
            if (!checkFlagMaster(playerData))
                return true;
            int fIndex = atoi(command + 1);
            FlagInfo* fi = FlagInfo::get(fIndex);
            if (fi != NULL)
            {
                const int playerIndex = fi->player;
                if (playerIndex != -1)
                    sendDrop(*fi);
                resetFlag(*fi);
            }
        }
        else if (ft != Flags::Null)
        {
            if (!checkFlagMaster(playerData))
                return true;
            // by flag abbreviation
            for (int i = 0; i < numFlags; i++)
            {
                FlagInfo* fi = FlagInfo::get(i);
                if ((fi != NULL) && (fi->flag.type == ft))
                {
                    const int playerIndex = fi->player;
                    if (playerIndex != -1)
                        sendDrop(*fi);
                    resetFlag(*fi);
                }
            }
        }
        else
        {
            flagCommandHelp(t);
            return true;
        }
    }
    else if (strncasecmp(msg, "take", 4) == 0)
    {
        if (!checkFlagMaster(playerData))
            return true;

        msg += 4;
        while ((*msg != '\0') && isspace(*msg)) msg++; // eat whitespace

        std::vector<std::string> argv = TextUtils::tokenize(msg, " \t", 0, true);
        if (argv.size() < 1)
        {
            flagCommandHelp(t);
            return true;
        }

        int pIndex = GameKeeper::Player::getPlayerIDByName(argv[0]);
        GameKeeper::Player* gkPlayer = GameKeeper::Player::getPlayerByIndex(pIndex);

        if (gkPlayer == NULL)
        {
            char buffer[MessageLen];
            snprintf(buffer, MessageLen,
                     "/flag drop: could not find player (%s)", msg);
            sendMessage(ServerPlayer, t, buffer);
            return true;
        }

        FlagInfo* fi = FlagInfo::get(gkPlayer->player.getFlag());
        if (fi != NULL)
        {
            resetFlag(*fi);
            char buffer[MessageLen];
            snprintf(buffer, MessageLen, "%s took flag %s/%i from %s",
                     playerData->player.getCallSign(),
                     fi->flag.type->flagAbbv.c_str(), fi->getIndex(),
                     gkPlayer->player.getCallSign());
            sendMessage(ServerPlayer, t, buffer);
            sendMessage(ServerPlayer, AdminPlayers, buffer);
        }
        else
        {
            char buffer[MessageLen];
            snprintf(buffer, MessageLen,
                     "/flag drop: player (%s) does not have a flag",
                     gkPlayer->player.getCallSign());
            sendMessage(ServerPlayer, t, buffer);
        }
    }
    else if (strncasecmp(msg, "give", 4) == 0)
    {
        if (!checkFlagMaster(playerData))
            return true;

        msg += 4;
        while ((*msg != '\0') && isspace(*msg)) msg++; // eat whitespace

        std::vector<std::string> argv = TextUtils::tokenize(msg, " \t", 0, true);
        if (argv.size() < 2)
        {
            flagCommandHelp(t);
            return true;
        }

        FlagInfo* fi = NULL;
        int pIndex = GameKeeper::Player::getPlayerIDByName(argv[0]);
        GameKeeper::Player* gkPlayer = GameKeeper::Player::getPlayerByIndex(pIndex);

        if (gkPlayer != NULL)
        {
            const bool force = ((argv.size() > 2) &&
                                strncasecmp(argv[2].c_str(), "force", 5) == 0);
            if (argv[1][0] == '#')
            {
                int fIndex = atoi(argv[1].c_str() + 1);
                fi = FlagInfo::get(fIndex);
                if ((fi != NULL) && ((fi->player >= 0) && !force))
                    fi = NULL;
            }
            else
            {
                FlagType* ft = Flag::getDescFromAbbreviation(argv[1].c_str());
                if (ft != Flags::Null)
                {
                    // find unused and forced candidates
                    FlagInfo* unused = NULL;
                    FlagInfo* forced = NULL;
                    for (int i = 0; i < numFlags; i++)
                    {
                        FlagInfo* fi2 = FlagInfo::get(i);
                        if ((fi2 != NULL) && (fi2->flag.type == ft))
                        {
                            forced = fi2;
                            if (fi2->player < 0)
                            {
                                unused = fi2;
                                break;
                            }
                        }
                    }
                    // see if we need to force it
                    if (unused != NULL)
                        fi = unused;
                    else if (forced != NULL)
                    {
                        if (force)
                            fi = forced;
                        else
                        {
                            sendMessage(ServerPlayer, t, "you may need to use the force");
                            return true;
                        }
                    }
                    else
                    {
                        sendMessage(ServerPlayer, t, "flag type not found");
                        return true;
                    }
                }
                else
                {
                    sendMessage(ServerPlayer, t, "bad flag type");
                    return true;
                }
            }
        }
        else
        {
            char buffer[MessageLen];
            snprintf(buffer, MessageLen,
                     "/flag give: could not find player (%s)", argv[0].c_str());
            sendMessage(ServerPlayer, t, buffer);
            return true;
        }

        if (gkPlayer && fi)
        {
            // do not give flags to dead players
            if (!gkPlayer->player.isAlive())
            {
                char buffer[MessageLen];
                snprintf(buffer, MessageLen,
                         "/flag give: player (%s) is not alive",
                         gkPlayer->player.getCallSign());
                sendMessage(ServerPlayer, t, buffer);
                return true;
            }

            // deal with the player's current flag
            const int flagId = gkPlayer->player.getFlag();
            if (flagId >= 0)
            {
                FlagInfo& currentFlag = *FlagInfo::get(flagId);
                if (currentFlag.flag.type->flagTeam != NoTeam)
                {
                    // drop team flags
                    dropFlag(currentFlag, gkPlayer->lastState.pos);
                }
                else
                {
                    // reset non-team flags
                    resetFlag(currentFlag);
                }
            }

            // deal with the flag's current player (for forced gives)
            if (fi->player >= 0)
            {
                GameKeeper::Player* fPlayer = GameKeeper::Player::getPlayerByIndex(fi->player);
                if (fPlayer)
                {
                    void *bufStart = getDirectMessageBuffer();
                    void *buf = nboPackUByte(bufStart, fi->player);
                    buf = fi->pack(buf);
                    broadcastMessage(MsgDropFlag, (char*)buf - (char*)bufStart, bufStart);
                    fPlayer->player.setFlag(-1);
                }
            }

            grabFlag(gkPlayer->getIndex(), *fi, false);

            // send the announcement
            char buffer[MessageLen];
            snprintf(buffer, MessageLen, "%s gave flag %s/%i to %s",
                     playerData->player.getCallSign(),
                     fi->flag.type->flagAbbv.c_str(), fi->getIndex(),
                     gkPlayer->player.getCallSign());
            sendMessage(ServerPlayer, t, buffer);
            sendMessage(ServerPlayer, AdminPlayers, buffer);
        }
    }
    else
        flagCommandHelp(t);

    return true;
}


bool LagWarnCommand::operator() (const char  *message,
                                 GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::lagwarn))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the lagwarn command");
        return true;
    }

    char reply[MessageLen] = {0};

    if (message[8] == ' ' && isdigit(message[9]))
    {
        // message is a parseable digit string
        // atoi() only requires 1st character to be digit
        const char *maxlag = message + 9;
        clOptions->lagwarnthresh = (float) (atoi(maxlag) / 1000.0);
        snprintf(reply, MessageLen, "lagwarn is now %d ms", int(clOptions->lagwarnthresh * 1000 + 0.5));
    }
    else if (message[8] == '\0' || strcmp (message + 8, " ") == 0)
    {
        // Command by itself, or with one trailing space
        snprintf(reply, MessageLen, "lagwarn is set to %d ms", int(clOptions->lagwarnthresh * 1000 + 0.5));
    }
    else
    {
        // arguments not parseable by atoi(); send syntax information
        snprintf(reply, MessageLen, "Syntax: /lagwarn [time]");
    }
    LagInfo::setThreshold(clOptions->lagwarnthresh,(float)clOptions->maxlagwarn);
    sendMessage(ServerPlayer, t, reply);
    return true;
}


bool LagDropCommand::operator() (const char      *message,
                                 GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::lagwarn))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the lagdrop command");
        return true;
    }

    char reply[MessageLen] = {0};

    if (message[8] == ' ' && isdigit(message[9]))
    {
        // message is a parseable digit string
        const char *maxwarn = message + 9;
        clOptions->maxlagwarn = atoi(maxwarn);
        snprintf(reply, MessageLen, "lagdrop is now %d", clOptions->maxlagwarn);
    }
    else if (message[8] == '\0' || strcmp (message + 8, " ") == 0)
    {
        // Command by itself, or with one trailing space
        snprintf(reply, MessageLen, "lagdrop is set to %d", clOptions->maxlagwarn);
    }
    else
    {
        // arguments not parseable by atoi(); send syntax information
        snprintf(reply, MessageLen, "Syntax: /lagdrop [num]");
    }

    LagInfo::setThreshold(clOptions->lagwarnthresh,(float)clOptions->maxlagwarn);
    sendMessage(ServerPlayer, t, reply);
    return true;
}


bool JitterWarnCommand::operator() (const char   *message,
                                    GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::jitterwarn))
    {
        sendMessage(ServerPlayer, t,
                    "You do not have permission to run the jitterwarn command");
        return true;
    }

    char reply[MessageLen] = {0};

    if (message[11] == ' ' && isdigit(message[12]))
    {
        const char *maxjit = message + 12;
        clOptions->jitterwarnthresh = (float) (atoi(maxjit) / 1000.0);
        snprintf(reply, MessageLen, "jitterwarn is now %d ms",
                 int(clOptions->jitterwarnthresh * 1000 + 0.5));
    }
    else if (message[11] == '\0' || strcmp (message + 11, " ") == 0)
    {
        snprintf(reply, MessageLen, "jitterwarn is set to %d ms",
                 int(clOptions->jitterwarnthresh * 1000 + 0.5));
    }
    else
    {
        // arguments not parseable by atoi(); send syntax information
        snprintf(reply, MessageLen, "Syntax: /jitterwarn [num]");
    }
    LagInfo::setJitterThreshold(clOptions->jitterwarnthresh,
                                (float)clOptions->maxjitterwarn);
    sendMessage(ServerPlayer, t, reply);
    return true;
}


bool JitterDropCommand::operator() (const char   *message,
                                    GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::jitterwarn))
    {
        sendMessage(ServerPlayer, t,
                    "You do not have permission to run the jitterdrop command");
        return true;
    }

    char reply[MessageLen] = {0};

    if (message[11] == ' ' && isdigit(message[12]))
    {
        const char *maxjittwarn = message + 12;
        clOptions->maxjitterwarn = atoi(maxjittwarn);
        snprintf(reply, MessageLen, "jitterdrop is now %d",
                 clOptions->maxjitterwarn);
    }
    else if (message[11] == '\0' || strcmp (message + 11, " ") == 0)
    {
        // Command by itself, or with one trailing space
        snprintf(reply, MessageLen, "jitterdrop is set to %d",
                 clOptions->maxjitterwarn);
    }
    else
    {
        // arguments not parseable by atoi(); send syntax information
        snprintf(reply, MessageLen, "Syntax: /jitterdrop [num]");
    }
    LagInfo::setJitterThreshold(clOptions->jitterwarnthresh,
                                (float)clOptions->maxjitterwarn);
    sendMessage(ServerPlayer, t, reply);
    return true;
}

bool PacketLossWarnCommand::operator() (const char  *message,
                                        GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::packetlosswarn))
    {
        sendMessage(ServerPlayer, t,
                    "You do not have permission to run the packetlosswarn command");
        return true;
    }

    char reply[MessageLen] = {0};
    if (message[15] == ' ' && isdigit(message[16]))
    {
        const char *maxloss = message + 16;
        clOptions->packetlosswarnthresh = (float) (atoi(maxloss) / 1000.0);
        snprintf(reply, MessageLen, "packetlosswarn is now %d%%",
                 int(clOptions->packetlosswarnthresh * 1000 + 0.5));
    }
    else if (message[15] == '\0' || strcmp (message + 15, " ") == 0)
    {
        snprintf(reply, MessageLen, "packetlosswarnthresh is set to %d%%",
                 int(clOptions->packetlosswarnthresh * 1000 + 0.5));
    }
    else
    {
        // arguments not parseable by atoi(); send syntax information
        snprintf(reply, MessageLen, "Syntax: /packetlosswarn [percent]");
    }
    LagInfo::setPacketLossThreshold(clOptions->packetlosswarnthresh,
                                    (float)clOptions->maxpacketlosswarn);
    sendMessage(ServerPlayer, t, reply);
    return true;
}

bool PacketLossDropCommand::operator() (const char  *message,
                                        GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::packetlosswarn))
    {
        sendMessage(ServerPlayer, t,
                    "You do not have permission to run the packetlossdrop command");
        return true;
    }

    char reply[MessageLen] = {0};
    if (message[15] == ' ' && isdigit(message[16]))
    {
        const char *maxlosswarn = message + 16;
        clOptions->maxpacketlosswarn = atoi(maxlosswarn);
        snprintf(reply, MessageLen, "packetlossdrop is now %d", clOptions->maxpacketlosswarn);
    }
    else if (message[15] == '\0' || strcmp (message + 15, " ") == 0)
    {
        // Command by itself, or with one trailing space
        snprintf(reply, MessageLen, "packetlossdrop is set to %d", clOptions->maxpacketlosswarn);
    }
    else
    {
        // arguments not parseable by atoi(); send syntax information
        snprintf(reply, MessageLen, "Syntax: /packetlossdrop [num]");
    }
    LagInfo::setPacketLossThreshold(clOptions->packetlosswarnthresh,
                                    (float)clOptions->maxpacketlosswarn);
    sendMessage(ServerPlayer, t, reply);
    return true;
}

bool lagCompare(const GameKeeper::Player *a, const GameKeeper::Player *b)
{
    if (a->player.isObserver() && !b->player.isObserver())
        return true;
    if (!a->player.isObserver() && b->player.isObserver())
        return false;
    return a->lagInfo.getLag() < b->lagInfo.getLag();
}

bool LagStatCommand::operator() (const char  *,
                                 GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::lagStats))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the lagstats command");
        return true;
    }
    // yeah, ok this is ugly but it works - curMaxPlayers is never >255
    GameKeeper::Player *sortedPlayer[256];
    int i = 0, j = 0;
    for (i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(i);
        if (p != NULL)
            sortedPlayer[j++] = p;
    }
    std::sort(sortedPlayer, sortedPlayer + j, lagCompare);

    char reply[MessageLen];
    for (i = 0; i < j; i++)
    {
        GameKeeper::Player *p = sortedPlayer[i];
        p->lagInfo.getLagStats(reply, playerData->accessInfo.isAdmin());
        if (reply[0])
            sendMessage(ServerPlayer, t, reply);
    }
    return true;
}


bool IdleStatCommand::operator() (const char     *,
                                  GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::idleStats))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the idlestats command");
        return true;
    }

    GameKeeper::Player *otherData;
    std::string reply;
    for (int i = 0; i < curMaxPlayers; i++)
    {
        otherData = GameKeeper::Player::getPlayerByIndex(i);
        if (!otherData)
            continue;
        reply = otherData->player.getIdleStat();
        if (reply != "")
            sendMessage(ServerPlayer, t, reply.c_str());
    }
    return true;
}


bool IdleTimeCommand::operator() (const char* message,
                                  GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();

    bool setting = false;
    message += commandName.size();
    while ((*message != '\0') && isspace(*message))
    {
        message++; // eat whitespace
    }

    float value;
    if (*message != '\0')
    {
        char* end;
        value = (float) strtod(message, &end);
        if (end != message)
            setting = true;
        else
        {
            std::string reply = "bad time format: ";
            reply += message;
            sendMessage(ServerPlayer, t, reply.c_str());
        }
    }

    if (setting)
    {
        if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::lagwarn))
        {
            sendMessage(ServerPlayer, t,
                        "You do not have permission to set the idletime");
            return true;
        }
        else
        {
            clOptions->idlekickthresh = value;
            char buf[256];
            snprintf(buf, 256, "idletime set to: %.1f seconds\n", value);
            sendMessage(ServerPlayer, t, buf);
        }
    }
    else
    {
        char buf[256];
        snprintf(buf, 256, "idletime is currently set to: %.1f seconds\n",
                 clOptions->idlekickthresh);
        sendMessage(ServerPlayer, t, buf);
    }
    return true;
}


bool HandicapCommand::operator() (const char     *,
                                  GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();

    if (clOptions->gameOptions & HandicapGameStyle)
    {
        const float maxhandicap = std::max(1.0f,
                                           BZDB.eval(StateDatabase::BZDB_HANDICAPSCOREDIFF));   // prevent division by zero below
        for (int i = 0; i < curMaxPlayers; i++)
        {
            GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(i);
            if (p != NULL && !p->player.isObserver())
            {
                char reply[MessageLen];
                snprintf(reply, MessageLen, "%-16s : %2d%%", p->player.getCallSign(),
                         int(100.0 * std::min((float)p->score.getHandicap(), maxhandicap) / maxhandicap + 0.5));
                sendMessage(ServerPlayer, t, reply);
            }
        }
    }
    else
        sendMessage(ServerPlayer, t, "Server does not use handicap mode.");
    return true;
}


bool FlagHistoryCommand::operator() (const char  *,
                                     GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::flagHistory))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the flaghistory command");
        return true;
    }

    char reply[MessageLen];
    for (int i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *otherData = GameKeeper::Player::getPlayerByIndex(i);
        if (otherData != NULL && otherData->player.isPlaying()
                && !otherData->player.isObserver())
        {
            snprintf(reply, MessageLen, "%-16s :%s", otherData->player.getCallSign(),
                     otherData->flagHistory.getStr().c_str());
            sendMessage(ServerPlayer, t, reply);
        }
    }
    return true;
}


bool IdListCommand::operator() (const char*, GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::playerList))
    {
        sendMessage(ServerPlayer, t,
                    "You do not have permission to run the /idlist command");
        return true;
    }

    GameKeeper::Player *gkPlayer;
    char buffer[MessageLen];
    for (int i = 0; i < curMaxPlayers; i++)
    {
        gkPlayer = GameKeeper::Player::getPlayerByIndex(i);
        if (gkPlayer && gkPlayer->player.isPlaying())
        {
            snprintf(buffer, MessageLen, "%-20s : %s",
                     gkPlayer->player.getCallSign(),
                     gkPlayer->getBzIdentifier().c_str());
            sendMessage(ServerPlayer, t, buffer);
        }
    }
    return true;
}


bool PlayerListCommand::operator() (const char   *,
                                    GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    GameKeeper::Player *otherData;
    char reply[MessageLen] = {0};

    if (playerData->accessInfo.hasPerm(PlayerAccessInfo::playerList))
    {
        for (int i = 0; i < curMaxPlayers; i++)
        {
            otherData = GameKeeper::Player::getPlayerByIndex(i);
            if (otherData && otherData->player.isPlaying())
            {
                if (playerData->accessInfo.hasPerm(PlayerAccessInfo::playerList))
                    snprintf(reply, MessageLen, "[%d]%-16s: %s",
                             otherData->getIndex(),
                             otherData->player.getCallSign(),
                             otherData->netHandler->getPlayerHostInfo().c_str());
                sendMessage(ServerPlayer, t, reply);
            }
        }
    }
    else
    {
        // allow all players to see their own connection, but not player ID
        snprintf(reply, MessageLen, "%-16s: %s",
                 playerData->player.getCallSign(),
                 playerData->netHandler->getPlayerHostInfo().c_str());
        sendMessage(ServerPlayer, t, reply);
    }
    return true;
}


bool ReportCommand::operator() (const char   *message,
                                GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::report))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the report command");
        return true;
    }

    if (strlen(message + 1) < 8)
        sendMessage(ServerPlayer, t, "Nothing reported");
    else
    {
        time_t now = time(NULL);
        char* timeStr = ctime(&now);
        std::string reportStr;
        reportStr = reportStr + timeStr + "Reported by " +
                    playerData->player.getCallSign() + ": " + (message + 8);
        if (clOptions->reportFile.size() > 0)
        {
            std::ofstream ofs(clOptions->reportFile.c_str(), std::ios::out | std::ios::app);
            ofs << reportStr << std::endl << std::endl;
        }
        if (clOptions->reportPipe.size() > 0)
        {
            FILE* pipeWrite = popen(clOptions->reportPipe.c_str(), "w");
            if (pipeWrite != NULL)
                fprintf(pipeWrite, "%s\n\n", reportStr.c_str());
            else
                logDebugMessage(1,"Couldn't write report to the pipe\n");
            pclose(pipeWrite);
        }
        if (clOptions->reportFile.size() == 0 && clOptions->reportPipe.size() == 0)
            sendMessage(ServerPlayer, t, "The report command is disabled on this server");
        else
        {
            std::string temp = std::string("**\"") + playerData->player.getCallSign() + "\" reports: " +
                               (message + 8);
            const std::vector<std::string> words = TextUtils::tokenize(temp, " \t");
            unsigned int cur = 0;
            const unsigned int wordsize = words.size();
            std::string temp2;
            while (cur != wordsize)
            {
                temp2.clear();
                while (cur != wordsize &&
                        (temp2.size() + words[cur].size() + 1 ) < (unsigned) MessageLen)
                {
                    temp2 += words[cur] + " ";
                    ++cur;
                }
                sendMessage (ServerPlayer, AdminPlayers, temp2.c_str());
            }
            logDebugMessage(1,"Player %s [%d] has filed a report (time: %s).\n",
                            playerData->player.getCallSign(), t, timeStr);

            sendMessage(ServerPlayer, t, "Your report has been filed. Thank you.");

            // Notify plugins of the report filed
            bz_ReportFiledEventData_V1 reportData;
            reportData.from = playerData->player.getCallSign();
            reportData.message = message;
            worldEventManager.callEvents(bz_eReportFiledEvent, &reportData);
        }
    }

    return true;
}



// return false if topic not found
static bool sendHelpTopic (int sendSlot, const char *helpTopic)
{
    bool foundChunk = false;
    const std::vector<std::string>& chunks = clOptions->textChunker.getChunkNames();

    for (int i = 0; i < (int)chunks.size() && (!foundChunk); i++)
    {
        if (TextUtils::compare_nocase(chunks[i], helpTopic) == 0)
        {
            const std::vector<std::string>* lines = clOptions->textChunker.getTextChunk(chunks[i]);
            if (lines != NULL)
            {
                for (int j = 0; j < (int)lines->size(); j++)
                    sendMessage(ServerPlayer, sendSlot, (*lines)[j].c_str());
                return true;
            }
        }
    }
    return false;
}


bool SendHelpCommand::operator() (const char *message, GameKeeper::Player *playerData)
{
    int sendFrom = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::sendHelp))
    {
        sendMessage(ServerPlayer, sendFrom,
                    "You do not have permission to run the sendhelp command");
        return true;
    }

    std::vector<std::string> argv = TextUtils::tokenize(message, " \t", 3, true);
    if (argv.size() < 3)
    {
        sendMessage(ServerPlayer, sendFrom,
                    "Syntax: /sendhelp <#slot | PlayerName | \"Player Name\"> <topic>");
        return true;
    }

    int sendTo = GameKeeper::Player::getPlayerIDByName(argv[1]);
    if ( sendTo < 0)
    {
        char errormessage[MessageLen];
        snprintf(errormessage, MessageLen, "player \"%s\" not found", argv[1].c_str());
        sendMessage(ServerPlayer, sendFrom, errormessage);
        return true;
    }

    if (sendHelpTopic (sendTo, argv[2].c_str()))
    {
        char reply[MessageLen] = {0};
        snprintf(reply, MessageLen, "This help (%s) was sent by %s (pgup/pgdn to scroll)",
                 argv[2].c_str(), GameKeeper::Player::getPlayerByIndex(sendFrom)->player.getCallSign());
        sendMessage(ServerPlayer, sendTo, reply);
        snprintf(reply, MessageLen, "Help topic %s was sent to %s by %s.",
                 argv[2].c_str(), GameKeeper::Player::getPlayerByIndex(sendTo)->player.getCallSign(),
                 GameKeeper::Player::getPlayerByIndex(sendFrom)->player.getCallSign());
        sendMessage(ServerPlayer, AdminPlayers, reply);
    }
    else
    {
        char reply[MessageLen] = {0};
        snprintf(reply, MessageLen, "Help command %s not found", argv[2].c_str());
        sendMessage(ServerPlayer, sendFrom, reply);
    }

    return true;
}


bool HelpCommand::operator() (const char *message, GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    char reply[MessageLen] = {0};

    if (strlen(message + 1) == 4)
    {
        const std::vector<std::string>& chunks = clOptions->textChunker.getChunkNames();
        sendMessage(ServerPlayer, t, "Available help pages (use /help <page>)");
        for (int i = 0; i < (int) chunks.size(); i++)
            sendMessage(ServerPlayer, t, chunks[i].c_str());
    }
    else
    {
        if ( !  sendHelpTopic (t, message + 6) )
        {
            snprintf(reply, MessageLen, "Help command %s not found", message + 6);
            sendMessage(ServerPlayer, t, reply);
        }
    }
    return true;
}


bool GroupListCommand::operator() (const char    *,
                                   GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    sendMessage(ServerPlayer, t, "Group List:");
    PlayerAccessMap::iterator itr;
    for (itr = groupAccess.begin(); itr != groupAccess.end(); ++itr)
        sendMessage(ServerPlayer, t, itr->first.c_str());
    return true;
}


bool ShowGroupCommand::operator() (const char* msg,
                                   GameKeeper::Player* playerData)
{
    int t = playerData->getIndex();

    std::string queryName = "";
    GameKeeper::Player* query = playerData;

    msg += commandName.size();
    std::vector<std::string> argv = TextUtils::tokenize(msg, " \t", 0, true);
    if (!argv.empty())
    {
        if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::showOthers))
        {
            sendMessage(ServerPlayer, t, "No permission!");
            return true;
        }
        queryName = TextUtils::toupper(argv[0]);

        // get the active player if possible
        int pIndex = GameKeeper::Player::getPlayerIDByName(queryName);
        query = GameKeeper::Player::getPlayerByIndex(pIndex);
    }
    else
    {
        if (!playerData->accessInfo.isVerified())
        {
            sendMessage(ServerPlayer, t, "You are not identified");
            return true;
        }
        else
            queryName = TextUtils::toupper(playerData->accessInfo.getName());
    }

    // once for global groups
    if (query)
    {
        PlayerAccessInfo &info = query->accessInfo;
        // FIXME remove local groups from this list. better yet unify the two.
        std::string line = "Global Groups (only extras) for ";
        line += queryName;
        line += ": ";
        std::vector<std::string>::iterator itr = info.groups.begin();
        while (itr != info.groups.end())
        {
            line += *itr;
            line += " ";
            ++itr;
        }
        while (line.size() >= (unsigned int)MessageLen)
        {
            sendMessage(ServerPlayer, t, line.substr(0, MessageLen - 1).c_str());
            line.erase(line.begin(), line.begin() + (MessageLen - 2));
        }
        sendMessage(ServerPlayer, t, line.c_str());
    }

    // once for local groups
    if (userExists(queryName))
    {
        PlayerAccessInfo &info = PlayerAccessInfo::getUserInfo(queryName);

        std::string line = "Local groups for ";
        line += queryName;
        line += ": ";
        std::vector<std::string>::iterator itr = info.groups.begin();
        while (itr != info.groups.end())
        {
            line += *itr;
            line += " ";
            ++itr;
        }
        while (line.size() >= (unsigned int)MessageLen)
        {
            sendMessage(ServerPlayer, t, line.substr(0, MessageLen - 1).c_str());
            line.erase(line.begin(), line.begin() + (MessageLen - 2));
        }
        sendMessage(ServerPlayer, t, line.c_str());
    }
    else
        sendMessage(ServerPlayer, t, "There is no user by that name");

    return true;
}


bool ShowPermsCommand::operator() (const char* msg,
                                   GameKeeper::Player* playerData)
{
    int t = playerData->getIndex();

    msg += commandName.size();
    GameKeeper::Player* query = playerData; // the asking player by default
    std::vector<std::string> argv = TextUtils::tokenize(msg, " \t", 0, true);
    if (!argv.empty())
    {
        if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::showOthers))
        {
            sendMessage(ServerPlayer, t, "No permission!");
            return true;
        }
        int pIndex = GameKeeper::Player::getPlayerIDByName(argv[0]);
        query = GameKeeper::Player::getPlayerByIndex(pIndex);
        if (query == NULL)
        {
            std::string warning = "Could not find player: ";
            warning += argv[0];
            sendMessage(ServerPlayer, t, warning.c_str());
            return true;
        }
    }
    else if (!playerData->accessInfo.isVerified())
    {
        sendMessage(ServerPlayer, t, "You are not identified");
        return true;
    }

    std::string header = "Permissions for: ";
    header += query->player.getCallSign();
    sendMessage(ServerPlayer, t, header.c_str());

    unsigned int permIndex = 0;
    std::vector<std::string> customPerms = query->accessInfo.customPerms;
    std::sort(customPerms.begin(), customPerms.end());

    for (int p = 0; p < PlayerAccessInfo::lastPerm; p++)
    {
        PlayerAccessInfo::AccessPerm perm = (PlayerAccessInfo::AccessPerm)p;
        const std::string& permName = nameFromPerm(perm);

        while (permIndex < customPerms.size())
        {
            const std::string& nextCustomPerm = bz_tolower(customPerms.at(permIndex).c_str());

            if (nextCustomPerm < permName)
            {
                sendMessage(ServerPlayer, t, nextCustomPerm.c_str());
                permIndex++;

                continue;
            }

            break;
        }

        if (query->accessInfo.hasPerm(perm))
            sendMessage(ServerPlayer, t, permName.c_str());
    }

    return true;
}


bool GroupPermsCommand::operator() (const char* msg,
                                    GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    msg += commandName.size();

    std::vector<std::string> argv = TextUtils::tokenize(msg, " \t", 0, true);
    std::string group;

    if (!argv.empty())
    {
        group = argv[0];
        if (groupAccess.find(group) == groupAccess.end())
        {
            std::string warning = "Group " + group + " does not exist.";
            sendMessage(ServerPlayer, t, warning.c_str());
            return true;
        }
    }

    if (group.empty())
        sendMessage(ServerPlayer, t, "Group List:");

    PlayerAccessMap::iterator itr;
    for (itr = groupAccess.begin(); itr != groupAccess.end(); ++itr)
    {
        if (!group.empty() && group != itr->first) continue;

        std::string line;
        line = itr->first + ":   ";
        sendMessage(ServerPlayer, t, line.c_str());

        // allows first
        if (itr->second.explicitAllows.any())
        {
            sendMessage(ServerPlayer, t, "  Allows");

            unsigned int permIndex = 0;
            std::vector<std::string> customPerms = itr->second.customPerms;
            std::sort(customPerms.begin(), customPerms.end());

            for (int i = 0; i < PlayerAccessInfo::lastPerm; i++)
            {
                const std::string& permName = nameFromPerm((PlayerAccessInfo::AccessPerm)i);

                while (permIndex < customPerms.size())
                {
                    const std::string& nextCustomPerm = bz_tolower(customPerms.at(permIndex).c_str());

                    if (nextCustomPerm < permName)
                    {
                        line = "     ";
                        line += nextCustomPerm;
                        sendMessage(ServerPlayer, t, line.c_str());
                        permIndex++;

                        continue;
                    }

                    break;
                }

                if (itr->second.explicitAllows.test(i) && !itr->second.explicitDenys.test(i) )
                {
                    line = "     ";
                    line += nameFromPerm((PlayerAccessInfo::AccessPerm)i);
                    sendMessage(ServerPlayer, t, line.c_str());
                }
            }
        }

        // same about denys
        if (itr->second.explicitDenys.any())
        {
            sendMessage(ServerPlayer, t, "  Denys");
            for (int i = 0; i < PlayerAccessInfo::lastPerm; i++)
            {
                if (itr->second.explicitDenys.test(i) )
                {
                    line = "     ";
                    line += nameFromPerm((PlayerAccessInfo::AccessPerm)i);
                    sendMessage(ServerPlayer, t, line.c_str());
                }
            }
        }
    }
    return true;
}


bool SetGroupCommand::operator() (const char* msg,
                                  GameKeeper::Player* playerData)
{
    int t = playerData->getIndex();

    if (!userDatabaseFile.size())
    {
        sendMessage(ServerPlayer, t, "/setgroup command disabled");
        return true;
    }

    msg += commandName.size();
    std::vector<std::string> argv = TextUtils::tokenize(msg, " \t", 0, true);
    if (argv.size() != 2)
    {
        sendMessage(ServerPlayer, t,
                    "Incorrect parameters, usage: /setgroup <player> <group>");
        return true;
    }
    std::string target = TextUtils::toupper(argv[0]);
    std::string group = TextUtils::toupper(argv[1]);

    if (!playerData->accessInfo.canSet(group))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to set this group");
        return true;
    }

    if (!userExists(target))
    {
        std::string warning = "Player is not listed: " + target;
        sendMessage(ServerPlayer, t, warning.c_str());
        return true;
    }

    PlayerAccessInfo &info = PlayerAccessInfo::getUserInfo(target);
    if (info.addGroup(group))
    {
        sendMessage(ServerPlayer, t, "Group Add successful");
        int getID = GameKeeper::Player::getPlayerIDByName(target);
        if (getID != -1)
        {
            char temp[MessageLen];
            snprintf(temp, MessageLen, "you have been added to the %s group, by %s",
                     group.c_str(), playerData->player.getCallSign());
            sendMessage(ServerPlayer, getID, temp);
            GameKeeper::Player::getPlayerByIndex(getID)->accessInfo.addGroup(group);
            sendPlayerInfo();
        }
        PlayerAccessInfo::updateDatabases();
    }
    else
        sendMessage(ServerPlayer, t, "Group Add failed (user may already be in that group)");

    return true;
}


bool RemoveGroupCommand::operator() (const char* msg,
                                     GameKeeper::Player* playerData)
{
    int t = playerData->getIndex();

    if (!userDatabaseFile.size())
    {
        sendMessage(ServerPlayer, t, "/removegroup command disabled");
        return true;
    }

    msg += commandName.size();
    std::vector<std::string> argv = TextUtils::tokenize(msg, " \t", 0, true);
    if (argv.size() != 2)
    {
        sendMessage(ServerPlayer, t,
                    "Incorrect parameters, usage: /removegroup <player> <group>");
        return true;
    }
    std::string target = TextUtils::toupper(argv[0]);
    std::string group = TextUtils::toupper(argv[1]);

    if (!playerData->accessInfo.canSet(group))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to set this group");
        return true;
    }

    if (!userExists(target))
    {
        std::string warning = "Player is not listed: " + target;
        sendMessage(ServerPlayer, t, warning.c_str());
        return true;
    }

    PlayerAccessInfo &info = PlayerAccessInfo::getUserInfo(target);
    if (info.removeGroup(group))
    {
        sendMessage(ServerPlayer, t, "Group Remove successful");
        int getID = GameKeeper::Player::getPlayerIDByName(target);
        if (getID != -1)
        {
            char temp[MessageLen];
            snprintf(temp, MessageLen, "You have been removed from the %s group, by %s",
                     group.c_str(), playerData->player.getCallSign());
            sendMessage(ServerPlayer, getID, temp);
            GameKeeper::Player::getPlayerByIndex(getID)->accessInfo.removeGroup(group);
            sendPlayerInfo();
        }
        PlayerAccessInfo::updateDatabases();
    }
    else
        sendMessage(ServerPlayer, t, "Group Remove failed (user may not have been in group)");

    return true;
}


bool ReloadCommand::operator() (const char   *message,
                                GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::setAll))
    {
        sendMessage(ServerPlayer, t,
                    "You do not have permission to run the reload command");
        return true;
    }

    std::string arguments = &message[7]; /* skip "/reload" */
    std::string cmd = "";

    logDebugMessage(3,"The arguments string is [%s]\n", arguments.c_str());

    /* find the start of the command */
    size_t startPosition = 0;
    while ((startPosition < arguments.size()) &&
            (isspace(arguments[startPosition])))
        startPosition++;

    logDebugMessage(3,"Start position is %d\n", (int)startPosition);

    /* find the end of the command */
    size_t endPosition = startPosition + 1;
    while ((endPosition < arguments.size()) &&
            (!isspace(arguments[endPosition])))
        endPosition++;

    logDebugMessage(3,"End position is %d\n", (int)endPosition);

    /* stash the command ('all', etc) in lowercase to simplify comparison */
    if ((startPosition != arguments.size()) &&
            (endPosition > startPosition))
    {
        for (size_t i = startPosition; i < endPosition; i++)
            cmd += tolower(arguments[i]);
    }

    logDebugMessage(3,"Command is %s\n", cmd.c_str());

    /* handle subcommands */

    bool reload_bans = false;
    bool reload_groups = false;
    bool reload_users = false;
    bool reload_helpfiles = false;
    bool reload_badwords = false;
    if ((cmd == "") || (cmd == "all"))
    {
        logDebugMessage(3,"Reload all\n");
        reload_bans = true;
        reload_groups = true;
        reload_users = true;
        reload_helpfiles = true;
        reload_badwords = true;
    }
    else if (cmd == "bans")
    {
        logDebugMessage(3,"Reload bans\n");
        reload_bans = true;
    }
    else if (cmd == "groups")
    {
        logDebugMessage(3,"Reload groups\n");
        reload_groups = true;
    }
    else if (cmd == "users")
    {
        logDebugMessage(3,"Reload users\n");
        reload_users = true;
    }
    else if (cmd == "helpfiles")
    {
        logDebugMessage(3,"Reload helpfiles\n");
        reload_helpfiles = true;
    }
    else if (cmd == "badwords")
    {
        logDebugMessage(3,"Reload badwords\n");
        reload_badwords = true;
    }
    else
    {
        sendMessage(ServerPlayer, t, "Invalid option for the reload command");
        sendMessage(ServerPlayer, t, "Usage: /reload [all|bans|badwords|helpfiles|groups|users]");
        return true; // Bail out
    }

    if (reload_helpfiles)
    {
        // reload the text chunks
        logDebugMessage(3,"Reloading helpfiles\n");
        clOptions->textChunker.reload();
    }

    if (reload_bans)
    {
        // reload the banlist
        logDebugMessage(3,"Reloading bans\n");
        clOptions->acl.load();
    }

    if (reload_groups)
    {
        logDebugMessage(3,"Reloading groups\n");
        groupAccess.clear();
        initGroups();
    }

    if (reload_users)
    {
        logDebugMessage(3,"Reloading users\n");
        userDatabase.clear();

        if (userDatabaseFile.size())
            PlayerAccessInfo::readPermsFile(userDatabaseFile);
        GameKeeper::Player::reloadAccessDatabase();
    }

    if (reload_badwords)
    {
        logDebugMessage(3,"Reloading badwords list\n");
        clOptions->filter.clear();
        loadBadwordsList();
    }

    sendMessage(ServerPlayer, t, "Databases reloaded");

    rescanForBans(playerData->accessInfo.isOperator(),playerData->player.getCallSign(),t);

    return true;
}


bool VoteCommand::operator() (const char     *message,
                              GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    char reply[MessageLen] = {0};
    std::string callsign = std::string(playerData->player.getCallSign());

    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::vote))
    {
        /* permission denied for /vote */
        snprintf(reply, MessageLen, "%s, you are presently not authorized to run /vote", callsign.c_str());
        sendMessage(ServerPlayer, t, reply);
        return true;
    }

    /* make sure that there is a poll arbiter */
    if (BZDB.isEmpty("poll"))
    {
        sendMessage(ServerPlayer, t, "ERROR: the poll arbiter has disappeared (this should never happen)");
        return true;
    }

    // only need to get this once
    static VotingArbiter *arbiter = (VotingArbiter *)BZDB.getPointer("poll");

    /* make sure that there is a poll to vote upon */
    if ((arbiter != NULL) && !arbiter->knowsPoll())
    {
        sendMessage(ServerPlayer, t, "A poll is not presently in progress.  There is nothing to vote on");
        return true;
    }

    std::string voteCmd = &message[5];
    std::string answer;

    /* find the start of the vote answer */
    size_t startPosition = 0;
    while ((startPosition < voteCmd.size()) &&
            (isspace(voteCmd[startPosition])))
        startPosition++;

    /* stash the answer ('yes', 'no', etc) in lowercase to simplify comparison */
    for (size_t i = startPosition;  i < voteCmd.size() && !isspace(voteCmd[i]); i++)
        answer += tolower(voteCmd[i]);

    std::vector<std::string> yesAnswers;
    yesAnswers.push_back("y");
    yesAnswers.push_back("1");
    yesAnswers.push_back("yes");
    yesAnswers.push_back("yea");
    yesAnswers.push_back("si"); // spanish+
    yesAnswers.push_back("ja");  // german
    yesAnswers.push_back("oui"); // french
    yesAnswers.push_back("sim"); // portuguese
    yesAnswers.push_back("tak"); // polish

    std::vector<std::string> noAnswers;
    noAnswers.push_back("n");
    noAnswers.push_back("0");
    noAnswers.push_back("no");
    noAnswers.push_back("nay");
    noAnswers.push_back("nein"); // german
    noAnswers.push_back("nien"); // german misspelled
    noAnswers.push_back("non"); // french
    noAnswers.push_back("nao"); // portuguese
    noAnswers.push_back("nie"); // polish

    // see if the vote response is a valid yes or no answer
    int vote = -1;
    unsigned int maxAnswerCount = noAnswers.size() > yesAnswers.size() ? noAnswers.size() : yesAnswers.size();
    for (unsigned int v = 0; v < maxAnswerCount; v++)
    {
        if (v < yesAnswers.size())
        {
            if (answer == yesAnswers[v])
            {
                vote = 1;
                break;
            }
        }
        if (v < noAnswers.size())
        {
            if (answer == noAnswers[v])
            {
                vote = 0;
                break;
            }
        }
    }

    // cast the vote or complain
    bool cast = false;
    if (vote > -1)
    {
        bz_PollVoteEventData_V1 voteData;
        voteData.playerID = t;
        voteData.inFavor = (vote == 1);

        worldEventManager.callEvents(bz_ePollVoteEvent, &voteData);

        if (!voteData.allow)
        {
            sendMessage(ServerPlayer, t, voteData.reason.c_str());
            return true;
        }

        if (vote == 0 && (cast = arbiter->voteNo(callsign)) == true)
            snprintf(reply, MessageLen, "%s, your vote in opposition of the %s has been recorded", callsign.c_str(),
                     arbiter->getPollAction().c_str());
        else if (vote == 1 && (cast = arbiter->voteYes(callsign)) == true)
            snprintf(reply, MessageLen, "%s, your vote in favor of the %s has been recorded", callsign.c_str(),
                     arbiter->getPollAction().c_str());

        sendMessage(ServerPlayer, t, reply);
    }
    else
    {
        if (answer.length() == 0)
        {
            snprintf(reply, MessageLen, "%s, you did not provide a vote answer", callsign.c_str());
            sendMessage(ServerPlayer, t, reply);
        }
        else
        {
            snprintf(reply, MessageLen, "%s, you did not vote in favor or in opposition", callsign.c_str());
            sendMessage(ServerPlayer, t, reply);
        }
        sendMessage(ServerPlayer, t, "Usage: /vote yes|no|y|n|1|0|yea|nay|si|ja|nein|oui|non|sim|nao");
        return true;
    }

    if (arbiter->hasVoted(callsign) && !cast)
    {
        /* player already voted */
        snprintf(reply, MessageLen, "%s, you have already voted on the poll to %s %s", callsign.c_str(),
                 arbiter->getPollAction().c_str(), arbiter->getPollTarget().c_str());
        sendMessage(ServerPlayer, t, reply);
        return true;
    }

    if (!cast)
    {
        /* There was an error while voting, probably could send a less generic message */
        snprintf(reply, MessageLen, "%s, there was an error while voting on the poll to %s %s", callsign.c_str(),
                 arbiter->getPollAction().c_str(), arbiter->getPollTarget().c_str());
        sendMessage(ServerPlayer, t, reply);
        return true;
    }

    return true;
}


bool VetoCommand::operator() (const char     *,
                              GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::veto))
    {
        /* permission denied for /veto */
        sendMessage(ServerPlayer, t,
                    TextUtils::format
                    ("%s, you are presently not authorized to run /veto",
                     playerData->player.getCallSign()).c_str());
        return true;
    }

    /* make sure that there is a poll arbiter */
    if (BZDB.isEmpty("poll"))
    {
        sendMessage(ServerPlayer, t, "ERROR: the poll arbiter has disappeared (this should never happen)");
        return true;
    }

    // only need to do this once
    static VotingArbiter *arbiter = (VotingArbiter *)BZDB.getPointer("poll");

    /* make sure there is an unexpired poll */
    if ((arbiter != NULL) && !arbiter->knowsPoll())
    {
        sendMessage(ServerPlayer, t,
                    TextUtils::format
                    ("%s, there is presently no active poll to veto",
                     playerData->player.getCallSign()).c_str());
        return true;
    }

    sendMessage(ServerPlayer, t,
                TextUtils::format("%s, you have cancelled the poll to %s %s",
                                  playerData->player.getCallSign(),
                                  arbiter->getPollAction().c_str(),
                                  arbiter->getPollTarget().c_str()).c_str());

    /* poof */
    arbiter->forgetPoll();

    sendMessage(ServerPlayer, AllPlayers,
                TextUtils::format("The poll was cancelled by %s",
                                  playerData->player.getCallSign()).c_str());

    bz_PollVetoEventData_V1 vetoData;
    vetoData.playerID  = t;

    worldEventManager.callEvents(bz_ePollVetoEvent, &vetoData);

    return true;
}


bool PollCommand::operator() (const char     *message,
                              GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    char reply[MessageLen] = {0};
    std::string callsign = std::string(playerData->player.getCallSign());

    logDebugMessage(2,"\"%s\" has requested a poll: %s\n", callsign.c_str(), message);

    /* make sure player has permission to request a poll */
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::poll))
    {
        snprintf(reply, MessageLen, "%s, you are presently not authorized to run /poll", callsign.c_str());
        sendMessage(ServerPlayer, t, reply);
        return true;
    }

    logDebugMessage(3,"Player has permission to run /poll\n");

    /* make sure that there is a poll arbiter */
    if (BZDB.isEmpty("poll"))
    {
        sendMessage(ServerPlayer, t, "ERROR: the poll arbiter has disappeared (this should never happen)");
        return true;
    }

    logDebugMessage(3,"BZDB poll value is not empty\n");

    // only need to do this once
    static VotingArbiter *arbiter = (VotingArbiter *)BZDB.getPointer("poll");

    logDebugMessage(3,"Arbiter was acquired with address 0x%p\n", arbiter);

    /* make sure that there is not a poll active already */
    if (arbiter->knowsPoll())
    {
        snprintf(reply, MessageLen, "A poll to %s %s is presently in progress", arbiter->getPollAction().c_str(),
                 arbiter->getPollTarget().c_str());
        sendMessage(ServerPlayer, t, reply);
        sendMessage(ServerPlayer, t, "Unable to start a new poll until the current one is over");
        return true;
    }

    logDebugMessage(3,"The arbiter says there is not another poll active\n");

    // get available voter count
    unsigned short int available = 0;
    for (int i = 0; i < curMaxPlayers; i++)
    {
        // any registered and verified users with poll permission (including observers)
        // are eligible to vote
        GameKeeper::Player *otherData = GameKeeper::Player::getPlayerByIndex(i);
        if (otherData && otherData->accessInfo.exists() && otherData->accessInfo.hasPerm(PlayerAccessInfo::poll))
            available++;
    }

    logDebugMessage(3,"There are %d available players for %d votes required\n", available, clOptions->votesRequired);

    /* make sure there are enough players to even make a poll that has a chance
     * of succeeding (not counting the person being acted upon)
     */
    if (available - 1 < clOptions->votesRequired)
    {
        sendMessage(ServerPlayer, t, "Unable to initiate a new poll.  There are not enough registered players playing.");
        snprintf(reply, MessageLen, "There needs to be at least %d other %s and only %d %s available.",
                 clOptions->votesRequired,
                 clOptions->votesRequired == 1 ? "player" : "players",
                 std::max(0, available - 1),
                 available - 1 == 1 ? "is" : "are");
        sendMessage(ServerPlayer, t, reply);
        return true;
    }

    std::string arguments = &message[5]; /* skip "/poll" */
    std::string cmd = "";

    logDebugMessage(3,"The arguments string is [%s]\n", arguments.c_str());

    /* find the start of the command */
    size_t startPosition = 0;
    while ((startPosition < arguments.size()) &&
            (isspace(arguments[startPosition])))
        startPosition++;

    logDebugMessage(3,"Start position is %d\n", (int)startPosition);

    /* find the end of the command */
    size_t endPosition = startPosition + 1;
    while ((endPosition < arguments.size()) &&
            (!isspace(arguments[endPosition])))
        endPosition++;

    logDebugMessage(3,"End position is %d\n", (int)endPosition);

    /* stash the command ('kick', etc) in lowercase to simplify comparison */
    if ((startPosition != arguments.size()) &&
            (endPosition > startPosition))
    {
        for (size_t i = startPosition; i < endPosition; i++)
            cmd += tolower(arguments[i]);
    }

    logDebugMessage(3,"Command is %s\n", cmd.c_str());

    /* handle subcommands */

    bool python_Poll    = bzPython_isPollActive(cmd);
    bool customPollType = (customPollTypes.find(cmd) != customPollTypes.end());
    if ((cmd == "ban") || (cmd == "kick") || (cmd == "kill") || (cmd == "set") || (cmd == "flagreset") || customPollType
            || python_Poll)
    {
        std::string target;
        std::string targetIP = "";

        arguments = arguments.substr(endPosition);

        if (arguments.size() == 0)
        {
            sendMessage(ServerPlayer, t, "/poll: incorrect syntax, argument required.");
            logDebugMessage(3,"No command arguments, stopping poll.\n");
            return true;
        }

        logDebugMessage(3,"Command arguments are [%s]\n", arguments.c_str());

        /* find the start of the target (e.g. player name) */
        startPosition = 0;
        while ((startPosition < arguments.size()) &&
                (isspace(arguments[startPosition])))
            startPosition++;
        // do not include a starting quote, if given
        if (arguments[startPosition] == '"')
            startPosition++;

        logDebugMessage(3,"Start position for target is %d\n", (int)startPosition);

        /* find the end of the target */
        endPosition = arguments.size() - 1;
        while ((endPosition > 0) &&
                (isspace(arguments[endPosition])))
            endPosition--;
        // do not include a trailing quote, if given
        if (arguments[endPosition] == '"')
            endPosition--;

        logDebugMessage(3,"End position for target is %d\n", (int)endPosition);

        target = arguments.substr(startPosition, endPosition - startPosition + 1);

        logDebugMessage(3,"Target specified to vote upon is [%s]\n", target.c_str());

        if ((target.length() == 0) && (cmd != "flagreset"))
        {
            snprintf(reply, MessageLen, "%s, no target was specified for the [%s] vote", callsign.c_str(), cmd.c_str());
            sendMessage(ServerPlayer, t, reply);
            snprintf(reply, MessageLen, "Usage: /poll %s target", cmd.c_str());
            sendMessage(ServerPlayer, t, reply);
            return true;
        }

        // Make sure the specific poll type is allowed
        if ((cmd == "set") && (!playerData->accessInfo.hasPerm(PlayerAccessInfo::pollSet)))
        {
            snprintf(reply, MessageLen, "%s, you may not /poll set on this server", callsign.c_str());
            sendMessage(ServerPlayer, t, reply);
            logDebugMessage(3,"Player %s is not allowed to /poll set\n", callsign.c_str());
            return true;
        }
        if ((cmd == "flagreset") && (!playerData->accessInfo.hasPerm(PlayerAccessInfo::pollFlagReset)))
        {
            snprintf(reply, MessageLen, "%s, you may not /poll flagreset on this server", callsign.c_str());
            sendMessage(ServerPlayer, t, reply);
            logDebugMessage(3,"Player %s is not allowed to /poll flagreset\n", callsign.c_str());
            return true;
        }
        if ((cmd == "ban") && (!playerData->accessInfo.hasPerm(PlayerAccessInfo::pollBan)))
        {
            snprintf(reply, MessageLen, "%s, you may not /poll ban on this server", callsign.c_str());
            sendMessage(ServerPlayer, t, reply);
            logDebugMessage(3,"Player %s is not allowed to /poll ban\n", callsign.c_str());
            return true;
        }
        if ((cmd == "kick") && (!playerData->accessInfo.hasPerm(PlayerAccessInfo::pollKick)))
        {
            snprintf(reply, MessageLen, "%s, you may not /poll kick on this server", callsign.c_str());
            sendMessage(ServerPlayer, t, reply);
            logDebugMessage(3,"Player %s is not allowed to /poll kick\n", callsign.c_str());
            return true;
        }

        if ((cmd == "kill") && (!playerData->accessInfo.hasPerm(PlayerAccessInfo::pollKill)))
        {
            snprintf(reply, MessageLen, "%s, you may not /poll kill on this server", callsign.c_str());
            sendMessage(ServerPlayer, t, reply);
            logDebugMessage(3,"Player %s is not allowed to /poll kill\n", callsign.c_str());
            return true;
        }

        if (customPollType)
        {
            bz_BasePlayerRecord *pr = bz_getPlayerByIndex(t);

            bool stopPoll = !(customPollTypes[cmd].pollHandler->PollOpen(pr, cmd.c_str(), target.c_str()));

            bz_freePlayerRecord(pr);

            if (stopPoll)
                return true;
        }
        else if (python_Poll)
        {
            bool stopPoll = !bzPython_PollOpen(cmd, t, target.c_str());

            if (stopPoll)
                return true;
        }


        if ((cmd != "set") && (cmd != "flagreset") && !customPollType && !python_Poll)
        {
            // kick, kill, and ban polls take a player name

            /* make sure the requested player is actually here */
            int v = GameKeeper::Player::getPlayerIDByName(target);
            if (v >= curMaxPlayers)
            {
                /* wrong name? */
                snprintf(reply, MessageLen,
                         "The player specified for a %s vote is not here", cmd.c_str());
                sendMessage(ServerPlayer, t, reply);
                return true;
            }
            GameKeeper::Player* targetData = GameKeeper::Player::getPlayerByIndex(v);
            if (!targetData)
            {
                /* wrong name? */
                snprintf(reply, MessageLen, "The server has no information on %s.", cmd.c_str());
                sendMessage(ServerPlayer, t, reply);
                return true;
            }
            if (cmd == "kill" && targetData->player.isObserver())
            {
                sendMessage(ServerPlayer, t, "You can't kill an observer!");
                return true;
            }
            targetIP = targetData->netHandler->getTargetIP();

            // operators can override antiperms
            if (!playerData->accessInfo.isOperator())
            {
                // otherwise make sure the player is not protected with an antiperm
                GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(v);
                if (p != NULL)
                {
                    if (p->accessInfo.hasPerm(PlayerAccessInfo::antipoll))
                    {
                        snprintf(reply, MessageLen, "%s is protected from being polled against.", p->player.getCallSign());
                        sendMessage(ServerPlayer, t, reply);
                        return true;
                    }
                    if (cmd == "ban")
                    {
                        if (p->accessInfo.hasPerm(PlayerAccessInfo::antipollban))
                        {
                            snprintf(reply, MessageLen, "%s is protected from being poll banned.", p->player.getCallSign());
                            sendMessage(ServerPlayer, t, reply);
                            return true;
                        }
                    }
                    else if (cmd == "kick")
                    {
                        if (p->accessInfo.hasPerm(PlayerAccessInfo::antipollkick))
                        {
                            snprintf(reply, MessageLen, "%s is protected from being poll kicked.", p->player.getCallSign());
                            sendMessage(ServerPlayer, t, reply);
                            return true;
                        }
                    }
                    else if (cmd == "kill")
                    {
                        if (p->accessInfo.hasPerm(PlayerAccessInfo::antipollkill))
                        {
                            snprintf(reply, MessageLen, "%s is protected from being poll killed.", p->player.getCallSign());
                            sendMessage(ServerPlayer, t, reply);
                            return true;
                        }
                    }
                }
            } // end admin check

        }

        bz_AllowPollEventData_V1 allowPollData;
        allowPollData.playerID = t;
        allowPollData.pollAction = cmd;
        allowPollData.pollTarget = target;

        worldEventManager.callEvents(bz_eAllowPollEvent, &allowPollData);

        if (!allowPollData.allow)
        {
            sendMessage(ServerPlayer, t, allowPollData.reason.c_str());
            return true;
        }

        /* create and announce the new poll */
        bool canDo = false;
        if (customPollType || python_Poll)
            canDo = (arbiter->poll(target, t, cmd));
        else if (cmd == "ban")
            canDo = (arbiter->pollToBan(target, t, targetIP));
        else if (cmd == "kick")
            canDo = (arbiter->pollToKick(target, t, targetIP));
        else if (cmd == "kill")
            canDo = (arbiter->pollToKill(target, t, targetIP));
        else if (cmd == "set")
            canDo = (arbiter->pollToSet(target, t));
        else if (cmd == "flagreset")
            canDo = (arbiter->pollToResetFlags(t));

        if (!canDo)
        {
            snprintf(reply, MessageLen, "You are not able to request a %s poll right now, %s", cmd.c_str(), callsign.c_str());
            sendMessage(ServerPlayer, t, reply);
            return true;
        }
        else
        {
            bz_PollStartEventData_V1 pollStartData;
            pollStartData.playerID  = t;
            pollStartData.pollAction = cmd;
            pollStartData.pollTarget = target;

            worldEventManager.callEvents(bz_ePollStartEvent, &pollStartData);

            snprintf(reply, MessageLen, "A poll to %s %s has been requested by %s", cmd.c_str(), target.c_str(), callsign.c_str());
            sendMessage(ServerPlayer, AllPlayers, reply);
        }

        unsigned int necessaryToSucceed = (unsigned int)((clOptions->votePercentage / 100.0) * (double)available);
        snprintf(reply, MessageLen, "%d player%s available, %d additional affirming vote%s required to pass the poll (%f %%)",
                 available, available==1?" is":"s are", necessaryToSucceed, necessaryToSucceed==1?"":"s", clOptions->votePercentage);
        sendMessage(ServerPlayer, AllPlayers, reply);

        // set the number of available voters
        arbiter->setAvailableVoters(available);

        // keep track of who is allowed to vote
        for (int j = 0; j < curMaxPlayers; j++)
        {
            // any registered/known users on the server (including
            // observers) are eligible to vote
            GameKeeper::Player *otherData = GameKeeper::Player::getPlayerByIndex(j);
            if (otherData && otherData->accessInfo.exists())
                arbiter->grantSuffrage(otherData->player.getCallSign());
        }

        // automatically place a vote for the player requesting the poll
        logDebugMessage(3,"Attempting to automatically place a vote for [%s]\n", callsign.c_str());

        bool voted = arbiter->voteYes(callsign);
        if (!voted)
        {
            sendMessage(ServerPlayer, t, "Unable to automatically place your vote for some unknown reason");
            logDebugMessage(3,"Unable to automatically place a vote for [%s]\n", callsign.c_str());
        }

    }
    else if (cmd == "vote")
    {
        std::string voteCmd = "/vote ";
        voteCmd += arguments;
        voteCommand(voteCmd.c_str(), playerData);
        return true;

    }
    else if (cmd == "veto")
    {
        std::string vetoCmd = "/veto ";
        vetoCmd += arguments;
        vetoCommand(vetoCmd.c_str(), playerData);
        return true;

    }
    else
    {
        sendMessage(ServerPlayer, t, "Invalid option to the poll command");
        sendMessage(ServerPlayer, t, "Usage: /poll vote yes|no");
        if (playerData->accessInfo.hasPerm(PlayerAccessInfo::pollBan))
            sendMessage(ServerPlayer, t, "    or /poll ban playername");
        if (playerData->accessInfo.hasPerm(PlayerAccessInfo::pollKick))
            sendMessage(ServerPlayer, t, "    or /poll kick playername");
        if (playerData->accessInfo.hasPerm(PlayerAccessInfo::pollKill))
            sendMessage(ServerPlayer, t, "    or /poll kill playername");
        if (playerData->accessInfo.hasPerm(PlayerAccessInfo::pollSet))
            sendMessage(ServerPlayer, t, "    or /poll set variable value");
        if (playerData->accessInfo.hasPerm(PlayerAccessInfo::pollFlagReset))
            sendMessage(ServerPlayer, t, "    or /poll flagreset");

        for (auto pollType = customPollTypes.begin(); pollType != customPollTypes.end(); ++pollType)
        {
            snprintf(reply, MessageLen, "    or /poll %s %s", pollType->first.c_str(), pollType->second.pollParameters.c_str());
            sendMessage(ServerPlayer, t, reply);
        }

        bzPython_PollHelp(t);

    } /* end handling of poll subcommands */

    return true;
}


bool ViewReportCommand::operator() (const char* message,
                                    GameKeeper::Player* playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::viewReports))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the viewreports command");
        return true;
    }
    if (clOptions->reportFile.size() == 0 && clOptions->reportPipe.size() == 0)
    {
        sendMessage(ServerPlayer, t,
                    "The /report command is disabled on this"
                    " server or there are no reports filed.");
    }
    std::ifstream ifs(clOptions->reportFile.c_str(), std::ios::in);
    if (ifs.fail())
    {
        sendMessage(ServerPlayer, t, "Error reading from report file.");
        return true;
    }

    // setup the glob pattern
    std::string pattern = "*";
    message += commandName.size();
    while ((*message != '\0') && isspace(*message)) message++;
    if (*message != '\0')
    {
        pattern = message;
        pattern = TextUtils::toupper(pattern);
        if (pattern.find('*') == std::string::npos)
            pattern = "*" + pattern + "*";
    }

    // assumes that empty lines separate the reports
    std::string line;
    std::vector<std::string> buffers;
    bool matched = false;
    while (std::getline(ifs, line))
    {
        buffers.push_back(line);
        if (line.size() <= 0)
        {
            // blank line
            if (matched)
            {
                for (int i = 0; i < (int)buffers.size(); i++)
                    sendMessage(ServerPlayer, t, buffers[i].c_str());
            }
            buffers.clear();
            matched = false;
        }
        else
        {
            // non-blank line
            if (glob_match(pattern, TextUtils::toupper(line)))
                matched = true;
        }
    }
    // in case the file doesn't end with a blank line
    if (matched)
    {
        for (int i = 0; i < (int)buffers.size(); i++)
            sendMessage(ServerPlayer, t, buffers[i].c_str());
    }

    return true;
}


bool ClientQueryCommand::operator() (const char  *message,
                                     GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::clientQuery))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the /clientquery command");
        return true;
    }
    logDebugMessage(2,"Clientquery requested by %s [%d]\n",
                    playerData->player.getCallSign(), t);
    if (message[12] != '\0')
    {
        std::string name = message + 13; // assumes there is a space
        while (isspace(name[0]))
            name.erase(name.begin());
        GameKeeper::Player *target;
        int i;
        if ((name.size() >= 2) &&
                (name[0] == '"') && (name[name.size()-1] == '"'))
        {
            name = name.substr(1, name.size() - 2); // remove the quotes
        }
        for (i = 0; i < curMaxPlayers; i++)
        {
            target = GameKeeper::Player::getPlayerByIndex(i);
            if (target && strcmp(target->player.getCallSign(), name.c_str()) == 0)
            {
                sendMessage(ServerPlayer, t, TextUtils::format("%s's client version:", target->player.getCallSign()).c_str());
                sendMessage(ServerPlayer, t, TextUtils::format("  %s", target->player.getClientVersion()).c_str());
                return true;
            }
        }
        sendMessage(ServerPlayer, t, "Player not found.");
        return true;
    }

    // send server's own version string just for kicks
    sendMessage(ServerPlayer, t, TextUtils::format("BZFS Version: %s", getAppVersion()).c_str());

    // send all players' version strings
    GameKeeper::Player *otherData;
    for (int i = 0; i < curMaxPlayers; i++)
    {
        otherData = GameKeeper::Player::getPlayerByIndex(i);
        if (otherData && otherData->player.isPlaying())
        {
            sendMessage(ServerPlayer, t, TextUtils::format("%s's client version:", otherData->player.getCallSign()).c_str());
            sendMessage(ServerPlayer, t, TextUtils::format("  %s", otherData->player.getClientVersion()).c_str());
        }
    }
    return true;
}


/** /record command
 *
 *  /record start          # start buffering
 *  /record stop        # stop buffering (or saving to file)
 *  /record size <Mbytes>       # set the buffer size, and truncate
 *  /record rate <secs>  # set the state capture rate
 *  /record stats          # display buffer time and memory information
 *  /record file [filename]     # begin capturing straight to file, flush buffer
 *  /record save [filename]     # save buffer to file (or default filename)
 */
bool RecordCommand::operator() (const char   *message,
                                GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    const char *buf = message + 8;
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::record))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the /record command");
        return true;
    }
    while ((*buf != '\0') && isspace (*buf)) buf++; // eat whitespace

    if (strncasecmp (buf, "start", 5) == 0)
        Record::start(t);
    else if (strncasecmp (buf, "stop", 4) == 0)
        Record::stop(t);
    else if (strncasecmp (buf, "size", 4) == 0)
    {
        buf = buf + 4;
        while ((*buf != '\0') && isspace (*buf)) buf++; // eat whitespace

        if (*buf == '\0')
        {
            Record::sendHelp (t);
            return true;
        }
        int size = atoi (buf);
        Record::setSize (t, size);
    }
    else if (strncasecmp (buf, "rate", 4) == 0)
    {
        buf = buf + 4;
        while ((*buf != '\0') && isspace (*buf)) buf++; // eat whitespace

        if (*buf == '\0')
        {
            Record::sendHelp (t);
            return true;
        }
        int seconds = atoi (buf);
        Record::setRate (t, seconds);
    }
    else if (strncasecmp (buf, "stats", 5) == 0)
        Record::sendStats(t);
    else if (strncasecmp (buf, "list", 4) == 0)
    {
        const char* options = buf + 4;
        if (!Replay::sendFileList (t, options))
            Record::sendHelp (t);
    }
    else if (strncasecmp (buf, "save", 4) == 0)
    {
        buf = buf + 4;
        char filename[MessageLen];

        while ((*buf != '\0') && isspace (*buf)) buf++; // eat whitespace
        if (*buf == '\0')
        {
            Record::sendHelp (t);
            return true;
        }

        // get the filename
        sscanf (buf, "%s", filename);

        // FIXME - do this a little better? use quotations for strings?
        while ((*buf != '\0') && !isspace (*buf)) buf++; // eat filename
        while ((*buf != '\0') && isspace (*buf)) buf++; // eat whitespace

        if (*buf == '\0')
            Record::saveBuffer (t, filename, 0);
        else
            Record::saveBuffer (t, filename, atoi(buf));
    }
    else if (strncasecmp (buf, "file", 4) == 0)
    {
        buf = buf + 4;
        while ((*buf != '\0') && isspace (*buf)) buf++; // eat whitespace

        if (*buf == '\0')
            Record::sendHelp (t);
        else
            Record::saveFile (t, buf);
    }
    else
        Record::sendHelp (t);

    return true;
}


/** /replay command
 *
 *  /replay list        # list available replay files
 *  /replay load [filename]     # set the replay file (or load the default)
 *  /replay play        # began playing
 *  /replay loop        # began playing in looped mode
 *  /replay stats       # report the current replay state
 *  /replay skip <secs>  # fast foward or rewind in time
 */
bool ReplayCommand::operator() (const char   *message,
                                GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    const char *buf = message + 7;
    while ((*buf != '\0') && isspace (*buf))   // eat whitespace
        buf++;

    // everyone can use the replay stats command
    if (strncasecmp (buf, "stats", 5) == 0)
    {
        Replay::sendStats (t);
        return true;
    }

    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::replay))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the /replay command");
        return true;
    }

    if (strncasecmp (buf, "list", 4) == 0)
    {
        const char* options = buf + 4;
        if (!Replay::sendFileList (t, options))
            Record::sendHelp (t);
    }
    else if (strncasecmp (buf, "load", 4) == 0)
    {
        buf = buf + 4;
        while ((*buf != '\0') && isspace (*buf)) buf++; // eat whitespace

        if (*buf == '\0')
            Replay::sendHelp (t);
        else
            Replay::loadFile (t, buf);
    }
    else if (strncasecmp (buf, "play", 4) == 0)
        Replay::play (t);
    else if (strncasecmp (buf, "loop", 4) == 0)
        Replay::loop (t);
    else if (strncasecmp (buf, "skip", 4) == 0)
    {
        buf = buf + 4;
        while ((*buf != '\0') && isspace (*buf)) buf++; // eat whitespace

        if (*buf == '\0')
            Replay::skip (t, 0);
        else
        {
            int skip = atoi (buf);
            Replay::skip (t, skip);
        }
    }
    else if (strncasecmp (buf, "pause", 5) == 0)
        Replay::pause (t);
    else
        Replay::sendHelp (t);

    return true;
}


bool SayCommand::operator() (const char  *message,
                             GameKeeper::Player *playerData)
{
    size_t messageStart = 0;
    int t = playerData->getIndex();

    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::say))
    {
        char reply[MessageLen] = {0};
        snprintf(reply, MessageLen, "%s, you do not have permission to run the /say command", playerData->player.getCallSign());
        sendMessage(ServerPlayer, t, reply);
        return true;
    }

    std::string messageText = &message[4];

    // skip any leading whitespace
    while ((messageStart < messageText.size()) &&
            (isspace(messageText[messageStart])))
        messageStart++;

    // make sure there was _some_ whitespace after /say
    if (messageStart == 0)
    {
        sendMessage(ServerPlayer, t, "Usage: /say some message");
        return true;
    }

    // no anonymous messages
    messageText += " (";
    messageText += playerData->player.getCallSign();
    messageText += ")";

    // send the message
    sendMessage(ServerPlayer, AllPlayers, messageText.c_str() + messageStart );
    return true;
}



bool DateTimeCommand::operator() (const char     *,
                                  GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::date))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the /date command");
        return true;
    }
    time_t now = time(NULL);
    char* timeStr = ctime(&now);
    timeStr[24] = '\0';
    sendMessage(ServerPlayer, t, timeStr);
    return true;
}

bool ModCountCommand::operator() (const char* message, GameKeeper::Player *playerData)
{
    size_t messageStart = 0;
    int t = playerData->getIndex();

    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::modCount))
    {
        char reply[MessageLen] = {0};
        snprintf(reply, MessageLen, "You do not have permission to run the /modcount command");
        sendMessage(ServerPlayer, t, reply);
        return true;
    }

    std::string messageText = &message[9];

    // skip any leading whitespace
    while ((messageStart < messageText.size()) && (isspace(messageText[messageStart])))
        messageStart++;

    if (messageStart == messageText.size())
    {
        sendMessage(ServerPlayer, t, "Usage: /modcount {+|-} SECONDS");
        return true;
    }

    if (!countdownActive && countdownDelay <= 0)
    {
        char reply[MessageLen] = {0};
        snprintf(reply, MessageLen, "%s, there is no current countdown in progress", playerData->player.getCallSign());
        sendMessage(ServerPlayer, t, reply);
        return true;
    }

    messageText.erase(0, --messageStart);
    float timeModification = (float)atof(messageText.c_str());
    clOptions->addedTime += timeModification; // remember to add the time

    char announce[MessageLen] = {0};
    snprintf(announce, MessageLen, "Countdown has been adjusted by %0.f seconds by %s",
             timeModification, playerData->player.getCallSign());
    sendMessage(ServerPlayer, AllPlayers, announce);

    if (countdownDelay > 0) // we are currently counting down to start
    {
        char reply[MessageLen] = {0};
        snprintf(reply, MessageLen, "%s, the countdown will be adjusted by %f when the match starts",
                 playerData->player.getCallSign(), clOptions->addedTime);
        sendMessage(ServerPlayer, t, reply);
        return true;
    }

    return true;
}



// parse server comands
void parseServerCommand(const char *message, int t, int sourceChannel)
{
    if (!message)
    {
        std::cerr << "WARNING: parseCommand was given a null message?!" << std::endl;
        return;
    }

    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(t);
    if (!playerData)
        return;

    // Notify plugins of slash command execution request
    bz_SlashCommandEventData_V2 commandData;
    commandData.from = t;
    commandData.sourceChannel = sourceChannel;
    commandData.message = message;

    worldEventManager.callEvents(bz_eSlashCommandEvent, &commandData);

    if (cmdHelp(message, playerData))
        return;

    // lets see if it is a custom command. custom commands take precedence over
    // built-in commands
    std::vector<std::string> params =
        TextUtils::tokenize(std::string(message+1), " ", 0, true);

    if (params.empty())
        return;

    tmCustomSlashCommandMap::iterator itr =
        customCommands.find(TextUtils::tolower(params[0]));

    bz_ApiString command = params[0];
    bz_ApiString APIMessage;
    bz_APIStringList APIParams;

    for ( unsigned int i = 1; i < params.size(); i++)
        APIParams.push_back(params[i]);

    if ( strlen(message+1) > params[0].size())
        APIMessage = (message+params[0].size()+2);

    // see if we have a registerd custom command and call it
    if (itr != customCommands.end())
    {
        // if it handles it, then we are good
        if (itr->second->SlashCommand(t, sourceChannel, command, APIMessage, &APIParams))
            return;
    }

#ifdef BZ_PYTHON
    // If python handles, it is ok
    if (bzPython_SlashCommand(t, command, APIMessage, &APIParams))
        return;
#endif

    // if it wasn't a custom command, so fallback to built-in commands
    if (ServerCommand::execute(message, playerData))
        return;

    // lets see if anyone wants to handle the unhandled event
    bz_UnknownSlashCommandEventData_V1 commandData1;
    commandData1.from = t;
    commandData1.message = message;

    worldEventManager.callEvents(bz_eUnknownSlashCommand, &commandData1);
    if (commandData1.handled) // did anyone do it?
        return;

    char reply[MessageLen];
    snprintf(reply, MessageLen, "Unknown command [%s]", message + 1);
    sendMessage(ServerPlayer, t, reply);
}

void registerCustomSlashCommand(std::string command, bz_CustomSlashCommandHandlerV2* handler)
{
    if (handler)
        customCommands[TextUtils::tolower(command)] = handler;
}

void removeCustomSlashCommand(std::string command)
{
    tmCustomSlashCommandMap::iterator itr = customCommands.find(TextUtils::tolower(command));
    if (itr != customCommands.end())
        customCommands.erase(itr);
}

bool DebugCommand::operator() (const char *message,
                               GameKeeper::Player *playerData)
{
    int t = playerData->getIndex();
    if (!playerData->accessInfo.hasPerm(PlayerAccessInfo::setAll))
    {
        sendMessage(ServerPlayer, t, "You do not have permission to run the debug command");
        logDebugMessage(3,"debug failed by %s\n",playerData->player.getCallSign());
        return true;
    }
    std::string arguments = &message[12]; /* skip "/serverdebug" */

    if (arguments.find_first_not_of(" ") == std::string::npos)
    {
        /* No arguments */
        sendMessage(ServerPlayer, t,
                    TextUtils::format("Debug Level is %d", debugLevel).c_str());
    }
    else
    {
        int newDebugLevel = atoi(arguments.c_str());

        sendMessage(ServerPlayer, AdminPlayers,
                    TextUtils::format("Debug Level changed from %d to %d by %s",
                                      debugLevel, newDebugLevel,
                                      playerData->player.getCallSign()).c_str());
        debugLevel = newDebugLevel;
    }

    return true;
}


bool OwnerCommand::operator() (const char* UNUSED(message),
                               GameKeeper::Player *playerData)
{
    const int playerIndex = playerData->getIndex();
    const std::string& owner = getPublicOwner();
    if (owner.empty())
        sendMessage(ServerPlayer, playerIndex, "server has no registered owner");
    else
    {
        std::string msg = "this server's registered owner is: ";
        msg += owner;
        sendMessage(ServerPlayer, playerIndex, msg.c_str());
    }
    return true;
}

bool LoadPyCommand::operator() (const char* message,
                                GameKeeper::Player *playerData)
{
    const int playerIndex = playerData->getIndex();

    stopPython();

    const char *moduleName;

    for (moduleName = message;
            (*moduleName != 0) && (*moduleName != ' ');
            moduleName++)
        ;
    for (; (*moduleName != 0) && (*moduleName == ' ') ; moduleName++)
        ;
    if (!(*moduleName))
    {
        sendMessage(ServerPlayer, playerIndex, "python module not specified");
        return true;
    }

    const char *endModule;
    for (endModule = moduleName;
            (*endModule != 0) && (*endModule != ' ');
            endModule++)
        ;

    char module[128];
    memcpy(module, moduleName, endModule - moduleName);
    module[endModule - moduleName] = 0;

    for (; (*endModule != 0) && (*endModule == ' ') ; endModule++)
        ;

    startPython(module, endModule);
    return true;
}

bool UnloadPyCommand::operator() (const char* UNUSED(message),
                                  GameKeeper::Player *UNUSED(playerData))
{
    stopPython();
    return true;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
