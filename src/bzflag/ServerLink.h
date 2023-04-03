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
 * Encapsulates communication between local player and the server.
 */

#ifndef BZF_SERVER_LINK_H
#define BZF_SERVER_LINK_H

#include "common.h"

#include <string>

#include "global.h"
#include "Address.h"
#include "Protocol.h"
#include "ShotPath.h"
#include "Flag.h"
#include "Player.h"

class ServerLink
{
public:
    enum State
    {
        Okay = 0,
        SocketError = 1,
        Rejected = 2,
        BadVersion = 3,
        Hungup = 4,     // only used by Winsock
        CrippledVersion = 5,
        Refused = 6
    };

    enum Abilities
    {
        Nothing = 0,
        CanDoUDP = 1,
        SendScripts = 2,
        SendTextures = 4,
        HasMessageLink = 8
    };

    ServerLink(Address& serverAddress);
    ~ServerLink();

    State                       getState() const;
    inline const std::string&   getRejectionMessage()
    {
        return rejectionMessage;
    }

    int                 getSocket() const;  // file descriptor actually
    const PlayerId&     getId() const;
    const char*         getVersion() const;

    void                send(uint16_t code, uint16_t len, const void* msg);
    int                 read(uint16_t& code, uint16_t& len, void* msg,
                             int millisecondsToBlock = 0); // if millisecondsToBlock < 0 then block forever
    int                 fillTcpReadBuffer(int blockTime);
    bool                tcpPacketIn(char headerBuffer[4], void *msg);

    void                sendEnter(PlayerType, TeamColor, int skinIndex, const char* name, const char* motto,
                                  const char* token,
                                  const char* locale);
    bool                readEnter(std::string& reason, uint16_t& code, uint16_t& rejcode);

    void                sendCaptureFlag(TeamColor);
    void                sendRequestFlag(int flagIndex);
    void                sendDropFlag(const float* position);
    void                sendKilled(const PlayerId&, int reason, int shotId, const FlagType::Ptr flag, int phydrv);

    // FIXME -- This is very ugly, but required to build bzadmin with gcc 2.9.5.
    //      It should be changed to something cleaner.
    void                sendPlayerUpdate(Player*);
    void                sendMsgFireShot(const FiringInfo&);
    void                sendEndShot(const PlayerId&, int shotId, int reason);
    void                sendAlive();
    void                sendTeleport(int from, int to);
    void                sendStealFlag(const PlayerId&, const PlayerId&);
    void                sendNewRabbit();
    void                sendPaused(bool paused);
    void                sendAutoPilot(bool autopilot);
    void                sendUDPlinkRequest();

    static ServerLink*  getServer(); // const
    static void         setServer(ServerLink*);
    void                enableOutboundUDP();
    void                confirmIncomingUDP();

private:
    State               state = SocketError;
    int                 fd = -1;

    sockaddr_in6        usendaddr;
    int                 urecvfd;
    sockaddr_in6        urecvaddr; // the clients udp listen address
    bool                ulinkup;

    PlayerId            id;
    char                version[9] = { 0 };
    static ServerLink*  server;
    int                 server_abilities;

    std::string         rejectionMessage;

    int                 udpLength = 0;
    const char*         udpBufferPtr = nullptr;
    char                ubuf[MaxPacketLen] = { 0 };

    int                 tcpBufferPos = 0;
    int                 tcpBufferConsumed = 0;
    char                tbuf[MaxPacketLen * 4] = { 0 };
};

#define SEND 1
#define RECEIVE 0

//
// ServerLink
//

inline ServerLink::State ServerLink::getState() const
{
    return state;
}

inline int      ServerLink::getSocket() const
{
    return fd;
}

inline const PlayerId&  ServerLink::getId() const
{
    return id;
}

inline const char*  ServerLink::getVersion() const
{
    return version;
}

#endif // BZF_SERVER_LINK_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
