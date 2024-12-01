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

// TODO
// - convert to packet lists to  STL lists ?
// - compression (piped and/or inline)
// - modify bzflag client to search for highest PlayerID first
//   for name matching (so that messages aren't sent to ghosts)
// - improve skipping

// interface header
#include "RecordReplay.h"

// system headers
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <vector>
#ifndef _WIN32
#  include <sys/time.h>
#  include <unistd.h>
#else
#  include <direct.h>
#  ifndef S_ISDIR
#    define S_ISDIR(m) ((m) & _S_IFDIR)
#  endif
#endif
#ifndef _MSC_VER
#  include <dirent.h>
#endif

// common headers
#include "bzglob.h"
#include "Pack.h"
#include "GameTime.h"
#include "StateDatabase.h"
#include "DirectoryNames.h"
#include "NetHandler.h"
#include "WorldEventManager.h"
#include "md5.h"
#include "Score.h"
#include "version.h"

// bzfs specific headers
#include "bzfs.h"


// Type Definitions
// ----------------

typedef struct
{
    u32 byteCount;
    u32 packetCount;
    // into the head, out of the tail
    RRpacket *head; // last packet in
    RRpacket *tail; // first packet in
} RRbuffer;

typedef struct
{
    std::string file;
    float time;
    int entryNum;
} FileEntry;


// Local Variables
// ---------------

#define REPLAY_LABEL "[REPLAY] "

static const u32 ReplayMagic       = 0x7272425A; // "rrBZ"
static const u32 ReplayVersion     = 0x0001;
static const u32 DefaultMaxBytes   = (16 * 1024 * 1024); // 16 Mbytes
static const u32 DefaultUpdateRate = (10 * 1000000); // seconds
static const int MaxListOutput     = 100;

static std::string RecordDir = getRecordDirName();

static bool Recording = false;
static RecordType RecordMode = BufferedRecord;
static RRtime RecordStartTime = 0;
static RRtime RecordUpdateTime = 0;
static RRtime RecordUpdateRate = 0;
static u32 RecordMaxBytes = DefaultMaxBytes;
static u32 RecordFileBytes = 0;
static u32 RecordFilePackets = 0;
static u32 RecordFilePrevPos = 0;
static bool allowFileRecords = true;


static bool Replaying = false;
static bool ReplayMode = false;
static bool ReplayLoop = false;
static RRtime ReplayFileTime = 0;
static RRtime ReplayStartTime = 0;
static RRtime ReplayOffset = 0;
static long ReplayFileStart = 0;
static RRpacket *ReplayPos = NULL;

static TimeKeeper StartTime;

static RRbuffer ReplayBuf = {0, 0, NULL, NULL}; // for replaying
static RRbuffer RecordBuf = {0, 0, NULL, NULL};  // for recording

static FILE *ReplayFile = NULL;
static std::string ReplayFilename = "";

static FILE *RecordFile = NULL;
static std::string RecordFilename = "";


// Local Function Prototypes
// -------------------------

static bool saveStates();
static bool saveTeamsState();
static bool saveFlagsState();
static bool saveRabbitState();
static bool savePlayersState();
static bool saveVariablesState();
static bool saveGameTimeState();
static bool resetStates();

static bool setVariables(const void *data);
static bool preloadVariables();

static void rewind();

// saves straight to a file, or into the buffer
static bool routePacket(u16 code, int len, const void *data, u16 mode);

static RRpacket *nextPacket();
static RRpacket *prevPacket();
static RRpacket *nextFilePacket();
static RRpacket *prevFilePacket();
static RRpacket *nextStatePacket();
static RRpacket *prevStatePacket();

static bool savePacket(RRpacket *p, FILE *f);
static RRpacket *loadPacket(FILE *f);       // makes a new packet

static bool saveHeader(int playerIndex, RRtime filetime, FILE *f);
static bool loadHeader(ReplayHeader *h, FILE *f);
static bool saveFileTime(RRtime filetime, FILE *f);
static bool loadFileTime(RRtime *filetime, FILE *f);
static bool replaceFlagTypes(ReplayHeader *h);
static bool replaceSettings(ReplayHeader *h);
static bool replaceWorldDatabase(ReplayHeader *h);
static bool flagIsActive(FlagType *type);
static bool packFlagTypes(char *flags, u32 *flagsSize);

static bool getFileList(int playerIndex, std::vector<FileEntry>& entries);
static FILE *openFile(const char *filename, const char *mode);
static FILE *openWriteFile(int playerIndex, const char *filename);
static bool badFilename(const char *name);
static bool makeDirExist(const char *dirname);
static bool makeDirExistMsg(const char *dirname, int playerIndex);

static RRpacket *newPacket(u16 mode, u16 code, int len, const void *data);
static void addHeadPacket(RRbuffer *b, RRpacket *p); // add to the head
static void addTailPacket(RRbuffer *b, RRpacket *p); // add to the tail
static RRpacket *delTailPacket(RRbuffer *b);     // delete from the tail
static RRpacket *delHeadPacket(RRbuffer *b);     // delete from the head
static void freeBuffer(RRbuffer *buf);         // clean it out
static void initPacket(u16 mode, u16 code, int len, const void *data,
                       RRpacket *p);        // copy params into packet

static RRtime getRRtime();
static void *nboPackRRtime(void *buf, RRtime value);
static const void *nboUnpackRRtime(const void *buf, RRtime& value);

static bool checkReplayMode(int playerIndex);

static const char *msgString(u16 code);


/******************************************************************************/

// Record Functions

static bool recordReset()
{
    if (RecordFile != NULL)
    {
        // replace the elapsed time placeholder
        if (RecordMode == StraightToFile)
        {
            RRtime filetime = getRRtime() - RecordStartTime;
            saveFileTime(filetime, RecordFile);
        }
        fclose(RecordFile);
        RecordFile = NULL;
    }
    RecordFilename = "";
    freeBuffer(&RecordBuf);

    Recording = false;
    RecordMode = BufferedRecord;
    RecordFileBytes = 0;
    RecordFilePackets = 0;
    RecordFilePrevPos = 0;
    RecordStartTime = 0;
    RecordUpdateTime = 0;

    return true;
}


bool Record::init()
{
    RecordDir = getRecordDirName();
    RecordMaxBytes = DefaultMaxBytes;
    RecordUpdateRate = DefaultUpdateRate;
    recordReset();
    return true;
}


bool Record::kill()
{
    recordReset();
    return true;
}


bool Record::start(int playerIndex)
{
    if (ReplayMode)
    {
        sendMessage(ServerPlayer, playerIndex, "Couldn't start capturing");
        return false;
    }
    if (!makeDirExistMsg(RecordDir.c_str(), playerIndex))
        return false;
    Recording = true;
    saveStates();
    sendMessage(ServerPlayer, playerIndex, "Recording started");

    bz_RecordingStartedEventData_V1 eventData;
    eventData.playerID = playerIndex;

    worldEventManager.callEvents(bz_eRecordingStartedEvent, &eventData);

    return true;
}


bool Record::stop(int playerIndex)
{
    if (Recording == false)
    {
        sendMessage(ServerPlayer, playerIndex, "Couldn't stop capturing");
        return false;
    }

    bz_RecordingEndedEventData_V1 eventData;
    eventData.playerID = playerIndex;

    worldEventManager.callEvents(bz_eRecordingEndedEvent, &eventData);

    sendMessage(ServerPlayer, playerIndex, "Recording stopped");

    recordReset();

    return true;
}


const char* Record::getDirectory()
{
    return RecordDir.c_str();
}


bool Record::setDirectory(const char *dirname)
{
    int len = strlen(dirname);
    RecordDir = dirname;
    if (dirname[len - 1] != DirectorySeparator)
        RecordDir += DirectorySeparator;

    if (!makeDirExist(RecordDir.c_str()))
    {
        // they've been warned, leave it at that
        logDebugMessage(1,"Could not open or create -recdir directory: %s\n",
                        RecordDir.c_str());
        return false;
    }
    return true;
}


bool Record::setSize(int playerIndex, int Mbytes)
{
    char buffer[MessageLen];
    if (Mbytes <= 0)
        Mbytes = 1;
    RecordMaxBytes = Mbytes * (1024) * (1024);
    snprintf(buffer, MessageLen, "Record size set to %i", Mbytes);
    sendMessage(ServerPlayer, playerIndex, buffer);
    return true;
}


bool Record::setRate(int playerIndex, int seconds)
{
    char buffer[MessageLen];
    if (seconds <= 0)
        seconds = 1;
    else if (seconds > 30)
        seconds = 30;
    RecordUpdateRate = seconds * 1000000;
    snprintf(buffer, MessageLen, "Record rate set to %i", seconds);
    sendMessage(ServerPlayer, playerIndex, buffer);
    return true;
}


