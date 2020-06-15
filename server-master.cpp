// Server side implementation of UDP client-server model 
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <time.h>
#include <thread>
#include <vector>
#include <cpp_redis/cpp_redis>
#include <tacopie/tacopie>
#include <signal.h>
#include <unordered_map>

#include "server.h"
#include "packet.h"

#include "socket.h"

using namespace std;

cpp_redis::subscriber sub;
cpp_redis::client redis;

bool exiting = false;

struct sockaddr_in servaddr, cliaddr;
int sockfd;
const uint16 PORT = 8050;

typedef struct {
    in_addr ip;
    char *ipString;
    int port;
    int users;
    int usage;
    bool valid;
} ServerInfo;

Socket *connection;
unordered_map<string,ServerInfo> gameServers;
unordered_map<string,ServerInfo> loginServers;
unordered_map<string,ServerInfo> userServers;

vector<uint8> buffer;

void initialize();
void initializeKillHandler();
void initRedis();
void spawnGameServer();
void spawnLoginServer();
void initializeSockets();
void cleanUp();
void sig_term_handler( int, siginfo_t*, void* );
void processNewServer( sockaddr_in, vector<uint8> );
void dumpBytes( string, unsigned char *, int );

// Driver code 
int main() {
    initialize();    

    // Make sure we have Starting Servers

    //spawnGameServer();
    //spawnLoginServer();

    int MAXLINE = 1024;
    buffer = vector<uint8>( MAXLINE );
    int n;
    unsigned int len = sizeof( cliaddr );

    char ip[INET_ADDRSTRLEN];
    int port;
    string key;    

    /*redis.publish( "space_quest", "INITIALIZE|GAME|" + guid , [](cpp_redis::reply& reply) {
        cout<<"Sent!"<<endl;
    });*/
    
    //auto client = new Socket();

    unordered_map<string,ServerInfo>::iterator itr;
    while( !exiting ) {
        n = connection->getPacket( buffer, cliaddr );
        //n = recvfrom(sockfd, buffer, MAXLINE, MSG_DONTWAIT, ( struct sockaddr *) &cliaddr, &len);
        while( n != -1 ) {            
            cout<<"GOT A PACKET"<<endl;
            cout<<"Uh...size is:"<<buffer.size()<<endl;   
            
            cout<<"Server("<<n<<"): ";
            for( int l = 0; l < n; l++ )
                cout<<static_cast<int>(buffer[ l ])<<" | ";
            cout<<endl;

            switch( buffer[ 1 ] ) {
                case INITIALIZE: // Notification of New Server  
                    cout<<"Uh...size is:"<<buffer.size()<<endl;              
                    processNewServer( cliaddr, buffer );
                    break;
                case STATE: // Server info update                    
                    key = convertAddressToKey( cliaddr );

                    if( gameServers.find( key ) != gameServers.end() ) {
                        //cout<<"UPDATE GAME SERVER"<<endl;
                    }
                    if( loginServers.find( key ) != loginServers.end() ) {
                        //cout<<"UPDATE LOGIN SERVER"<<endl;
                    }
                    break;
                case REQUEST_ADDRESS: // Ping for login server                    
                    /*client->setPort( ntohs( cliaddr.sin_port ) );
                    client->setIP( ip );

                    uint8 *packet = new uint8[ 2 ];
                    packet[ 0 ] = 1;
                    packet[ 1 ] = 7;
                    client->Send( packet );*/

                    cout<<"HELLO?"<<endl;
                    key = convertAddressToKey( cliaddr );
                    cout<<"KEY: "<<key<<endl;

                    switch( buffer[ 2 ] ) {
                        case GAME: {
                            itr = userServers.find( key );
                            if( itr != userServers.end() ) {
                                cout<<"SEND GAME SERVER"<<endl; 
                                string gameServer = gameServers.begin()->first;

                                auto len = 2 + strlen( gameServer.c_str());                                
                                vector<uint8> packet( len );
                                packet[ 0 ] = 0;
                                packet[ 1 ] = ADDRESS;
                                auto addr = gameServer.c_str();
                                for( int i = 0; i < strlen( addr ); i++ ) {
                                    packet[ 2 + i ] = addr[ i ];
                                }

                                connection->Send( packet, itr->second.ipString, itr->second.port );
                            } else cout<<"INVALID GAME SERVER REQUEST FROM NON-USER SERVER"<<endl;                            
                        }
                        break;
                        case LOGIN: cout<<"SEND LOGIN SERVER"<<endl; break;
                        case USER: {
                            itr = loginServers.find( key );
                            if( itr != loginServers.end() ) {
                                cout<<"SEND USER SERVER"<<endl; 
                                string userServer = userServers.begin()->first;

                                auto len = 2 + strlen( userServer.c_str());                                
                                vector<uint8> packet( len );
                                packet[ 0 ] = 0;
                                packet[ 1 ] = ADDRESS;
                                auto addr = userServer.c_str();
                                for( int i = 0; i < strlen( addr ); i++ ) {
                                    packet[ 2 + i ] = addr[ i ];
                                }

                                connection->Send( packet, itr->second.ipString, itr->second.port );
                            } else cout<<"INVALID USER SERVER REQUEST FROM NON-LOGIN SERVER"<<endl;
                        }
                        break;
                    }
                    
		            /*itr = loginServers.find( key );
                    if( itr != loginServers.end() ) {
                        cout<<"SEND GAME SERVER"<<endl;
                        string gameServer = gameServers.begin()->first;
                        cout<<"GAME SERVER: "<<gameServer<<endl;

                        //inet_ntop( AF_INET, &( itr->second.ip ), ip, INET_ADDRSTRLEN );
                        //client->setPort( ntohs( itr->second.port ) );
                        //client->setIP( ip );

                        //gameServer.insert( 0, (const char *)2 );
                        unsigned char *packet = new unsigned char[ 1 + strlen( gameServer.c_str()) ];
                        packet[ 0 ] = ADDRESS;
                        auto addr = gameServer.c_str();
                        for( int i = 0; i < strlen( addr ); i++ ) {
                            packet[ 1 + i ] = addr[ i ];
                        }

                        cout<<"LENGTH:"<<strlen(gameServer.c_str())<<endl;
                        //char *packet = strcat( packID, (const char*)gameServer.c_str() );
                        cout<<"VAL: "<<packet<<endl;
                        //client->Send( packet );                    
                        connection->Send( packet, itr->second.ipString, itr->second.port );
                    }*/
                    break;
                default: cout<<"NO HANDLER FOR: "<<buffer[ 0 ]<<endl; break;
            }
            
            n = connection->getPacket( buffer, cliaddr );
        }

        usleep( 1000000 );        
        //cout<<"TICK"<<endl;
    }

    return 0; 
}

