// Server side implementation of UDP client-server model 
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <thread>
#include <signal.h>
#include <vector>
#include <cpp_redis/cpp_redis>
#include <tacopie/tacopie>

#include "entity.h"
#include "ship.h"
#include "bullet.h"
#include "factory-entities.h"
#include "packet.h"
#include "server.h"

#include "socket.h"

typedef struct {
    int id;
    string ip;
    int port;
} UserInfo;

cpp_redis::subscriber sub;
cpp_redis::client redis;

bool initialized = false;
bool debug = false;
bool exiting = false;

using namespace std;

int connections = 0;
struct timespec startTime, endTime;
struct sockaddr_in cliaddr;
uint64 sleepTimes = 0;
int counter = 0;
vector<uint8> buffer;

Socket *connection;

vector<shared_ptr<Entity>> entities;
vector<shared_ptr<Bullet>> bullets;
vector<shared_ptr<Ship>> ships;
queue<int> available;
unordered_map<string,UserInfo> users;

FactoryEntities *factoryEntities;

thread *reportThread;

void reporterDaemon();
vector<uint8> buildState( vector<POSITION>, int& );
void initRedis();
void initializeSockets( int );
void initialize( int );
void sendNotifyActive();
void sendState( int, unsigned char );
void sendID( int, vector<uint8>, sockaddr_in );
void sendJoined( vector<uint8>, sockaddr_in );
void updateUniverse( bool, int );
void checkCollisions();
void buildState();
void commitState( string );
void dumpBytes( vector<uint8>, int );
void cleanUp();
void sig_term_handler( int, siginfo_t*, void* );
void initializeKillHandler();
void spawnShips();
shared_ptr<Entity> getEntity( int );

