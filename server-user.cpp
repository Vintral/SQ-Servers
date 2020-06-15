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
#include <memory>

#include <uuid.h>

#include "ship.h"
#include "packet.h"
#include "server.h"

#include "socket.h"

using namespace std;

cpp_redis::subscriber sub;
cpp_redis::client redis;

bool initialized = false;
bool debug = false;

Socket *connection;

class UserInfo {
public:
    int id;
    string ip;
    int port;
    string serverIP;
    int serverPort;

    UserInfo() {
        cout<<"UserInfo Constructed"<<endl;

        this->id = 0;
        this->port = 0;
        this->serverPort = 0;

        this->ip = "";
        this->serverIP = "";
    }

    ~UserInfo() {
        cout<<"UserInfo Destructed"<<endl;
    }
};

struct timespec startTime, endTime;
struct sockaddr_in cliaddr;
uint64 sleepTimes = 0;
int counter = 0;
int connections = 0;
unordered_map<string,shared_ptr<UserInfo>> usersInfo;
queue<shared_ptr<UserInfo>> pendingUsers;
queue<uint64_t> pendingRequests;
uint8 stateCounter = 0;

vector<uint8> buffer;

shared_ptr<UserInfo> currentUser;
int userID;

const int MAX_BUCKETS = 5;
int frame = 0;
//vector<shared_ptr<UserInfo> > shipsByFrame[ MAX_FRAMES ];
unordered_map<string,shared_ptr<UserInfo>> shipBucket[ MAX_BUCKETS ];

bool exiting = false;
thread *reportThread;
thread *requestThread;

void reporterDaemon();
void requestDaemon();
void initRedis();
void initialize( int );
void initializeSocket( int );
void sendNotifyActive();
void sendUniverse( shared_ptr<UserInfo>, const char*, int );
void sendState( int, unsigned char );
void sendRequestServer();
void sendJoinServer( string, const char*, int );
void sendSelectShip( shared_ptr<UserInfo>, int );
void sendTurn( shared_ptr<UserInfo>, uint8 );
void sendStopTurn( shared_ptr<UserInfo>, uint8 );
void sendAccelerate( shared_ptr<UserInfo>, uint8 );
void sendStopAccelerate( shared_ptr<UserInfo>, uint8 );
void sendDecelerate( shared_ptr<UserInfo> );
void sendStopDecelerate( shared_ptr<UserInfo> );
void sendFire( shared_ptr<UserInfo>, uint8 );
void sendUse( string, uint8 );
void sendReady( string );
void sendJoined( shared_ptr<UserInfo> );
void sendDisconnect( shared_ptr<UserInfo> );
void createUserEndpoint( sockaddr_in );
void removeUserEndpoint( sockaddr_in );
void processAddressResponse( vector<uint8>, int );
void processID( vector<uint8>, int );
int getID( string );
shared_ptr<UserInfo> getUser( string );
void addIDToPacket( vector<uint8>&, int );
void addUserToBucket( shared_ptr<UserInfo> );
void cleanUp();
void sig_term_handler( int, siginfo_t*, void* );
void initializeKillHandler();

void dumpBytes( unsigned char*, int );
void dumpBytes( vector<uint8> );

