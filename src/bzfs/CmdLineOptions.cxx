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

/* this should be the only header necessary except for headers specific
 * to the class implementation (such as version.h)
 */
#include "CmdLineOptions.h"

// System headers
#include <time.h>

// implementation-specific bzflag headers
#include "version.h"
#include "Team.h"
#include "TextUtils.h"
#include "BZDBCache.h"
#include "BzMaterial.h"
#include "ObstacleMgr.h"

/* FIXME implementation specific header for global that should eventually go away */
#include <vector>
#include <set>

// for -pidfile option
#ifdef _WIN32
#include <process.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

// implementation-specific bzfs-specific headers
#include "bzfs.h"
#include "RecordReplay.h"
#include "BZWError.h"
#include "Permissions.h"
#include "EntryZones.h"


// import from TextUtils for convenience
using TextUtils::compare_nocase;

const char *usageString =
    "[-a <vel> <rot>] "
    "[-adminlagannounce <time/ms>] "
    "[-admsg <text>] "
    "[-advertise <group,group...>] "
    "[-autoTeam] "
    "[-b] "
    "[-badwords <filename>] "
    "[-ban ip{,ip}*] "
    "[-banfile <filename>] "
    "[-c] "
    "[-cache <url prefix>] "
    "[-cacheout <filename>] "
    "[-conf <filename>] "
    "[-cr] "
    "[-d] "
    "[-density <num>] "
    "[-disableBots] "
    "[+f {good|<id>}] "
    "[-f {bad|<id>}] "
    "[-fb] "
    "[-filterCallsigns] "
    "[-filterChat] "
    "[-filterSimple] "
    "[-g] "
    "[-gndtex <texture name>] "
    "[-groupdb <group file>] "
    "[-h] "
    "[-handicap] "
    "[-helpmsg <file> <name>] "
    "[-i interface] "
    "[-j] "
    "[-jitterdrop <num>] "
    "[-jitterwarn <time/ms>] "
    "[-lagannounce <time/ms>] "
    "[-lagdrop <num>] "
    "[-lagwarn <time/ms>] "
    "[-loadplugin <pluginname,commandline>] "
    "[-loadpython <pluginname,commandline>] "
    "[-masterBanURL <URL>] "
    "[-maxidle <time/s>] "
    "[-mp {<count>|[<count>][,<count>][,<count>][,<count>][,<count>][,<count>]}] "
    "[-mps <score>] "
    "[-ms <shots>] "
    "[-mts <score>] "
    "[-noMasterBanlist] "
    "[-noradar] "
    "[-noTeamKills] "
    "[-offa] "
    "[-p <port>] "
    "[-packetlossdrop <num>] "
    "[-packetlosswarn <%>] "
    "[-passwd <password>] "
    "[-pidfile <filename>] "
    "[-poll <variable>=<value>] "
#ifdef PRINTSCORE
    "[-printscore] "
#endif
    "[-publictitle <server-description>] "
    "[-publicaddr <server-hostname>[:<server-port>]] "
    "[-publiclist <list-server-url>] "
    "[-publickey <key>] "
    "[-q] "
    "[+r] "
    "[-rabbit [score|killer|random]] "
    "[-recbuf <Mbytes>] "
    "[-recbufonly] "
    "[-recdir <dirname>] "
    "[-replay] "
    "[-reportfile <filename>] "
    "[-reportpipe <filename>] "
    "[+s <flag-count>] "
    "[-s <flag-count>] "
    "[-sa] "
    "[-sb] "
    "[-set <name> <value>] "
    "[-setforced <name> <value>] "
    "[-sl <id> <num>] "
    "[-spamtime <time>] "
    "[-spamwarn <warnAmt>] "
    "[-speedtol <tolerance>] "
    "[-srvmsg <text>] "
    "[-st <time>] "
    "[-sw <num>] "
    "[-synctime] "
    "[-synclocation] "
    "[-t] "
    "[-tftimeout <seconds>] "
    "[-time {<seconds>|endTime}] "
    "[-timemanual] "
    "[-tk] "
    "[-tkannounce] "
    "[-tkkr <percent>] "
    "[-ts [micros]] "
#ifdef HAVE_MINIUPNPC_MINIUPNPC_H
    "[-UPnP] "
#endif
    "[-userdb <user permissions file>] "
    "[-utc] "
    "[-vars <filename>] "
    "[-version] "
    "[-world <filename>] "
    "[-worldsize <world size>] "
    "[-ws <number of wall sides>] ";

const char *extraUsageString =
    "\n"
    "BZFS Option Descriptions\n"
    "\n"
    "\t-a: maximum acceleration settings\n"
    "\t-adminlagannounce: lag announcement threshold time [ms]\n"
    "\t-admsg: specify a <msg> which will be broadcast every 15 minutes\n"
    "\t-advertise: specify which groups to advertise to (list server)\n"
    "\t-autoTeam: automatically assign players to teams when they join\n"
    "\t-b: randomly oriented buildings\n"
    "\t-badwords: bad-word file\n"
    "\t-ban ip{,ip}*: ban players based on ip address\n"
    "\t-banfile: specify a file to load and store the banlist in\n"
    "\t-c: classic capture-the-flag style game,\n"
    "\t-cache: url to get binary formatted world\n"
    "\t-cacheout: generate a binary cache file\n"
    "\t-conf: configuration file\n"
    "\t-cr: capture-the-flag style game with random world\n"
    "\t-d: increase debugging level\n"
    "\t-density: specify building density for random worlds (default=5)\n"
    "\t-disableBots: disallow clients from using autopilot or robots\n"
    "\t+f: always have flag <id> available\n"
    "\t-f: never randomly generate flag <id>\n"
    "\t-fb: allow flags on box buildings\n"
    "\t-filterCallsigns: filter callsigns to disallow inappropriate user names\n"
    "\t-filterChat: filter chat messages\n"
    "\t-filterSimple: perform simple exact matches with the bad word list\n"
    "\t-g: serve one game and then exit\n"
    "\t-gndtex: specify ground texture\n"
    "\t-groupdb: file to read for group permissions\n"
    "\t-h: use random building heights\n"
    "\t-handicap: give advantage based on relative playing ability\n"
    "\t-helpmsg: show the lines in <file> on command /help <name>\n"
    "\t-i: listen on <interface>\n"
    "\t-j: allow jumping\n"
    "\t-jitterdrop: drop player after this many jitter warnings\n"
    "\t-jitterwarn: jitter warning threshold time [ms]\n"
    "\t-lagannounce: lag announcement threshold time [ms]\n"
    "\t-lagdrop: drop player after this many lag warnings\n"
    "\t-lagwarn: lag warning threshold time [ms]\n"
    "\t-loadplugin: load the specified plugin with the specified commandline\n"
    "\t-loadpython: load the specified python file with the specified commandline\n"
    "\t\tstring\n"
    "\t-masterBanURL: URL to attempt to get the master ban list from <URL>\n"
    "\t-maxidle: idle kick threshold [s]\n"
    "\t-mp: maximum players total or per team\n"
    "\t-mps: set player score limit on each game\n"
    "\t-ms: maximum simultaneous shots per player\n"
    "\t-mts: set team score limit on each game\n"
    "\t-noMasterBanlist: has public servers ignore the master ban list\n"
    "\t-noradar: disallow the use of radar\n"
    "\t-noTeamKills: Players on the same team are immune to each other's shots. Rogue is excepted.\n"
    "\t-offa: teamless free-for-all game style\n"
    "\t-p: use alternative port (default=5154)\n"
    "\t-packetlossdrop: drop player after this many packetloss warnings\n"
    "\t-packetlosswarn: packetloss warning threshold [%]\n"
    "\t-passwd: specify a <password> for operator commands\n"
    "\t-pidfile: write the process id into <filename> on startup\n"
    "\t-poll: configure several aspects of the in-game polling system\n"
#ifdef PRINTSCORE
    "\t-printscore: write score to stdout whenever it changes\n"