void handleFire( shared_ptr<Ship> );

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

    // Fire up reporter thread
    reportThread = new thread( reporterDaemon );

    int delay;
    uint64_t diff;
    int MAXLINE = 1024;
    //unsigned char buffer[ MAXLINE ];

    buffer = vector<uint8>( MAXLINE );

    int n;

    factoryEntities = new FactoryEntities();

    spawnShips();

    /*ss = static_pointer_cast<Ship>( factoryEntities->Create( EntityType::SHIP, 2 ) );
    ss->x = 2;
    ss->y = 1;
    ss->id = 2;
    ss->Turn( 2 );
    entities.push_back( ss );

    ss = static_pointer_cast<Ship>( factoryEntities->Create( EntityType::SHIP, 1 ) );
    ss->x = 3;
    ss->y = 2;
    ss->id = 3;
    ss->Turn( 2 );
    entities.push_back( ss );
    ss = NULL;*/

    long frame = 0;
    int id;

    initialized = true;

    shared_ptr<Ship> ship;

    bool fired = false;
    int c = 0;
    while( !exiting ) {
        frame++;
        clock_gettime( CLOCK_REALTIME, &startTime );

        /*auto ship = entities[ 0 ];
        if( ship != NULL ) cout<<"References("<<ship->id<<"): "<<ship.use_count()<<endl;
        ship = entities[ 1 ];
        if( ship != NULL ) cout<<"References1("<<ship->id<<"): "<<ship.use_count()<<endl;
        ship = entities[ 2 ];
        if( ship != NULL ) cout<<"References2("<<ship->id<<"): "<<ship.use_count()<<endl;*/

        n = connection->getPacket( buffer, cliaddr );
        while( n != -1 ) {
            cout<<"GOT A PACKET"<<endl;

            cout<<"Server("<<n<<"): ";
            for( int l = 0; l < n; l++ )
                cout<<static_cast<unsigned>((unsigned char)buffer[ l ])<<" | ";
            cout<<endl;

            if( n >= 4 ) id = buffer[ CLIENT_ID_1 ] + ( buffer[ CLIENT_ID_2 ] << 8 );
            ship = dynamic_pointer_cast<Ship>( getEntity( id ) );            
    
            switch( buffer[ COMMAND ] ) {
                case 1: initialized = true; break;
                case JOIN: {
                    cout<<"SOMEONE WANTS TO JOIN"<<endl;
                    
                    UserInfo *user = new UserInfo();

                    // Grab the address                    
                    vector<uint8> addr( n - 2 );
                    cout<<"LEngth: "<<( n - 2 )<<endl;
                    memcpy( &addr[ 0 ], &buffer[ 2 ], n - 2 );                    

                    // Grab the ship
                    //int ship = buffer[ 1 ] + ( buffer[ 2 ] << 8 );

                    string key( addr.begin(), addr.end() );
                    cout<<"ADDRESS: "<<key<<endl;

                    // Create a new ship and grab the spot
                    //entities.push_back( new Ship( ship ) );
                    //n = entities.size();
                    
                    user->id = -1;
                    users[ key ] = *user;
                    delete user;

                    cout<<"KEY: "<<key<<endl;

                    // Return the key to the user
                    sendJoined( addr, cliaddr );
                } break;
                case TURN: {                    
                    cout<<"RECEIVED TURN: "<<id<<endl;                    
                    if( ship != NULL ) ship->Turn( buffer[ 4 ] );
                } break;
                case STOP_TURN: {
                    cout<<"RECEIVED STOP TURN: "<<id<<endl;                    
                    if( ship != NULL ) ship->StopTurn( buffer[ 4 ] );                    
                } break;
                case ACCELERATE: {
                    cout<<"RECEIVED ACCELERATE"<<endl;                    
                    if( ship != NULL ) ship->Accelerate( buffer[ 4 ] );
                } break;
                case STOP_ACCELERATE: {
                    cout<<"RECEIVED STOP ACCELERATE: "<<id<<endl;                                        
                    if( ship != NULL ) ship->StopAccelerate( buffer[ 4 ] );                    
                } break;
                case DECELERATE: {
                    cout<<"RECEIVED DECELERATE: "<<id<<endl;
                    if( ship != NULL ) ship->Decelerate();
                } break;
                case STOP_DECELERATE: {
                    cout<<"RECEIVED STOP_DECELERATE: "<<id<<endl;
                    if( ship != NULL ) ship->StopDecelerate();
                } break;
                case USE: {
                    cout<<"RECEIVED USE"<<endl;
                } break;
                case FIRE: handleFire( ship ); break;
                case SELECT_SHIP: {
                    int ship = buffer[ 4 ] + ( buffer[ 5 ] << 8 );
                    cout<<"RECEIVED SELECT_SHIP: "<<ship<<endl;
                    
                    cout<<"FOR ID: "<<id<<endl;

                    auto s = static_pointer_cast<Ship>( factoryEntities->Create( EntityType::SHIP, ship ) );
                    if( s == NULL ) cout<<"Error Selecting Ship"<<endl;
                    else {
                        s->x = 99;
                        s->y = 99;
                        if( id == 0 ) {
                            // Grab the address                            
                            vector<uint8> addr( n - 6 );
                            memcpy( &addr[ 0 ], &buffer[ 6 ], n - 6 );                            

                            cout<<"ADDRESS: "<<string( addr.begin(), addr.end() )<<endl;

                            // See if we have any open spots in our vector
                            if( !available.empty() ) {
                                // We have unused indexes, pull off one
                                n = available.front();
                                available.pop();

                                cout<<"TAKING OVER SPOT: "<<n<<endl;
                                cout<<"SIZE: "<<entities.size()<<endl;

                                if( n < 0 ) {
                                    entities.push_back( s );
                                    n = entities.size();
                                } else {
                                    entities[ n ] = s;
                                    n++;
                                }
                            } else {
                                // We have no unused indexes, tack it onto the back
                                entities.push_back( s );
                                ships.push_back( s );
                                n = entities.size();
                            }

                            if( s == NULL ) cout<<"SHIP IS NULL WTF"<<endl;
                            s->id = n;

                            // Return the key to the user
                            sendID( n, addr, cliaddr );
                        } else {

                        }
                    }

                    updateUniverse( true, frame );
                } break;
                case DISCONNECT: {
                    int id = buffer[ CLIENT_ID_1 ] + ( buffer[ CLIENT_ID_2 ] << 8 );
                    cout<<"RECEIVED DISCONNECT: "<<id<<endl;

                    available.push( id - 1 );
                } break;
            }

            n = connection->getPacket( buffer, cliaddr );
        }

        updateUniverse( false, frame );
        checkCollisions();
        buildState();

        clock_gettime( CLOCK_REALTIME, &endTime );
        diff = BILLION * ( endTime.tv_sec - startTime.tv_sec ) + endTime.tv_nsec - startTime.tv_nsec;

        if( ( diff / 1000 ) < 16666 ) {
            delay = 16666 - ( diff / 1000 );
            usleep( delay );
            sleepTimes += delay;
        }

        counter++;
        c++;

        if( !fired && c > 60 ) {
            fired = true;

            cout<<"Trying to fire"<<endl;
            buffer.resize( 5 );
            buffer[ 4 ] = 1;
            auto ship1 = dynamic_pointer_cast<Ship>( getEntity( 1 ) ); 
            if( ship1 != NULL ) handleFire( ship1 );
        }
    }

    return 0; 
}