// Driver code 
int main( int argc, char *argv[] ) {    
    int port = -1;    

    // Make sure we have a port to initialize to
    if( argc > 1 ) {        
        port = atoi( argv[ 1 ] );        
    } else {
        cout<<"ERROR: NO PORT"<<endl;
        return -1;
    }
    initialize( port );

    sendNotifyActive();  

    // Fire up thread to communicate with the master
    reportThread = new thread( reporterDaemon );
    requestThread = new thread( requestDaemon );

    //==================================//
    //  Declarations                    //
    //==================================//
    int delay;
    uint64_t diff;
    int MAXLINE = 1024;    
    buffer = vector<uint8>( MAXLINE );
    int n;
    //string key;
    UserInfo userInfo;
    shared_ptr<UserInfo> user;

    struct timespec startRedis, endRedis;

    //==================================//
    //  Main Loop                       //
    //==================================//
    while( !exiting ) {
        // Grab our initial time
        clock_gettime( CLOCK_REALTIME, &startTime );

        // Grab a packet        
        n = connection->getPacket( buffer, cliaddr );        
        while( n != -1 ) {
            if( n == -2 ) continue; // -2 is a "dupe" packet flag by socket code

            cout<<"Server("<<n<<"): ";
            for( int l = 0; l < n; l++ )
                cout<<static_cast<unsigned>((unsigned char)buffer[ l ])<<" | ";
            cout<<endl;         

            // Grab the key for the packet
            auto key = convertAddressToKey( cliaddr ); 
            cout<<"RECEIVED KEY: "<<key<<endl;
            // Grab the user for the key, if one exists
            user = getUser( key );
            switch( buffer[ COMMAND ] ) {
                case 1: initialized = true; break;
                case ADDRESS: {
                    cout<<"WE HAVE AN ADDRESS"<<endl;
                    processAddressResponse( buffer, n );                  
                } break;
                case JOIN: {
                    cout<<"WE HAVE A USER"<<endl;
                    cout<<"KEY: "<<key<<endl;

                    if( user != NULL ) {
                        cout<<"BUT WE ALREADY HAVE ONE?"<<endl;
                    } else createUserEndpoint( cliaddr );
                } break;
                case JOINED: {
                    cout<<"WE HAVE JOINED GAME SERVER"<<endl;
                    
                    int offset = 2;                    
                    vector<char> k( n - offset );
                    memcpy( &k[ 0 ], &buffer[ offset ], n - offset );                    

                    sendJoined( getUser( string( k.begin(), k.end() ) ) );

                    /*cout<<"String 1: "<<testkey.size()<<endl;
                    cout<<"String 2: "<<tempkey.size()<<endl;
                    cout<<"Equals: "<<testkey.compare( tempkey )<<endl;*/
                } break;
                case ID: {
                    cout<<"WE HAVE AN ID"<<endl;
                    processID( buffer, n );
                }
                break;
                case DISCONNECT:
                    cout<<"WE LOST A USER"<<endl;
                    sendDisconnect( user );
                    removeUserEndpoint( cliaddr );
                    break;
                case SELECT_SHIP: {                  
                    cout<<"SELECT SHIP"<<endl;
                    int ship = buffer[ 2 ] + ( buffer[ 3 ] << 8 );
                    sendSelectShip( user, ship );
                } break;
                case TURN: {
                    cout<<"TURN"<<endl; 
                    uint8 dir = buffer[ 2 ];
                    sendTurn( user, dir );
                } break;
                case STOP_TURN: {
                    cout<<"STOP TURN"<<endl;
                    uint8 dir = buffer[ 2 ];
                    sendStopTurn( user, dir );
                } break;
                case ACCELERATE: {
                    cout<<"ACCELERATE"<<endl;
                    uint8 dir = buffer[ 2 ];
                    sendAccelerate( user, dir );
                } break;
                case STOP_ACCELERATE: {
                    cout<<"STOP ACCELERATE"<<endl;
                    uint8 dir = buffer[ 2 ];
                    sendStopAccelerate( user, dir );
                } break;
                case DECELERATE: {
                    cout<<"DECELERATE"<<endl;
                    sendDecelerate( user );
                } break;
                case STOP_DECELERATE: {
                    cout<<"STOP_DECELERATE"<<endl;
                    sendStopDecelerate( user );
                } break;
                case FIRE: {
                    cout<<"FIRE"<<endl;
                    uint8 weapon = buffer[ 2 ];
                    sendFire( user, weapon );
                } break;
                case USE: {
                    cout<<"USE"<<endl;
                    uint8 item = buffer[ 2 ];
                    sendUse( key, item );
                } break;
            }

            // Loop until we run out of packets
            n = connection->getPacket( buffer, cliaddr );
        }

        frame++;
        if( frame >= MAX_BUCKETS ) frame = 0;
        
        // same as client.send({ "GET", "hello" }, ...)
        //cout<<"TESTING REDIS: "<<redis.get( "universe" ).get()<<endl;        
        clock_gettime( CLOCK_REALTIME, &startTime );
        redis.get( "universe", [](cpp_redis::reply &reply ) {
            if( debug ) {
                clock_gettime( CLOCK_REALTIME, &endTime );
                auto diff = BILLION * ( endTime.tv_sec - startTime.tv_sec ) + endTime.tv_nsec - startTime.tv_nsec;
                diff /= 1000;
                diff /= 1000;

                cout<<"Redis Time: "<<diff<<endl;
            }

            int id, type;
            float x, y, angle;
            int chunkSize = 10;            
            if( reply.is_string() ) {                
                int len = reply.as_string().length();                
                if( len != 0 ) {                    
                    stateCounter++;
                    if( stateCounter > 255 ) stateCounter = 0;
                    int size = shipBucket[ frame ].size();                    
                    if( size > 0 ) {                        
                        unordered_map<string, shared_ptr<UserInfo>>::iterator it = shipBucket[ frame ].begin();
                        while( it != shipBucket[ frame ].end() ) {
                            sendUniverse( it->second, reply.as_string().c_str(), len );
                            it++;
                        }
                    }                    
                }
            }            
        } );
        redis.sync_commit();                

        // Grab our ending time
        clock_gettime( CLOCK_REALTIME, &endTime );
        diff = ( BILLION * ( endTime.tv_sec - startTime.tv_sec ) + endTime.tv_nsec - startTime.tv_nsec ) / 1000;

        // If we've taken less than our time, sleep to sync up
        if( diff < 16666 ) {
            delay = 16666 - diff;
            usleep( delay );
            sleepTimes += delay;
        }

        counter++;
    }

    return 0; 
}