#endif
    "\t-publictitle <server-description>\n"
    "\t-publicaddr <effective-server-hostname>[:<effective-server-port>]\n"
    "\t-publiclist <list-server-url>\n"
    "\t-publickey <key>\n"
    "\t-q: don't listen for or respond to pings\n"
    "\t+r: all shots ricochet\n"
    "\t-rabbit [score|killer|random]: rabbit chase style\n"
    "\t-recbuf <Mbytes>: start with a recording buffer of specified megabytes\n"
    "\t-recbufonly: disable recording directly to files\n"
    "\t-recdir <dirname>: specify the directory for recorded file\n"
    "\t-replay: setup the server to replay a previously saved game\n"
    "\t-reportfile <filename>: the file to store reports in\n"
    "\t-reportpipe <filename>: the program to pipe reports through\n"
    "\t+s: always have <num> super flags (default=16)\n"
    "\t-s: allow up to <num> super flags (default=16)\n"
    "\t-sa: insert antidote superflags\n"
    "\t-sb: allow tanks to respawn on buildings\n"
    "\t-set <name> <value>: set a BZDB variable's value\n"
    "\t-setforced <name> <value>: set a BZDB variable's value (whether it\n"
    "\t\texists or not)\n"
    "\t-sl: limit flag <id> to <num> shots\n"
    "\t-spamtime <time>: make <time> be the required time in seconds between\n"
    "\t\tmessages sent that are alike\n"
    "\t-spamwarn <warnAmt>: warn a spammer that sends messages before\n"
    "\t\tspamtime times out <warnAmt> many times\n"
    "\t-speedtol: multiplier of normal speed for auto kick (default=1.25)\n"
    "\t\tshould not be less than 1.0\n"
    "\t-srvmsg: specify a <msg> to print upon client login\n"
    "\t-st: shake bad flags in <time> seconds\n"
    "\t-sw: shake bad flags after <num> wins\n"
    "\t-synctime: synchronize time of day on all clients\n"
    "\t-synclocation: synchronize latitude and longitude on all clients\n"
    "\t-t: allow teleporters\n"
    "\t-tftimeout: set timeout for team flag zapping (default=30)\n"
    "\t-time: set time limit on each game in format of either seconds or ending time in h[h]:[mm:[ss]] format\n"
    "\t-timemanual: countdown for timed games is started with /countdown\n"
    "\t-tk: player does not die when killing a teammate\n"
    "\t-tkannounce: announces team kills to the admin channel\n"
    "\t-tkkr: team-kills-to-wins percentage (1-100) for kicking tk-ing players\n"
    "\t-ts [micros]: timestamp all console output, [micros] to include\n"
    "\t\tmicroseconds\n"
#ifdef HAVE_MINIUPNPC_MINIUPNPC_H
    "\t-UPnP: enable UPnP\n"
#endif
    "\t-userdb: file to read for user access permissions\n"
    "\t-utc: timestamp console output in UTC instead of local time, implies -ts\n"
    "\t-vars: file to read for worlds configuration variables\n"
    "\t-version: print version and exit\n"
    "\t-world: world file to load\n"
    "\t-worldsize: numeric value for the size of the world (default=400)\n"
    "\t-ws: numeric value for the number off outer walls (default=4)\n"
    "\n"
    "Poll Variables:  (see -poll)\n"
    "\n"
    "\tbanTime: number of minutes player should be banned (default=300)\n"
    "\tvetoTime: max seconds authorized user has to abort poll (default=20)\n"
    "\tvotePercentage: percentage of players required to affirm a poll\n"
    "\t\t(default=50.1%)\n"
    "\tvoteRepeatTime: minimum seconds required before a player may request\n"
    "\t\tanother vote (default=300)\n"
    "\tvotesRequired: minimum number of additional votes required to make a\n"
    "\t\tvote valid (default=2)\n"
    "\tvoteTime: maximum amount of time player has to vote, in seconds\n"
    "\t\t(default=60)\n"
    "\n";


// temp variables for carrying flag options from parse to finalize
static std::vector<std::string> storedFlagDisallows;
static std::vector<std::string> storedFlagCounts;
static std::map<std::string, int> storedFlagLimits;
static std::map<std::string, std::string> bzdbVarQueue;

/* private */

static void printVersion()
{
    std::cout << "BZFlag server " << getAppVersion() << " (protocol " << getProtocolVersion() <<
              ") http://BZFlag.org/\n";
    std::cout << bzfcopyright << std::endl;
    std::cout.flush();
}

static void usage(const char *pname)
{
    printVersion();
    std::cerr << "\nUsage: " << pname << ' ' << usageString << std::endl;
    exit(1);
}

static void extraUsage(const char *pname)
{
    char buffer[64];
    printVersion();
    std::cout << "\nUsage: " << pname << ' ' << usageString << std::endl;
    std::cout << std::endl << extraUsageString << std::endl << "Flag codes:\n";
    for (FlagTypeMap::iterator it = FlagType::getFlagMap().begin(); it != FlagType::getFlagMap().end(); ++it)
    {
        snprintf(buffer, 64, "\t%2.2s %s\n", (*it->second).flagAbbv.c_str(), (*it->second).flagName.c_str());
        std::cout << buffer;
    }
    exit(0);
}

static void checkArgc(int count, int& i, int argc, const char* option, const char *type = NULL)
{
    if ((i+count) >= argc)
    {
        if (count > 1)
            std::cerr << count << " argument(s) expected for " << option << '\n';
        else if (type != NULL)
            std::cerr << type << " argument expected for " << option << '\n';
        else
            std::cerr << "argument expected for " << option << '\n';
        usage("bzfs");
    }

    i++; // just skip the option argument string
}

static void checkFromWorldFile (const char *option, bool fromWorldFile)
{
    if (fromWorldFile)
    {
        std::cerr << "option \"" << option << "\" cannot be set within a world file" << '\n';
        usage("bzfs");
    }
}



static bool parsePlayerCount(const char *argv, CmdLineOptions &options)
{
    /* either a single number or 5 or 6 optional numbers separated by
     * 4 or 5 (mandatory) commas.
     */
    const char *scan = argv;
    while (*scan && *scan != ',') scan++;

    if (*scan == ',')
    {
        // okay, it's the comma separated list.  count commas
        int commaCount = 1;
        while (*++scan)
        {
            if (*scan == ',')
                commaCount++;
        }
        if (commaCount != 5)
        {
            std::cerr << "improper player count list\n";
            return false;
        }

        // no team size limit by default for real teams, set it to max players
        int i;
        for (i = 0; i < CtfTeams ; i++)
            options.maxTeam[i] = maxRealPlayers;
        // allow 5 Observers by default
        options.maxTeam[ObserverTeam] = 5;

        // now get the new counts

        // number of counts given
        int countCount = 0;
        scan = argv;
        // ctf teams plus observers
        for (i = 0; i < CtfTeams + 1; i++)
        {
            char *tail;
            long count = strtol(scan, &tail, 10);
            if (tail != scan)
            {
                // got a number
                countCount++;
                if (count < 0)
                    options.maxTeam[i] = 0;
                else
                {
                    if (count > maxRealPlayers)
                    {
                        if (i == ObserverTeam && count > MaxPlayers)
                            options.maxTeam[i] = MaxPlayers;
                        else
                            options.maxTeam[i] = maxRealPlayers;
                    }
                    else
                        options.maxTeam[i] = uint8_t(count);
                }
            } // end if tail != scan
            while (*tail && *tail != ',') tail++;
            scan = tail + 1;
        } // end iteration over teams

        // if no/zero players were specified, allow a bunch of rogues
        bool allZero = true;
        for (i = 0; i < CtfTeams; i++)
        {
            if (options.maxTeam[i] != 0)
                allZero = false;
        }
        if (allZero)
            options.maxTeam[RogueTeam] = MaxPlayers;

        // if all counts explicitly listed then add 'em up and set maxRealPlayers
        // (unless max total players was explicitly set)
        if (countCount >= CtfTeams && maxRealPlayers == MaxPlayers)
        {
            maxRealPlayers = 0;
            for (i = 0; i < CtfTeams ; i++)
                maxRealPlayers += options.maxTeam[i];
        }

    }
    else
    {
        /* single number was provided instead of comma-separated */
        char *tail;
        long count = strtol(argv, &tail, 10);
        if (argv == tail)
        {
            std::cerr << "improper player count\n";
            return false;
        }
        if (count < 1)
            maxRealPlayers = 1;
        else
        {
            if (count > MaxPlayers)
                maxRealPlayers = MaxPlayers;
            else
                maxRealPlayers = uint8_t(count);
        }
        // limit max team size to max players
        for (int i = 0; i < CtfTeams ; i++)
        {
            if (options.maxTeam[i] > maxRealPlayers)
                options.maxTeam[i] = maxRealPlayers;
        }
    } // end check if comm-separated list

    maxPlayers = maxRealPlayers + options.maxTeam[ObserverTeam];
    if (maxPlayers > MaxPlayers)
        maxPlayers = MaxPlayers;

    return true;
}

