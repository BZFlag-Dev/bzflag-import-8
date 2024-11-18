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
#include "AresHandler.h"

/* system implementation headers */
#include <cerrno>
#include <cstring>
#include <mutex>

bool AresHandler::globallyInited = false;
std::mutex callback_mutex;

AresHandler::AresHandler(int _index)
    : index(_index), status(None)
{
    // clear the host address
    memset(&hostAddress, 0, sizeof(hostAddress));

    /* ask for local "hosts" lookups too */
    static char lookups[] = "fb";
    struct ares_options opts;
    memset(&opts, 0, sizeof(opts));
    int    flags = 0;

    opts.lookups = lookups;
    flags       |= ARES_OPT_LOOKUPS;
#if ARES_VERSION_MAJOR > 1 || ARES_VERSION_MINOR >= 26
    opts.evsys   = ARES_EVSYS_DEFAULT;
    flags       |= ARES_OPT_EVENT_THREAD;
#endif

    /* start up our resolver */
    if (!globallyInited)
    {
        aresFailed = !globalInit();
        if (aresFailed)
        {
            status = Failed;
            logDebugMessage(2,"Ares failed initializing library\n");
        }
    }

    /* start up our resolver */
    int code = ares_init_options (&aresChannel, &opts, flags);
    aresFailed = (code != ARES_SUCCESS);
    if (aresFailed)
    {
        status = Failed;
        logDebugMessage(2,"Ares Failed initializing\n");
    }
}

AresHandler::~AresHandler()
{
    if (aresFailed)
        return;
    ares_destroy(aresChannel);
}

bool AresHandler::globalInit()
{
    if (!globallyInited)
    {
#ifdef HAVE_ARES_LIBRARY_INIT
        if (ares_library_init(ARES_LIB_INIT_ALL) == ARES_SUCCESS)
#endif
            globallyInited = true;
    }
    return globallyInited;
}

void AresHandler::globalShutdown()
{
#ifdef HAVE_ARES_LIBRARY_INIT
    if (globallyInited)
        ares_library_cleanup();
#endif
}

void AresHandler::queryHostname(const struct sockaddr *clientAddr)
{
    if (aresFailed)
        return;

    status = HbAPending;
    // launch the asynchronous query to look up this hostname
    ares_gethostbyaddr(aresChannel, &((const sockaddr_in *)clientAddr)->sin_addr,
                       sizeof(in_addr), AF_INET, staticCallback, (void *)this);
    logDebugMessage(2,"Player [%d] submitted reverse resolve query\n", index);
}

void AresHandler::queryHost(const char *name)
{
    if (aresFailed)
        return;
    ares_cancel(aresChannel);

    if (!name)
    {
        status = Failed;
        return;
    }

    if (inet_aton(name, &hostAddress) != 0)
    {
        status = HbNSucceeded;
        return;
    }

    // launch the asynchronous query to look up this hostname
    status = HbNPending;
    ares_gethostbyname(aresChannel, name, AF_INET, staticCallback,
                       (void *)this);
}

#if ARES_VERSION_MAJOR > 1 || ARES_VERSION_MINOR >= 5
void AresHandler::staticCallback(void *arg, int callbackStatus,
                                 int, struct hostent *hostent)
#else
void AresHandler::staticCallback(void *arg, int callbackStatus,
                                 struct hostent *hostent)
#endif
{
    ((AresHandler *)arg)->callback(callbackStatus, hostent);
}

void AresHandler::callback(int callbackStatus, struct hostent *hostent)
{
    if (callbackStatus == ARES_EDESTRUCTION)
        return;

    const std::lock_guard<std::mutex> lock(callback_mutex);

    if (callbackStatus != ARES_SUCCESS)
    {
        logDebugMessage(1,"Player [%d] failed to resolve: error %d\n", index,
                        callbackStatus);
        status = Failed;
    }
    else if (status == HbAPending)
    {
        hostName = hostent->h_name;
        status = HbASucceeded;
        logDebugMessage(2,"Player [%d] resolved to %s\n", index, hostName.c_str());
    }
    else if (status == HbNPending)
    {
        memcpy(&hostAddress, hostent->h_addr_list[0], sizeof(hostAddress));
        status = HbNSucceeded;
    }
}

const char *AresHandler::getHostname()
{
    if (!callback_mutex.try_lock())
        return "";
    const char *host = hostName.c_str();
    callback_mutex.unlock();
    return host;
}

AresHandler::ResolutionStatus AresHandler::getHostAddress(struct in_addr
        *clientAddr)
{
    if (!callback_mutex.try_lock())
        return HbNPending;

    const ResolutionStatus oldStatus = status;
    if (status == HbNSucceeded)
        memcpy(clientAddr, &hostAddress, sizeof(hostAddress));
    callback_mutex.unlock();

    return oldStatus;
}

#if ARES_VERSION_MAJOR > 1 || ARES_VERSION_MINOR >= 26
void AresHandler::setFd(fd_set *, fd_set *, int &)
{
}
void AresHandler::process(fd_set *, fd_set *)
{
}
#else
void AresHandler::setFd(fd_set *read_set, fd_set *write_set, int &maxFile)
{
    if (aresFailed)
        return;
    int aresMaxFile = ares_fds(aresChannel, read_set, write_set) - 1;
    if (aresMaxFile > maxFile)
        maxFile = aresMaxFile;
}

void AresHandler::process(fd_set *read_set, fd_set *write_set)
{
    if (aresFailed)
        return;
    ares_process(aresChannel, read_set, write_set);
}
#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