bool Record::sendStats(int playerIndex)
{
    if (!Recording)
    {
        sendMessage(ServerPlayer, playerIndex, "Not Recording");
        return false;
    }

    char buffer[MessageLen];
    float saveTime = 0.0f;

    if (RecordMode == BufferedRecord)
    {
        if ((RecordBuf.tail != NULL) && (RecordBuf.head != NULL))
        {
            RRtime diff = RecordBuf.head->timestamp - RecordBuf.tail->timestamp;
            saveTime = (float)diff / 1000000.0f;
        }
        snprintf(buffer, MessageLen,
                 "Buffered:  %i bytes / %i packets / %.1f seconds",
                 RecordBuf.byteCount, RecordBuf.packetCount, saveTime);
        sendMessage(ServerPlayer, playerIndex, buffer);
    }
    else
    {
        RRtime diff = getRRtime() - RecordStartTime;
        saveTime = (float)diff / 1000000.0f;
        snprintf(buffer, MessageLen,
                 "Filename:  %s", RecordFilename.c_str());
        sendMessage(ServerPlayer, playerIndex, buffer);
        snprintf(buffer, MessageLen,
                 "   saved:  %i bytes / %i packets / %.1f seconds",
                 RecordFileBytes, RecordFilePackets, saveTime);
        sendMessage(ServerPlayer, playerIndex, buffer);
    }

    return true;
}


bool Record::saveFile(int playerIndex, const char *filename)
{
    char buffer[MessageLen];
    std::string name = RecordDir;
    name += filename;

    if (ReplayMode)
    {
        sendMessage(ServerPlayer, playerIndex, "Can't record in replay mode");
        return false;
    }

    if (!allowFileRecords)
    {
        sendMessage(ServerPlayer, playerIndex,
                    "This server does not allow recording straight to a file");
        return false;
    }

    if (badFilename(filename))
    {
        sendMessage(ServerPlayer, playerIndex,
                    "Files must be with the local directory");
        return false;
    }

    recordReset();
    Recording = true;
    RecordMode = StraightToFile;

    RecordFile = openWriteFile(playerIndex, filename);
    if (RecordFile == NULL)
    {
        recordReset();
        snprintf(buffer, MessageLen, "Could not open for writing: %s", name.c_str());
        sendMessage(ServerPlayer, playerIndex, buffer);
        return false;
    }

    if (!saveHeader(playerIndex, 0 /* placeholder */, RecordFile))
    {
        recordReset();
        snprintf(buffer, MessageLen, "Could not save header: %s", name.c_str());
        sendMessage(ServerPlayer, playerIndex, buffer);
        return false;
    }

    if (!saveStates())
    {
        recordReset();
        snprintf(buffer, MessageLen, "Could not save states: %s", name.c_str());
        sendMessage(ServerPlayer, playerIndex, buffer);
        return false;
    }

    RecordStartTime = getRRtime();

    snprintf(buffer, MessageLen, "Recording to file: %s", name.c_str());
    sendMessage(ServerPlayer, playerIndex, buffer);

    RecordFilename = name.c_str();

    return true;
}


bool Record::saveBuffer(int playerIndex, const char *filename, int seconds)
{
    RRpacket *p = (RRpacket *)NULL;
    char buffer[MessageLen];
    std::string name = RecordDir;
    name += filename;

    if (ReplayMode)
    {
        sendMessage(ServerPlayer, playerIndex, "Can't record in replay mode");
        return false;
    }

    if (!Recording || (RecordMode != BufferedRecord))
    {
        sendMessage(ServerPlayer, playerIndex, "No buffer to save");
        return false;
    }

    if (badFilename(filename))
    {
        sendMessage(ServerPlayer, playerIndex,
                    "Files must be within the local directory");
        return false;
    }


    // setup the beginning position for the recording
    if (seconds != 0)
    {
        logDebugMessage(3,"Record: saving %i seconds to %s\n", seconds, name.c_str());
        // start the first update that happened at least 'seconds' ago
        p = RecordBuf.head;
        RRtime usecs = (RRtime)seconds * (RRtime)1000000;
        while (p != NULL)
        {
            if (p->mode == UpdatePacket)
            {
                RRtime diff = RecordBuf.head->timestamp - p->timestamp;
                if (diff >= usecs)
                    break;
            }
            p = p->prev;
        }
    }

    if ((seconds == 0) || (p == NULL))
    {
        // save the whole buffer from the first update
        p = RecordBuf.tail;
        while (p->mode != UpdatePacket)
            p = p->next;

        if (p == NULL)
        {
            sendMessage(ServerPlayer, playerIndex, "No buffer to save");
            return false;
        }
    }

    // setup the elapsed file time
    RRtime filetime = RecordBuf.head->timestamp - p->timestamp;

    RecordFile = openWriteFile(playerIndex, filename);
    if (RecordFile == NULL)
    {
        snprintf(buffer, MessageLen, "Could not open for writing: %s", name.c_str());
        sendMessage(ServerPlayer, playerIndex, buffer);
        return false;
    }

    if (!saveHeader(playerIndex, filetime, RecordFile))
    {
        fclose(RecordFile);
        RecordFile = NULL;
        snprintf(buffer, MessageLen, "Could not save header: %s", name.c_str());
        sendMessage(ServerPlayer, playerIndex, buffer);
        return false;
    }

    // Save the packets
    while (p != NULL)
    {
        savePacket(p, RecordFile);
        p = p->next;
    }

    fclose(RecordFile);
    RecordFile = NULL;
    RecordFileBytes = 0;
    RecordFilePackets = 0;
    RecordFilePrevPos = 0;

    snprintf(buffer, MessageLen, "Record buffer saved to: %s", name.c_str());
    sendMessage(ServerPlayer, playerIndex, buffer);

    return true;
}


bool Record::addPacket(u16 code, int len, const void * data, u16 mode)
{
    bool retval = false;

    // If this packet adds a player, save it before the
    // state update. If not, you'll get those annoying
    // "Server error when adding player" messages. I'd
    // just put all messages before the state updates,
    // but it's nice to be able to see the trigger message.

    if (code == MsgAddPlayer)
        retval = routePacket(code, len, data, mode);

    if ((getRRtime() - RecordUpdateTime) > (int)RecordUpdateRate)
    {
        // save the states periodically. if there's nothing happening
        // on the server, then this won't get called, and the file size
        // will not increase.
        saveStates();
    }

    if (code == MsgAddPlayer)
        return retval;
    else
        return routePacket(code, len, data, mode);
}


static bool routePacket(u16 code, int len, const void * data, u16 mode)
{
    if (!Recording)
        return false;

    if (RecordMode == BufferedRecord)
    {
        RRpacket *p = newPacket(mode, code, len, data);
        p->timestamp = getRRtime();
        addHeadPacket(&RecordBuf, p);
        logDebugMessage(4,"routeRRpacket(): mode = %i, len = %4i, code = %s, data = %p\n",
                        (int)p->mode, p->len, msgString(p->code), p->data);

        if (RecordBuf.byteCount > RecordMaxBytes)
        {
            logDebugMessage(4,"routePacket: deleting until State Update\n");
            while (((p = delTailPacket(&RecordBuf)) != NULL) &&
                    (p->mode != UpdatePacket))
            {
                delete[] p->data;
                delete p;
            }
        }
    }
    else
    {
        RRpacket p;
        p.timestamp = getRRtime();
        initPacket(mode, code, len, data, &p);
        savePacket(&p, RecordFile);
        logDebugMessage(4, "routeRRpacket(): mode = %i, len = %4i, code = %s, data = %p\n",
                        (int)p.mode, p.len, msgString(p.code), p.data);
    }

    return true;
}


void Record::setAllowFileRecs(bool value)
{
    allowFileRecords = value;
    return;
}


bool Record::getAllowFileRecs()
{
    return allowFileRecords;
}


bool Record::enabled()
{
    return Recording;
}


void Record::sendHelp(int playerIndex)
{
    sendMessage(ServerPlayer, playerIndex, "usage:");
    sendMessage(ServerPlayer, playerIndex, "  /record start");
    sendMessage(ServerPlayer, playerIndex, "  /record stop");
    sendMessage(ServerPlayer, playerIndex, "  /record size <Mbytes>");
    sendMessage(ServerPlayer, playerIndex, "  /record rate <seconds>");
    sendMessage(ServerPlayer, playerIndex, "  /record stats");
    sendMessage(ServerPlayer, playerIndex, "  /record save <filename> [seconds]");
    sendMessage(ServerPlayer, playerIndex, "  /record file <filename>");
    sendMessage(ServerPlayer, playerIndex, "  /record list [-t | -n | --] [pattern]");
    return;
}

/******************************************************************************/

// Replay Functions

static bool checkReplayMode(int playerIndex)
{
    if (!ReplayMode)
    {
        sendMessage(ServerPlayer, playerIndex,
                    "Server is not in replay mode. "
                    "Restart server with '-replay' option to enable playback.");
        return false;
    }
    return true;
}


static bool replayReset()
{
    if (ReplayFile != NULL)
    {
        fclose(ReplayFile);
        ReplayFile = NULL;
    }
    ReplayFilename = "";
    freeBuffer(&ReplayBuf);

    ReplayMode = true;
    Replaying = false;
    ReplayLoop = false;
    ReplayOffset = 0;
    ReplayFileStart = 0;
    ReplayPos = NULL;

    // reset the local view of the players' state
    for (int i = MaxPlayers; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *gkPlayer = GameKeeper::Player::getPlayerByIndex(i);
        if (gkPlayer != NULL)
            gkPlayer->player.setReplayState(ReplayNone);
    }

    return true;
}


bool Replay::init()
{
    if (Recording)
        return false;
    replayReset();

    return true;
}


bool Replay::kill()
{
    replayReset();
    return true;
}