void handleFire( shared_ptr<Ship> ship ) {
    cout<<"handleFire"<<endl;

    if( ship != NULL ) {
        auto result = ship->Fire( buffer[ 4 ] );
        if( result != -1 ) {
            auto bullet = dynamic_pointer_cast<Bullet>( factoryEntities->Create( EntityType::BULLET, result ) );
            if( bullet != NULL ) {
                cout<<"Bullet Created!"<<endl;

                auto vx = sin( ship->angle * PI / 180 ) * bullet->speed;
                auto vy = cos( ship->angle * PI / 180 ) * bullet->speed;

                bullet->x = ship->x;
                bullet->y = ship->y;
                bullet->vx = ship->vx + vx;
                bullet->vy = ship->vy + vy;
                bullet->angle = ship->angle;

                //cout<<"VX: "<<bullet->vx<<"  Ship: "<<ship->vx<<"  vx:"<<vx<<endl;

                entities.push_back( bullet );
                bullet->id = entities.size();

                bullet->Dump();

                bullets.push_back( bullet );

                auto message = "FIRED|" + to_string( bullet->id ) + "|" + to_string( static_cast<int>( bullet->x * 100 ) ) + "|" + to_string( static_cast<int>( bullet->y * 100 ) );
                cout<<"Send: "<<message<<endl;
                redis.publish( "user_event", message, []( cpp_redis::reply& reply ) {
                    cout<<"Sent User Event"<<endl;
                } );
                redis.sync_commit();
            } else cout<<"Bullet Not Created!"<<endl;
        }
    }
}

void reporterDaemon() {
    cout<<"Starting up Reporter"<<endl;

    struct timespec startTime, endTime;
    uint64_t diff;
    float32 usage;    

    while( !exiting ) {        
        usleep( 1000000 ); // Waits for 1 second

        if( debug ) cout<<"TICK: "<<counter<<"   Entities: "<<entities.size()<<endl;
        if( initialized ) {
            diff = BILLION * ( endTime.tv_sec - startTime.tv_sec ) + endTime.tv_nsec - startTime.tv_nsec;
            usage = sleepTimes * 100.0 / ( counter * 16666 );

            if( debug || true ) {
                printf( "Usage: %d\%\n", (int)( 100 - usage ) );
            }
            
            if( !exiting ) sendState( connections, 100 - usage );
        }

        counter = 0;
        sleepTimes = 0;
    }
}