void reporterDaemon() {
    cout<<"Starting up Reporter"<<endl;

    struct timespec startTime, endTime;
    uint64_t diff;
    float32 usage;

    while( !exiting ) {        
        usleep( 1000000 ); // Waits for 1 second

        if( initialized ) {
            diff = BILLION * ( endTime.tv_sec - startTime.tv_sec ) + endTime.tv_nsec - startTime.tv_nsec;
            usage = sleepTimes * 100.0 / ( counter * 16666 );

            if( debug ) {
                printf( "Frames: %d   ", counter );
                printf( "Usage: %d\%\n", (int)( 100 - usage ) );
            }

            sendState( connections, (unsigned char)( 100 - usage ) );            
        }

        counter = 0;
        sleepTimes = 0;        
    }

    cout<<"Shutting down Reporter"<<endl;
}

void requestDaemon() {
    cout<<"Starting up Requester"<<endl;

    struct timespec startTime, endTime;
    uint64_t diff;
    float32 usage;

    while( !exiting ) {        
        usleep( 1000000 ); // Waits for 1 second

        if( !pendingRequests.empty() ) {
            clock_gettime( CLOCK_REALTIME, &endTime );
            diff = BILLION * endTime.tv_sec + endTime   .tv_nsec;

            diff -= pendingRequests.front();

            if( diff > BILLION ) {
                cout<<"FOUND STALE REQUEST"<<endl;
                sendRequestServer();
            }
        }
    }

    cout<<"Shutting down Requester"<<endl;
}