static bool preloadVariables()
{
    RRpacket *p = ReplayBuf.tail;

    // find the first BZDB update packet in the first state update block
    while ((p != NULL) && (p->mode != RealPacket) && (p->code != MsgSetVar))
        p = p->next;
    if ((p == NULL) || (p->mode != StatePacket) || (p->code != MsgSetVar))
        return false;

    // load the variables into BZDB
    do
    {
        setVariables(p->data);
        p = p->next;
    }
    while ((p != NULL) && (p->mode == StatePacket) && (p->code == MsgSetVar));

    return true;
}


bool Replay::loadFile(int playerIndex, const char *filename)
{
    if (!checkReplayMode(playerIndex))
        return false;

    std::string indexname;
    if (filename[0] == '#')
    {
        std::vector<FileEntry> entries;
        if (!getFileList(playerIndex, entries))
        {
            sendMessage(ServerPlayer, playerIndex, "Could not get file index list");
            return false;
        }
        int index = atoi(filename + 1);
        if ((index < 1) || (index > (int)entries.size()))
        {
            sendMessage(ServerPlayer, playerIndex, "Invalid file index");
            return false;
        }
        indexname = entries[index - 1].file;
        filename = indexname.c_str();    // new filename
    }
    else
    {
        if (badFilename(filename))
        {
            sendMessage(ServerPlayer, playerIndex,
                        "Files must be in the recordings directory");
            return false;
        }
    }

    ReplayHeader header;
    RRpacket *p;
    char buffer[MessageLen];
    std::string name = RecordDir;
    name += filename;

    unloadFile(playerIndex);

    bz_ReplayRequestedEventData_V1 requestedEventData;
    requestedEventData.filename = filename;
    requestedEventData.playerID = playerIndex;

    ReplayFile = openFile(filename, "rb");

    if (ReplayFile == NULL)
    {
        snprintf(buffer, MessageLen, "Could not open: %s", name.c_str());
        requestedEventData.success = false;
    }
    else if (!loadHeader(&header, ReplayFile))
    {
        snprintf(buffer, MessageLen, "Could not open header: %s", name.c_str());
        fclose(ReplayFile);
        ReplayFile = NULL;
        requestedEventData.success = false;
    }
    else if (header.magic != ReplayMagic)
    {
        snprintf(buffer, MessageLen, "Not a bzflag replay file: %s", name.c_str());
        fclose(ReplayFile);
        ReplayFile = NULL;
        requestedEventData.success = false;
    }

    if (!requestedEventData.success)
    {
        sendMessage(ServerPlayer, playerIndex, requestedEventData.errorMsg = buffer);

        worldEventManager.callEvents(bz_eReplayRequestedEvent, &requestedEventData);

        return false;
    }

    // preload the buffer
    while (ReplayBuf.byteCount < RecordMaxBytes)
    {
        p = loadPacket(ReplayFile);
        if (p == NULL)
            break;
        else
            addHeadPacket(&ReplayBuf, p);
    }

    if (ReplayBuf.tail == NULL)
    {
        snprintf(buffer, MessageLen, "No valid data: %s", name.c_str());
        replayReset();

        requestedEventData.success = false;
    }
    else
    {
        ReplayPos = ReplayBuf.tail; // setup the initial position
        ReplayFileTime = header.filetime;
        ReplayStartTime = ReplayPos->timestamp;

        if (!preloadVariables())
        {
            snprintf(buffer, MessageLen, "Could not preload variables: %s", name.c_str());
            replayReset();

            requestedEventData.success = false;
        }
    }

    if (strlen(requestedEventData.errorMsg = buffer) > 0)
        sendMessage(ServerPlayer, playerIndex, requestedEventData.errorMsg);

    worldEventManager.callEvents(bz_eReplayRequestedEvent, &requestedEventData);

    if (!requestedEventData.success)
        return false;

    ReplayFilename = filename;

    bz_ReplayLoadedEventData_V1 loadedEventData;
    loadedEventData.filename = name.c_str();
    loadedEventData.authorCallsign = header.callSign;
    loadedEventData.authorMotto = header.motto;
    loadedEventData.protocol = header.ServerVersion;
    loadedEventData.serverVersion = header.appVersion;
    loadedEventData.seconds = (float)header.filetime / 1000000.0f;

    snprintf(buffer, MessageLen, "Loaded file:  %s", loadedEventData.filename);
    sendMessage(ServerPlayer, playerIndex, buffer);
    snprintf(buffer, MessageLen, "  author:     %s (%.79s)", loadedEventData.authorCallsign, loadedEventData.authorMotto);
    sendMessage(ServerPlayer, playerIndex, buffer);
    snprintf(buffer, MessageLen, "  protocol:   %.8s", loadedEventData.protocol);
    sendMessage(ServerPlayer, playerIndex, buffer);
    snprintf(buffer, MessageLen, "  server:     %.113s", loadedEventData.serverVersion);
    sendMessage(ServerPlayer, playerIndex, buffer);
    snprintf(buffer, MessageLen, "  seconds:    %.1f", loadedEventData.seconds);
    sendMessage(ServerPlayer, playerIndex, buffer);

    time_t startTime = (time_t)(ReplayPos->timestamp / 1000000);
    bz_makeApiTime(startTime, loadedEventData.start);
    snprintf(buffer, MessageLen, "  start:      %s", ctime(&startTime));
    sendMessage(ServerPlayer, playerIndex, buffer);

    time_t endTime = (time_t)((header.filetime + ReplayPos->timestamp) / 1000000);
    bz_makeApiTime(endTime, loadedEventData.end);
    snprintf(buffer, MessageLen, "  end:        %s", ctime(&endTime));
    sendMessage(ServerPlayer, playerIndex, buffer);

    worldEventManager.callEvents(bz_eReplayLoadedEvent, &loadedEventData);

    return true;
}

bool Replay::unloadFile(int playerIndex)
{
    return replayReset() && resetStates();
}


static FILE *getRecordFile(const char *filename)
{
    u32 magic;
    char buffer[sizeof(magic)];

    FILE *file = fopen(filename, "rb");
    if (file == NULL)
        return NULL;

    if (fread(buffer, sizeof(magic), 1, file) != 1)
    {
        fclose(file);
        return NULL;
    }

    nboUnpackUInt(buffer, magic);
    if (magic != ReplayMagic)
    {
        fclose(file);
        return NULL;
    }

    return file;
}


static bool getFileList(int playerIndex, std::vector<FileEntry>& entries)
{
    entries.clear();
    int entNum = 0;

#ifndef _MSC_VER

    DIR *dir;
    struct dirent *de;

    if (!makeDirExistMsg(RecordDir.c_str(), playerIndex))
        return false;

    dir = opendir(RecordDir.c_str());
    if (dir == NULL)
        return false;

    while ((de = readdir(dir)) != NULL)
    {
        std::string name = RecordDir;
        name += de->d_name;
        FILE *file = getRecordFile(name.c_str());
        if (file != NULL)
        {
            RRtime filetime;
            if (loadFileTime(&filetime, file))
            {
                FileEntry entry;
                entry.file = de->d_name;
                entry.time = (float)filetime / 1000000.0f;
                entry.entryNum = entNum++;
                entries.push_back(entry);
            }
            fclose(file);
        }
    }

    closedir(dir);

#else  // _MSC_VER

    if (!makeDirExistMsg(RecordDir.c_str(), playerIndex))
        return false;

    std::string pattern = RecordDir;
    pattern += "*";
    WIN32_FIND_DATA findData;
    HANDLE h = FindFirstFile(pattern.c_str(), &findData);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            std::string name = RecordDir;
            name += findData.cFileName;
            FILE *file = getRecordFile(name.c_str());
            if (file != NULL)
            {
                RRtime filetime;
                if (loadFileTime(&filetime, file))
                {
                    FileEntry entry;
                    entry.file = findData.cFileName;
                    entry.time = (float)filetime / 1000000.0f;
                    entry.entryNum = entNum++;
                    entries.push_back(entry);
                }
                fclose(file);
            }
        }
        while (FindNextFile(h, &findData));

        FindClose(h);
    }

#endif // _MSC_VER

    return true;
}


static bool sortFileTime (const FileEntry& a, const FileEntry& b)
{
    return (a.time < b.time);
}

static bool sortFileName (const FileEntry& a, const FileEntry& b)
{
    return (a.file < b.file);
}


static bool parseListOptions(const char* opts,
                             int& sortMode, std::string& pattern)
{
    // defaults
    pattern = "*";
    sortMode = Replay::SortNone;

    // parse the opts
    while (opts[0] != '\0')
    {
        while ((opts[0] != '\0') && isspace(opts[0]))
        {
            opts++; // eat whitespace
        }
        if (opts[0] == '-')
        {
            if (opts[1] == '-')
            {
                opts += 2;
                break; // end of options
            }
            else if (opts[1] == 't')
            {
                sortMode = Replay::SortByTime;
                opts += 2;
            }
            else if (opts[1] == 'n')
            {
                sortMode = Replay::SortByName;
                opts += 2;
            }
            else
            {
                return false; // unknown option
            }
        }
        else
            break;
    }

    while ((opts[0] != '\0') && isspace(opts[0]))
    {
        opts++; // eat whitespace
    }

    // setup the globbing pattern
    if (opts[0] != '\0')
    {
        pattern = opts;
        if (strstr(pattern.c_str(), "*") == NULL)
            pattern = "*" + pattern + "*";
    }

    return true;
}


