#pragma once

enum packet_map {
    PACKET_ID,
    COMMAND,
    CLIENT_ID_1,
    CLIENT_ID_2
};

enum packet_type {
    EMPTY,
    PING,
    PONG,
    ACK,
    INITIALIZE,
    INITIALIZED,    
    REQUEST_ADDRESS,
    ADDRESS,
    STATE,
    JOIN,
    JOINED,
    ID,
    DISCONNECT,
    SELECT_SHIP,
    TURN,
    STOP_TURN,
    ACCELERATE,
    STOP_ACCELERATE,
    DECELERATE,
    STOP_DECELERATE,
    FIRE,
    USE,
    READY
};