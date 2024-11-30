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
#include "ServerLink.h"
// #include "MsgStrings.h"

#if defined(DEBUG)
#define NETWORK_STATS
#endif

// system headers
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>
#include <vector>
#if !defined(_WIN32)
#include <unistd.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <stdlib.h>

// common implementation headers
#include "ErrorHandler.h"
// invoke persistent rebuilding for current version dates
#include "version.h"
#if defined(NETWORK_STATS)
#include "bzfio.h"
#endif
#include "TimeKeeper.h"

#define UDEBUG if (UDEBUGMSG) printf
#define UDEBUGMSG false

#if defined(NETWORK_STATS)
static TimeKeeper   startTime;
static uint32_t     bytesSent;
static uint32_t     bytesReceived;
static uint32_t     packetsSent;
static uint32_t     packetsReceived;
#endif

#if defined(_WIN32)
DWORD ThreadID;     // Thread ID
HANDLE hConnected;  // "Connected" event
HANDLE hThread;     // Connection thread

typedef struct
{
    int query;
    CNCTType* addr;
    int saddr;
} TConnect;

TConnect conn;

DWORD WINAPI ThreadConnect(LPVOID params)
{
    TConnect *conn = (TConnect*)params;
    if (connect(conn->query, conn->addr, conn->saddr) >= 0)
    {
        SetEvent(hConnected); // Connect successful
    }
    ExitThread(0);
    return 0;
}

#endif // !defined(_WIN32)

// FIXME -- packet recording
FILE* packetStream = NULL;
TimeKeeper packetStartTime;
static const unsigned long serverPacket = 1;
static const unsigned long endPacket = 0;

ServerLink*     ServerLink::server = NULL;