bool Replay::sendFileList(int playerIndex, const char* options)
{
    int sortMode;
    std::string pattern;
    if (!parseListOptions(options, sortMode, pattern))
        return false;

    std::vector<FileEntry> entries;
    if (!getFileList(playerIndex, entries))
        return false;

    char buffer[MessageLen];
    snprintf(buffer, MessageLen, "dir:  %s",RecordDir.c_str());
    sendMessage(ServerPlayer, playerIndex, buffer);

    if (sortMode == SortByTime)
        std::sort (entries.begin(), entries.end(), sortFileTime);
    else if (sortMode == SortByName)
        std::sort (entries.begin(), entries.end(), sortFileName);

    int entriesSent = 0;
    for (unsigned int i = 0; i < entries.size(); i++)
    {
        const FileEntry& entry = entries[i];
        if (glob_match(pattern, entry.file))
        {
            entriesSent++;
            snprintf(buffer, MessageLen, "#%02i:  %-30s  [%9.1f seconds]",
                     entry.entryNum + 1, entry.file.c_str(), entry.time);
            sendMessage(ServerPlayer, playerIndex, buffer);
            if (entriesSent >= MaxListOutput)
            {
                snprintf(buffer, MessageLen, "Not listing more then %i entries, "
                         "try using pattern matching.", MaxListOutput);
                sendMessage(ServerPlayer, playerIndex, buffer);
                break;
            }
        }
    }

    return true;
}

bool Replay::exists(const char* filename)
{
#ifndef _MSC_VER

    DIR *dir;
    struct dirent *de;

    if (!makeDirExist(RecordDir.c_str()))
        return false;

    dir = opendir(RecordDir.c_str());
    if (dir == NULL)
        return false;

    while ((de = readdir(dir)) != NULL)
    {
        if (strcmp(de->d_name, filename) == 0)
            return true;
    }

    closedir(dir);

    return false;

#else  // _MSC_VER

    if (!makeDirExist(RecordDir.c_str()))
        return false;

    std::string pattern = RecordDir;
    pattern += "*";
    WIN32_FIND_DATA findData;
    HANDLE h = FindFirstFile(pattern.c_str(), &findData);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (strcmp(findData.cFileName, filename))
                return true;
        }
        while (FindNextFile(h, &findData));

        FindClose(h);
    }

    return false

#endif // _MSC_VER
}

bool Replay::play(int playerIndex)
{
    if (!checkReplayMode(playerIndex))
        return false;

    if (ReplayFile == NULL)
    {
        sendMessage(ServerPlayer, playerIndex, "No replay file loaded");
        return false;
    }

    Replaying = true;
    ReplayLoop = false;

    if (ReplayPos != NULL)
        ReplayOffset = getRRtime() - ReplayPos->timestamp;
    else
    {
        sendMessage(ServerPlayer, playerIndex, "Internal replay error");
        replayReset();
        return false;
    }

    // reset the replay observers' view of state
    resetStates();

    sendMessage(playerIndex, AllPlayers, REPLAY_LABEL "starting replay");

    return true;
}


bool Replay::loop(int playerIndex)
{
    if (!checkReplayMode(playerIndex))
        return false;

    if (ReplayFile == NULL)
    {
        sendMessage(ServerPlayer, playerIndex, "No replay file loaded");
        return false;
    }

    Replaying = true;
    ReplayLoop = true;

    if (ReplayPos != NULL)
        ReplayOffset = getRRtime() - ReplayPos->timestamp;
    else
    {
        sendMessage(ServerPlayer, playerIndex, "Internal replay error");
        replayReset();
        return false;
    }

    // reset the replay observers' view of state
    resetStates();

    sendMessage(playerIndex, AllPlayers, REPLAY_LABEL "starting replay loop");

    return true;
}


bool Replay::sendStats(int playerIndex)
{
    if (!checkReplayMode(playerIndex))
        return false;

    if (ReplayFile == NULL)
    {
        sendMessage(ServerPlayer, playerIndex, "No replay file loaded");
        return false;
    }

    if (ReplayPos == NULL)
    {
        sendMessage(ServerPlayer, playerIndex, "Internal replay error");
        return false;
    }

    char buffer[MessageLen];
    snprintf(buffer, MessageLen,
             "Replay File:  %s", ReplayFilename.c_str());
    sendMessage(ServerPlayer, playerIndex, buffer);

    time_t replayTime = (time_t)(ReplayPos->timestamp / 1000000);
    float fullTime = (float)ReplayFileTime / 1000000.0f;
    float usedTime =(float)(ReplayPos->timestamp - ReplayStartTime) / 1000000.0f;
    float percent = 100.0f * (usedTime / fullTime);
    snprintf(buffer, MessageLen,
             "Replay Date:  %s [%.2f %%]  (%.1f secs / %.1f secs)",
             ctime(&replayTime), percent, usedTime, fullTime);
    sendMessage(ServerPlayer, playerIndex, buffer);

    return true;
}


bool Replay::skip(int playerIndex, int seconds)
{
    if (!checkReplayMode(playerIndex))
        return false;

    if ((ReplayFile == NULL) || (ReplayPos == NULL))
    {
        sendMessage(ServerPlayer, playerIndex, "No replay file loaded");
        return false;
    }

    RRtime nowtime;
    if (Replaying)
        nowtime = getRRtime() - ReplayOffset;
    else
        nowtime = ReplayPos->timestamp;

    if (seconds != 0)
    {
        RRpacket *p = ReplayPos;
        RRtime target = nowtime + ((RRtime)seconds * (RRtime)1000000);

        if (seconds > 0)
        {
            do
            {
                p = nextStatePacket();
            }
            while ((p != NULL) && (p->timestamp < target));
            if (p == NULL)
                sendMessage(playerIndex, AllPlayers, REPLAY_LABEL "skipped to the end");
        }
        else
        {
            do
            {
                p = prevStatePacket();
            }
            while ((p != NULL) && (p->timestamp > target));
            if (p == NULL)
                sendMessage(playerIndex, AllPlayers, REPLAY_LABEL "skipped to the beginning");
        }

        // reset the replay observers' view of state
        resetStates();
    }

    // setup the new time offset
    ReplayOffset = getRRtime() - ReplayPos->timestamp;

    // print the amount of time skipped
    RRtime diff = ReplayPos->timestamp - nowtime;
    char buffer[MessageLen];
    snprintf(buffer, MessageLen, REPLAY_LABEL "skipped %.1f seconds (asked %i)",
             (float)diff/1000000.0f, seconds);
    sendMessage(playerIndex, AllPlayers, buffer);

    return true;
}


bool Replay::pause(int playerIndex)
{
    if ((ReplayFile != NULL) && (ReplayPos != NULL) && Replaying)
    {
        Replaying = false;
        sendMessage(playerIndex, AllPlayers, REPLAY_LABEL "paused");
    }
    else
    {
        sendMessage(ServerPlayer, playerIndex, "Can't pause, not playing");
        return false;
    }
    return true;
}


bool Replay::sendPackets()
{
    bool sent = false;
    RRpacket *p = ReplayPos;

    if (!Replaying)
        return false;

    while ((p != NULL) && (Replay::nextTime() < 0.0f))
    {
        if (p == NULL)
        {
            // this is a safety, it shouldn't happen
            resetStates();
            Replaying = false;
            ReplayPos = ReplayBuf.tail;
            sendMessage(ServerPlayer, AllPlayers, "Replay Finished!!!");
            return false;
        }

        logDebugMessage(4,"sendPackets(): mode = %i, len = %4i, code = %s, data = %p\n",
                        (int)p->mode, p->len, msgString(p->code), p->data);

        if (p->mode != HiddenPacket)
        {
            int i;

            // set the database variables if this is MsgSetVar
            if (p->code == MsgSetVar)
                setVariables(p->data);

            // send message to all replay observers
            for (i = MaxPlayers; i < curMaxPlayers; i++)
            {
                GameKeeper::Player *gkPlayer = GameKeeper::Player::getPlayerByIndex(i);
                if (gkPlayer == NULL)
                    continue;

                PlayerInfo *pi = &gkPlayer->player;

                if (pi->isPlaying())
                {
                    // State machine for State Updates
                    if (p->mode == UpdatePacket)
                    {
                        if (pi->getReplayState() == ReplayNone)
                            pi->setReplayState(ReplayReceiving);
                        else if (pi->getReplayState() == ReplayReceiving)
                        {
                            // two state updates in a row
                            pi->setReplayState(ReplayStateful);
                        }
                    }
                    else if (p->mode != StatePacket)
                    {
                        if (pi->getReplayState() == ReplayReceiving)
                            pi->setReplayState(ReplayStateful);
                    }

                    PlayerReplayState state = pi->getReplayState();
                    // send the packets
                    if (((p->mode == StatePacket) && (state == ReplayReceiving)) ||
                            ((p->mode == RealPacket) && (state == ReplayStateful)))
                    {
                        // the 4 bytes before p->data need to be allocated
                        void *buf = getDirectMessageBuffer();
                        memcpy(buf, p->data, p->len);
                        directMessage(i, p->code, p->len, buf);
                    }
                }

            } // for loop
        } // if (p->mode != HiddenPacket)
        else
            logDebugMessage(4,"  skipping hidden packet\n");

        p = nextPacket();
        sent = true;

    } // while loop

    if (p == NULL)
    {
        if (ReplayLoop)
        {
            rewind();
            resetStates();
            sendMessage(ServerPlayer, AllPlayers, "Replay Looping");
            return true;
        }
        else
        {
            resetStates();
            Replaying = false;
            ReplayPos = ReplayBuf.tail;
            sendMessage(ServerPlayer, AllPlayers, "Replay Finished");
            return false;
        }
    }

    if (sent && (ReplayPos->prev != NULL))
    {
        RRtime diff = (ReplayPos->timestamp - ReplayPos->prev->timestamp);
        if (diff > (10 * 1000000))
        {
            char buffer[MessageLen];
            sprintf(buffer, "No activity for the next %.1f seconds",
                    (float)diff / 1000000.0f);
            sendMessage(ServerPlayer, AllPlayers, buffer);
        }
    }

    return true;
}


