import time
import math
import re
from bzEvents import *
from bzBasic  import *

class TankCoordinate:
    """bzflag tank coordinate"""

    def __init__(self):
        self.x   = 0.0
        self.y   = 0.0
        self.z   = 0.0
        self.rot = 0.0

    def __repr__(self):
        return f'<TankCoordinate: x:{self.x:.2f} y:{self.y:.2f} z:{self.z:.2f} rot:{self.rot:.2f}>'

class bz_BasePlayerRecord:
    """Player Info"""

    version        = 1
    playerID       = -1
    callsign       = ''
    team           = bz_eTeamType.eNoTeam
    ipAddress      = ''
    currentFlagID  = -1
    currentFlag    = ''
    flagHistory    = []
    lastUpdateTime = 0.0
    #bz_PlayerUpdateState lastKnownState;
    clientVersion  = ''
    spawned        = False
    verified       = False
    globalUser     = False
    bzID           = ''
    admin          = False
    op             = False
    canSpawn       = False
    groups         = []
    lag            = 0
    jitter         = 0
    packetLoss     = 0.0
    rank           = 0.0
    wins           = 0
    losses         = 0
    teamKills      = 0

def bz_getPlayerBySlotOrCallsign(name):
    playerID = bz_getPlayerIDByName(name)
    return bz_getPlayerByIndex(playerID)

class bzTime():
    pass

def bz_getLocaltime():
    now           = time.localtime()
    myTime        = bzTime()
    myTime.minute = now.tm_min
    myTime.hour   = now.tm_hour
    myTime.day    = now.tm_mday
    myTime.month  = now.tm_mon
    myTime.year   = now.tm_year
    myTime.dow    = (now.tm_wday + 1) % 7
    return myTime

class bz_CustomMapObjectHandler():
    pass

class bz_CustomZoneObject():
    box      = False
    radius2  = 25
    cX       = 0
    cY       = 0
    cZ       = 0
    hh       = 0
    hw       = 0
    height   = 5
    sin_val  = 0
    cos_val  = 1

    def pointInZone(self, pos):
        # Coordinates of player relative to the "fake" origin
        px = pos[0] - self.cX;
        py = pos[1] - self.cY;
        pz = pos[2] - self.cZ;

        if pos[2] < 0:
            return False
        if pos[2] > self.height:
            return False

        if self.box:
            # Instead of rotating the box against (0,0)
            # rotates the world in the opposite direction
            px, py = \
                    ( px * self.cos_val + py * self.sin_val), \
                    (-px * self.sin_val + py * self.cos_val)
            # As the world is now simmetric remove the sign
            px = abs(px)
            py = abs(py)

            # Check now it the point is within the box
            if px > self.hw:
                return False
            if py > self.hh:
                return False
        else:
            dist2 = px * px + py * py

            if dist2 > self.radius2:
                return False
        return True

    def handleDefaultOptions(self, data):
        # default values just in case
        self.radius2  = 25 # Default radius 5
        self.height   = 5
        self.sin_val  = 0
        self.cos_val  = 1

        # parse all the chunks
        for line in data:
            nubs = line.split()

            if len(nubs) <= 1:
                continue

            key = nubs[0].upper()

            if key == 'BBOX' and len(nubs) > 6:
                self.box = True

                xMin = float(nubs[1])
                xMax = float(nubs[2])
                yMin = float(nubs[3])
                yMax = float(nubs[4])

                # Center of the rectangle, we can treat this as the "fake"
                # origin
                self.cX = (xMax + xMin) / 2
                self.cY = (yMax + yMin) / 2

                self.hh  = abs(yMax - yMin) / 2
                self.hw  = abs(xMax - xMin) / 2

                self.cZ     = float(nubs[5])
                self.height = float(nubs[6]) - self.cZ

                bz_debugMessage(0,
                        'WARNING: The "BBOX" attribute has been deprecated.'\
                                ' Please use the `position` and `size` '\
                                'attributes instead:')
                bz_debugMessage(0, '  position {f} {f} {f}'
                        .format(self.cX, self.cY, self.cZ))
                bz_debugMessage(0, '  size {} {} {}'
                        .format(self.hw, self.hh, self.height))
            elif key == 'CYLINDER' and len(nubs) > 5:
                self.box = False
                # Center of the cylinder
                self.cX = float(nubs[1])
                self.cY = float(nubs[2])
                self.cZ = float(nubs[3])
                self.height = float(nubs[4]) - self.cZ
                self.radius = float(nubs[5])

                bz_debugMessage(0,
                        'WARNING: The "CYLINDER" attribute has been '\
                                'deprecated. Please use `radius` and '\
                                '`height` instead:')
                bz_debugMessage(0, '  position {} {} {}'
                        .format(self.cX, self.cY, self.cZ))
                bz_debugMessage(0, '  radius {}'.format(self.radius))
                bz_debugMessage(0, '  height {}'.format(self.height))
            elif (key == 'POSITION' or key == 'POS') and len(nubs) > 3:
                self.cX = float(nubs[1])
                self.cY = float(nubs[2])
                self.cZ = float(nubs[3])
            elif key == 'SIZE' and len(nubs) > 3:
                self.box = True
                # Half Width and Half Heigth
                self.hw     = float(nubs[1])
                self.hh     = float(nubs[2])
                self.height = float(nubs[3])
            elif (key == 'ROTATION') or (key == 'ROT'):
                rotation     = float(nubs[1])
                if not 0 < rotation < 360:
                    rotation = 0
                rotRad       = rotation * math.pi / 180
                self.cos_val = math.cos(rotRad)
                self.sin_val = math.sin(rotRad)
            elif (key == 'RADIUS') or (key == 'RAD'):
                self.box     = False
                self.radius  = float(nubs[1])
                self.radius *= self.radius
            elif key == 'HEIGHT':
                self.height = float(nubs[1])