ServerLink::ServerLink(const Address& serverAddress, int port) :
    state(SocketError), // assume failure
    fd(-1),         // assume failure
    udpLength(0),
    udpBufferPtr(),
    ubuf()
{
    int i;

    struct protoent* p;
#if defined(_WIN32)
    BOOL off = FALSE;
#else
    int off = 0;
#endif

    // standard server has no special abilities;
    server_abilities = Nothing;

    // queue is empty

    urecvfd = -1;

    ulinkup = false;

    // initialize version to a bogus number
    strcpy(version, "BZFS0000");

    // open connection to server.  first connect to given port.
    // don't wait too long.
    int query = socket(AF_INET, SOCK_STREAM, 0);
    if (query < 0) return;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = serverAddress;

    UDEBUG("Remote %s\n", inet_ntoa(addr.sin_addr));

    // for UDP, used later
    memcpy((unsigned char *)&usendaddr,(unsigned char *)&addr, sizeof(addr));

    bool okay = true;
    int fdMax = query;
    struct timeval timeout;
    fd_set write_set;
    fd_set read_set;
    int nfound;

#if !defined(_WIN32)
    okay = true;
    fdMax = query;
    if (BzfNetwork::setNonBlocking(query) < 0)
    {
        close(query);
        return;
    }
    if (connect(query, (CNCTType*)&addr, sizeof(addr)) < 0)
    {
        if (getErrno() != EINPROGRESS)
        {
            close(query);
            return;
        }
        FD_ZERO(&write_set);
        FD_SET((unsigned int)query, &write_set);
        timeout.tv_sec = long(5);
        timeout.tv_usec = 0;
        nfound = select(fdMax + 1, NULL, (fd_set*)&write_set, NULL, &timeout);
        if (nfound <= 0)
        {
            close(query);
            return;
        }
        int       connectError;
        socklen_t errorLen = sizeof(int);
        if (getsockopt(query, SOL_SOCKET, SO_ERROR, &connectError, &errorLen)
                < 0)
        {
            close(query);
            return;
        }
        if (connectError != 0)
        {
            close(query);
            return;
        }
    }
#else // Connection timeout for Windows

    // Initialize structure
    conn.query = query;
    conn.addr = (CNCTType*)&addr;
    conn.saddr = sizeof(addr);

    // Create event
    hConnected = CreateEvent(NULL, FALSE, FALSE, "Connected Event");

    hThread = CreateThread(NULL, 0, ThreadConnect, &conn, 0, &ThreadID);
    okay = (WaitForSingleObject(hConnected, 5000) == WAIT_OBJECT_0);
    if (!okay)
        TerminateThread(hThread, 1);

    // Do some cleanup
    CloseHandle(hConnected);
    CloseHandle(hThread);

#endif // !defined(_WIN32)
    if (!okay)
    {
        close(query);
        return;
    }

    // send out the connect header
    // this will let the server know we are BZFS protocol.
    // after the server gets this it will send back a version for us to check
    int sendRepply = ::send(query,BZ_CONNECT_HEADER,(int)strlen(BZ_CONNECT_HEADER),0);

    logDebugMessage(2,"CONNECT:send in connect returned %d\n",sendRepply);

    // wait to get data back. we are still blocking so these
    // calls should be sync.

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    FD_SET((unsigned int)query, &read_set);
    FD_SET((unsigned int)query, &write_set);

    timeout.tv_sec = long(10);
    timeout.tv_usec = 0;

    // pick some limit to time out on ( in seconds )
    double thisStartTime = TimeKeeper::getCurrent().getSeconds();
    double connectTimeout = 30.0;
    if (BZDB.isSet("connectionTimeout"))
        connectTimeout = BZDB.eval("connectionTimeout")  ;

    bool gotNetData = false;

    // loop calling select untill we read some data back.
    // its only 8 bytes so it better come back in one packet.
    int loopCount = 0;
    while (!gotNetData)
    {
        loopCount++;
        nfound = select(fdMax + 1, (fd_set*)&read_set, (fd_set*)&write_set, NULL, &timeout);

        // there has to be at least one socket active, or we are screwed
        if (nfound <= 0)
        {
            logDebugMessage(1,"CONNECT:select in connect failed, nfound = %d\n",nfound);
            close(query);
            return;
        }

        // try and get data back from the server
        i = recv(query, (char*)version, 8, 0);

        // if we got some, then we are done
        if (i > 0)
        {
            logDebugMessage(2,"CONNECT:got net data in connect, bytes read = %d\n",i);
            logDebugMessage(2,"CONNECT:Time To Connect = %f\n",(TimeKeeper::getCurrent().getSeconds() - thisStartTime));
            gotNetData = true;
        }
        else
        {
            // if we have waited too long, then bail
            if ((TimeKeeper::getCurrent().getSeconds() - thisStartTime) > connectTimeout)
            {
                logDebugMessage(1,"CONNECT:connect time out failed\n");
                logDebugMessage(2,"CONNECT:connect loop count = %d\n",loopCount);
                close(query);
                return;
            }

            TimeKeeper::sleep(0.25f);
        }
    }

    logDebugMessage(2,"CONNECT:connect loop count = %d\n",loopCount);

    // if we got back less than the expected connect response (BZFSXXXX)
    // then something went bad, and we are done.
    if (i < 8)
    {
        close(query);
        return;
    }

    // since we are connected, we can go non blocking
    // on BSD sockets systems
    // all other messages after this are handled via the normal
    // message system
#if !defined(_WIN32)
    if (BzfNetwork::setNonBlocking(query) < 0)
    {
        close(query);
        return;
    }
#endif

    // FIXME is it ok to try UDP always?
    server_abilities |= CanDoUDP;
    if (strcmp(version, getServerVersion()) != 0)
    {
        state = BadVersion;

        if (strcmp(version, BanRefusalString) == 0)
        {
            state = Refused;
            char message[512];
            int len = recv(query, (char*)message, 512, 0);
            if (len > 0)
                message[len - 1] = 0;
            else
                message[0] = 0;
            rejectionMessage = message;
        }

        close(query);
        return;
    }

    // read local player's id
#if !defined(_WIN32)
    FD_ZERO(&read_set);
    FD_SET((unsigned int)query, &read_set);
    timeout.tv_sec = long(5);
    timeout.tv_usec = 0;
    nfound = select(fdMax + 1, (fd_set*)&read_set, NULL, NULL, &timeout);
    if (nfound <= 0)
    {
        close(query);
        return;
    }
#endif // !defined(_WIN32)
    i = recv(query, (char *) &id, sizeof(id), 0);
    if (i < (int) sizeof(id))
        return;
    if (id == 0xff)
    {
        state = Rejected;
        close(query);
        return;
    }

#if !defined(_WIN32)
    if (BzfNetwork::setBlocking(query) < 0)
    {
        close(query);
        return;
    }
#endif // !defined(_WIN32)

    fd = query;

    // turn on TCP no delay
    p = getprotobyname("tcp");
    if (p)
        setsockopt(fd, p->p_proto, TCP_NODELAY, (SSOType)&off, sizeof(off));  // changed

    state = Okay;
#if defined(NETWORK_STATS)
    bytesSent = 0;
    bytesReceived = 0;
    packetsSent = 0;
    packetsReceived = 0;
#endif

    // FIXME -- packet recording
    if (getenv("BZFLAGSAVE"))
    {
        packetStream = fopen(getenv("BZFLAGSAVE"), "w");
        packetStartTime = TimeKeeper::getCurrent();
    }
    return;

}