float Replay::nextTime()
{
    if (!ReplayMode || !Replaying || (ReplayPos == NULL))
        return 1000.0f;
    else
    {
        RRtime diff = (ReplayPos->timestamp + ReplayOffset) - getRRtime();
        return (float)diff / 1000000.0f;
    }
}


bool Replay::enabled()
{
    return ReplayMode;
}


bool Replay::playing()
{
    return Replaying;
}

void Replay::sendHelp(int playerIndex)
{
    sendMessage(ServerPlayer, playerIndex, "usage:");
    sendMessage(ServerPlayer, playerIndex, "  /replay list [-t | -n | --] [pattern]");
    sendMessage(ServerPlayer, playerIndex, "  /replay load <filename|#index>");
    sendMessage(ServerPlayer, playerIndex, "  /replay play");
    sendMessage(ServerPlayer, playerIndex, "  /replay loop");
    sendMessage(ServerPlayer, playerIndex, "  /replay skip [+/-seconds]");
    sendMessage(ServerPlayer, playerIndex, "  /replay stats");
    return;
}


static void rewind()
{
    RRpacket* p;
    do
    {
        p = prevStatePacket();
    }
    while (p != NULL);

    // setup the new time offset
    ReplayOffset = getRRtime() - ReplayPos->timestamp;
}


static bool setVariables(const void *data)
{
    // copied this function from [playing.cxx]

    uint16_t numVars;
    uint8_t nameLen, valueLen;

    char name[MaxPacketLen];
    char value[MaxPacketLen];

    data = nboUnpackUShort(data, numVars);
    for (int i = 0; i < numVars; i++)
    {
        data = nboUnpackUByte(data, nameLen);
        data = nboUnpackString(data, name, nameLen);
        name[nameLen] = '\0';

        data = nboUnpackUByte(data, valueLen);
        data = nboUnpackString(data, value, valueLen);
        value[valueLen] = '\0';

        if (strcmp(name, "poll") != 0)
        {
            // do not save the poll state, it can
            // lead to SEGV's when players leave
            // and there is no ongoing poll
            // [see bzfs.cxx removePlayer()]
            BZDB.set(name, value);
        }
    }
    return true;
}


static RRpacket *nextPacket()
{
    if (ReplayPos == NULL)
    {
        ReplayPos = ReplayBuf.head;
        return NULL;
    }
    else if (ReplayPos->next != NULL)
    {
        ReplayPos = ReplayPos->next;
        return ReplayPos;
    }
    else
        return nextFilePacket();
}


static RRpacket *prevPacket()
{
    if (ReplayPos == NULL)
    {
        ReplayPos = ReplayBuf.tail;
        return NULL;
    }
    else if (ReplayPos->prev != NULL)
    {
        ReplayPos = ReplayPos->prev;
        return ReplayPos;
    }
    else
        return prevFilePacket();
}


static RRpacket *nextFilePacket()
{
    RRpacket *p = NULL;

    // set the file position
    if ((ReplayPos->nextFilePos == 0) ||
            (fseek(ReplayFile, ReplayPos->nextFilePos, SEEK_SET) < 0))
    {
    }
    else
    {
        p = loadPacket(ReplayFile);

        if (p != NULL)
        {
            RRpacket *tail = delTailPacket(&ReplayBuf);
            if (tail != NULL)
            {
                delete[] tail->data;
                delete tail;
            }
            addHeadPacket(&ReplayBuf, p);
        }
    }

    ReplayPos = ReplayBuf.head;
    return p;
}


static RRpacket *prevFilePacket()
{
    RRpacket *p = NULL;

    // set the file position
    if ((ReplayPos->prevFilePos <= 0) ||
            (fseek(ReplayFile, ReplayPos->prevFilePos, SEEK_SET) < 0))
    {
    }
    else
    {
        p = loadPacket(ReplayFile);

        if (p != NULL)
        {
            RRpacket *head = delHeadPacket(&ReplayBuf);
            if (head != NULL)
            {
                delete[] head->data;
                delete head;
            }
            addTailPacket(&ReplayBuf, p);
        }
    }

    ReplayPos = ReplayBuf.tail;
    return p;
}


static RRpacket *nextStatePacket()
{

    RRpacket *p = nextPacket();

    while ((p != NULL) && (p->mode != UpdatePacket))
        p = nextPacket();

    return p;
}


static RRpacket *prevStatePacket()
{
    RRpacket *p = prevPacket();

    while ((p != NULL) && (p->mode != UpdatePacket))
        p = prevPacket();

    return p;
}


/******************************************************************************/

// State Management Functions

// The goal is to save all of the states, such that if
// the packets are simply sent to a clean-state client,
// the client's state will end up looking like the state
// at the time which these functions were called.

static bool saveStates()
{
    routePacket(0, 0, NULL, UpdatePacket);

    // order is important
    saveVariablesState();
    saveTeamsState();
    saveFlagsState();
    savePlayersState();
    saveRabbitState();
    saveGameTimeState();

    RecordUpdateTime = getRRtime();

    return true;
}


static bool saveTeamsState()
{
    u16 i;
    char bufStart[MaxPacketLen];
    void *buf;

    buf = nboPackUByte(bufStart, CtfTeams);
    for (i = 0; i < CtfTeams; i++)
    {
        buf = nboPackUShort(buf, i);
        buf = team[i].team.pack(buf); // 3 ushorts: size, won, lost
    }

    routePacket(MsgTeamUpdate,
                (char*)buf - (char*)bufStart,  bufStart, StatePacket);

    return true;
}


// look at sendFlagUpdate() in bzfs.cxx ... very similar
static bool saveFlagsState()
{
    int flagIndex;
    char bufStart[MaxPacketLen];
    void *buf;

    buf = nboPackUShort(bufStart,0); //placeholder
    int cnt = 0;
    int length = sizeof(u16);

    for (flagIndex = 0; flagIndex < numFlags; flagIndex++)
    {
        FlagInfo &flag = *FlagInfo::get(flagIndex);
        if (flag.exist())
        {
            if ((length + sizeof(u16) + FlagPLen) > MaxPacketLen - 2*sizeof(u16))
            {
                // packet length overflow
                nboPackUShort(bufStart, cnt);
                routePacket(MsgFlagUpdate,
                            (char*)buf - (char*)bufStart, bufStart, StatePacket);

                cnt = 0;
                length = sizeof(u16);
                buf = nboPackUShort(bufStart,0); //placeholder
            }

            buf = flag.pack(buf);
            length += sizeof(u16)+FlagPLen;
            cnt++;
        }
    }

    if (cnt > 0)
    {
        nboPackUShort(bufStart, cnt);
        routePacket(MsgFlagUpdate,
                    (char*)buf - (char*)bufStart, bufStart, StatePacket);
    }

    return true;
}


static bool saveRabbitState()
{
    if (clOptions->gameType == RabbitChase)
    {
        char bufStart[MaxPacketLen];
        void *buf;
        buf = nboPackUByte(bufStart, rabbitIndex);
        routePacket(MsgNewRabbit, (char*)buf - (char*)bufStart, bufStart,
                    StatePacket);
    }
    return true;
}


static bool savePlayersState()
{
    int i, count = 0;
    char bufStart[MaxPacketLen];
    char infoBuf[MaxPacketLen];
    char adminBuf[MaxPacketLen];
    void *buf, *infoPtr, *adminPtr;

    // placeholders for the player counts
    infoPtr = infoBuf + sizeof(unsigned char);
    adminPtr = adminBuf + sizeof(unsigned char);

    for (i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *gkPlayer = GameKeeper::Player::getPlayerByIndex(i);
        if (gkPlayer == NULL)
            continue;
        PlayerInfo *pi = &gkPlayer->player;
        if (pi->isPlaying())
        {
            // Complete MsgAddPlayer
            buf = nboPackUByte(bufStart, i);
            buf = pi->packUpdate(buf);
            buf = gkPlayer->score.pack(buf);
            buf = pi->packId(buf);
            routePacket(MsgAddPlayer,
                        (char*)buf - (char*)bufStart, bufStart, StatePacket);
            // Part of MsgPlayerInfo
            infoPtr = gkPlayer->packPlayerInfo(infoPtr);
            // Part of MsgAdminInfo
            adminPtr = gkPlayer->packAdminInfo(adminPtr);

            count++;
        }
    }

    // As well as recording the original messages sent out when a player
    // first joins, we also resend the player and admin info during state
    // updates. This way, the scoreboard is current for players that joined
    // before an initializing state update.
    if (infoPtr != (infoBuf + sizeof(unsigned char)))
    {
        nboPackUByte(infoBuf, count);
        routePacket(MsgPlayerInfo,
                    (char*)infoPtr - (char*)infoBuf, infoBuf, StatePacket);
    }
    if (adminPtr != (adminBuf + sizeof(unsigned char)))
    {
        nboPackUByte(adminBuf, count);
        routePacket(MsgAdminInfo,
                    (char*)adminPtr - (char*)adminBuf, adminBuf, HiddenPacket);
        // use a hidden packet for the IPs
    }

    for (i = 0; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *gkPlayer = GameKeeper::Player::getPlayerByIndex(i);
        if (gkPlayer == NULL)
            continue;
        PlayerInfo *pi = &gkPlayer->player;
        if (pi->isAlive())
        {
            float pos[3] = {0.0f, 0.0f, 0.0f};
            // Complete MsgAlive
            buf = nboPackUByte(bufStart, i);
            buf = nboPackVector(buf, pos);
            buf = nboPackFloat(buf, pos[0]); // azimuth
            routePacket(MsgAlive,
                        (char*)buf - (char*)bufStart, bufStart, StatePacket);
        }
    }

    return true;
}


