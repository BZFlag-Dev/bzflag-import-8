/* bzflag
 * Copyright (c) 1993-2021 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "bzfsPython.h"

#ifdef BZ_PYTHON

#ifdef HAVE_NCURSES_H
#undef HAVE_NCURSES_H
#endif

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "bzfsAPI.h"
#include "GameKeeper.h"

static PyConfig config;

static PyObject *bzfsItf_mod;
static PyObject *cleanup;
static PyObject *tickEventData_V1;
static PyObject *getPlayerSpawnPosEventData_V1;
static PyObject *chatEventData_V1;
static PyObject *flagTransferredEventData_V1;
static PyObject *flagGrabbedEventData_V1;
static PyObject *flagDroppedEventData_V1;
static PyObject *shotFiredEventData_V1;
static PyObject *playerDieEventData_V2;
static PyObject *playerUpdateEventData_V1;
static PyObject *allowFlagGrabData_V1;
static PyObject *playerJoinPartEventData_V1;
static PyObject *tankCoordinate;
static PyObject *callEvents;
static PyObject *bz_slashCommand;
static PyObject *bz_isPollActive;
static PyObject *bz_PollOpen;
static PyObject *bz_PollClose;
static PyObject *bz_PollHelp;
static PyObject *bz_CheckIfCustomMap;
static PyObject *bz_customZoneMapObject;
static PyObject *addedServerPlayer;
static PyObject *plugin = NULL;

class PlayerHandler : public bz_ServerSidePlayerHandler
{
    void added(int player);
};

static PlayerHandler  *playerHandler  = NULL;

void initPython (int argc, char **argv)
{
    PyStatus status;

    PyConfig_InitPythonConfig(&config);
    config.isolated   = 1;
    config.parse_argv = 0;

    /* Decode command line arguments.
     * Implicitly preinitialize Python (in isolated mode). */
    status = PyConfig_SetBytesArgv(&config, argc, argv);
    if (PyStatus_Exception(status))
    {
        /* Display the error message and exit the process with
         * non-zero exit code */
        Py_ExitStatusException(status);
    }
}

void destroyPython ()
{
    PyConfig_Clear(&config);
}

static PyObject *
bzPython_getNumFlags(PyObject *, PyObject *)
{
    int numFlag = bz_getNumFlags();
    return PyLong_FromLong(numFlag);
}

static PyObject *
bzPython_flagPlayer(PyObject *, PyObject *args)
{
    int flagID;
    if (!PyArg_ParseTuple(args, "i", &flagID))
        return NULL;
    int player = bz_flagPlayer(flagID);
    return PyLong_FromLong(player);
}

static PyObject *
bzPython_getCurrentTime(PyObject *, PyObject *)
{
    double currentTime = bz_getCurrentTime();
    return PyFloat_FromDouble(currentTime);
}

static PyObject *
bzPython_getFlagName(PyObject *, PyObject *args)
{
    int flagID;
    if (!PyArg_ParseTuple(args, "i", &flagID))
        return NULL;
    const char *flagName = bz_getFlagName(flagID).c_str();
    return PyUnicode_FromString(flagName);
}

static PyObject *
bzPython_resetFlag(PyObject *, PyObject *args)
{
    int flagID;
    if (!PyArg_ParseTuple(args, "i", &flagID))
        return NULL;
    bz_resetFlag(flagID);
    Py_RETURN_NONE;
}

static PyObject *
bzPython_debugMessage(PyObject *, PyObject *args)
{
    int level;
    const char* message;
    if (!PyArg_ParseTuple(args, "is", &level, &message))
        return NULL;
    bz_debugMessage(level, message);
    Py_RETURN_NONE;
}

static PyObject *
bzPython_getDebugLevel(PyObject *, PyObject *)
{
    int level = bz_getDebugLevel();
    return PyLong_FromLong(level);
}

static float maxTime = 1000.0;

static PyObject *
bzPython_setMaxWaitTime(PyObject *, PyObject *args)
{
    float maxWaitTime;
    if (!PyArg_ParseTuple(args, "f", &maxWaitTime))
        return NULL;
    maxTime = maxWaitTime;
    Py_RETURN_NONE;
}