void initRedis() {
    cout<<"Initializing Redis"<<endl;    

    cout<<"Trying to connect...";
    sub.connect( "game.fbfafa.0001.usw1.cache.amazonaws.com", 6379, [](const string& host, size_t port, cpp_redis::subscriber::connect_state status) {
        if (status == cpp_redis::subscriber::connect_state::dropped) {
            cout<<"client disconnected from "<<host<<":"<<port<<endl;            
        }
    } );
    cout<<"Success!"<<endl;

    sub.subscribe( "space_quest", [](const string& chan, const string& msg) {
        cout<<"MESSAGE "<<chan<<": "<<msg<<endl;
    } );    
    cout<<"After Subscribing"<<endl;

    sub.commit();

    cout<<"Connecting..."<<endl;
    redis.connect( "game.fbfafa.0001.usw1.cache.amazonaws.com", 6379, [](const string& host, size_t port, cpp_redis::client::connect_state status) {
        if (status == cpp_redis::client::connect_state::dropped) {
            cout << "client disconnected from " << host << ":" << port << endl;
        }
    });
    cout<<"...connected!"<<endl;    

    cout<<"Sending Messages...";
    redis.publish( "game_server", "STARTED", [](cpp_redis::reply& reply) {
        cout<<"Sent to Game Servers....";
    });
    redis.publish( "login_server", "STARTED", [](cpp_redis::reply& reply) {
        cout<<"Sent to Login Servers...";
    });
    redis.publish( "user_server", "STARTED", [](cpp_redis::reply& reply) {
        cout<<"Sent to User Servers...";
    });
    redis.commit();
    cout<<"Sent!..."<<endl;
}