typedef struct
{
    void *bufStart;
    void *buf;
    int len;
    int count;
} packVarData;

static void packVars(const std::string& key, void *data)
{
    packVarData& pvd = *((packVarData*) data);
    std::string value = BZDB.get(key);
    int pairLen = key.length() + 1 + value.length() + 1;
    if ((pairLen + pvd.len) > (int)(MaxPacketLen - 2*sizeof(u16)))
    {
        nboPackUShort(pvd.bufStart, pvd.count);
        pvd.count = 0;
        routePacket(MsgSetVar, pvd.len, pvd.bufStart, StatePacket);
        pvd.buf = nboPackUShort(pvd.bufStart, 0); //placeholder
        pvd.len = sizeof(u16);
    }

    pvd.buf = nboPackUByte(pvd.buf, (uint8_t)key.length());
    pvd.buf = nboPackString(pvd.buf, key.c_str(), key.length());
    pvd.buf = nboPackUByte(pvd.buf, (uint8_t)value.length());
    pvd.buf = nboPackString(pvd.buf, value.c_str(), value.length());
    pvd.len += pairLen;
    pvd.count++;
}

static bool saveVariablesState()
{
    // This is basically a PackVars.h rip-off, with the
    // difference being that instead of sending packets
    // to the network, it sends them to routePacket().

    char buffer[MaxPacketLen];
    packVarData pvd;

    pvd.bufStart = buffer;
    pvd.buf      = buffer + sizeof(u16); // u16 placeholder for count
    pvd.len      = sizeof(u16);
    pvd.count    = 0;

    BZDB.iterate(packVars, &pvd);
    if (pvd.len > 0)
    {
        nboPackUShort(pvd.bufStart, pvd.count);
        routePacket(MsgSetVar, pvd.len, pvd.bufStart, StatePacket);
    }
    return true;
}


static bool saveGameTimeState()
{
    // FIXME: the packets that are sent out during replay will not
    //    be properly lag compensated for the connections over
    //    which they are sent.
    //    - provide GameTime with an offset capability
    //    - allow resetting of players' next GameTime update
    //      (and do so during a replay state update)
    //    - send GameTime packets out regardless of replay state
    char buffer[MaxPacketLen];
    void* buf = GameTime::pack(buffer, 0.150f);
    int length = (char*)buf - buffer;
    routePacket(MsgGameTime, length, buffer, RealPacket);
    return true;
}


static bool resetStates()
{
    int i;
    void *buf, *bufStart = getDirectMessageBuffer();

    // reset team scores
    buf = nboPackUByte(bufStart, CtfTeams);
    for (i = 0; i < CtfTeams; i++)
    {
        buf = nboPackUShort(buf, i);
        buf = team[i].team.pack(buf);
    }
    for (i = MaxPlayers; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *gkPlayer = GameKeeper::Player::getPlayerByIndex(i);
        if (gkPlayer == NULL)
            continue;
        if (gkPlayer->player.isPlaying())
            directMessage(i, MsgTeamUpdate, (char*)buf-(char*)bufStart, bufStart);
    }

    // reset players and flags using MsgReplayReset
    buf = nboPackUByte(bufStart, MaxPlayers); // the last player to remove
    for (i = MaxPlayers; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *gkPlayer = GameKeeper::Player::getPlayerByIndex(i);
        if (gkPlayer == NULL)
            continue;
        if (gkPlayer->player.isPlaying())
            directMessage(i, MsgReplayReset, (char*)buf-(char*)bufStart, bufStart);
    }

    // reset the local view of the players' state
    for (i = MaxPlayers; i < curMaxPlayers; i++)
    {
        GameKeeper::Player *gkPlayer = GameKeeper::Player::getPlayerByIndex(i);
        if (gkPlayer == NULL)
            continue;
        gkPlayer->player.setReplayState(ReplayNone);
    }

    return true;
}


/******************************************************************************/

// File Functions

// The replay files should work on different machine
// types, so everything is saved in network byte order.

static bool savePacket(RRpacket *p, FILE *f)
{
    char bufStart[RRpacketHdrSize];
    void *buf;

    if (f == NULL)
        return false;

    // file pointer to the next packet location
    u32 thisFilePos = ftell(f);
    u32 nextFilePos = ftell(f) + RRpacketHdrSize + p->len;

    buf = nboPackUShort(bufStart, p->mode);
    buf = nboPackUShort(buf, p->code);
    buf = nboPackUInt(buf, p->len);
    buf = nboPackUInt(buf, nextFilePos);
    buf = nboPackUInt(buf, RecordFilePrevPos);
    buf = nboPackRRtime(buf, p->timestamp);

    if (fwrite(bufStart, RRpacketHdrSize, 1, f) != 1)
        return false;

    if ((p->len != 0) && (fwrite(p->data, p->len, 1, f) != 1))
        return false;

    RecordFileBytes += p->len + RRpacketHdrSize;
    RecordFilePackets++;
    RecordFilePrevPos = thisFilePos;

    return true;
}


static RRpacket *loadPacket(FILE *f)
{
    RRpacket *p;
    char bufStart[RRpacketHdrSize];
    const void *buf;

    if (f == NULL)
        return NULL;

    p = new RRpacket;

    if (fread(bufStart, RRpacketHdrSize, 1, f) != 1)
    {
        delete p;
        return NULL;
    }
    buf = nboUnpackUShort(bufStart, p->mode);
    buf = nboUnpackUShort(buf, p->code);
    buf = nboUnpackUInt(buf, p->len);
    buf = nboUnpackUInt(buf, p->nextFilePos);
    buf = nboUnpackUInt(buf, p->prevFilePos);
    buf = nboUnpackRRtime(buf, p->timestamp);

    if (p->len > (MaxPacketLen - ((int)sizeof(u16) * 2)))
    {
        fprintf(stderr, "loadRRpacket: ERROR, packtlen = %i\n", p->len);
        delete p;
        replayReset();
        return NULL;
    }

    if (p->len == 0)
        p->data = NULL;
    else
    {
        char *d = new char [p->len];
        if (fread(d, p->len, 1, f) != 1)
        {
            delete[] d;
            delete p;
            return NULL;
        }
        p->data = d;
    }

    logDebugMessage(4,"loadRRpacket(): mode = %i, len = %4i, code = %s, data = %p\n",
                    (int)p->mode, p->len, msgString(p->code), p->data);

    return p;
}


static FILE *openFile(const char *filename, const char *mode)
{
    std::string name = RecordDir.c_str();
    name += DirectorySeparator;
    name += filename;

    return fopen(name.c_str(), mode);
}


static FILE *openWriteFile(int playerIndex, const char *filename)
{
    if (!makeDirExistMsg(RecordDir.c_str(), playerIndex))
        return NULL;

    return openFile(filename, "wb");
}

static inline int osStat(const char *dir, struct stat *buf)
{
#ifdef _WIN32
    // Windows sucks yet again, if there is a trailing  "\"
    // at the end of the filename, _stat will return -1.
    std::string dirname = dir;
    while (dirname.find_last_of('\\') == (dirname.size() - 1))
        dirname.resize(dirname.size() - 1);
    return _stat(dirname.c_str(), (struct _stat *) buf);
#else
    return stat(dir, buf);
#endif
}

static inline int osMkDir(const char *dir, int mode)
{
#ifdef _WIN32
    mode = mode;
    return mkdir(dir);
#else
    return mkdir(dir, mode);
#endif
}

static bool makeDirExist(const char *dirname)
{
    struct stat statbuf;

    // does the file exist?
    if (osStat(dirname, &statbuf) < 0)
    {
        // try to make the directory
        if (osMkDir(dirname, 0755) < 0)
            return false;
    }
    // is it a directory?
    else if (!S_ISDIR (statbuf.st_mode))
        return false;

    return true;
}


static bool makeDirExistMsg(const char *dirname, int playerIndex)
{
    if (!makeDirExist(dirname))
    {
        char buffer[MessageLen];
        sendMessage(ServerPlayer, playerIndex,
                    "Could not open or create record directory:");
        snprintf(buffer, MessageLen, "  %s", RecordDir.c_str());
        sendMessage(ServerPlayer, playerIndex, buffer);
        return false;
    }
    return true;
}


static bool badFilename(const char *name)
{
    while (name[0] != '\0')
    {
        switch (name[0])
        {
        case '/':
        case ':':
        case '\\':
        {
            return true;
        }
        case '.':
        {
            if (name[1] == '.')
                return true;
        }
        }
        name++;
    }
    return false;
}