void initialize( int port ) {
    initRedis();
    initializeSocket( port );
    initializeKillHandler();
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

    sub.subscribe( "user_server", [](const string& chan, const string& msg) {
        cout<<"MESSAGE "<<chan<<": "<<msg<<endl;

        // A new master started, let's notify it we exist
        if( msg.compare( "MASTER_STARTED" ) ) {
            cout<<"NEW MASTER SERVER -- CALL HOME"<<endl;

            initialized = false;
            sendNotifyActive();
        }
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
}

void initializeSocket( int port ) {
    // Clear out the structs    
    memset( &cliaddr, 0, sizeof( cliaddr ) );

    connection = new Socket();
    connection->setPort( port );
    connection->Bind();
}

shared_ptr<UserInfo> getUser( string key ) {
    cout<<"getUser: "<<key<<endl;

    if( usersInfo.find( key ) != usersInfo.end() )
        return usersInfo[ key ];
    return NULL;
}

void addUserToBucket( shared_ptr<UserInfo> user ) {
    // Make sure we're not null
    if( user == NULL ) return;

    cout<<"addUserToBucket"<<endl;

    // Find whichever bucket has the fewest ships in it
    int n = 0;
    int min = 0;
    for( int i = 0; i < MAX_BUCKETS; i++ ) {
        if( i == 0 ) {
            min = shipBucket[ i ].size();
        } else {
            if( shipBucket[ i ].size() < min ) {
                min = shipBucket[ i ].size();
                n = i;
            }
        }
    }

    // Build the key and add the user to the bucket
    string key = user->ip + ":" + to_string( user->port );
    shipBucket[ n ][ key ] = move( user );

    cout<<"Added User to Bucket  ---   Before: "<<min<<" After: "<<shipBucket[ n ].size()<<endl;
}

void processID( vector<uint8> data, int len ) {
    //dumpBytes( (unsigned char*)data, len );
    int id = data[ 2 ] + ( data[ 3 ] << 8 );
    cout<<"ID: "<<id<<endl;

    // Grab the address
    int offset = 4;
    cout<<"SIZE: "<<(len - offset)<<endl;

    string addr( data.begin(), data.end() );
    addr = addr.substr( offset, len - offset );    

    cout<<"FOR ADDRESS: "<<addr<<endl;

    shared_ptr<UserInfo> user = NULL;
    user = getUser( addr );
    if( user != NULL ) user->id = id;

    addUserToBucket( user );
    sendReady( addr );    
}

void processAddressResponse( vector<uint8> data, int len ) {
    cout<<"processAddressResponse"<<endl;

    // Pull off the first pending request
    pendingRequests.pop();

    // Grab our first user
    shared_ptr<UserInfo> userInfo = pendingUsers.front();
    pendingUsers.pop();

    if( userInfo == NULL ) cout<<"NO USER PULLED FROM PENDING"<<endl;

    dumpBytes( data );

    // Parse out our return
    int offset = 2; // Offset in the packet    
    vector<uint8> buff( len - offset );
	memcpy( &buff[ 0 ], &data[ offset ], len - offset );

    // Build the address
    string addr = string( data.begin(), data.end() );
    auto index = addr.rfind( ":" ); // Find the rightmost :
    auto port = stoi( addr.substr( index + 1, len - ( index + 1 ) ) );
    addr = addr.substr( 0, index );

    //Build the key
    string key = userInfo->ip;
    key.append( ":" );
    key.append( to_string( userInfo->port ) );

    cout<<"LOOKING FOR USER: "<<key<<endl;

    // Grab the userinfo, and set the addresses
    shared_ptr<UserInfo> u = usersInfo[ key ];
    if( u == NULL ) cout<<"USER NOT FOUND FOR: "<<key<<endl;
    u->serverIP = addr;
    u->serverPort = port;

    cout<<"Set usersInfo for: "<<key<<endl;

    // Send off our address
    sendJoinServer( key, addr.c_str(), port );  
}

void createUserEndpoint( sockaddr_in addr ) {
    cout<<"createUserEndpoint"<<endl;

    // Get the key, and check to see if we have a user with it
    auto key = convertAddressToKey( addr );
    auto user = usersInfo.find( key );
    if( user != usersInfo.end() ) {
        // TO DO -- Dupe user?
        return;
    }

    // Grab the address
    char ip[ INET_ADDRSTRLEN ];
    inet_ntop( AF_INET, &( addr.sin_addr ), ip, INET_ADDRSTRLEN );            
    int port = ntohs( addr.sin_port );

    // Create the user info struct
    auto userInfo = make_shared<UserInfo>();
    userInfo->ip = string( ip );
    userInfo->port = port;
    usersInfo[ key ] = userInfo;

    cout<<"Adding Pending User: "<<key<<"  address: "<<userInfo->ip<<":"<<userInfo->port<<endl;

    // Mark us as pending user
    pendingUsers.push( move( userInfo ) );
    sendRequestServer();    

    userInfo = pendingUsers.front();
}

void removeUserEndpoint( sockaddr_in addr ) {
    cout<<"removeUserEndpoint"<<endl;

    auto key = convertAddressToKey( cliaddr );
    auto user = usersInfo.find( key );

    for( int i = 0; i < MAX_BUCKETS; i++ ) {
        auto u = shipBucket[ i ].find( key );
        if( u != shipBucket[ i ].end() ) {
            cout<<"Found user in bucket"<<endl;
            shipBucket[ i ].erase( u );
            break;
        }
    }

    if( user != usersInfo.end() ) {
        cout<<"FOUND A USER"<<endl;
        usersInfo.erase( user );
        cout<<"ERASED USER"<<endl;
    } else cout<<"NO USER FOUND"<<endl;
}

void sendState( int connections, unsigned char usage ) {    
    vector<uint8> packet( 4 );
    packet[ 0 ] = 0;
    packet[ 1 ] = STATE;
    packet[ 2 ] = connections;
    packet[ 3 ] = usage;    

    connection->Send( packet, masterAddress, masterPort );    
}

void sendUniverse( shared_ptr<UserInfo> user, const char *universe, int size ) {
    vector<uint8> packet( size + 2 );
    packet[ 0 ] = stateCounter;
    packet[ 1 ] = STATE;
    memcpy( &packet[ 2 ], universe, size );    

    //dumpBytes( packet );
    connection->Send( packet, user->ip.c_str(), user->port );    
}

void sendNotifyActive() {
    vector<uint8> packet( 3 );
    packet[ 0 ] = 0;
    packet[ 1 ] = INITIALIZE;
    packet[ 2 ] = USER;    

    connection->Send( packet, masterAddress, masterPort );    
}

void sendRequestServer() {
    struct timespec now;
    clock_gettime( CLOCK_REALTIME, &now );
    auto diff = BILLION * now.tv_sec + now.tv_nsec;
    cout<<"NOW:"<<diff<<endl;
    pendingRequests.push( diff );
    
    vector<uint8> packet( 3 );
    packet[ 0 ] = 0;
    packet[ 1 ] = REQUEST_ADDRESS;
    packet[ 2 ] = GAME;    

    connection->SendReliable( packet, masterAddress, masterPort );    
}

void sendJoinServer( string key, const char* ip, int port ) {
    cout<<"sendJoinServer: "<<key<<endl;

    int len = key.length() + 2;    
    vector<uint8> packet( len );
    packet[ 0 ] = 0;
    packet[ 1 ] = JOIN;
    for( int n = 0; n < key.length(); n++ )
        packet[ n + 2 ] = key.at( n );    
    
    connection->SendReliable( packet, ip, port );       
}

void sendJoined( shared_ptr<UserInfo> user ) {
    if( user == NULL ) {
        cout<<"sendJoined: USER NULL"<<endl;
        return;
    }
    cout<<"sendJoined"<<endl;

    vector<uint8> packet( 2 );
    packet[ 0 ] = 0;
    packet[ 1 ] = JOINED;
    
    connection->SendReliable( packet, user->ip.c_str(), user->port );    
}

void dumpBytes( unsigned char *data, int length ) {    
	for( int i = 0; i < length; i++ )
		cout<<static_cast<unsigned>((unsigned char)data[ i ])<<" ";
	cout<<endl;
}

void dumpBytes( vector<uint8> data ) {    
	for( vector<uint8>::iterator it = data.begin(); it != data.end(); ++it )
        cout<<static_cast<unsigned>( (unsigned char) *it )<<" ";
    cout<<endl;
}

void sendSelectShip( shared_ptr<UserInfo> user, int ship ) {
    cout<<"sendSelectShip:"<<ship<<" - "<<user->id<<endl;

    if( user == NULL ) return;

    int length = 6;
    string key;

    if( user->id == 0 ) {
        key = user->ip;
        key.append( ":" );
        key.append( to_string( user->port ) );

        length += key.length();
        cout<<"Key: "<<key<<endl;
    }
    
    vector<uint8> packet( length );
    packet[ 0 ] = 0;
    packet[ 1 ] = SELECT_SHIP;
    packet[ 4 ] = ship & 0xFF;
    packet[ 5 ] = ( ship >> 8 ) & 0xFF;
    addIDToPacket( packet, user->id );

    if( user->id == 0 ) {
        cout<<"KEY = "<<key<<endl;
        cout<<"APPEND KEY:";        
        for( int i = 0; i < key.size(); i++ ) {
            packet[ length - key.size() + i ] = key.at( i );
            cout<<key.at( i );
        }
        cout<<endl;
    }

    connection->SendReliable( packet, user->serverIP.c_str(), user->serverPort );    
}

void sendTurn( shared_ptr<UserInfo> user, uint8 dir ) {
    cout<<"sendTurn:"<<dir<<endl;

    if( user == NULL ) return;

    vector<uint8> packet( 5 );
    packet[ 0 ] = 0;
    packet[ 1 ] = TURN;
    packet[ 4 ] = dir;
    addIDToPacket( packet, user->id );

    //dumpBytes( packet, 5 );

    connection->SendReliable( packet, user->serverIP.c_str(), user->serverPort );    
}

void sendStopTurn( shared_ptr<UserInfo> user, uint8 dir ) {
    cout<<"sendStopTurn:"<<dir<<endl;

    if( user == NULL ) return;

    vector<uint8> packet( 5 );
    packet[ 0 ] = 0;
    packet[ 1 ] = STOP_TURN;
    packet[ 4 ] = dir;
    addIDToPacket( packet, user->id );    

    connection->SendReliable( packet, user->serverIP.c_str(), user->serverPort );    
}

void sendAccelerate( shared_ptr<UserInfo> user, uint8 dir ) {
    cout<<"sendAccelerate:"<<dir<<endl;

    if( user == NULL ) return;

    vector<uint8> packet( 5 );
    packet[ 0 ] = 0;
    packet[ 1 ] = ACCELERATE;
    packet[ 4 ] = dir;
    addIDToPacket( packet, user->id );

    connection->SendReliable( packet, user->serverIP.c_str(), user->serverPort );    
}

void sendStopAccelerate( shared_ptr<UserInfo> user, uint8 dir ) {
    cout<<"sendStopAccelerate:"<<dir<<endl;

    if( user == NULL ) return;

    vector<uint8> packet( 5 );
    packet[ 0 ] = 0;
    packet[ 1 ] = STOP_ACCELERATE;
    packet[ 4 ] = dir;
    addIDToPacket( packet, user->id );

    connection->SendReliable( packet, user->serverIP.c_str(), user->serverPort );
}

void sendDecelerate( shared_ptr<UserInfo> user ) {
    cout<<"sendDecelerate"<<endl;

    if( user == NULL ) return;

    vector<uint8> packet( 4 );
    packet[ 0 ] = 0;
    packet[ 1 ] = DECELERATE;    
    addIDToPacket( packet, user->id );

    connection->SendReliable( packet, user->serverIP.c_str(), user->serverPort );
}

void sendStopDecelerate( shared_ptr<UserInfo> user ) {
    cout<<"sendStopDecelerate"<<endl;

    if( user == NULL ) return;

    vector<uint8> packet( 4 );
    packet[ 0 ] = 0;
    packet[ 1 ] = STOP_DECELERATE;    
    addIDToPacket( packet, user->id );

    connection->SendReliable( packet, user->serverIP.c_str(), user->serverPort );
}

void sendFire( shared_ptr<UserInfo> user, uint8 weapon ) {
    if( user == NULL ) return;

    cout<<"sendFire"<<endl;

    vector<uint8> packet( 5 );
    packet[ 0 ] = 0;
    packet[ 1 ] = FIRE;
    packet[ 4 ] = weapon;
    addIDToPacket( packet, user->id );    
    
    connection->SendReliable( packet, user->serverIP.c_str(), user->serverPort );
}

void sendDisconnect( shared_ptr<UserInfo> user ) {
    if( user == NULL ) return;

    cout<<"sendDisconnect"<<endl;

    vector<uint8> packet( 4 );
    packet[ 0 ] = 0;
    packet[ 1 ] = DISCONNECT;
    addIDToPacket( packet, user->id );

    connection->SendReliable( packet, user->serverIP.c_str(), user->serverPort );    
}

void sendUse( string key, uint8 item ) {
    cout<<"sendUse:"<<key<<" - "<<item<<endl;

    vector<uint8> packet( 5 );
    packet[ 0 ] = 0;
    packet[ 1 ] = USE;
    packet[ 4 ] = item;
    addIDToPacket( packet, getID( key ) );

    //dumpBytes( packet, 5 );    
}

void sendReady( string key ) {
    cout<<"sendReady: "<<key<<endl;
    
    vector<uint8> packet( 4 );
    packet[ 0 ] = 0;
    packet[ 1 ] = READY;
    addIDToPacket( packet, getID( key ) );

    //dumpBytes( packet, 4 );

    shared_ptr<UserInfo> u = getUser( key );
    if( u != NULL ) connection->SendReliable( packet, u->ip.c_str(), u->port );    
}

int getID( string key ) {
    shared_ptr<UserInfo> user = usersInfo[ key ];
    if( user == NULL ) return 0;
    return user->id;
}

void addIDToPacket( vector<uint8> &data, int id ) {
    if( data.size() < 4 ) return;

    data[ 2 ] = id & 0xFF;
    data[ 3 ] = ( id >> 8 ) & 0xFF;
}

void cleanUp() {
    cout<<"Cleaning Up..."<<endl;
    
    cout<<"Killing Connection...";
    connection->Kill();
    delete connection;
    cout<<"Done!"<<endl;

    cout<<"Killing threads..."<<endl;
    exiting = true;
    if( reportThread->joinable() ) reportThread->join();
    delete reportThread;

    if( requestThread->joinable() ) requestThread->join();
    delete requestThread;
    cout<<"Done!"<<endl;

    cout<<"Done!"<<endl;
}

void sig_term_handler(int signum, siginfo_t *info, void *ptr){
    if( signum == 2 ) cleanUp();
    exit( 0 );
}

void initializeKillHandler() {
    std::cout<<"Setup Kill Handler"<<std::endl;

    static struct sigaction _sigact;

    memset(&_sigact, 0, sizeof(_sigact));
    _sigact.sa_sigaction = sig_term_handler;
    _sigact.sa_flags = SA_SIGINFO;

    sigaction( SIGTERM, &_sigact, NULL );
    sigaction( SIGINT, &_sigact, NULL ); // Handles ctrl-c
}