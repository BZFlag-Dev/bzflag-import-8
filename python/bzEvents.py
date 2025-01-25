from bzBasic import *

bz_eGetPlayerSpawnPosEvent = 1
bz_eTickEvent              = 2
bz_eRawChatMessageEvent    = 3
bz_eFlagTransferredEvent   = 4
bz_eFlagGrabbedEvent       = 5
bz_eFlagDroppedEvent       = 6
bz_eShotFiredEvent         = 7
bz_ePlayerDieEvent         = 8
bz_ePlayerUpdateEvent      = 9
bz_eAllowFlagGrab          = 10
bz_ePlayerJoinEvent        = 11
bz_ePlayerPartEvent        = 12

class bz_EventData():
    def __init__(self, eventType):
        self.eventType = eventType

class bz_TickEventData_V1 (bz_EventData):
    def __init__(self):
        super().__init__(bz_eTickEvent)

class bz_GetPlayerSpawnPosEventData_V1 (bz_EventData):
    def __init__(self, pos):
        super().__init__(bz_eGetPlayerSpawnPosEvent)
        self.pos = pos

class bz_ChatEventData_V1 (bz_EventData):
    def __init__(self, fromPlayer, message):
        super().__init__(bz_eRawChatMessageEvent)
        self.fromPlayer = fromPlayer
        self.message    = message

class bz_FlagTransferredEventData_V1 (bz_EventData):
    def __init__(self, flagType):
        super().__init__(bz_eFlagTransferredEvent)
        self.flagType = flagType

class bz_FlagGrabbedEventData_V1 (bz_EventData):
    def __init__(self, flagType):
        super().__init__(bz_eFlagGrabbedEvent)
        self.flagType = flagType

class bz_FlagDroppedEventData_V1 (bz_EventData):
    def __init__(self, flagType):
        super().__init__(bz_eFlagDroppedEvent)
        self.flagType = flagType

class bz_ShotFiredEventData_V1 (bz_EventData):
    def __init__(self, playerID):
        super().__init__(bz_eShotFiredEvent)
        self.playerID = playerID

class bz_PlayerDieEventData_V2 (bz_EventData):
    def __init__(self, playerID, flagKilledWith):
        super().__init__(bz_ePlayerDieEvent)
        self.playerID       = playerID
        self.flagKilledWith = flagKilledWith

class bz_PlayerUpdateEventData_V1 (bz_EventData):
    def __init__(self, playerID, pos):
        super().__init__(bz_ePlayerUpdateEvent)
        self.playerID  = playerID
        self.state     = bz_PlayerUpdateState()
        self.state.pos = pos

class bz_AllowFlagGrabData_V1 (bz_EventData):
    allow = True

    def __init__(self, flagID, playerID):
        super().__init__(bz_eAllowFlagGrab)
        self.flagID   = flagID
        self.playerID = playerID

class bz_BasePlayerRecord():
    def __init__(self, team):
        self.team = team

class bz_PlayerJoinPartEventData_V1 (bz_EventData):
    def __init__(self, part, team):
        if part:
            super().__init__(bz_ePlayerPartEvent)
        else:
            super().__init__(bz_ePlayerJoinEvent)
        self.record = bz_BasePlayerRecord(bz_eTeamType(team))

# events handling

eventHandler = []
def RegisterEvent (eventType, plugin):
    eventHandler.append((eventType, plugin))

def RemoveEvent (eventType, plugin):
    eventHandler.remove((eventType, plugin))

def FlushEvents(plugin):
    for couple in eventHandler:
        if couple[1] == plugin:
            eventHandler.remove(couple)

def callEvents(event):
    for couple in eventHandler:
        if not isinstance(event, bz_EventData):
            break
        if couple[0] != event.eventType:
            continue
        event = couple[1].Event(event)
    return event