void commitState( string state ) {
    string key( "universe" );

    struct timespec startTime, endTime;
    if( debug ) clock_gettime( CLOCK_REALTIME, &startTime );
    redis.set( key.c_str(), state );
    redis.sync_commit();
    //redis.commit();
    if( debug ) {
        clock_gettime( CLOCK_REALTIME, &endTime );
        auto diff = BILLION * ( endTime.tv_sec - startTime.tv_sec ) + endTime.tv_nsec - startTime.tv_nsec;
        diff /= 1000;
        diff /= 1000;

        cout<<"CommitState Time: "<<diff<<endl;
    }
}

void buildState() {
    int length = entities.size();
    if( length == 0 ) {
        commitState( "" );        
        return;
    }

    int chunkSize = 11;
    //unsigned char *buffer = createBuffer( length * chunkSize );
    vector<uint8> buffer( length * chunkSize );
    int type, x, y, angle, id;
    uint8 status;
    for( int i = 0; i < length; i++ ) {
        //cout<<"PROCESS ENTITY: "<<i<<endl;

        //entities[ i ]->Dump();

        type = entities[ i ]->type;
        x = static_cast<int>( entities[ i ]->x * 100 );
        y = static_cast<int>( entities[ i ]->y * 100 );
        angle = static_cast<int>( entities[ i ]->angle * 100 );
        id = entities[ i ]->id;
        status = entities[ i ]->status;

        //cout<<"PROCESSING: "<<type<<" : "<<x<<","<<y<<" : "<<angle<<endl;

        //======================================//
        // Packet                               //
        // 1-2   ID                             //
        // 3     Status                         //
        // 4-5   Type                           //
        // 6-7   X                              //
        // 8-9   Y                              //
        // 10-11 Angle                          //
        //======================================//
        buffer[ ( i * chunkSize ) + 0 ] = id & 0xFF;
        buffer[ ( i * chunkSize ) + 1 ] = ( id >> 8 ) & 0xFF;
        buffer[ ( i * chunkSize ) + 2 ] = status & 0xFF;
        buffer[ ( i * chunkSize ) + 3 ] = type & 0xFF;
        buffer[ ( i * chunkSize ) + 4 ] = ( type >> 8 ) & 0xFF;
        buffer[ ( i * chunkSize ) + 5 ] = x & 0xFF;
        buffer[ ( i * chunkSize ) + 6 ] = ( x >> 8 ) & 0xFF;
        buffer[ ( i * chunkSize ) + 7 ] = y & 0xFF;
        buffer[ ( i * chunkSize ) + 8 ] = ( y >> 8 ) & 0xFF;
        buffer[ ( i * chunkSize ) + 9 ] = angle & 0xFF;
        buffer[ ( i * chunkSize ) + 10 ] = ( angle >> 8 ) & 0xFF;                
    }

    // Build up the state string
    string buff( "" );
    for( int i = 0; i < length * chunkSize; i++ )
        buff += buffer[ i ];

    //dumpBytes( buffer, length * chunkSize );

    // Throw it into redis
    commitState( buff );
}

void updateUniverse( bool debug, int frame ) {
    int length = entities.size();
    if( length == 0 ) return;

    if( debug ) cout<<"updateUniverse"<<endl;
    
    shared_ptr<Ship> ship;
    shared_ptr<Bullet> bullet;
    for( int i = 0; i < length; i++ ) {
        if( debug ) cout<<"PROCESS ENTITY: "<<i<<endl;
        
        ship = dynamic_pointer_cast<Ship>( entities[ i ] );
        if( ship != NULL ) {            
            ship->UpdateSpeed();
            ship->angle += ship->va;

            //if( ship->va != 0 )
            //    cout<<"Angle: "<<ship->angle<<endl;
        }

        bullet = dynamic_pointer_cast<Bullet>( entities[ i ] );
        if( bullet != NULL ) {
            //cout<<"UPDATE BULLET"<<endl;
            //bullet->Dump();
        }

        /*bullet = dynamic_cast<shared_ptr<Bullet>>( entities[ i ] );
        if( bullet != NULL ) cout<<"UPDATE BULLET"<<endl;*/

        entities[ i ]->x += entities[ i ]->vx;
        entities[ i ]->y += entities[ i ]->vy;        

        if( entities[ i ]->x < 0 ) entities[ i ]->x = 0;
        if( entities[ i ]->y < 0 ) entities[ i ]->y = 0;

        if( entities[ i ]->angle >= 360 ) entities[ i ]->angle -= 360;
        if( entities[ i ]->angle < 0 ) entities[ i ]->angle += 360;

        //if( entities[ i ]->angle == 0 && i == 0 ) entities[ i ]->Dump();
    }
}