static bool saveHeader(int p, RRtime filetime, FILE *f)
{
    char buffer[ReplayHeaderSize];
    char flagsBuf[MaxPacketLen]; // for the FlagType's
    void *buf;
    ReplayHeader hdr;

    if (f == NULL)
        return false;

    // player callsign and motto
    const char* callsign = "SERVER";
    const char* motto = "";
    if (p != ServerPlayer)
    {
        GameKeeper::Player *gkPlayer = GameKeeper::Player::getPlayerByIndex(p);
        if (gkPlayer == NULL)
            return false;
        else
        {
            PlayerInfo *pi = &gkPlayer->player;
            callsign = pi->getCallSign();
            motto = pi->getMotto();
        }
    }

    // setup the data
    memset(&hdr, 0, sizeof(hdr));
    strncpy(hdr.callSign, callsign, sizeof(hdr.callSign) - 1);
    strncpy(hdr.motto, motto, sizeof(hdr.motto) - 1);
    memcpy(hdr.ServerVersion, getServerVersion(), sizeof(hdr.ServerVersion));
    strncpy(hdr.appVersion, getAppVersion(), sizeof(hdr.appVersion) - 1);
    strncpy(hdr.realHash, hexDigest.c_str(), sizeof(hdr.realHash) - 1);
    packFlagTypes(flagsBuf, &hdr.flagsSize);
    hdr.flags = flagsBuf;

    int totalSize = ReplayHeaderSize + worldDatabaseSize + hdr.flagsSize;

    // pack the data
    buf = nboPackUInt(buffer, ReplayMagic);
    buf = nboPackUInt(buf, ReplayVersion);
    buf = nboPackUInt(buf, totalSize);
    buf = nboPackRRtime(buf, filetime); // placeholder when saving to file
    buf = nboPackUInt(buf, p); // player index
    buf = nboPackUInt(buf, hdr.flagsSize);
    buf = nboPackUInt(buf, worldDatabaseSize);
    buf = nboPackString(buf, hdr.callSign, sizeof(hdr.callSign));
    buf = nboPackString(buf, hdr.motto, sizeof(hdr.motto));
    buf = nboPackString(buf, hdr.ServerVersion, sizeof(hdr.ServerVersion));
    buf = nboPackString(buf, hdr.appVersion, sizeof(hdr.appVersion));
    buf = nboPackString(buf, hdr.realHash, sizeof(hdr.realHash));
    buf = nboPackString(buf, worldSettings, sizeof(worldSettings));

    // store the data
    if (fwrite(buffer, ReplayHeaderSize, 1, f) != 1)
        return false;
    if (hdr.flagsSize > 0)
    {
        if (fwrite(hdr.flags, hdr.flagsSize, 1, f) != 1)
            return false;
    }
    if (fwrite(worldDatabase, worldDatabaseSize, 1, f) != 1)
        return false;

    RecordFileBytes += totalSize;

    return true;
}


static bool loadHeader(ReplayHeader *h, FILE *f)
{
    char buffer[ReplayHeaderSize];
    const void *buf;

    if (fread(buffer, ReplayHeaderSize, 1, f) != 1)
        return false;

    buf = nboUnpackUInt(buffer, h->magic);
    buf = nboUnpackUInt(buf, h->version);
    buf = nboUnpackUInt(buf, h->offset);
    buf = nboUnpackRRtime(buf, h->filetime);
    buf = nboUnpackUInt(buf, h->player);
    buf = nboUnpackUInt(buf, h->flagsSize);
    buf = nboUnpackUInt(buf, h->worldSize);
    buf = nboUnpackString(buf, h->callSign, sizeof(h->callSign));
    buf = nboUnpackString(buf, h->motto, sizeof(h->motto));
    buf = nboUnpackString(buf, h->ServerVersion, sizeof(h->ServerVersion));

    // A change (a80ab87) added between 2.4.12 and 2.4.14 resulted in the header being packed wrong.
    // Detect if this has occurred and work around the issue.
    if (buffer[313] == '\0' && (buffer[314] == 't' || buffer[314] == 'p'))
        buf = nboUnpackString(buf, h->appVersion, sizeof(h->appVersion) - 14);
    else
        buf = nboUnpackString(buf, h->appVersion, sizeof(h->appVersion));
    buf = nboUnpackString(buf, h->realHash, sizeof(h->realHash));
    buf = nboUnpackString(buf, h->worldSettings, sizeof(h->worldSettings));

    // load the flags, if there are any
    if (h->flagsSize > 0)
    {
        h->flags = new char [h->flagsSize];
        if (fread(h->flags, h->flagsSize, 1, f) != 1)
            return false;
    }
    else
        h->flags = NULL;

    // load the world database
    h->world = new char [h->worldSize];
    if (fread(h->world, h->worldSize, 1, f) != 1)
        return false;

    // remember where the header ends
    ReplayFileStart = ftell(f);

    // do the worldDatabase or flagTypes need to be replaced?
    bool replaced = false;
    if (replaceFlagTypes(h))
    {
        logDebugMessage(1,"Replay: replaced flags\n");
        replaced = true;
    }
    if (replaceSettings(h))
    {
        logDebugMessage(1,"Replay: replaced settings\n");
        replaced = true;
    }
    if (replaceWorldDatabase(h))
    {
        logDebugMessage(1,"Replay: replaced world database\n");
        replaced = true;
    }

    if (replaced)
    {
        sendMessage(ServerPlayer, AllPlayers,
                    "An incompatible recording has been loaded");
        sendMessage(ServerPlayer, AllPlayers,
                    "Please rejoin or face the consequences (client crashes)");
    }
    /* FIXME -- having to rejoin when replay files are loaded
     *
     * Ok, this is where it gets a bit borked. The bzflag client
     * has dynamic arrays for some of its objects (players, flags,
     * shots, etc...) If the client array is too small, there will
     * be memory overruns. The maxPlayers problem is already dealt
     * with, because it is set to (MaxPlayers + ReplayObservers)
     * as soon as the -replay flag is used. The rest of them are
     * still an issue.
     *
     * Here are a few of options:
     *
     * 1) make the command line option  -replay <filename>, and
     *    only allow loading of world DB's that match the one
     *    from the command line file. This is probably how this
     *    feature will get used for the most part anyways.
     *
     * 2) kick all observers off of the server if an incompatible
     *    record file is loaded (with an appropriate warning).
     *    then they can reload with the original DB upon rejoining
     *    (DB with modified maxPlayers).
     *
     * 3) make fixed sized arrays on the client side
     *    (but what if someone really needs 1000 flags?)
     *
     * 4) implement a world reload feature on the client side,
     *    so that if the server sends a MsgGetWorld to the client
     *    when it isn't expecting one, it reacquires and regenerates
     *    its world DB. this would be the slick way to do it.
     *
     * 5) implement a resizing command, but that's icky.
     *
     * 6) leave it be, and let clients fall where they may.
     *
     * 7) MAC: get to the client to use STL, so segv's aren't a problem
     *     (and kick 'em anyways, to force a map reload)
     *
     *
     * maxPlayers [from WorldBuilder.cxx]
     *   world->players = new RemotePlayer*[world->maxPlayers];
     *
     * maxFlags [from WorldBuilder.cxx]
     *   world->flags = new Flag[world->maxFlags];
     *   world->flagNodes = new FlagSceneNode*[world->maxFlags];
     *   world->flagWarpNodes = new FlagWarpSceneNode*[world->maxFlags];
     *
     * maxShots [from RemotePlayer.cxx]
     *   numShots = World::getWorld()->getMaxShots();
     *   shots = new RemoteShotPath*[numShots];
     */

    return true;
}


static bool saveFileTime(RRtime filetime, FILE *f)
{
    rewind(f);
    if (fseek(f, sizeof(u32) * 3, SEEK_SET) < 0)
        return false;
    char buffer[sizeof(RRtime)];
    nboPackRRtime(buffer, filetime);
    if (fwrite(buffer, sizeof(RRtime), 1, f) != 1)
        return false;
    return true;
}


static bool loadFileTime(RRtime *filetime, FILE *f)
{
    rewind(f);
    if (fseek(f, sizeof(u32) * 3, SEEK_SET) < 0)
        return false;
    char buffer[sizeof(RRtime)];
    if (fread(buffer, sizeof(RRtime), 1, f) != 1)
        return false;
    nboUnpackRRtime(buffer, *filetime);
    return true;
}


static bool replaceFlagTypes(ReplayHeader *h)
{
    bool replace = false;
    const void *buf = h->flags;
    FlagOptionMap headerFlag;
    FlagTypeMap::iterator it;

    // Unpack the stored list of flags from the header
    while (buf < (h->flags + h->flagsSize))
    {
        FlagType *type;
        buf = FlagType::unpack(buf, type);
        headerFlag[type] = false;
        if (type != Flags::Null)
            headerFlag[type] = true;
    }

    // we're done with this
    delete[] h->flags;

    // See if all of the flags required by the header are currently active
    for (it = FlagType::getFlagMap().begin();
            it != FlagType::getFlagMap().end(); ++it)
    {
        FlagType* &type = it->second;
        if ((type != Flags::Null) &&
                (headerFlag[type]) && !flagIsActive(type))
        {
            replace = true; // this flag type isn't currently active
        }
    }

    if (replace)
    {
        // replace the flags
        logDebugMessage(3,"Replay: replacing Flag Types\n");
        clOptions->numExtraFlags = 0;
        for (it = FlagType::getFlagMap().begin();
                it != FlagType::getFlagMap().end(); ++it)
        {
            FlagType* &type = it->second;
            if (headerFlag[type])
                clOptions->flagCount[type] = 1;
            clOptions->flagDisallowed[type] = false;
        }
        return true; // flag types were replaced
    }

    return false;  // flag types were not replaced
}