void spawnGameServer() {
    cout<<"Starting Game Server...";
    system( "/home/ec2-user/Space/server-game.o 8100 &" );
    cout<<"done!"<<endl;
}

void spawnLoginServer() {
    int port = 0;
    for( auto it = loginServers.begin(); it != loginServers.end(); ++it ) {
        cout<<"PORT1: "<<it->second.port<<endl;
    }

    cout<<"Starting Login Server...";
    system( "/home/ec2-user/Space/server-login.o 8077 &" );
    cout<<"done!"<<endl;
}

void spawnUserServer() {
    int port = 0;
    for( auto it = loginServers.begin(); it != loginServers.end(); ++it ) {
        cout<<"PORT1: "<<it->second.port<<endl;
    }

    cout<<"Starting User Server...";
    system( "/home/ec2-user/Space/server-user.o 8078 &" );
    cout<<"done!"<<endl;
}

void initializeSockets() {
    // Clear out the structs    
    memset( &cliaddr, 0, sizeof( cliaddr ) );

    connection = new Socket();
    connection->setPort( PORT );    
    connection->Bind();
}

void cleanUp() {
    cout<<"Cleaning Up...";

    exiting = true;

    cout<<"Killing Connection...";
    connection->Kill();
    delete connection;
    cout<<"Done!"<<endl;

    //system( "/home/ec2-user/Space/stop" );

    cout<<"Done!"<<endl;
}

void sig_term_handler(int signum, siginfo_t *info, void *ptr){
    if( signum == 2 ) cleanUp();
    exit( 0 );
}

void initializeKillHandler() {
    cout<<"Setup Kill Handler"<<endl;

    static struct sigaction _sigact;

    memset(&_sigact, 0, sizeof(_sigact));
    _sigact.sa_sigaction = sig_term_handler;
    _sigact.sa_flags = SA_SIGINFO;

    sigaction( SIGTERM, &_sigact, NULL );
    sigaction( SIGINT, &_sigact, NULL ); // Handles ctrl-c
}

void initialize() {
    cout<<"Initializing Server"<<endl;
        
    initializeKillHandler();
    initializeSockets();
    initRedis();
}

void processNewServer( sockaddr_in address, vector<uint8> packet ) {
    char ip[INET_ADDRSTRLEN];

    inet_ntop( AF_INET, &( address.sin_addr ), ip, INET_ADDRSTRLEN );            
    int port = ntohs( address.sin_port );

    /*port = packet[ 2 ] + ( packet[ 3 ] << 8 );*/

    cout<<"PORT IS: "<<port<<endl;
	
    ServerInfo server;
    memset( &server, 0, sizeof( server ) );
    server.ip = address.sin_addr;
    server.ipString = ip;
    server.port = port;

    string key = ip;
    key.append( ":" );
    key.append( to_string( port ) );
    
    cout<<"New Server";

    cout<<" -- "<<key<<" --- ";

    cout<<"PACKET LENGTH: "<<packet.size()<<endl;
    
    uint8 *pack;

    switch( packet[ 2 ] ) {
        case MASTER: cout<<"Master Server"; break;
        case USER:
            cout<<"User Server";
            userServers[ key ] = server;
            break;
        case LOGIN:
            cout<<"Login Server"; 
            loginServers[ key ] = server;
            break;        
        case GAME:
            cout<<"Game Server";
            gameServers[ key ] = server;            
            break;
        default:
            cout<<"Unknown Server Type!";
            break;        
    }

    cout<<endl;
}