static char **parseConfFile( const char *file, int &ac)
{
    std::vector<std::string> tokens;
    ac = 0;

    BZWError errorHandler(file);

    std::ifstream confStrm(file);
    if (confStrm.is_open())
    {
        char buffer[1024];
        confStrm.getline(buffer,1024);

        if (!confStrm.good())
        {
            errorHandler.fatalError(std::string("could not find bzflag configuration file"), 0);
            exit(1);
        }

        confStrm.seekg(0, std::ifstream::beg);
        while (confStrm.good())
        {
            confStrm.getline(buffer,1024);
            std::string line = buffer;
            int startPos = line.find_first_not_of("\t \r\n");
            while ((startPos >= 0) && (line.at(startPos) != '#'))
            {
                int endPos;
                if (line.at(startPos) == '"')
                {
                    startPos++;
                    endPos = line.find_first_of('"', startPos);
                }
                else
                    endPos = line.find_first_of("\t \r\n", startPos+1);
                if (endPos < 0)
                    endPos = line.length();
                tokens.push_back(line.substr(startPos,endPos-startPos));
                startPos = line.find_first_not_of("\t \r\n", endPos+1);
            }
        }
    }
    else
    {
        errorHandler.fatalError(std::string("could not find bzflag configuration file"), 0);
        exit(1);
    }

    char **av = new char*[tokens.size()+1];
    av[0] = strdup("bzfs");
    ac = 1;
    for (std::vector<std::string>::iterator it = tokens.begin(); it != tokens.end(); ++it)
        av[ac++] = strdup((*it).c_str());
    return av;
}

static char **parseWorldOptions (const char *file, int &ac)
{
    std::vector<std::string> tokens;
    ac = 0;

    std::ifstream confStrm(file);
    if (confStrm.is_open())
    {
        char buffer[1024];
        confStrm.getline(buffer,1024);

        if (!confStrm.good())
        {
            std::cerr << "world file not found\n";
            usage("bzfs");
        }

        while (confStrm.good())
        {
            std::string line = buffer;
            int startPos = line.find_first_not_of("\t \r\n");
            if (strncasecmp ("options", line.c_str() + startPos, 7) == 0)
            {
                confStrm.getline(buffer,1024);
                break;
            }
            confStrm.getline(buffer,1024);
        }

        while (confStrm.good())
        {
            std::string line = buffer;
            int startPos = line.find_first_not_of("\t \r\n");
            if (strncasecmp ("end", line.c_str() + startPos, 3) == 0)
                break;

            while ((startPos >= 0) && (line.at(startPos) != '#'))
            {
                int endPos;
                if (line.at(startPos) == '"')
                {
                    startPos++;
                    endPos = line.find_first_of('"', startPos);
                }
                else
                    endPos = line.find_first_of("\t \r\n", startPos+1);
                if (endPos < 0)
                    endPos = line.length();
                tokens.push_back(line.substr(startPos,endPos-startPos));
                startPos = line.find_first_not_of("\t \r\n", endPos+1);
            }
            confStrm.getline(buffer,1024);
        }
    }

    char **av = new char*[tokens.size()+1];
    av[0] = strdup("bzfs");
    ac = 1;
    for (std::vector<std::string>::iterator it = tokens.begin(); it != tokens.end(); ++it)
        av[ac++] = strdup((*it).c_str());

    return av;
}


static bool accLimitSet = false;
static bool allFlagsOut = false;