ServerLink::~ServerLink()
{
    if (state != Okay) return;
    shutdown(fd, 2);
    close(fd);

    if (urecvfd >= 0)
        close(urecvfd);

    urecvfd = -1;
    ulinkup = false;

    // FIXME -- packet recording
    if (packetStream)
    {
        long dt = (long)((TimeKeeper::getCurrent() - packetStartTime) * 10000.0f);
        size_t items_written = fwrite(&endPacket, sizeof(endPacket), 1, packetStream);
        if (items_written == 1)
            items_written = fwrite(&dt, sizeof(dt), 1, packetStream);
        if (items_written != 1)
            printError("Error writing on packetStream");
        fclose(packetStream);
    }

#if defined(NETWORK_STATS)
    const float dt = float(TimeKeeper::getCurrent() - startTime);
    logDebugMessage(1,"Server network statistics:\n");
    logDebugMessage(1,"  elapsed time    : %f\n", dt);
    logDebugMessage(1,"  bytes sent      : %d (%f/sec)\n", bytesSent, (float)bytesSent / dt);
    logDebugMessage(1,"  packets sent    : %d (%f/sec)\n", packetsSent, (float)packetsSent / dt);
    if (packetsSent != 0)
        logDebugMessage(1,"  bytes/packet    : %f\n", (float)bytesSent / (float)packetsSent);
    logDebugMessage(1,"  bytes received  : %d (%f/sec)\n", bytesReceived, (float)bytesReceived / dt);
    logDebugMessage(1,"  packets received: %d (%f/sec)\n", packetsReceived, (float)packetsReceived / dt);
    if (packetsReceived != 0)
        logDebugMessage(1,"  bytes/packet    : %f\n", (float)bytesReceived / (float)packetsReceived);
#endif
}

ServerLink*     ServerLink::getServer() // const
{
    return server;
}

void            ServerLink::setServer(ServerLink* _server)
{
    server = _server;
}

void            ServerLink::send(uint16_t code, uint16_t len,
                                 const void* msg)
{
    bool needForSpeed=false;
    if (state != Okay) return;
//  if (code != MsgPlayerUpdateSmall && code != MsgPlayerUpdate)
//    logDebugMessage(1,"send %s len %d\n",MsgStrings::strMsgCode(code),len);
    char msgbuf[MaxPacketLen];
    void* buf = msgbuf;
    buf = nboPackUShort(buf, len);
    buf = nboPackUShort(buf, code);
    if (msg && len != 0) buf = nboPackString(buf, msg, len);

    if ((urecvfd>=0) && ulinkup )
    {
        switch (code)
        {
        case MsgShotBegin:
        case MsgShotEnd:
        case MsgPlayerUpdate:
        case MsgPlayerUpdateSmall:
        case MsgGMUpdate:
        case MsgUDPLinkRequest:
        case MsgUDPLinkEstablished:
            needForSpeed=true;
            break;
        }
    }
    // MsgUDPLinkRequest always goes udp
    if (code == MsgUDPLinkRequest)
        needForSpeed=true;

    if (needForSpeed)
    {
#ifdef TESTLINK
        if ((random()%TESTQUALTIY) != 0)
#endif
            sendto(urecvfd, (const char *)msgbuf, (char*)buf - msgbuf, 0, &usendaddr, sizeof(usendaddr));
        // we don't care about errors yet
        return;
    }

    int r = ::send(fd, (const char*)msgbuf, len + 4, 0);
    (void)r; // silence g++
#if defined(_WIN32)
    if (r == SOCKET_ERROR)
    {
        const int e = WSAGetLastError();
        if (e == WSAENETRESET || e == WSAECONNABORTED ||
                e == WSAECONNRESET || e == WSAETIMEDOUT)
            state = Hungup;
        r = 0;
    }
#endif

#if defined(NETWORK_STATS)
    bytesSent += r;
    packetsSent++;
#endif
}