void checkCollisions() {
    shared_ptr<Entity> entity;
    auto length = entities.size();
    for( auto i = 0; i < length; i++ ) {
        entity = entities[ i ];

        for( auto n = i + 1; n < length; n++ ) {
            entities[ i ]->CheckCollide( entities[ n ].get() );
        }
    }
}

vector<uint8> buildState( vector<POSITION> positions, int& length ) {
    int rows = 9;
    length = 1 + ( positions.size() * rows );
    //unsigned char *buffer = createBuffer( length );
    vector<uint8> buffer( length );

    buffer[ 0 ] = STATE;    
    for( int i = 0; i < positions.size(); i++ ) {
        //======================================//
        // Packet                               //
        // 1-2 ID                               //
        // 3 Type                               //
        // 4-5 X                                //
        // 5-6 Y                                //
        // 7-8 Angle                            //
        //======================================//
        buffer[ 1 + ( i * rows ) ] = (unsigned char)((positions[ i ].id >> 8) & 0xFF);
        buffer[ 2 + ( i * rows ) ] = (unsigned char)(positions[ i ].id & 0xFF);
        buffer[ 3 + ( i * rows ) ] = positions[ i ].type;
        buffer[ 4 + ( i * rows ) ] = (unsigned char)((positions[ i ].x >> 8) & 0xFF);
        buffer[ 5 + ( i * rows ) ] = (unsigned char)(positions[ i ].x & 0xFF);
        buffer[ 6 + ( i * rows ) ] = (unsigned char)((positions[ i ].y >> 8) & 0xFF);
        buffer[ 7 + ( i * rows ) ] = (unsigned char)(positions[ i ].y & 0xFF);
        buffer[ 8 + ( i * rows ) ] = (unsigned char)((positions[ i ].angle >> 8) & 0xFF);
        buffer[ 9 + ( i * rows ) ] = (unsigned char)(positions[ i ].angle & 0xFF);
    }

    return buffer;
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

    sub.subscribe( "game_server", [](const string& chan, const string& msg) {
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

    cout<<"Sending Message..."<<endl;
    /*redis.publish( "space_quest", "INITIALIZE|GAME|" + guid , [](cpp_redis::reply& reply) {
        cout<<"Sent!"<<endl;
    });*/
    redis.commit();
}

void initializeSockets( int port ) {
    // Clear out the struct
    memset( &cliaddr, 0, sizeof( cliaddr ) );

    connection = new Socket();
    connection->setPort( port );    
    connection->Bind();
}

void initialize( int port ) {
    initializeSockets( port );
    initRedis();
    initializeKillHandler();
}

void sendNotifyActive() {
    if( debug ) cout<<"sendNotifyActive"<<endl;
    
    vector<uint8> packet( 3 );
    packet[ 0 ] = 0;
    packet[ 1 ] = INITIALIZE;
    packet[ 2 ] = GAME;    

    connection->Send( packet, masterAddress, masterPort );
}

void sendState( int connections, unsigned char usage ) {
    if( debug ) cout<<"sendState"<<endl;
    
    vector<uint8> packet( 4 );
    packet[ 0 ] = 0;
    packet[ 1 ] = STATE;
    packet[ 2 ] = connections;
    packet[ 3 ] = usage;    

    connection->Send( packet, masterAddress, masterPort );    
}

void sendID( int id, vector<uint8> user, sockaddr_in address ) {    
    char ip[ INET_ADDRSTRLEN ];
    inet_ntop( AF_INET, &( address.sin_addr ), ip, INET_ADDRSTRLEN );
    int port = ntohs( address.sin_port );

    string userString( user.begin(), user.end() );
    cout<<"sendID: "<<id<<" for "<<userString<<" to "<<ip<<":"<<port<<endl;  
    
    int n = user.size();
    cout<<"LENGTH: "<<n<<endl;    
    vector<uint8> packet( 4 + n );
    packet[ 0 ] = 0;
    packet[ 1 ] = ID;
    packet[ 2 ] = id & 0xFF;
    packet[ 3 ] = (uint8)(( id >> 8 ) & 0xFF );
    for( int i = 0; i < n; i++ ) {
        packet[ 4 + i ] = user[ i ];
    }    
    
    connection->Send( packet, ip, port );    
}

void sendJoined( vector<uint8> user, sockaddr_in address ) {
    char ip[ INET_ADDRSTRLEN ];
    inet_ntop( AF_INET, &( address.sin_addr ), ip, INET_ADDRSTRLEN );
    int port = ntohs( address.sin_port );

    int n = user.size();
    //uint8 *packet = createBuffer( 2 + n );
    vector<uint8> packet( 2 + n );
    packet[ 0 ] = 0;
    packet[ 1 ] = JOINED;
    memcpy( &packet[ 2 ], &user[ 0 ], user.size() );

    cout<<"SENDING JOINED"<<endl;    
    connection->Send( packet, ip, port );    
}

shared_ptr<Entity> getEntity( int id ) {
    //cout<<"getEntity: "<<id<<endl;
    
    id -= 1;
    if( id < entities.size() && entities[ id ] != NULL )
        return entities[ id ];

    cout<<"ID: "<<id<<endl;
    cout<<"SIZE: "<<entities.size()<<endl;
    cout<<"WHY ARE WE HERE"<<endl;
    return NULL;
}

void spawnShips() {
    cout<<"Spawn Ships"<<endl;

    auto cols = 25;
    auto rows = 25;
    auto gap = 2.25f;
    auto base = 100;

    for( int x = 0; x < cols; x++ ) {
        for( int y = 0; y < rows; y++ ) {
            auto ss = factoryEntities->Create( EntityType::SHIP, y % 13 );
            ss->x = base + ( x * gap );
            ss->y = base + ( y * gap );
            ss->id = ( x * cols ) + y; 
            dynamic_pointer_cast<Ship>( ss )->Turn( ss->id % 2 + 1 );
            entities.push_back( ss );
            ss = NULL;
        }
    }

    cout<<"Ships have been spawned"<<endl;
}

void dumpBytes( vector<uint8> data, int length ) {    
	for( int i = 0; i < length; i++ )
		cout<<static_cast<unsigned>((unsigned char)data[ i ])<<" ";
	cout<<endl;
}

void cleanUp() {
    cout<<"Cleaning Up..."<<endl;
    
    // Set our exiting flag for the threads
    exiting = true;

    //Kill off the connection and release it
    cout<<"Killing Connection...";
    connection->Kill();
    delete connection;
    cout<<"Done!"<<endl;

    cout<<"Killing threads..."<<endl;
    if( reportThread->joinable() ) reportThread->join();
    delete reportThread;
    cout<<"Done!"<<endl;

    cout<<"Clearing Factories...";
    delete factoryEntities;
    cout<<"Done!"<<endl;

    cout<<"Clearing Entities...";
    entities.clear();
    cout<<"Done!"<<endl;

    cout<<"Done Cleaning!"<<endl;
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