static bool replaceSettings(ReplayHeader *h)
{
    bool replaced = true;
    const unsigned int length = sizeof(worldSettings);

    // change the settings maxPlayer size to the current value
    const int maxPlayersOffset =
        sizeof(uint16_t) + // packet len
        sizeof(uint16_t) + // packet code
        sizeof(float)    + // world size
        sizeof(uint16_t) + // gamestyle
        sizeof(uint16_t);  // gameoptions
    char *hdrMaxPlayersPtr = h->worldSettings + maxPlayersOffset;
    nboPackUShort(hdrMaxPlayersPtr, MaxPlayers + ReplayObservers);


    // compare the settings (now that maxPlayer has been adjusted)
    if (memcmp(worldSettings, h->worldSettings, length) == 0)
        replaced = false;

    // replace the world settings
    memcpy(worldSettings, h->worldSettings, length);

    return replaced;
}


static bool replaceWorldDatabase(ReplayHeader *h)
{
    if ((h->worldSize != worldDatabaseSize) ||
            (memcmp(h->world, worldDatabase, h->worldSize) != 0))
    {
        // they don't match, replace the world

        logDebugMessage(3,"Replay: replacing World Database\n");

        char *oldWorld = worldDatabase;
        worldDatabase = h->world;
        worldDatabaseSize = h->worldSize;

        MD5 md5;
        md5.update((unsigned char *)worldDatabase, worldDatabaseSize);
        md5.finalize();
        hexDigest = h->realHash[0];
        hexDigest += md5.hexdigest();

        delete[] oldWorld;
        return true;   // the world was replaced
    }

    delete[] h->world;
    return false;    // the world was not replaced
}


static bool flagIsActive(FlagType *type)
{
    // Please see the MsgNegotiateFlags code in [bzfs.cxx]
    // to see what it is that we are trying to fake.

    if ((clOptions->flagCount[type] > 0) ||
            ((clOptions->numExtraFlags > 0) &&
             !clOptions->flagDisallowed[type]))
        return true;
    return false;
}


static bool packFlagTypes(char *flags, u32 *flagsSize)
{
    void *buf = flags;
    FlagTypeMap::iterator it;

    for (it = FlagType::getFlagMap().begin();
            it != FlagType::getFlagMap().end(); ++it)
    {
        FlagType* &type = it->second;
        if ((type != Flags::Null) && flagIsActive(type))
            buf = type->pack(buf);
    }

    *flagsSize = (char*)buf - flags;

    return true;
}


/******************************************************************************/

// Buffer Functions

static void initPacket(u16 mode, u16 code, int len, const void *data, RRpacket *p)
{
    // RecordFilePrevPos takes care of p->prevFilePos
    p->mode = mode;
    p->code = code;
    p->len = len;
    p->data = (const char *)data;
}


static RRpacket *newPacket(u16 mode, u16 code, int len, const void *data)
{
    RRpacket *p = new RRpacket;

    p->next = NULL;
    p->prev = NULL;

    char *d = new char [len];
    if (d != NULL)
        memcpy(d, data, len);
    p->data = d;
    initPacket(mode, code, len, p->data, p);

    return p;
}


static void addHeadPacket(RRbuffer *b, RRpacket *p)
{
    if (b->head != NULL)
        b->head->next = p;
    else
        b->tail = p;
    p->prev = b->head;
    p->next = NULL;
    b->head = p;

    b->byteCount = b->byteCount + (p->len + RRpacketHdrSize);
    b->packetCount++;

    return;
}


static void addTailPacket(RRbuffer *b, RRpacket *p)
{
    if (b->tail != NULL)
        b->tail->prev = p;
    else
        b->head = p;
    p->next = b->tail;
    p->prev = NULL;
    b->tail = p;

    b->byteCount = b->byteCount + (p->len + RRpacketHdrSize);
    b->packetCount++;

    return;
}


static RRpacket *delTailPacket(RRbuffer *b)
{
    RRpacket *p = b->tail;

    if (p == NULL)
        return NULL;

    b->byteCount = b->byteCount - (p->len + RRpacketHdrSize);
    b->packetCount--;

    b->tail = p->next;

    if (p->next != NULL)
        p->next->prev = NULL;
    else
    {
        b->head = NULL;
        b->tail = NULL;
    }

    return p;
}


static RRpacket *delHeadPacket(RRbuffer *b)
{
    RRpacket *p = b->head;

    if (p == NULL)
        return NULL;

    b->byteCount = b->byteCount - (p->len + RRpacketHdrSize);
    b->packetCount--;

    b->head = p->prev;

    if (p->prev != NULL)
        p->prev->next = NULL;
    else
    {
        b->tail = NULL;
        b->head = NULL;
    }

    return p;
}


static void freeBuffer(RRbuffer *b)
{
    RRpacket *p, *ptmp;

    p = b->tail;

    while (p != NULL)
    {
        ptmp = p->next;
        delete[] p->data;
        delete p;
        p = ptmp;
    }

    b->tail = NULL;
    b->head = NULL;
    b->byteCount = 0;
    b->packetCount = 0;

    return;
}


/******************************************************************************/

// Timing Functions

static RRtime getRRtime()
{
#ifndef _WIN32

    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((RRtime)tv.tv_sec * (RRtime)1000000) + (RRtime)tv.tv_usec;

#else //_WIN32

    // FIXME - use QPC if available? (10ms[pat] good enough?)
    //       - during rollovers, check time() against the
    //     current value to see if a rollover was missed?

    static RRtime offset = ((RRtime)time(NULL) * (RRtime)1000000) -
                           ((RRtime)timeGetTime() * (RRtime)1000);
    static u32 lasttime = (u32)timeGetTime();
    u32 nowtime = (u32)timeGetTime();

    // we've got 49.71 days to catch the rollovers
    if (nowtime < lasttime)
    {
        // add the rollover value
        offset += ((RRtime)1 << 32);
    }
    lasttime = nowtime;
    return offset + ((RRtime)nowtime * (RRtime)1000);

#endif //_WIN32
}


static void *nboPackRRtime(void *buf, RRtime value)
{
    buf = nboPackUInt(buf, (u32) (value >> 32));       // msb's
    buf = nboPackUInt(buf, (u32) (value & 0xFFFFFFFF)); // lsb's
    return buf;
}


static const void *nboUnpackRRtime(const void *buf, RRtime& value)
{
    u32 msb, lsb;
    buf = nboUnpackUInt(buf, msb);
    buf = nboUnpackUInt(buf, lsb);
    value = ((RRtime)msb << 32) + (RRtime)lsb;
    return buf;
}


/******************************************************************************/

static const char *msgString(u16 code)
{

#define STRING_CASE(x)  \
  case x: return #x

    switch (code)
    {
        STRING_CASE (MsgNull);

        STRING_CASE (MsgAccept);
        STRING_CASE (MsgAlive);
        STRING_CASE (MsgAdminInfo);
        STRING_CASE (MsgAddPlayer);
        STRING_CASE (MsgAutoPilot);
        STRING_CASE (MsgCaptureFlag);
        STRING_CASE (MsgDropFlag);
        STRING_CASE (MsgEnter);
        STRING_CASE (MsgExit);
        STRING_CASE (MsgFlagUpdate);
        STRING_CASE (MsgGrabFlag);
        STRING_CASE (MsgGMUpdate);
        STRING_CASE (MsgGetWorld);
        STRING_CASE (MsgKilled);
        STRING_CASE (MsgMessage);
        STRING_CASE (MsgNewRabbit);
        STRING_CASE (MsgNegotiateFlags);
        STRING_CASE (MsgPause);
        STRING_CASE (MsgPlayerInfo);
        STRING_CASE (MsgPlayerUpdate);
        STRING_CASE (MsgPlayerUpdateSmall);
        STRING_CASE (MsgQueryGame);
        STRING_CASE (MsgQueryPlayers);
        STRING_CASE (MsgReject);
        STRING_CASE (MsgReplayReset);
        STRING_CASE (MsgRemovePlayer);
        STRING_CASE (MsgShotBegin);
        STRING_CASE (MsgScore);
        STRING_CASE (MsgScoreOver);
        STRING_CASE (MsgShotEnd);
        STRING_CASE (MsgSuperKill);
        STRING_CASE (MsgSetVar);
        STRING_CASE (MsgTimeUpdate);
        STRING_CASE (MsgTeleport);
        STRING_CASE (MsgTransferFlag);
        STRING_CASE (MsgTeamUpdate);
        STRING_CASE (MsgWantWHash);

        STRING_CASE (MsgUDPLinkRequest);
        STRING_CASE (MsgUDPLinkEstablished);
        STRING_CASE (MsgLagPing);

    default:
        static char buf[32];
        sprintf(buf, "MsgUnknown: 0x%04X", code);
        return buf;
    }
}


/******************************************************************************/

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