#ifdef WIN32
/* This is a really really fugly hack to get around winsock sillyness
 * The newer versions of winsock have a socken_t typedef, and there
 * doesn't seem to be any way to tell the versions apart. However,
 * VC++ helps us out here by treating typedef as #define
 * If we've got a socklen_t typedefed, define HAVE_SOCKLEN_T to
 * avoid #define'ing it in common.h */

#ifndef socklen_t
#define socklen_t int
#endif
#endif //WIN32

int         ServerLink::read(uint16_t& code, uint16_t& len,
                             void* msg, int blockTime)
{

    code = MsgNull;
    len = 0;

    if (state != Okay) return -1;

    if ((urecvfd >= 0) /* && ulinkup */)
    {

        if (!udpLength)
        {
            AddrLen recvlen = sizeof(urecvaddr);
            int n = recvfrom(urecvfd, ubuf, MaxPacketLen, 0,
                             &urecvaddr, (socklen_t*) &recvlen);
            if (n > 0)
            {
                udpLength    = n;
                udpBufferPtr = ubuf;
            }
        }
        if (udpLength)
        {
            // unpack header and get message
            udpLength -= 4;
            if (udpLength < 0)
            {
                udpLength = 0;
                return -1;
            }
            udpBufferPtr = (const char *)nboUnpackUShort(udpBufferPtr, len);
            udpBufferPtr = (const char *)nboUnpackUShort(udpBufferPtr, code);
//      if (code != MsgPlayerUpdateSmall && code != MsgPlayerUpdate && code != MsgGameTime)
//  logDebugMessage(1,"rcvd %s len %d\n",MsgStrings::strMsgCode(code),len);
            UDEBUG("<** UDP Packet Code %x Len %x\n",code, len);
            if (len > udpLength)
            {
                udpLength = 0;
                return -1;
            }
            memcpy((char *)msg, udpBufferPtr, len);
            udpBufferPtr += len;
            udpLength    -= len;
            return 1;
        }
        if (UDEBUGMSG) printError("Fallback to normal TCP receive");
        len = 0;
        code = MsgNull;

        blockTime = 0;
    }

    // block for specified period.  default is no blocking (polling)
    struct timeval timeout;
    timeout.tv_sec = blockTime / 1000;
    timeout.tv_usec = 1000 * (blockTime % 1000);

    // only check server
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET((unsigned int)fd, &read_set);
    int nfound = select(fd+1, (fd_set*)&read_set, NULL, NULL,
                        (struct timeval*)(blockTime >= 0 ? &timeout : NULL));
    if (nfound == 0) return 0;
    if (nfound < 0) return -1;

    // printError("<** TCP Packet Code Received %d", time(0));
    // FIXME -- don't really want to take the chance of waiting forever
    // on the remaining select() calls, but if the server and network
    // haven't been hosed then the data will get here soon.  And if the
    // server or network is down then we don't really care anyway.

    // get packet header -- keep trying until we get 4 bytes or an error
    char headerBuffer[4];


    int rlen = 0;
    rlen = recv(fd, (char*)headerBuffer, 4, 0);

    int tlen = rlen;
    while (rlen >= 1 && tlen < 4)
    {
        printError("ServerLink::read() loop");
        FD_ZERO(&read_set);
        FD_SET((unsigned int)fd, &read_set);
        nfound = select(fd+1, (fd_set*)&read_set, NULL, NULL, NULL);
        if (nfound == 0) continue;
        if (nfound < 0) return -1;
        rlen = recv(fd, (char*)headerBuffer + tlen, 4 - tlen, 0);
        if (rlen >= 0) tlen += rlen;
    }
    if (tlen < 4) return -1;
#if defined(NETWORK_STATS)
    bytesReceived += 4;
    packetsReceived++;
#endif

    // unpack header and get message
    const void* buf = headerBuffer;
    buf = nboUnpackUShort(buf, len);
    buf = nboUnpackUShort(buf, code);

//  logDebugMessage(1,"rcvd %s len %d\n",MsgStrings::strMsgCode(code),len);
    if (len > MaxPacketLen - 4)
        return -1;
    if (len > 0)
        rlen = recv(fd, (char*)msg, int(len), 0);
    else
        rlen = 0;
#if defined(NETWORK_STATS)
    if (rlen >= 0) bytesReceived += rlen;
#endif
    if (rlen != int(len))
    {
        // keep reading until we get the whole message
        tlen = rlen;
        while (rlen >= 1 && tlen < int(len))
        {
            FD_ZERO(&read_set);
            FD_SET((unsigned int)fd, &read_set);
            nfound = select(fd+1, (fd_set*)&read_set, 0, 0, NULL);
            if (nfound == 0) continue;
            if (nfound < 0) return -1;
            rlen = recv(fd, (char*)msg + tlen, int(len) - tlen, 0);
            if (rlen >= 0) tlen += rlen;
#if defined(NETWORK_STATS)
            if (rlen >= 0) bytesReceived += rlen;
#endif
        }
        if (tlen < int(len)) return -1;
    }

    // FIXME -- packet recording
    if (packetStream)
    {
        long dt = (long)((TimeKeeper::getCurrent() - packetStartTime) * 10000.0f);
        size_t items_written = fwrite(&serverPacket, sizeof(serverPacket), 1, packetStream);
        if (items_written == 1)
            items_written = fwrite(&dt, sizeof(dt), 1, packetStream);
        if (items_written == 1)
            items_written = fwrite(headerBuffer, 4, 1, packetStream);
        if (items_written == 1)
            items_written = fwrite(msg, len, 1, packetStream);
        if (items_written != 1)
            printError("Error writing on packetStream");
    }
    return 1;
}

