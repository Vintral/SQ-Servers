#pragma once

#include <iostream>
#include <memory>
#include <netinet/in.h>

// Various typedefs
typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef long long int64;
typedef int int32;
typedef short int16;
typedef char int8;
typedef int bool32;
typedef float float32;
typedef double float64;

// Max Buffer size
const uint32 MAXLINE = 1024;

// Master server info
const char *masterAddress = "172.31.11.230";
const int masterPort = 8050;

// Position definition
typedef struct {
    int id;
    unsigned char type;
    unsigned char x;
    unsigned char y;
    int angle;    
} POSITION;

enum ServerType { MASTER, LOGIN, GAME, USER };

std::string convertAddressToKey( sockaddr_in address ) {
    char ip[INET_ADDRSTRLEN];
    int port;

    inet_ntop( AF_INET, &( address.sin_addr ), ip, INET_ADDRSTRLEN );
    port = ntohs( address.sin_port );

    std::string key = ip;
    key.append( ":" );
    key.append( std::to_string( port ) );

    std::cout<<"KEY: "<<key<<std::endl;    

    return key;
}