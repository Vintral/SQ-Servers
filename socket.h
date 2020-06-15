#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <arpa/inet.h>
#include <thread> 
#include "types.h"
#include <netinet/in.h>

#define MAX_PACKET_SIZE 1024

using namespace std;

typedef struct {
    vector<uint8> data;
    int size;
    const char* ip;
    int port;
} Packet;

class Socket {
public:
    Socket();
    ~Socket();

    sockaddr_in getAddress();
    void setPort( int );
    void setIP( string );
    int getPacket( string&, sockaddr_in& );
    int getPacket( unsigned char*, sockaddr_in& );
    int getPacket( vector<uint8>&, sockaddr_in& );
    int getConnection( sockaddr_in& );
    void Bind();
    void Connect();
    sockaddr_in getClient();

    void SendReliable( vector<uint8>, const char*, int );
    void Send( vector<uint8>, const char*, int );    
    void setIP( char* );

    void setTCP();
    void setUDP();

    void setIPv4();
    void setIPv6();

    void setTrackByKey();
    void setTrackByID();

    void setDebug( bool );
    void Kill();    
protected:
    int _handle;
    char _buffer[ MAX_PACKET_SIZE ];
    void setDefaults();
    void dumpBytes( unsigned char*, int  );
    void dumpBytes( vector<uint8>, int );
    void dumpBytes( vector<uint8> );
    unsigned int len;
    int port;
    int type;
    int domain;
    bool exiting;

    unsigned char packetID;

    struct sockaddr_in _address, _client;
    bool _debug;

    thread reliableThread;

    bool trackByKey;

    string convertAddressToKey( sockaddr_in );

    void reliableDaemon();    
    unordered_map<int, Packet> _reliablePackets;
    unordered_map<string, unordered_map<int,timespec>> _receivedPackets;    

    void create();
    void reset();
    void debug( std::string );
};