eGoodFlag        = 0
eBadFlag         = 1
eLastFlagQuality = 2

BZ_ALLUSERS      = -1

class bz_Plugin():
    MaxWaitTime = -1
    Unloadable  = True

    def Cleanup(self):
        pass

    def Event(self, eventData):
        return eventData

    # used for inter plugin communication
    def GeneralCallback(self, name, data):
        return 0

    def Register (self, eventType):
        RegisterEvent(eventType, self)

    def Remove (eventType):
        RemoveEvent(eventType, self)

    def Flush (self):
        FlushEvents(self)

# Command Handler

class bz_CustomSlashCommandHandler():
    def SlashCommand(self, playerID, command, message, params):
        pass

customCommands = {}

def bz_SlashCommand(playerID, command, message, params):
    command = command.lower()
    if command not in customCommands:
        return False
    handler = customCommands[command]
    return handler.SlashCommand(playerID, command, message, params)

def bz_registerCustomSlashCommand (command, handler):
    if not command:
        return False
    if not handler:
        return False

    customCommands[command.lower()] = handler

    return True

def bz_removeCustomSlashCommand (command):
    if not command:
        return False

    command = command.lower()
    del customCommands[command]
    return True

# Custom poll section ################################

class bz_CustomPollTypeHandler():
    # Should return false to prevent the poll from starting
    def PollOpen (self, player, action, parameters):
        return False
    def PollClose (self, action, parameters, success):
        pass

def bz_registerCustomPollType (option, parameters, handler):
    if not option:
        return False
    if not handler:
        return False

    registerCustomPollType(option, parameters, handler)
    return True

def bz_removeCustomPollType (option):
    if not option:
        return False

    removeCustomPollType(option)
    return True

# Custom Poll Types

class PollType():
    pollParameters = ''
    pollHandler    = None

customPollTypes = {}

def registerCustomPollType (option, parameters, handler):
    global customPollTypes
    o = PollType()
    o.pollParameters = parameters;
    o.pollHandler    = handler;
    objectName       = option.lower()
    customPollTypes[objectName] = o;

def removeCustomPollType (option):
    global customPollTypes
    objectName = option.lower()

    if objectName in customPollTypes:
        del customPollTypes[objectName]

def bz_isPollActive(cmd):
    global customPollTypes
    return cmd in customPollTypes

def bz_PollOpen(cmd, playerId, target):
    global customPollTypes
    if not cmd in customPollTypes:
        return False
    player = bz_getPlayerByIndex(playerId)
    if not player:
        return False
    return customPollTypes[cmd].pollHandler.PollOpen(player, cmd, target)

def bz_PollClose(cmd, target, success):
    global customPollTypes
    return customPollTypes[cmd].pollHandler.PollClose(cmd, target, success)

def bz_PollHelp(playerID):
    global customPollTypes
    for k, v in customPollTypes.items():
        reply = '    or /poll ' + k + ' ' + v.pollParameters
        bz_sendTextMessage(playerID, reply)


customObjectMap = {}

def bz_registerCustomMapObject (objectName, handler):
    if not objectName or not handler:
        return False

    customObjectMap[objectName.upper()] = handler;
    return True

def bz_removeCustomMapObject (objectName):
    if not objectName:
        return False

    obj = objectName.upper()
    if obj in customObjectMap:
        del customObjectMap[obj]
    return True

def bz_CheckIfCustomMap(objectName):
    return objectName in customObjectMap

def bz_customZoneMapObject(customObject, customLines):
    customObjectMap[customObject].MapObject(customObject, customLines);

def bz_tokenize(line):
    return re.findall(r'[^"\s]\S*|".+?"', line)