void            ServerLink::sendEnter(PlayerType type,
                                      TeamColor team,
                                      const char* name,
                                      const char* motto,
                                      const char* token)
{
    if (state != Okay) return;
    char msg[PlayerIdPLen + 4 + CallSignLen + MottoLen + TokenLen + VersionLen];
    ::memset(msg, 0, sizeof(msg));
    void* buf = msg;
    buf = nboPackUShort(buf, uint16_t(type));
    buf = nboPackUShort(buf, uint16_t(team));
    ::memcpy(buf, name, ::strlen(name));
    buf = (void*)((char*)buf + CallSignLen);
    ::memcpy(buf, motto, ::strlen(motto));
    buf = (void*)((char*)buf + MottoLen);
    ::memcpy(buf, token, ::strlen(token));
    buf = (void*)((char*)buf + TokenLen);
    ::memcpy(buf, getAppVersion(), ::strlen(getAppVersion()) + 1);
    buf = (void*)((char*)buf + VersionLen);
    send(MsgEnter, sizeof(msg), msg);
}

bool ServerLink::readEnter(std::string& reason,
                           uint16_t& code, uint16_t& rejcode)
{
    // wait for response
    uint16_t len;
    char msg[MaxPacketLen];

    while (true)
    {
        if (this->read(code, len, msg, -1) < 0)
        {
            reason = "Communication error joining game [No immediate response].";
            return false;
        }

        if (code == MsgAccept)
            return true;
        else if (code == MsgSuperKill)
        {
            reason = "Server forced disconnection.";
            return false;
        }
        else if (code == MsgReject)
        {
            const void *buf;
            char buffer[MessageLen];
            buf = nboUnpackUShort (msg, rejcode); // filler for now
            buf = nboUnpackString (buf, buffer, MessageLen);
            buffer[MessageLen - 1] = '\0';
            reason = buffer;
            return false;
        }
        // ignore other codes so that bzadmin doesn't choke
        // on the MsgMessage's that the server can send before
        // the MsgAccept (authorization holdoff, etc...)
    }

    return true;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
