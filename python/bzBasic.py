from enum import IntEnum

class bz_PlayerUpdateState():
    pass

class bz_eTeamType(IntEnum):
    eNoTeam         = -1
    eRogueTeam      =  0
    eRedTeam        =  1
    eGreenTeam      =  2
    eBlueTeam       =  3
    ePurpleTeam     =  4
    eRabbitTeam     =  5
    eHunterTeam     =  6
    eObservers      =  7
    eAdministrators =  8