void parse(int argc, char **argv, CmdLineOptions &options, bool fromWorldFile)
{
    // prepare flag counts
    int i;

    // InertiaGameStyle maintained just for compatibility
    // Same effect is achieved setting linear/angular Acceleration
    options.gameOptions |= int(InertiaGameStyle);

    // parse command line
    int playerCountArg = 0,playerCountArg2 = 0;
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-a") == 0)
        {
            // momentum settings
            accLimitSet = true;
            checkArgc(2, i, argc, argv[i]);
            options.linearAcceleration = (float)atof(argv[i]);
            options.angularAcceleration = (float)atof(argv[++i]);
            if (options.linearAcceleration < 0.0f)
                options.linearAcceleration = 0.0f;
            if (options.angularAcceleration < 0.0f)
                options.angularAcceleration = 0.0f;
        }
        else if (strcmp(argv[i], "-adminlagannounce") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.adminlagannounce = atoi(argv[i])/1000.0f;
        }
        else if (strcmp(argv[i], "-admsg") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            if ((options.advertisemsg != "") || (strlen (argv[i]) == 0))
                options.advertisemsg += "\\n";
            options.advertisemsg += argv[i];
        }
        else if (strcmp(argv[i], "-advertise") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            if (checkCommaList (argv[i], 2048))
                std::cerr << "Invalid group list for -advertise" << std::endl;
            else
                options.advertiseGroups = argv[i];
        }
        else if (strcmp(argv[i], "-autoTeam") == 0)
            options.autoTeam = true;
        else if (strcmp(argv[i], "-b") == 0)
        {
            // random rotation to boxes in capture-the-flag game
            options.randomBoxes = true;
        }
        else if (strcmp(argv[i], "-badwords") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            options.filterFilename = argv[i];
        }
        else if (strcmp(argv[i], "-ban") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            options.acl.ban(argv[i]);
        }
        else if (strcmp(argv[i], "-banfile") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            options.acl.setBanFile(argv[i]);
            if (!options.acl.load())
            {
                std::cerr << "could not load banfile \"" << argv[i] << "\"" << std::endl;
                usage(argv[0]);
            }
        }
        else if (strcmp(argv[i], "-c") == 0)
        {
            // capture the flag style
            if (options.gameType == RabbitChase)
            {
                std::cerr << "Capture the flag incompatible with Rabbit Chase" << std::endl;
                std::cerr << "Capture the flag assumed" << std::endl;
            }
            options.gameType = ClassicCTF;
        }
        else if (strcmp(argv[i], "-cache") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            options.cacheURL = argv[i];
        }
        else if (strcmp(argv[i], "-cacheout") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            options.cacheOut = argv[i];
        }
        else if (strcmp(argv[i], "-conf") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            int ac;
            char **av;
            av = parseConfFile(argv[i], ac);
            // Theoretically we could merge the options specified in the conf file after parsing
            // the cmd line options. But for now just override them on the spot
            parse(ac, av, options);
            for (int j = 0; j < ac; j++)
                free(av[j]);
            delete[] av;
            options.numAllowedFlags = 0;
        }
        else if (strcmp(argv[i], "-cr") == 0)
        {
            // CTF with random world
            options.randomCTF = true;
            // capture the flag style
            if (options.gameType == RabbitChase)
            {
                std::cerr << "Capture the flag incompatible with Rabbit Chase" << std::endl;
                std::cerr << "Capture the flag assumed" << std::endl;
            }
            options.gameType = ClassicCTF;
        }
        else if (strcmp(argv[i], "-density") == 0)
        {
            if (i+1 != argc && isdigit(*argv[i+1]))
            {
                options.citySize = atoi(argv[i+1]);
                i++;
            }
            else
                checkArgc(1, i, argc, argv[i], "integer");
        }
        else if (strcmp(argv[i], "-disableBots") == 0)
        {
            // disallow clients from using autopilot or bots
            BZDB.set(StateDatabase::BZDB_DISABLEBOTS, "true");
        }
        else if (strncmp(argv[i], "-d", 2) == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            // increase debug level - this must be the last
            // option beginning with -d so that -dd, -ddd, etc. work
            const char num = argv[i][2];
            if ((num >= '0') && (num <= '9') && (argv[i][3] == 0))
                debugLevel = num - '0';
            else
            {
                int count = 0;
                const char *scan;
                for (scan = (argv[i] + 1); *scan == 'd'; scan++)
                    count++;
                if (*scan != '\0')
                {
                    std::cerr << "ERROR: bad argument [" << argv[i] << "]" << std::endl;
                    usage(argv[0]);
                }
                debugLevel += count;
                // std::cout << "Debug level is now " << debugLevel << "" << std::endl;
            }
        }
        else if (strcmp(argv[i], "-f") == 0)
        {
            // disallow given flag
            checkArgc(1, i, argc, argv[i]);
            storedFlagDisallows.push_back(argv[i]);
        }
        else if (strcmp(argv[i], "+f") == 0)
        {
            // add required flag
            checkArgc(1, i, argc, argv[i]);
            storedFlagCounts.push_back(argv[i]);
        }
        else if (strcmp(argv[i], "-fb") == 0)
        {
            // flags on buildings
            options.flagsOnBuildings = true;
        }
        else if (strcmp(argv[i], "-filterCallsigns") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            options.filterCallsigns = true;
        }
        else if (strcmp(argv[i], "-filterChat") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            options.filterChat = true;
        }
        else if (strcmp(argv[i], "-filterSimple") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            options.filterSimple = true;
        }
        else if (strcmp(argv[i], "-g") == 0)
            options.oneGameOnly = true;
        else if (strcmp(argv[i], "-gndtex") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            BzMaterial material;
            material.setName("GroundMaterial");
            material.setTexture(argv[i]);
            MATERIALMGR.addMaterial(&material);
        }
        else if (strcmp(argv[i], "-groupdb") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            groupsFile = argv[i];
            logDebugMessage(1,"using group file \"%s\"\n", argv[i]);
        }
        else if (strcmp(argv[i], "-h") == 0)
            options.randomHeights = true;
        else if (strcmp(argv[i], "-help") == 0)
            extraUsage(argv[0]);
        else if (strcmp(argv[i], "-helpmsg") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(2, i, argc, argv[i]);
            std::ifstream helpfile(argv[i]);
            if (!helpfile)
                std::cerr << "warning: helpmsg file \"" << argv[i] << "\" does not exist or can't be read" << std::endl;
            else if (!options.textChunker.parseFile(argv[i], argv[i+1], 50, MessageLen))
            {
                std::cerr << "couldn't read helpmsg file \"" << argv[i] << "\"" << std::endl;
                usage(argv[0]);
            }
            i++;
        }
        else if (strcmp(argv[i], "-i") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            // use a different interface
            checkArgc(1, i, argc, argv[i]);
            options.pingInterface = argv[i];
        }
        else if (strcmp(argv[i], "-j") == 0)
        {
            // allow jumping
            options.gameOptions |= int(JumpingGameStyle);
        }
        else if (strcmp(argv[i], "-handicap") == 0)
        {
            // allow handicap advantage
            options.gameOptions |= int(HandicapGameStyle);
        }
        else if (strcmp(argv[i], "-jitterdrop") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.maxjitterwarn = atoi(argv[i]);
        }
        else if (strcmp(argv[i], "-jitterwarn") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.jitterwarnthresh = atoi(argv[i])/1000.0f;
        }
        else if (strcmp(argv[i], "-lagannounce") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.lagannounce = atoi(argv[i])/1000.0f;
        }
        else if (strcmp(argv[i], "-lagdrop") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.maxlagwarn = atoi(argv[i]);
        }
        else if (strcmp(argv[i], "-lagwarn") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.lagwarnthresh = atoi(argv[i])/1000.0f;
        }
        else if (strcmp(argv[i], "-loadplugin") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            std::vector<std::string> a = TextUtils::tokenize(argv[i],std::string(","), 2);
            CmdLineOptions::pluginDef pDef;
            if (a.size() >= 1)
                pDef.plugin = a[0];
            if (a.size() >= 2)
                pDef.command = a[1];
            if (pDef.plugin.size())
                options.pluginList.push_back(pDef);
        }
        else if (strcmp(argv[i], "-loadpython") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            std::vector<std::string> a = TextUtils::tokenize(argv[i],std::string(","), 2);
            CmdLineOptions::pluginDef pDef;
            if (a.size() >= 1)
                pDef.plugin = a[0];
            if (a.size() >= 2)
                pDef.command = a[1];
            options.pythonPlugin = pDef;
        }
        else if (strcmp(argv[i], "-maxidle") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.idlekickthresh = (float) atoi(argv[i]);
        }
        else if (strcmp(argv[i], "-mp") == 0)
        {
            // set maximum number of players
            checkArgc(1, i, argc, argv[i]);
            if (playerCountArg == 0)
                playerCountArg = i;
            else
                playerCountArg2 = i;
        }
        else if (strcmp(argv[i], "-mps") == 0)
        {
            // set maximum player score
            checkArgc(1, i, argc, argv[i]);
            options.maxPlayerScore = atoi(argv[i]);
            if (options.maxPlayerScore < 1)
            {
                std::cerr << "disabling player score limit" << std::endl;
                options.maxPlayerScore = 0;
            }
        }
        else if (strcmp(argv[i], "-ms") == 0)
        {
            // set maximum number of shots
            checkArgc(1, i, argc, argv[i]);
            int newMaxShots = atoi(argv[i]);
            if (newMaxShots == 0)
            {
                std::cerr << "WARNING: tanks will not be able to shoot" << std::endl;
                options.maxShots = 0;
            }
            else if (newMaxShots < 1)
            {
                std::cerr << "using minimum number of shots of 1" << std::endl;
                options.maxShots = 1;
            }
            else if (newMaxShots > MaxShots)
            {
                std::cerr << "using maximum number of shots of " << MaxShots << std::endl;
                options.maxShots = uint16_t(MaxShots);
            }
            else options.maxShots = uint16_t(newMaxShots);
        }
        else if (strcmp(argv[i], "-mts") == 0)
        {
            // set maximum team score
            checkArgc(1, i, argc, argv[i]);
            options.maxTeamScore = atoi(argv[i]);
            if (options.maxTeamScore < 1)
            {
                std::cerr << "disabling team score limit" << std::endl;
                options.maxTeamScore = 0;
            }
        }
        else if (strcmp(argv[i],"-noMasterBanlist") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            options.suppressMasterBanList = true;
        }
        else if (strcmp(argv[i],"-noradar") == 0)
            BZDB.set(StateDatabase::BZDB_RADARLIMIT, "-1.0");
        else if (strcmp(argv[i],"-masterBanURL") == 0)
        {
            /* if this is the first master ban url, override the default
             * list.  otherwise just keep adding urls.
             */
            checkFromWorldFile(argv[i], fromWorldFile);
            if (!options.masterBanListOverridden)
            {
                options.masterBanListURL.clear();
                options.masterBanListOverridden = true;
            }
            checkArgc(1, i, argc, argv[i]);
            options.masterBanListURL.push_back(argv[i]);
        }
        else if (strcmp(argv[i], "-noTeamKills") == 0)
        {
            // disable team killing
            if (options.gameType == OpenFFA)
                std::cerr << "noteamkills check WARNING: -noTeamKills is incompatible with -offa, ignoring" << std::endl;
            else
                options.gameOptions |= int(NoTeamKillsGameStyle);
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            // use a different port
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            options.wksPort = atoi(argv[i]);
            if (options.wksPort < 1 || options.wksPort > 65535)
                options.wksPort = ServerPort;
            else
                options.useGivenPort = true;
        }
        else if (strcmp(argv[i], "-packetlossdrop") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.maxpacketlosswarn = atoi(argv[i]);
        }
        else if (strcmp(argv[i], "-packetlosswarn") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.packetlosswarnthresh = atoi(argv[i])/1000.0f;
        }
        else if (strcmp(argv[i], "-passwd") == 0 || strcmp(argv[i], "-password") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            // at least put password someplace that ps won't see
            options.password = argv[i];
            memset(argv[i], 'X', options.password.size());
        }
        else if (strcmp(argv[i], "-pidfile") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            unsigned int pid = 0;
            checkArgc(1, i, argc, argv[i]);
            FILE *fp = fopen(argv[i], "wt");
#ifndef _WIN32
            pid = getpid();
#else
            pid = _getpid();
#endif
            if (fp)
            {
                fprintf(fp, "%d", pid);
                fclose(fp);
            }
        }
        else if (strcmp(argv[i], "-poll") == 0)
        {
            // parse the variety of poll system variables
            checkArgc(1, i, argc, argv[i]);

            std::vector<std::string> args = TextUtils::tokenize(argv[i], std::string("="), 2, true);
            if (args.size() != 2)
            {
                std::cerr << "expected -poll variable=value" << std::endl;
                usage(argv[0]);
            }

            if (compare_nocase(args[0], "bantime") == 0)
                options.banTime = (unsigned short int)atoi(args[1].c_str());
            else if (compare_nocase(args[0], "vetotime") == 0)
                options.vetoTime = (unsigned short int)atoi(args[1].c_str());
            else if (compare_nocase(args[0], "votepercentage") == 0)
                options.votePercentage = (float)atof(args[1].c_str());
            else if (compare_nocase(args[0], "voterepeattime") == 0)
                options.voteRepeatTime = (unsigned short int)atoi(args[1].c_str());
            else if (compare_nocase(args[0], "votesrequired") == 0)
                options.votesRequired = (unsigned short int)atoi(args[1].c_str());
            else if (compare_nocase(args[0], "votetime") == 0)
                options.voteTime = (unsigned short int)atoi(args[1].c_str());
            else
                std::cerr << "unknown variable for -poll, skipping";
#ifdef PRINTSCORE
        }
        else if (strcmp(argv[i], "-printscore") == 0)
        {
            // dump score whenever it changes
            options.printScore = true;
#endif
        }
        else if (strcmp(argv[i], "-publictitle") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.publicizeServer = true;
            options.publicizedTitle = argv[i];
            if (options.publicizedTitle.length() > 127)
            {
                argv[i][127] = '\0';
                std::cerr << "description too long... truncated" << std::endl;
            }
        }
        else if (strcmp(argv[i], "-publicaddr") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            options.publicizedAddress = argv[i];
            options.publicizeServer = true;
        }
        else if (strcmp(argv[i], "-publiclist") == 0)
        {
            /* if this is the first -publiclist, override the default list
             * server.  otherwise just keep adding urls.
             */
            checkFromWorldFile(argv[i], fromWorldFile);
            if (!options.listServerOverridden)
            {
                options.listServerURL.clear();
                options.listServerOverridden = true;
            }
            checkArgc(1, i, argc, argv[i]);
            options.listServerURL.push_back(argv[i]);
        }
        else if (strcmp(argv[i], "-publickey") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            options.publicizeServer = true;
            // at least put publickey someplace that ps won't see
            options.publicizedKey = argv[i];
            memset(argv[i], 'X', options.publicizedKey.size());
            if (options.publicizedKey.length() == 0)
            {
                options.publicizeServer = false;
                std::cerr << "key too short server will not be made public" << std::endl;
            }
        }
        else if (strcmp(argv[i], "-q") == 0)
        {
            // don't handle pings
            checkFromWorldFile(argv[i], fromWorldFile);
            handlePings = false;
        }
        else if (strcmp(argv[i], "+r") == 0)
        {
            // all shots ricochet style
            options.gameOptions |= int(RicochetGameStyle);
        }
        else if (strcmp(argv[i], "-rabbit") == 0)
        {
            // rabbit chase style
            if (options.gameType == ClassicCTF)
            {
                std::cerr << "Rabbit Chase incompatible with Capture the flag" << std::endl;
                std::cerr << "Rabbit Chase assumed" << std::endl;
            }
            options.gameType = RabbitChase;
            // default selection style
            options.rabbitSelection = ScoreRabbitSelection;

            // if there are any arguments following, see if they are a
            // rabbit selection styles.
            if (i+1 != argc)
            {
                if (strcmp(argv[i+1], "score") == 0)
                {
                    options.rabbitSelection = ScoreRabbitSelection;
                    i++;
                }
                else if (strcmp(argv[i+1], "killer") == 0)
                {
                    options.rabbitSelection = KillerRabbitSelection;
                    i++;
                }
                else if (strcmp(argv[i+1], "random") == 0)
                {
                    options.rabbitSelection = RandomRabbitSelection;
                    i++;
                }
            }
        }
        else if (strcmp(argv[i], "-recbuf") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            Record::setSize (ServerPlayer, atoi(argv[i]));
            options.startRecording = true;
        }
        else if (strcmp(argv[i], "-recbufonly") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            Record::setAllowFileRecs (false);
        }
        else if (strcmp(argv[i], "-recdir") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            Record::setDirectory (argv[i]);
        }
        else if (strcmp(argv[i], "-replay") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            options.replayServer = true;
        }
        else if (strcmp(argv[i], "-reportfile") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            options.reportFile = argv[i];
        }
        else if (strcmp(argv[i], "-reportpipe") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            options.reportPipe = argv[i];
        }
        else if (strcmp(argv[i], "-tkannounce") == 0)
            options.tkAnnounce = true;
        else if (strcmp(argv[i], "+s") == 0 || strcmp(argv[i], "-s") == 0)
        {
            // with +s all flags are required to exist all the time
            allFlagsOut = argv[i][0] == '+' ? true : false;
            // set number of random flags
            if (i+1 < argc && isdigit(argv[i+1][0]))
            {
                ++i;
                if ((options.numExtraFlags = atoi(argv[i])) == 0)
                    options.numExtraFlags = 16;
            }
            else
                options.numExtraFlags = 16;
        }
        else if (strcmp(argv[i], "-sa") == 0)
        {
            // insert antidote flags
            options.gameOptions |= int(AntidoteGameStyle);
        }
        else if (strcmp(argv[i], "-sb") == 0)
        {
            // respawns on buildings
            options.respawnOnBuildings = true;
        }
        else if (strcmp(argv[i], "-set") == 0)
        {
            const char *name, *value;
            checkArgc(2, i, argc, argv[i]);
            name = argv[i];
            i++;
            value = argv[i];

            // give unknown BZDB variables the benefit of the doubt, allow plug-ins to define them
            if (!BZDB.isSet(name))
            {
                bzdbVarQueue[name] = value;
                logDebugMessage(1, "queued variable: %s = %s\n", name, value);

                continue;
            }

            BZDB.set(name, value);
            logDebugMessage(1, "set variable: %s = %s\n", name, BZDB.get(name).c_str());
        }
        else if (strcmp(argv[i], "-setforced") == 0)
        {
            const char *name, *value;
            checkArgc(2, i, argc, argv[i]);
            name = argv[i];
            i++;
            value = argv[i];

            BZDB.set(name, value);
            logDebugMessage(1, "set variable: %s = %s\n", name, BZDB.get(name).c_str());
        }
        else if (strcmp(argv[i], "-sl") == 0)
        {
            // shot limits
            checkArgc(2, i, argc, argv[i]);
            i++; // move past the flag descriptor for now (we'll store it in a bit)
            int x = 1;
            if (isdigit(argv[i][0]))
            {
                x = atoi(argv[i]);
                if (x < 1)
                {
                    std::cerr << "can only limit to 1 or more shots, changing to 1" << std::endl;
                    x = 1;
                }
            }
            else
            {
                std::cerr << "ERROR: invalid shot limit [" << argv[i] << "]" << std::endl;
                usage(argv[0]);
            }
            storedFlagLimits[argv[i-1]] = x;
        }
        else if (strcmp(argv[i], "-spamtime") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.msgTimer = atoi(argv[i]);
            logDebugMessage(1,"using spam time of %d seconds\n", options.msgTimer);
        }
        else if (strcmp(argv[i], "-spamwarn") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.spamWarnMax = atoi(argv[i]);
            logDebugMessage(1,"using spam warn threshold of %d\n", options.spamWarnMax);
        }
        else if (strcmp(argv[i], "-speedtol") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            speedTolerance = (float) atof(argv[i]);
            logDebugMessage(1,"using speed autokick tolerance of \"%f\"\n", speedTolerance);
        }
        else if (strcmp(argv[i], "-srvmsg") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            if ((options.servermsg != "") || (strlen (argv[i]) == 0))
                options.servermsg += "\\n";
            options.servermsg += argv[i];
        }
        else if (strcmp(argv[i], "-st") == 0)
        {
            // set shake timeout
            checkArgc(1, i, argc, argv[i]);
            float timeout = (float)atof(argv[i]);
            if (timeout < 0.1f)
            {
                options.shakeTimeout = 1;
                std::cerr << "using minimum shake timeout of " << 0.1f * (float)options.shakeTimeout << std::endl;
            }
            else if (timeout > 300.0f)
            {
                options.shakeTimeout = 3000;
                std::cerr << "using maximum shake timeout of " << 0.1f * (float)options.shakeTimeout << std::endl;
            }
            else
                options.shakeTimeout = uint16_t(timeout * 10.0f + 0.5f);
            options.gameOptions |= int(ShakableGameStyle);
        }
        else if (strcmp(argv[i], "-sw") == 0)
        {
            // set shake win count
            checkArgc(1, i, argc, argv[i]);
            int count = atoi(argv[i]);
            if (count < 1)
            {
                options.shakeWins = 1;
                std::cerr << "using minimum shake win count of " << options.shakeWins << std::endl;
            }
            else if (count > 20)
            {
                options.shakeWins = 20;
                std::cerr << "using maximum ttl of " << options.shakeWins << std::endl;
            }
            else
                options.shakeWins = uint16_t(count);
            options.gameOptions |= int(ShakableGameStyle);
        }
        else if (strcmp(argv[i], "-synctime") == 0)
        {
            // client clocks should be synchronized to server clock
            BZDB.set(StateDatabase::BZDB_SYNCTIME, "1.0"); // any positive number
        }
        else if (strcmp(argv[i], "-synclocation") == 0)
        {
            // client coordinates should be set to server coordinates
            BZDB.set(StateDatabase::BZDB_SYNCLOCATION, "true");
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            // allow teleporters
            options.useTeleporters = true;
            if (options.worldFile != "")
                std::cerr << "-t is meaningless when using a custom world, ignoring" << std::endl;
        }
        else if (strcmp(argv[i], "-offa") == 0)
        {
            // capture the flag style
            if (options.gameType == RabbitChase || options.gameType == ClassicCTF)
            {
                std::cerr << "Open (Teamless) Free-for-all incompatible with other modes" << std::endl;
                std::cerr << "Open Free-for-all assumed" << std::endl;
            }
            if ((options.gameOptions & int(NoTeamKillsGameStyle)) != 0)
            {
                std::cerr << "offa check WARNING: -noTeamKills is incompatible with -offa, ignoring" << std::endl;
                options.gameOptions ^= int(NoTeamKillsGameStyle);
            }
            options.gameType = OpenFFA;
        }
        else if (strcmp(argv[i], "-tftimeout") == 0)
        {
            // use team flag timeout
            checkArgc(1, i, argc, argv[i]);
            options.teamFlagTimeout = atoi(argv[i]);
            if (options.teamFlagTimeout < 0)
                options.teamFlagTimeout = 0;
            logDebugMessage(1,"using team flag timeout of %d seconds\n", options.teamFlagTimeout);
        }
        else if (strcmp(argv[i], "-time") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            const std::string timeStr = argv[i];
            // If there is no colon in the time, consider it to be the time limit in seconds
            if (timeStr.find(':') == std::string::npos)
                options.timeLimit = (float)atof(timeStr.c_str());
            // Otherwise treat it as the end time
            else
            {
                std::vector<std::string> endTime = TextUtils::tokenize(timeStr, std::string(":"));
                {
                    unsigned int sizer = endTime.size();
                    if (sizer > 3)
                    {
                        std::cerr << "ERROR: too many arguments to -time" << std::endl;
                        usage(argv[0]);
                    }
                    while (sizer != 3)
                    {
                        endTime.push_back("00");
                        ++sizer;
                    }

                    // Force the countdown to start right away
                    gameOver = false;
                    gameStartTime = TimeKeeper::getCurrent();
                    countdownActive = true;
                }
                int hour, min, sec;
                TimeKeeper::localTime(nullptr, nullptr, nullptr, &hour, &min, &sec);
                unsigned int cmdHour = atoi(endTime[0].c_str()),
                             cmdMin = atoi(endTime[1].c_str()),
                             cmdSec = atoi(endTime[2].c_str());
                unsigned long secsToday = (hour * 3600) + (min * 60) + sec,
                              secsTill = (cmdHour * 3600) + (cmdMin * 60) + cmdSec;
                if (secsToday > secsTill)   //if the requested time has already past
                {
                    options.timeLimit = (float)((86400 - secsToday) + secsTill); //secs left today + till req. time
                }
                else
                    options.timeLimit = (float)(secsTill - secsToday);
            }
            if (options.timeLimit <= 0.0f)
            {
                // league matches are 30 min
                options.timeLimit = 1800.0f;
            }
            logDebugMessage(1,"using time limit of %d seconds\n", (int)options.timeLimit);
        }
        else if (strcmp(argv[i], "-timemanual") == 0)
            options.timeManualStart = true;
        else if (strcmp(argv[i], "-tk") == 0)
        {
            // team killer does not die
            options.teamKillerDies = false;
        }
        else if (strcmp(argv[i], "-tkkr") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.teamKillerKickRatio = atoi(argv[i]);
            if (options.teamKillerKickRatio < 0)
            {
                options.teamKillerKickRatio = 0;
                std::cerr << "disabling team killer kick ratio";
            }
        }
        else if (strcmp(argv[i], "-ts") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            // timestamp output
            options.timestampLog = true;
            // if there is an argument following, see if it is 'micros'
            if (i+1 != argc)
            {
                if (strcasecmp(argv[i+1], "micros") == 0)
                {
                    options.timestampMicros = true;
                    i++;
                }
            }
        }
#ifdef HAVE_MINIUPNPC_MINIUPNPC_H
        else if (strcmp(argv[i], "-UPnP") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            // timestamp output
            options.UPnP = true;
        }
#endif
        else if (strcmp(argv[i], "-utc") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            // timestamp output
            options.timestampLog = true;
            options.timestampUTC = true;
        }
        else if (strcmp(argv[i], "-userdb") == 0)
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            userDatabaseFile = argv[i];
            logDebugMessage(1,"using userDB file \"%s\"\n", argv[i]);
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-version") == 0)
        {
            printVersion();
            exit(0);
        }
        else if (strcmp(argv[i], "-vars") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            options.bzdbVars = argv[i];
        }
        else if ((strcmp(argv[i], "-w") == 0) ||
                 (strcmp(argv[i], "-world") == 0))
        {
            checkFromWorldFile(argv[i], fromWorldFile);
            checkArgc(1, i, argc, argv[i]);
            options.worldFile = argv[i];
            int ac;
            char **av = parseWorldOptions(argv[i], ac);
            parse(ac, av, options, true); // true - from a world file

            for (int j = 0; j < ac; j++)
                free(av[j]);
            delete[] av;

            options.numAllowedFlags = 0; // FIXME - Huh, does a reset?

            if (options.useTeleporters)
                std::cerr << "-t is meaningless when using a custom world, ignoring" << std::endl;
        }
        else if (strcmp(argv[i], "-worldsize") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            BZDB.set(StateDatabase::BZDB_WORLDSIZE, TextUtils::format("%d",atoi(argv[i])*2));
            logDebugMessage(1,"using world size of \"%f\"\n", (float)BZDBCache::worldSize);
        }
        else if (strcmp(argv[i], "-ws") == 0)
        {
            checkArgc(1, i, argc, argv[i]);
            int sides = atoi(argv[i]);
            if (sides > 2)
                options.wallSides = sides;
        }
        else
        {
            std::cerr << "bad argument \"" << argv[i] << "\"" << std::endl;
            usage(argv[0]);
        }
    }

    // get player counts.  done after other arguments because we need
    // to ignore counts for rogues if rogues aren't allowed.
    if ((playerCountArg > 0) && !parsePlayerCount(argv[playerCountArg], options))
        usage(argv[0]);
    if ((playerCountArg2 > 0)
            && !parsePlayerCount(argv[playerCountArg2], options))
        usage(argv[0]);

    return;
}