static PyObject *
bzPython_hasPerm(PyObject *, PyObject *args)
{
    int         playerID;
    const char* perm;
    if (!PyArg_ParseTuple(args, "is", &playerID, &perm))
        return NULL;
    if (bz_hasPerm (playerID, perm))
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *
bzPython_revokePerm(PyObject *, PyObject *args)
{
    int         playerID;
    const char* perm;
    if (!PyArg_ParseTuple(args, "is", &playerID, &perm))
        return NULL;
    if (bz_revokePerm (playerID, perm))
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *
bzPython_getAdmin(PyObject *, PyObject *args)
{
    int         playerID;
    if (!PyArg_ParseTuple(args, "i", &playerID))
        return NULL;
    if (bz_getAdmin (playerID))
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *
bzPython_sendTextMessage(PyObject *, PyObject *args)
{
    int         playerID;
    const char *message;
    if (!PyArg_ParseTuple(args, "is", &playerID, &message))
        return NULL;
    bz_sendTextMessage (BZ_SERVER, playerID, message);
    Py_RETURN_NONE;
}

static PyObject *
bzPython_addServerSidePlayer(PyObject *, PyObject *)
{
    int playerID = bz_addServerSidePlayer(playerHandler);
    return PyLong_FromLong(playerID);
}

static PyObject *
bzPython_removeServerSidePlayer(PyObject *, PyObject *args)
{
    int playerID;
    if (!PyArg_ParseTuple(args, "i", &playerID))
        return NULL;
    bz_removeServerSidePlayer(playerID, playerHandler);
    Py_RETURN_NONE;
}

static PyObject *
bzPython_sendServerCommand(PyObject *, PyObject *args)
{
    const char *command;
    if (!PyArg_ParseTuple(args, "s", &command))
        return NULL;
    playerHandler->sendServerCommand(command);
    Py_RETURN_NONE;
}

static PyObject *
bzPython_RegisterCustomFlag(PyObject *, PyObject *args)
{
    const char     *abbr;
    const char     *name;
    const char     *helpString;
    bz_eShotType    shotType;
    bz_eFlagQuality quality;

    if (!PyArg_ParseTuple(args, "sssii", &abbr, &name, &helpString, &shotType, &quality))
        return NULL;
    if (bz_RegisterCustomFlag(abbr, name, helpString, shotType, quality))
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *
bzPython_getPlayerByIndex(PyObject *, PyObject *args)
{
    int                  playerIndex;
    bz_BasePlayerRecord *playerRecord;
    PyObject            *playerInfo;

    if (!PyArg_ParseTuple(args, "i", &playerIndex))
        return NULL;

    // Retrieve the player information
    playerRecord = bz_getPlayerByIndex(playerIndex);
    if (!playerRecord)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    /* call the class inside the bzfsItf module */
    playerInfo = PyObject_CallMethod(bzfsItf_mod, "bz_BasePlayerRecord", "");
    if (!playerInfo)
    {
        bz_freePlayerRecord(playerRecord);
        return Py_None;
    }

    PyObject *currentFlag = PyUnicode_FromString(playerRecord->currentFlag.c_str());

    if (currentFlag)
    {
        PyObject_SetAttrString(playerInfo, "currentFlag", currentFlag);
        Py_DECREF(currentFlag);
    }

    PyObject *callsign = PyUnicode_FromString(playerRecord->callsign.c_str());
    if (callsign)
    {
        PyObject_SetAttrString(playerInfo, "callsign", callsign);
        Py_DECREF(callsign);
    }

    PyObject *playerId = PyLong_FromLong(playerRecord->playerID);
    if (playerId)
    {
        PyObject_SetAttrString(playerInfo, "playerID", playerId);
        Py_DECREF(playerId);
    }

    // Free the memory from the player record
    bz_freePlayerRecord(playerRecord);

    return playerInfo;
}

static PyObject *
bzPython_getPlayerIDByName(PyObject *, PyObject *args)
{
    const char *name;
    int         playerID;

    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;
    playerID = GameKeeper::Player::getPlayerIDByName(name);
    return PyLong_FromLong(playerID);
}

static PyObject *
bzPython_incrementPlayerWins(PyObject *, PyObject *args)
{
    int playerId;
    int increment;

    if (!PyArg_ParseTuple(args, "ii", &playerId, &increment))
        return NULL;
    if (bz_incrementPlayerWins(playerId, increment))
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *
bzPython_getPlayerFlagID(PyObject *, PyObject *args)
{
    int playerId;

    if (!PyArg_ParseTuple(args, "i", &playerId))
        return NULL;
    int flagID = bz_getPlayerFlagID(playerId);
    return PyLong_FromLong(flagID);
}

static PyObject *
bzPython_getPlayerFlag(PyObject *, PyObject *args)
{
    int playerId;

    if (!PyArg_ParseTuple(args, "i", &playerId))
        return NULL;
    const char *flag = bz_getPlayerFlag(playerId);
    if (flag)
        return PyUnicode_FromString(flag);
    else
        Py_RETURN_NONE;
}

static PyObject *
bzPython_removePlayerFlag(PyObject *, PyObject *args)
{
    int playerId;

    if (!PyArg_ParseTuple(args, "i", &playerId))
        return NULL;
    if (bz_removePlayerFlag(playerId))
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *
bzPython_getTeamCount(PyObject *, PyObject *args)
{
    int _team;

    if (!PyArg_ParseTuple(args, "i", &_team))
        return NULL;
    int count = bz_getTeamCount(bz_eTeamType(_team));
    return PyLong_FromLong(count);
}

static PyObject *
bzPython_getPlayerIndexList(PyObject *, PyObject *)
{
    bz_APIIntList *pl = bz_getPlayerIndexList();

    PyObject *pList = PyTuple_New(pl->size());
    if (!pList)
    {
        delete pl;
        PyErr_Print();
        Py_RETURN_NONE;
    }

    for (unsigned int x = 0; x < pl->size(); x++)
        PyTuple_SetItem(pList, x, PyLong_FromLong(pl->get(x)));

    delete pl;
    return pList;
}

static PyMethodDef bzfsItfMethods[] =
{
    {
        "bz_getNumFlags", bzPython_getNumFlags, METH_VARARGS,
        "Get Flags count"
    },
    {
        "bz_flagPlayer", bzPython_flagPlayer,  METH_VARARGS,
        "Get Player assoc to flag"
    },
    {
        "bz_getCurrentTime", bzPython_getCurrentTime,  METH_VARARGS,
        "Get current time"
    },
    {
        "bz_getFlagName", bzPython_getFlagName,  METH_VARARGS,
        "Get flag name"
    },
    {
        "bz_resetFlag", bzPython_resetFlag,  METH_VARARGS,
        "Reset Indicated flag"
    },
    {
        "bz_debugMessage", bzPython_debugMessage,  METH_VARARGS,
        "Send a message for log"
    },
    {
        "bz_getDebugLevel", bzPython_getDebugLevel,  METH_VARARGS,
        "Get the debug level"
    },
    {
        "bz_setMaxWaitTime", bzPython_setMaxWaitTime,  METH_VARARGS,
        "Set Max Wait Time"
    },
    {
        "bz_hasPerm", bzPython_hasPerm, METH_VARARGS, "Ask for Permission"
    },
    {
        "bz_revokePerm", bzPython_revokePerm, METH_VARARGS, "Revoke Permission"
    },
    {
        "bz_getAdmin", bzPython_getAdmin, METH_VARARGS, "Ask if Admin"
    },
    {
        "bz_sendTextMessage", bzPython_sendTextMessage, METH_VARARGS,
        "Send a Text Message"
    },
    {
        "bz_addServerSidePlayer", bzPython_addServerSidePlayer, METH_VARARGS,
        "Add a Server Side Player"
    },
    {
        "bz_removeServerSidePlayer", bzPython_removeServerSidePlayer,
        METH_VARARGS, "Remove a Server Side Player"
    },
    {
        "sendServerCommand", bzPython_sendServerCommand, METH_VARARGS,
        "Send a Server Command"
    },
    {
        "bz_RegisterCustomFlag", bzPython_RegisterCustomFlag, METH_VARARGS,
        "Register a Custom Flag"
    },
    {
        "bz_getPlayerByIndex", bzPython_getPlayerByIndex, METH_VARARGS,
        "Get Player Information from Player Index"
    },
    {
        "bz_getPlayerIDByName", bzPython_getPlayerIDByName, METH_VARARGS,
        "Get Player Information from Player Index"
    },
    {
        "bz_incrementPlayerWins", bzPython_incrementPlayerWins, METH_VARARGS,
        "Increment player Wins"
    },
    {
        "bz_getPlayerFlagID", bzPython_getPlayerFlagID, METH_VARARGS,
        "Get Flag Id from player ID"
    },
    {
        "bz_getPlayerFlag", bzPython_getPlayerFlag, METH_VARARGS,
        "Get Flag from player ID"
    },
    {
        "bz_removePlayerFlag", bzPython_removePlayerFlag, METH_VARARGS,
        "Get Flag from player ID"
    },
    {
        "bz_getTeamCount", bzPython_getTeamCount, METH_VARARGS,
        "Get Team Count"
    },
    {
        "bz_getPlayerIndexList", bzPython_getPlayerIndexList, METH_VARARGS,
        "Get Players List"
    },
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

bool bzPython_SlashCommand(int playerID, bz_ApiString command, bz_ApiString message,
                           bz_APIStringList *params)
{
    int paramsCount = params ? params->size() : 0;

    PyObject *pArgs = PyTuple_New(4);
    if (!pArgs)
    {
        PyErr_Print();
        return false;
    }

    PyObject *py_Params = PyTuple_New(paramsCount);

    if (!py_Params)
    {
        PyErr_Print();
        return false;
    }

    for (int i = 0; i < paramsCount; i++)
        PyTuple_SetItem(py_Params, i, PyUnicode_FromString((*params)[i].c_str()));

    PyTuple_SetItem(pArgs, 0, PyLong_FromLong(playerID));
    PyTuple_SetItem(pArgs, 1, PyUnicode_FromString(command.c_str()));
    PyTuple_SetItem(pArgs, 2, PyUnicode_FromString(message.c_str()));
    PyTuple_SetItem(pArgs, 3, py_Params);

    PyObject *pValue = PyObject_CallObject(bz_slashCommand, pArgs);
    Py_DECREF(pArgs);
    if (!pValue)
    {
        PyErr_Print();
        return false;
    }
    int result = PyObject_IsTrue(pValue);
    Py_DECREF(pValue);

    return result;
}

void PlayerHandler::added(int player)
{
    if (!addedServerPlayer)
        return;

    PyObject *pArgs = PyTuple_New(1);
    if (!pArgs)
    {
        PyErr_Print();
        return;
    }

    PyTuple_SetItem(pArgs, 0, PyLong_FromLong(player));
    PyObject *pValue = PyObject_CallObject(addedServerPlayer, pArgs);
    Py_DECREF(pArgs);
    if (!pValue)
    {
        PyErr_Print();
        return;
    }
}

void startPython (const char *fileName, const char *args)
{
    PyStatus status;

    // Python initialization
    status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status))
    {
        /* Display the error message and exit the process with
         * non-zero exit code */
        Py_ExitStatusException(status);
    }

    PyObject* sysPath = PySys_GetObject("path");
    PyObject* curDir  = PyUnicode_FromString(".");
    PyList_Append(sysPath, curDir);
    Py_DECREF(curDir);
    curDir  = PyUnicode_FromString("python");
    PyList_Append(sysPath, curDir);
    Py_DECREF(curDir);

    // Import the bzfsItf python file
    bzfsItf_mod = PyImport_ImportModule("bzfsItf");
    if (!bzfsItf_mod)
    {
        Py_ExitStatusException(status);
        return;
    }

    if (PyModule_AddFunctions(bzfsItf_mod, bzfsItfMethods) < 0)
    {
        Py_ExitStatusException(status);
        return;
    }

    tickEventData_V1 = PyObject_GetAttrString(
                           bzfsItf_mod,
                           "bz_TickEventData_V1");
    if (!tickEventData_V1)
    {
        Py_ExitStatusException(status);
        return;
    }

    getPlayerSpawnPosEventData_V1 = PyObject_GetAttrString(
                                        bzfsItf_mod,
                                        "bz_GetPlayerSpawnPosEventData_V1");
    if (!getPlayerSpawnPosEventData_V1)
    {
        Py_ExitStatusException(status);
        return;
    }

    chatEventData_V1 = PyObject_GetAttrString(
                           bzfsItf_mod,
                           "bz_ChatEventData_V1");
    if (!chatEventData_V1)
    {
        Py_ExitStatusException(status);
        return;
    }

    flagTransferredEventData_V1 = PyObject_GetAttrString(
                                      bzfsItf_mod,
                                      "bz_FlagTransferredEventData_V1");
    if (!flagTransferredEventData_V1)
    {
        Py_ExitStatusException(status);
        return;
    }

    flagGrabbedEventData_V1 = PyObject_GetAttrString(
                                  bzfsItf_mod,
                                  "bz_FlagGrabbedEventData_V1");
    if (!flagGrabbedEventData_V1)
    {
        Py_ExitStatusException(status);
        return;
    }

    flagDroppedEventData_V1 = PyObject_GetAttrString(
                                  bzfsItf_mod,
                                  "bz_FlagDroppedEventData_V1");
    if (!flagDroppedEventData_V1)
    {
        Py_ExitStatusException(status);
        return;
    }

    shotFiredEventData_V1 = PyObject_GetAttrString(
                                bzfsItf_mod,
                                "bz_ShotFiredEventData_V1");
    if (!shotFiredEventData_V1)
    {
        Py_ExitStatusException(status);
        return;
    }

    playerDieEventData_V2 = PyObject_GetAttrString(
                                bzfsItf_mod,
                                "bz_PlayerDieEventData_V2");
    if (!playerDieEventData_V2)
    {
        Py_ExitStatusException(status);
        return;
    }

    playerUpdateEventData_V1 = PyObject_GetAttrString(
                                   bzfsItf_mod,
                                   "bz_PlayerUpdateEventData_V1");
    if (!playerUpdateEventData_V1)
    {
        Py_ExitStatusException(status);
        return;
    }

    allowFlagGrabData_V1 = PyObject_GetAttrString(
                               bzfsItf_mod,
                               "bz_AllowFlagGrabData_V1");
    if (!allowFlagGrabData_V1)
    {
        Py_ExitStatusException(status);
        return;
    }

    playerJoinPartEventData_V1 = PyObject_GetAttrString(
                                     bzfsItf_mod,
                                     "bz_PlayerJoinPartEventData_V1");
    if (!playerJoinPartEventData_V1)
    {
        Py_ExitStatusException(status);
        return;
    }

    callEvents = PyObject_GetAttrString(bzfsItf_mod, "callEvents");
    if (!callEvents)
    {
        Py_ExitStatusException(status);
        return;
    }

    /* call the class inside the bzfsItf module */
    tankCoordinate = PyObject_CallMethod(bzfsItf_mod, "TankCoordinate", "");
    if (!tankCoordinate)
    {
        Py_ExitStatusException(status);
        return;
    }

    bz_slashCommand = PyObject_GetAttrString(bzfsItf_mod, "bz_SlashCommand");
    if (!bz_slashCommand)
    {
        Py_ExitStatusException(status);
        return;
    }

    bz_isPollActive = PyObject_GetAttrString(bzfsItf_mod, "bz_isPollActive");
    if (!bz_isPollActive)
    {
        Py_ExitStatusException(status);
        return;
    }

    bz_PollOpen = PyObject_GetAttrString(bzfsItf_mod, "bz_PollOpen");
    if (!bz_PollOpen)
    {
        Py_ExitStatusException(status);
        return;
    }

    bz_PollClose = PyObject_GetAttrString(bzfsItf_mod, "bz_PollClose");
    if (!bz_PollClose)
    {
        Py_ExitStatusException(status);
        return;
    }

    bz_PollHelp = PyObject_GetAttrString(bzfsItf_mod, "bz_PollHelp");
    if (!bz_PollHelp)
    {
        Py_ExitStatusException(status);
        return;
    }

    bz_CheckIfCustomMap = PyObject_GetAttrString(bzfsItf_mod, "bz_CheckIfCustomMap");
    if (!bz_CheckIfCustomMap)
    {
        Py_ExitStatusException(status);
        return;
    }

    bz_customZoneMapObject = PyObject_GetAttrString(bzfsItf_mod, "bz_customZoneMapObject");
    if (!bz_customZoneMapObject)
    {
        Py_ExitStatusException(status);
        return;
    }

    // Decode python filename to run
    PyObject *pName = PyUnicode_DecodeFSDefault(fileName);
    if (!pName)
        return;

    // Import the given python file
    PyObject *pModule = PyImport_Import(pName);

    Py_DECREF(pName);

    if (!pModule)
    {
        PyErr_Print();
        return;
    }

    if (!PyObject_HasAttrString(pModule, "plugin"))
    {
        Py_DECREF(pModule);
        return;
    }
    plugin = PyObject_GetAttrString(pModule, "plugin");

    playerHandler = new PlayerHandler();

    if (PyObject_HasAttrString(plugin, "Init"))
    {
        PyObject *init = PyObject_GetAttrString(plugin, "Init");
        if (init && PyCallable_Check(init))
        {
            PyObject *pValue = PyObject_CallFunction(init, "s", args);
            if (!pValue)
                PyErr_Print();
            Py_XDECREF(pValue);
        }
        Py_XDECREF(init);
    }

    if (PyObject_HasAttrString(plugin, "Cleanup"))
    {
        cleanup = PyObject_GetAttrString(plugin, "Cleanup");
        if (cleanup && !PyCallable_Check(cleanup))
        {
            Py_DECREF(cleanup);
            cleanup = NULL;
        }
    }

    if (PyObject_HasAttrString(pModule, "bzAddedServerPlayer"))
    {
        addedServerPlayer = PyObject_GetAttrString(pModule,
                            "bzAddedServerPlayer");
        if (addedServerPlayer && !PyCallable_Check(addedServerPlayer))
        {
            Py_DECREF(addedServerPlayer);
            addedServerPlayer = NULL;
        }
    }

    Py_DECREF(pModule);
}

void stopPython ()
{
    if (cleanup)
    {
        PyObject *pValue = PyObject_CallObject(cleanup, NULL);
        if (!pValue)
        {
            PyErr_Print();
            return;
        }
        Py_DECREF(pValue);
        Py_DECREF(cleanup);
    }
    Py_XDECREF(plugin);

    delete playerHandler;
    playerHandler = NULL;

    Py_XDECREF(bzfsItf_mod);
    Py_XDECREF(addedServerPlayer);
    Py_XDECREF(tankCoordinate);
    Py_XDECREF(bz_slashCommand);

    if (Py_FinalizeEx() < 0)
        exit(120);
}

static PyObject *genEvent (bz_GetPlayerSpawnPosEventData_V1 &spawnData)
{
    PyObject *pX = PyFloat_FromDouble(spawnData.pos[0]);
    PyObject *pY = PyFloat_FromDouble(spawnData.pos[1]);
    PyObject *pZ = PyFloat_FromDouble(spawnData.pos[2]);
    PyObject *pR = PyFloat_FromDouble(spawnData.rot);

    int result = 0;
    if (pX && (result >= 0))
        result = PyObject_SetAttrString(tankCoordinate, "x", pX);

    if (pY && (result >= 0))
        result = PyObject_SetAttrString(tankCoordinate, "y", pY);

    if (pZ && (result >= 0))
        result = PyObject_SetAttrString(tankCoordinate, "z", pZ);

    if (pR && (result >= 0))
        result = PyObject_SetAttrString(tankCoordinate, "rot", pR);

    Py_XDECREF(pX);
    Py_XDECREF(pY);
    Py_XDECREF(pZ);
    Py_XDECREF(pR);

    if (result < 0)
        return nullptr;

    return PyObject_CallFunctionObjArgs(getPlayerSpawnPosEventData_V1,
                                        tankCoordinate,
                                        NULL);
}

static void treatEvent (
    bz_GetPlayerSpawnPosEventData_V1 &spawnData,
    PyObject *pValue)
{
    PyObject *pPos = PyObject_GetAttrString(pValue, "pos");
    if (!pPos)
    {
        PyErr_Print();
        return;
    }

    PyObject *pX = PyObject_GetAttrString(pPos, "x");
    PyObject *pY = PyObject_GetAttrString(pPos, "y");
    PyObject *pZ = PyObject_GetAttrString(pPos, "z");
    PyObject *pR = PyObject_GetAttrString(pPos, "rot");

    if (pX)
        spawnData.pos[0] = PyFloat_AsDouble(pX);
    if (pY)
        spawnData.pos[1] = PyFloat_AsDouble(pY);
    if (pZ)
        spawnData.pos[2] = PyFloat_AsDouble(pZ);
    if (pZ)
        spawnData.rot = PyFloat_AsDouble(pR);
    Py_DECREF(pPos);
}

static PyObject *genEvent (bz_ChatEventData_V1 &chatData)
{
    PyObject *pArgs = PyTuple_New(2);
    if (!pArgs)
        return nullptr;

    PyTuple_SetItem(pArgs, 0, PyLong_FromLong(chatData.from));
    PyTuple_SetItem(pArgs, 1, PyUnicode_FromString(chatData.message.c_str()));

    PyObject *pEvent = PyObject_CallObject(chatEventData_V1, pArgs);
    Py_DECREF(pArgs);
    return pEvent;
}

static PyObject *genEvent (bz_FlagTransferredEventData_V1 &data)
{
    PyObject *pArg = PyUnicode_FromString(data.flagType);
    if (!pArg)
        return nullptr;

    PyObject *pEvent = PyObject_CallFunctionObjArgs(
                           flagTransferredEventData_V1,
                           pArg,
                           NULL);
    Py_DECREF(pArg);
    return pEvent;
}

static PyObject *genEvent (bz_FlagGrabbedEventData_V1 &data)
{
    PyObject *pArg = PyUnicode_FromString(data.flagType);
    if (!pArg)
        return nullptr;

    PyObject *pEvent = PyObject_CallFunctionObjArgs(flagGrabbedEventData_V1,
                       pArg,
                       NULL);
    Py_DECREF(pArg);
    return pEvent;
}

static PyObject *genEvent (bz_FlagDroppedEventData_V1 &data)
{
    PyObject *pArg = PyUnicode_FromString(data.flagType);
    if (!pArg)
        return nullptr;

    PyObject *pEvent = PyObject_CallFunctionObjArgs(flagDroppedEventData_V1,
                       pArg,
                       NULL);
    Py_DECREF(pArg);
    return pEvent;
}

static PyObject *genEvent (bz_ShotFiredEventData_V1 &data)
{
    PyObject *pArg = PyLong_FromLong(data.playerID);
    if (!pArg)
        return nullptr;

    PyObject *pEvent = PyObject_CallFunctionObjArgs(shotFiredEventData_V1,
                       pArg,
                       NULL);
    Py_DECREF(pArg);
    return pEvent;
}

static PyObject *genEvent (bz_PlayerDieEventData_V2 &data)
{
    PyObject *pArgs = PyTuple_New(2);
    if (!pArgs)
        return nullptr;
    PyTuple_SetItem(pArgs, 0, PyLong_FromLong(data.playerID));
    PyTuple_SetItem(pArgs, 1, PyUnicode_FromString(data.flagKilledWith.c_str()));

    PyObject *pEvent = PyObject_CallObject(playerDieEventData_V2, pArgs);
    Py_DECREF(pArgs);
    return pEvent;
}

static PyObject *genEvent (bz_PlayerUpdateEventData_V1 &data)
{
    PyObject *pArgs = PyTuple_New(2);
    if (!pArgs)
        return nullptr;

    PyObject *pPos = PyTuple_New(3);
    if (!pPos)
        return nullptr;
    for (int i = 0; i < 3; i++)
        PyTuple_SetItem(pPos, i, PyFloat_FromDouble(data.state.pos[i]));

    PyTuple_SetItem(pArgs, 0, PyLong_FromLong(data.playerID));
    PyTuple_SetItem(pArgs, 1, pPos);

    PyObject *pEvent = PyObject_CallObject(playerUpdateEventData_V1, pArgs);
    Py_DECREF(pArgs);
    return pEvent;
}

static PyObject *genEvent (bz_AllowFlagGrabData_V1 &data)
{
    PyObject *pArgs = PyTuple_New(2);
    if (!pArgs)
        return nullptr;

    PyTuple_SetItem(pArgs, 0, PyLong_FromLong(data.flagID));
    PyTuple_SetItem(pArgs, 1, PyLong_FromLong(data.playerID));

    PyObject *pEvent = PyObject_CallObject(allowFlagGrabData_V1, pArgs);
    Py_DECREF(pArgs);
    return pEvent;
}

static void treatEvent (bz_AllowFlagGrabData_V1 &data, PyObject *pValue)
{
    PyObject *pAllow = PyObject_GetAttrString(pValue, "allow");
    if (!pAllow)
    {
        PyErr_Print();
        return;
    }

    data.allow = PyObject_IsTrue(pAllow);
    Py_DECREF(pAllow);
}

static PyObject *genEvent (bz_PlayerJoinPartEventData_V1 &data)
{
    PyObject *pArgs = PyTuple_New(2);
    if (!pArgs)
        return nullptr;

    bool part = (data.eventType == bz_ePlayerPartEvent);
    PyTuple_SetItem(pArgs, 0, PyLong_FromLong(part ? 1 : 0));
    PyTuple_SetItem(pArgs, 1, PyLong_FromLong(data.record->team));

    PyObject *pEvent = PyObject_CallObject(playerJoinPartEventData_V1, pArgs);
    Py_DECREF(pArgs);
    return pEvent;
}

float getPythonMinWaitTime ( void )
{
    return maxTime;
}

void bzPythonEvent(bz_EventData *eventData)
{
    PyObject *pEvent;

    switch (eventData->eventType)
    {
    case bz_eTickEvent:
        pEvent = PyObject_CallFunctionObjArgs(tickEventData_V1, NULL);
        break;
    case bz_eGetPlayerSpawnPosEvent:
        pEvent = genEvent (
                     *dynamic_cast<bz_GetPlayerSpawnPosEventData_V1 *>(eventData));
        break;
    case bz_eRawChatMessageEvent:
        pEvent = genEvent (*dynamic_cast<bz_ChatEventData_V1 *>(eventData));
        break;
    case bz_eFlagTransferredEvent:
        pEvent = genEvent (
                     *dynamic_cast<bz_FlagTransferredEventData_V1 *>(eventData));
        break;
    case bz_eFlagGrabbedEvent:
        pEvent = genEvent (*dynamic_cast<bz_FlagGrabbedEventData_V1 *>(eventData));
        break;
    case bz_eFlagDroppedEvent:
        pEvent = genEvent (*dynamic_cast<bz_FlagDroppedEventData_V1 *>(eventData));
        break;
    case bz_eShotFiredEvent:
        pEvent = genEvent (*dynamic_cast<bz_ShotFiredEventData_V1 *>(eventData));
        break;
    case bz_ePlayerDieEvent:
        pEvent = genEvent (*dynamic_cast<bz_PlayerDieEventData_V2 *>(eventData));
        break;
    case bz_ePlayerUpdateEvent:
        pEvent = genEvent (*dynamic_cast<bz_PlayerUpdateEventData_V1 *>(eventData));
        break;
    case bz_eAllowFlagGrab:
        pEvent = genEvent(*dynamic_cast<bz_AllowFlagGrabData_V1 *>(eventData));
        break;
    case bz_ePlayerJoinEvent:
    case bz_ePlayerPartEvent:
        pEvent = genEvent(*dynamic_cast<bz_PlayerJoinPartEventData_V1 *>(eventData));
        break;
    default:
        return;
    }

    if (!pEvent)
    {
        PyErr_Print();
        return;
    }

    PyObject *pValue = PyObject_CallFunctionObjArgs(callEvents, pEvent, NULL);
    Py_DECREF(pEvent);
    if (!pValue)
        PyErr_Print();

    if (!pValue)
    {
        Py_DECREF(pEvent);
        return;
    }

    switch (eventData->eventType)
    {
    case bz_eGetPlayerSpawnPosEvent:
        treatEvent (
            *dynamic_cast<bz_GetPlayerSpawnPosEventData_V1 *>(eventData), pValue);
        break;
    case bz_eAllowFlagGrab:
        treatEvent(*dynamic_cast<bz_AllowFlagGrabData_V1 *>(eventData), pValue);
        break;
    default:
        break;
    }

    Py_DECREF(pValue);
}

bool bzPython_isPollActive(std::string command)
{
    PyObject *pArgs = PyTuple_New(1);
    if (!pArgs)
    {
        PyErr_Print();
        return false;
    }

    PyTuple_SetItem(pArgs, 0, PyUnicode_FromString(command.c_str()));

    PyObject *pValue = PyObject_CallObject(bz_isPollActive, pArgs);
    Py_DECREF(pArgs);
    if (!pValue)
    {
        PyErr_Print();
        return false;
    }
    int result = PyObject_IsTrue(pValue);
    Py_DECREF(pValue);

    return result;
}

bool bzPython_PollOpen(std::string command, int playerID, const char *target)
{
    PyObject *pArgs = PyTuple_New(3);
    if (!pArgs)
    {
        PyErr_Print();
        return false;
    }

    PyTuple_SetItem(pArgs, 0, PyUnicode_FromString(command.c_str()));
    PyTuple_SetItem(pArgs, 1, PyLong_FromLong(playerID));
    PyTuple_SetItem(pArgs, 2, PyUnicode_FromString(target));

    PyObject *pValue = PyObject_CallObject(bz_PollOpen, pArgs);
    Py_DECREF(pArgs);
    if (!pValue)
    {
        PyErr_Print();
        return false;
    }
    int result = PyObject_IsTrue(pValue);
    Py_DECREF(pValue);

    return result;
}

bool bzPython_PollClose(std::string command, const char *target,
                        bool success)
{
    PyObject *pArgs = PyTuple_New(3);
    if (!pArgs)
    {
        PyErr_Print();
        return false;
    }

    PyTuple_SetItem(pArgs, 0, PyUnicode_FromString(command.c_str()));
    PyTuple_SetItem(pArgs, 1, PyUnicode_FromString(target));
    PyTuple_SetItem(pArgs, 2, PyBool_FromLong(success));

    PyObject *pValue = PyObject_CallObject(bz_PollClose, pArgs);
    Py_DECREF(pArgs);
    if (!pValue)
    {
        PyErr_Print();
        return false;
    }
    int result = PyObject_IsTrue(pValue);
    Py_DECREF(pValue);

    return result;
}

void bzPython_PollHelp(int playerID)
{
    PyObject *pArgs = PyTuple_New(1);
    if (!pArgs)
    {
        PyErr_Print();
        return;
    }

    PyTuple_SetItem(pArgs, 0, PyLong_FromLong(playerID));

    PyObject *pValue = PyObject_CallObject(bz_PollHelp, pArgs);
    Py_DECREF(pArgs);
    if (!pValue)
    {
        PyErr_Print();
        return;
    }
    Py_DECREF(pValue);
}

bool bzPython_CheckIfCustomMap(const std::string &customObject)
{
    PyObject *pArgs = PyTuple_New(1);
    if (!pArgs)
    {
        PyErr_Print();
        return false;
    }

    PyTuple_SetItem(pArgs, 0, PyUnicode_FromString(customObject.c_str()));

    PyObject *pValue = PyObject_CallObject(bz_CheckIfCustomMap, pArgs);
    Py_DECREF(pArgs);
    if (!pValue)
    {
        PyErr_Print();
        return false;
    }
    int result = PyObject_IsTrue(pValue);
    Py_DECREF(pValue);

    return result;
}

void bzPython_customZoneMapObject(
    const std::string &customObject,
    const std::vector<std::string> &customLines)
{
    PyObject *pArgs = PyTuple_New(2);
    if (!pArgs)
    {
        PyErr_Print();
        return;
    }

    PyObject *pLines = PyTuple_New(customLines.size());
    if (!pArgs)
    {
        PyErr_Print();
        return;
    }

    for (unsigned int i = 0; i < customLines.size(); i++)
        PyTuple_SetItem(pLines, i, PyUnicode_FromString(customLines[i].c_str()));

    PyTuple_SetItem(pArgs, 0, PyUnicode_FromString(customObject.c_str()));
    PyTuple_SetItem(pArgs, 1, pLines);

    PyObject *pValue = PyObject_CallObject(bz_customZoneMapObject, pArgs);
    Py_DECREF(pArgs);
    if (!pValue)
    {
        PyErr_Print();
        return;
    }
    Py_DECREF(pValue);
}

#else
void initPython (const char *argv0, const char *fileName) {}
void destroyPython () {}
void bzTickEvent () {}
#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