static int getZoneTeamFlagCount(FlagType* flagType, EntryZones& entryZones,
                                const std::set<FlagType*>& forbidden)
{
    if (forbidden.find(flagType) != forbidden.end())
        return 0;
    // tally zone team flags
    int count = 0;
    const ZoneList& zl = entryZones.getZoneList();
    for (int z = 0; z < (int)zl.size(); z++)
    {
        const ZoneFlagMap& zfm = zl[z].getZoneFlagMap();
        ZoneFlagMap::const_iterator zfmIt;
        for (zfmIt = zfm.begin(); zfmIt != zfm.end(); ++zfmIt)
        {
            if (zfmIt->first == flagType)
                count += zfmIt->second;
        }
    }
    return count;
}


static int addZoneTeamFlags(int startIndex,
                            FlagType* flagType, EntryZones& entryZones,
                            const std::set<FlagType*>& forbidden)
{
    if (forbidden.find(flagType) != forbidden.end())
        return startIndex;
    // add zone team flags
    const ZoneList& zl = entryZones.getZoneList();
    for (int z = 0; z < (int)zl.size(); z++)
    {
        const ZoneFlagMap& zfm = zl[z].getZoneFlagMap();
        ZoneFlagMap::const_iterator zfmIt;
        for (zfmIt = zfm.begin(); zfmIt != zfm.end(); ++zfmIt)
        {
            if (zfmIt->first == flagType)
            {
                const int count = zfmIt->second;
                for (int c = 0; c < count; c++)
                {
                    entryZones.addZoneFlag(z, startIndex);
                    FlagInfo::get(startIndex++)->setRequiredFlag(flagType);
                }
            }
        }
    }
    return startIndex;
}


void finalizeParsing(int UNUSED(argc), char **argv,
                     CmdLineOptions &options, EntryZones& entryZones)
{
    // set queued BZDB variables; allow plugins to define valid BZDB variables
    for (auto it = bzdbVarQueue.begin(); it != bzdbVarQueue.end(); it++)
    {
        if (!BZDB.isSet(it->first))
        {
            std::cerr << "Unknown BZDB variable: " << it->first << std::endl;
            exit(1);
        }

        BZDB.set(it->first, it->second);
        BZDB.setDefault(it->first, it->second);
        logDebugMessage(1, "queued variable set: %s = %s\n", it->first.c_str(), BZDB.get(it->first).c_str());
    }

    if (options.flagsOnBuildings && !(options.gameOptions & JumpingGameStyle))
    {
        std::cerr << "flags on boxes requires jumping" << std::endl;
        usage(argv[0]);
    }

    if (options.gameType == RabbitChase)
    {
        for (int j = RedTeam; j <= PurpleTeam; j++)
        {
            if (options.maxTeam[j] > 0
                    && options.maxTeam[RogueTeam] != maxRealPlayers)
                std::cout << "only rogues are allowed in Rabbit Chase; zeroing out "
                          << Team::getName((TeamColor) j) << std::endl;
            options.maxTeam[j] = 0;
        }
        options.maxTeam[RabbitTeam] = 1;
        options.maxTeam[HunterTeam] = options.maxTeam[RogueTeam];
    }
    else
    {
        options.maxTeam[RabbitTeam] = 0;
        options.maxTeam[HunterTeam] = 0;
    }

    // disallowed flags
    std::vector<std::string>::iterator vsitr;
    for (vsitr = storedFlagDisallows.begin(); vsitr != storedFlagDisallows.end(); ++vsitr)
    {
        if (strcmp(vsitr->c_str(), "bad") == 0)
        {
            FlagSet badFlags = Flag::getBadFlags();
            for (FlagSet::iterator it = badFlags.begin(); it != badFlags.end(); ++it)
                options.flagDisallowed[*it] = true;
        }
        else if (strcmp(vsitr->c_str(), "good") == 0)
        {
            FlagSet goodFlags = Flag::getGoodFlags();
            for (FlagSet::iterator it = goodFlags.begin(); it != goodFlags.end(); ++it)
                options.flagDisallowed[*it] = true;
        }
        else
        {
            FlagType* fDesc = Flag::getDescFromAbbreviation(vsitr->c_str());
            if (fDesc == Flags::Null)
            {
                std::cerr << "ERROR: invalid flag [" << (*vsitr) << "]" << std::endl;
                usage(argv[0]);
            }
            options.flagDisallowed[fDesc] = true;
        }
    }
    storedFlagDisallows.clear();

    // explicitly added flags
    for (vsitr = storedFlagCounts.begin(); vsitr != storedFlagCounts.end(); ++vsitr)
    {
        size_t rptBgn = vsitr->find_first_of('{');
        int rptCnt = 1;
        if (rptBgn > 0)
        {
            rptCnt = atoi(vsitr->substr(rptBgn+1, vsitr->length() - rptBgn).c_str());
            if (rptCnt <= 0)
                rptCnt = 1;
            (*vsitr) = vsitr->substr(0, rptBgn);
        }
        if (strcmp(vsitr->c_str(), "good") == 0)
        {
            FlagSet goodFlags = Flag::getGoodFlags();
            for (FlagSet::iterator it = goodFlags.begin(); it != goodFlags.end(); ++it)
                options.flagCount[*it] += rptCnt;
        }
        else if (strcmp(vsitr->c_str(), "bad") == 0)
        {
            FlagSet badFlags = Flag::getBadFlags();
            for (FlagSet::iterator it = badFlags.begin(); it != badFlags.end(); ++it)
                options.flagCount[*it] += rptCnt;
        }
        else if (strcmp(vsitr->c_str(), "team") == 0)
        {
            for (int t = RedTeam; t <= PurpleTeam; t++)
                options.numTeamFlags[t] += rptCnt;
        }
        else
        {
            FlagType *fDesc = Flag::getDescFromAbbreviation(vsitr->c_str());
            if (fDesc == Flags::Null)
            {
                std::cerr << "ERROR: invalid flag [" << (*vsitr) << "]" << std::endl;
                usage(argv[0]);
            }
            else if (fDesc->flagTeam != NoTeam)
                options.numTeamFlags[fDesc->flagTeam] += rptCnt;
            else
                options.flagCount[fDesc] += rptCnt;
        }
    }
    storedFlagCounts.clear();

    if (!accLimitSet)
    {
        printf("Note: no acceleration limit has been set.  Players using \"mouse\n"
               "enhancements\" may cause problems on this server due to very high\n"
               "acceleration rates which are not handled well by dead reckoning\n"
               "algorithms.  To eliminate this warning, set the -a switch in your\n"
               "configuration.  '-a 50 38' is recommended for standard-speed servers.\n"
               "Higher speed servers will need higher values for -a in order to not\n"
               "affect gameplay.  '-a 0 0' may be used to shut this message up without\n"
               "affecting any players, including those using \"mouse enhancements\".\n");
    }


    // do we have any team flags?
    bool hasTeam = false;
    for (int p = RedTeam; p <= PurpleTeam; p++)
    {
        if (options.maxTeam[p] > 1)
        {
            hasTeam = true;
            break;
        }
    }

    std::set<FlagType*> forbidden;
    forbidden.insert(Flags::Null);

    // first disallow flags inconsistent with game style
    if (options.gameOptions & JumpingGameStyle)
        forbidden.insert(Flags::Jumping);
    else
        forbidden.insert(Flags::NoJumping);
    if (options.gameOptions & RicochetGameStyle)
        forbidden.insert(Flags::Ricochet);
    if (OBSTACLEMGR.getTeles().size() == 0)
        forbidden.insert(Flags::PhantomZone);
    if (!hasTeam)
    {
        forbidden.insert(Flags::Genocide);
        forbidden.insert(Flags::Colorblindness);
        forbidden.insert(Flags::Masquerade);
    }
    if ((options.gameType == ClassicCTF) == 0)
    {
        forbidden.insert(Flags::RedTeam);
        forbidden.insert(Flags::GreenTeam);
        forbidden.insert(Flags::BlueTeam);
        forbidden.insert(Flags::PurpleTeam);
    }
    if (options.maxTeam[RedTeam] <= 0)
        forbidden.insert(Flags::RedTeam);
    if (options.maxTeam[GreenTeam] <= 0)
        forbidden.insert(Flags::GreenTeam);
    if (options.maxTeam[BlueTeam] <= 0)
        forbidden.insert(Flags::BlueTeam);
    if (options.maxTeam[PurpleTeam] <= 0)
        forbidden.insert(Flags::PurpleTeam);

    // void the forbidden flags
    std::set<FlagType*>::const_iterator sit;
    for (sit = forbidden.begin(); sit != forbidden.end(); ++sit)
    {
        FlagType* ft = *sit;
        options.flagCount[ft] = 0;
        options.flagDisallowed[ft] = true;
    }

    // zone team flag counts
    const int zoneTeamFlagCounts[5] =
    {
        0, // rogue
        getZoneTeamFlagCount(Flags::RedTeam, entryZones, forbidden),
        getZoneTeamFlagCount(Flags::GreenTeam, entryZones, forbidden),
        getZoneTeamFlagCount(Flags::BlueTeam, entryZones, forbidden),
        getZoneTeamFlagCount(Flags::PurpleTeam, entryZones, forbidden)
    };

    // make sure there is at least one team flag for each active team
    if (options.gameType == ClassicCTF)
    {
        for (int col = RedTeam; col <= PurpleTeam; col++)
        {
            if ((options.maxTeam[col] > 0) &&
                    (options.numTeamFlags[col] <= 0) &&
                    (zoneTeamFlagCounts[col] <= 0))
                options.numTeamFlags[col] = 1;
        }
    }

    // make table of allowed extra flags
    if (options.numExtraFlags > 0)
    {
        // now count how many aren't disallowed
        for (FlagTypeMap::iterator it = FlagType::getFlagMap().begin();
                it != FlagType::getFlagMap().end(); ++it)
        {
            if (!options.flagDisallowed[it->second])
                options.numAllowedFlags++;
        }
        // if none allowed then no extra flags either
        if (options.numAllowedFlags == 0)
            options.numExtraFlags = 0;
        else
        {
            // types of extra flags allowed
            std::vector<FlagType*> allowedFlags;
            allowedFlags.clear();
            for (FlagTypeMap::iterator it = FlagType::getFlagMap().begin();
                    it != FlagType::getFlagMap().end(); ++it)
            {
                FlagType *fDesc = it->second;
                if ((fDesc == Flags::Null) || (fDesc->flagTeam != ::NoTeam))
                    continue;
                if (!options.flagDisallowed[it->second])
                    allowedFlags.push_back(it->second);
            }
            // Loading allowedFlags vector
            FlagInfo::setAllowed(allowedFlags);
        }
    }

    const ZoneList& zl = entryZones.getZoneList();

    // allocate space for extra flags
    numFlags = options.numExtraFlags;
    // allocate space for team flags
    if (options.gameType & ClassicCTF)
    {
        for (int col = RedTeam; col <= PurpleTeam; col++)
        {
            if (options.maxTeam[col] > 0)
                numFlags += options.numTeamFlags[col];
        }
    }
    // allocate space for normal flags
    for (FlagTypeMap::iterator it = FlagType::getFlagMap().begin();
            it != FlagType::getFlagMap().end(); ++it)
        numFlags += options.flagCount[it->second];
    // allocate space for zone flags (including teams flags)
    for (int z = 0; z < (int)zl.size(); z++)
    {
        const CustomZone& zone = zl[z];
        const ZoneFlagMap& zfm = zone.getZoneFlagMap();
        ZoneFlagMap::const_iterator zfmIt;
        for (zfmIt = zfm.begin(); zfmIt != zfm.end(); ++zfmIt)
        {
            if (forbidden.find(zfmIt->first) == forbidden.end())
                numFlags += zfmIt->second;
        }
    }

    FlagInfo::setSize(numFlags);

    // add team flags (ordered)
    int f = 0;
    if (options.gameType == ClassicCTF)
    {
        if (options.maxTeam[RedTeam] > 0)
        {
            f = addZoneTeamFlags(f, Flags::RedTeam, entryZones, forbidden);
            for (int n = 0; n < options.numTeamFlags[RedTeam]; n++)
                FlagInfo::get(f++)->setRequiredFlag(Flags::RedTeam);
        }
        if (options.maxTeam[GreenTeam] > 0)
        {
            f = addZoneTeamFlags(f, Flags::GreenTeam, entryZones, forbidden);
            for (int n = 0; n < options.numTeamFlags[GreenTeam]; n++)
                FlagInfo::get(f++)->setRequiredFlag(Flags::GreenTeam);
        }
        if (options.maxTeam[BlueTeam] > 0)
        {
            f = addZoneTeamFlags(f, Flags::BlueTeam, entryZones, forbidden);
            for (int n = 0; n < options.numTeamFlags[BlueTeam]; n++)
                FlagInfo::get(f++)->setRequiredFlag(Flags::BlueTeam);
        }
        if (options.maxTeam[PurpleTeam] > 0)
        {
            f = addZoneTeamFlags(f, Flags::PurpleTeam, entryZones, forbidden);
            for (int n = 0; n < options.numTeamFlags[PurpleTeam]; n++)
                FlagInfo::get(f++)->setRequiredFlag(Flags::PurpleTeam);
        }
    }

    // super flags?
    if (f < numFlags)
        options.gameOptions |= int(SuperFlagGameStyle);

    // shot limits
    std::map<std::string, int>::iterator msiitr;
    for (msiitr = storedFlagLimits.begin(); msiitr != storedFlagLimits.end(); ++msiitr)
    {
        FlagType *fDesc = Flag::getDescFromAbbreviation(msiitr->first.c_str());
        if (fDesc == Flags::Null)
        {
            std::cerr << "ERROR: invalid flag [" << msiitr->first << "]" << std::endl;
            usage(argv[0]);
        }
        else
        {
            // this parameter has already been validated
            options.flagLimit[fDesc] = msiitr->second;
        }
    }
    storedFlagLimits.clear();

    // add normal flags
    for (FlagTypeMap::iterator it2 = FlagType::getFlagMap().begin();
            it2 != FlagType::getFlagMap().end(); ++it2)
    {
        FlagType *fDesc = it2->second;

        if ((fDesc != Flags::Null) && (fDesc->flagTeam == NoTeam))
        {
            for (int j = 0; j < options.flagCount[fDesc]; j++)
            {
                FlagInfo *flag = FlagInfo::get(f++);
                if (!flag)
                    continue;
                flag->setRequiredFlag(fDesc);
            }
        }
    }
    // add zone flags
    for (int z = 0; z < (int)zl.size(); z++)
    {
        const CustomZone& zone = zl[z];
        const ZoneFlagMap& zfm = zone.getZoneFlagMap();
        ZoneFlagMap::const_iterator zfmIt;
        for (zfmIt = zfm.begin(); zfmIt != zfm.end(); ++zfmIt)
        {
            FlagType* ft = zfmIt->first;
            if ((ft->flagTeam == ::NoTeam) && // no team flags here
                    (forbidden.find(ft) == forbidden.end()))
            {
                const int count = zfmIt->second;
                for (int c = 0; c < count; c++, f++)
                {
                    FlagInfo *flag = FlagInfo::get(f);
                    if (!flag)
                        continue;
                    entryZones.addZoneFlag(z, f);
                    flag->setRequiredFlag(ft);
                }
            }
        }
    }
    // add extra flags
    for (; f < numFlags; f++)
    {
        FlagInfo *flag = FlagInfo::get(f);
        if (!flag)
            continue;
        flag->required = allFlagsOut;
    }

    // sum the sources of team flags
    if (options.gameType & ClassicCTF)
    {
        for (int col = RedTeam; col <= PurpleTeam; col++)
            options.numTeamFlags[col] += zoneTeamFlagCounts[col];
    }

    // debugging
    logDebugMessage(1,"type: %d\n", options.gameType);
    if (options.gameType == ClassicCTF)
        logDebugMessage(1,"  capture the flag\n");
    if (options.gameType == RabbitChase)
        logDebugMessage(1,"  rabbit chase\n");
    if (options.gameType == OpenFFA)
        logDebugMessage(1,"  open free-for-all\n");
    if (options.gameType == TeamFFA)
        logDebugMessage(1,"  teamed free-for-all\n");

    logDebugMessage(1,"options: %X\n", options.gameOptions);
    if (options.gameOptions & int(SuperFlagGameStyle))
        logDebugMessage(1,"  super flags allowed\n");
    if (options.gameOptions & int(JumpingGameStyle))
        logDebugMessage(1,"  jumping allowed\n");
    if (options.gameOptions & int(RicochetGameStyle))
        logDebugMessage(1,"  all shots ricochet\n");
    if (options.gameOptions & int(ShakableGameStyle))
        logDebugMessage(1,"  shakable bad flags: timeout=%f, wins=%i\n",
                        0.1f * float(options.shakeTimeout), options.shakeWins);
    if (options.gameOptions & int(AntidoteGameStyle))
        logDebugMessage(1,"  antidote flags\n");

    return;
}


// simple syntax check of comma-seperated list of group names (returns true if error)
bool checkCommaList (const char *list, int maxlen)
{
    int x = strlen (list);
    unsigned char c;
    if (x > maxlen)
        return true;
    if (*list==',' || list[x-1]==',')
        return true;
    while ((c=*list++) != '\0')
        if (c<' ' || c>'z' ||  c=='\'' || c=='"')
            return true;
    return false;
}



// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
