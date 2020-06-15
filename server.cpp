#include "server.h"
#include <iostream>
#include <string.h>

Server::Server() {
    this->_debug = true;
    this->_type = "Server";
    
    this->debug( "Created" );    
}

void Server::initRedis() {
    this->debug( "initRedis" );
}

void Server::debug( std::string msg ) {
    if( this->_debug )
        std::cout<<this->_type<<": "<<msg<<std::endl;
}

// Server side implementation of UDP client-server model 
/*#include <stdio.h> 
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

#include "ship.h"
#include "packet.h"
#include "server.h"
#include "connection.h"

const uint32 TICKS_PER_SECOND = 60;
const float32 SECONDS_PER_TICK = 1.0f / float32( TICKS_PER_SECOND );
const uint64 MS_LOOP_TIME = SECONDS_PER_TICK * 1000;

const uint16 PORT = 8881;

int counter = 0;
long sleepTimes = 0;

cpp_redis::subscriber sub;

void checker() {
    std::cout<<"Starting up Checker"<<std::endl;

    struct timespec start, end;
    uint64_t diff;
    float32 usage;

    while( true ) {
        clock_gettime( CLOCK_REALTIME, &start );
        usleep( 1000000 ); // Waits for 1 second
        clock_gettime( CLOCK_REALTIME, &end );

        diff = BILLION * ( end.tv_sec - start.tv_sec ) + end.tv_nsec - start.tv_nsec;
        printf( "Frames: %d   ", counter );

        usage = sleepTimes * 100.0 / ( counter * 16666 );
        printf( "Usage: %f\%\n", 100 - usage );

        counter = 0;
        sleepTimes = 0;
    }
}

void dumpBytes( unsigned char *data, int length ) {
    std::cout<<"Bytes: ";
    for( int i = 0; i < length; i++ ) {
        std::cout<<data[ i ]<<" ";
    }
    std::cout<<std::endl;
}

unsigned char* buildState( std::vector<POSITION> positions, int& length ) {
    int rows = 9;
    length = 1 + ( positions.size() * rows );
    unsigned char *buffer = new unsigned char[ length ];
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
    std::cout<<"Initializing Redis"<<std::endl;    

    std::cout<<"Trying to connect...";
    sub.connect( "game.fbfafa.0001.usw1.cache.amazonaws.com", 6379, [](const std::string& host, std::size_t port, cpp_redis::subscriber::connect_state status) {
        if (status == cpp_redis::subscriber::connect_state::dropped) {
            std::cout<<"client disconnected from "<<host<<":"<<port<<std::endl;
            //should_exit.notify_all();
        }
    } );
    std::cout<<"Success!"<<std::endl;

    sub.subscribe( "space_quest", [](const std::string& chan, const std::string& msg) {
        std::cout<<"MESSAGE "<<chan<<": "<<msg<<std::endl;
    } );    
    std::cout<<"After Subscribing"<<std::endl;

    sub.commit();
}

// Driver code 
int main() {
    int sockfd; 
    char buffer[MAXLINE];
    unsigned int len;    
    int n;
    struct sockaddr_in servaddr, cliaddr;
    std::vector<Connection *> connections;
    struct timespec start, end;
    uint64_t diff;
    uint64 sleepTime;
    struct timespec ts = { 0, 100000000L };
    int stateSize;

    int delay = 0;

    bool _debug = false;

    initRedis();

    // Clear out the structs
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        std::cout<<"Socket Creation Failed!"<<std::endl;
        exit(EXIT_FAILURE); 
    }
      
    // Filling server information 
    servaddr.sin_family = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY; // From anyone
    servaddr.sin_port = htons(PORT); // On port
      
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ) { 
        std::cout<<"Binding Failed"<<std::endl;
        exit(EXIT_FAILURE); 
    } 
    
    // We're all setup and good to go!
    std::cout<<"Server Started"<<std::endl;

    // Fire up our thread to DC people
    std::thread first( checker );

    len = sizeof( cliaddr );

    unsigned char *state;    
    int i = 1;    
    int key;
    int index;

    std::vector<POSITION> positions;
    POSITION current;

    sleepTimes = 0;

    // Loop Forever!
    while( true ) {
        counter++;
        // Grab out time at the start
        clock_gettime( CLOCK_REALTIME, &start );
        //usleep( 10000000 ); // Waits for 10 seconds        
        //usleep( 1800 );
        //clock_gettime( CLOCK_REALTIME, &end );

        n = recvfrom(sockfd, buffer, MAXLINE, MSG_DONTWAIT, ( struct sockaddr *) &cliaddr, &len);
        while( n != -1 ) {
            if( _debug ) {
                std::cout<<"Server("<<n<<"): ";
                for( int l = 0; l < n; l++ )
                    std::cout<<static_cast<const unsigned char&>(buffer[ l ])<<" ";
                std::cout<<std::endl;
            }

            if( n >= 3 ) {
                key = ( buffer[ 1 ] << 8 ) + buffer[ 2 ];
                if( _debug ) std::cout<<"WE HAZ KEY: "<<key<<std::endl;
            }

            switch( buffer[ 0 ] ) {
                case INITIALIZE: {
                    if( _debug ) {
                        std::cout<<"Process Initialize"<<std::endl;
                        std::cout<<"IP: "<<cliaddr.sin_addr.s_addr<<std::endl;
                        std::cout<<"Port: "<<cliaddr.sin_port<<std::endl;
                    }

                    connections.push_back( new Connection( sockfd, cliaddr ) );

                    index = connections.size() - 1;
                    connections[ index ]->Dump();
                    connections[ index ]->SendInitialized( index + 1 );
                }
                break;
                case DISCONNECT: {
                    if( _debug ) std::cout<<"DISCONNECT: "<<key<<std::endl;

                    if( key <= connections.size() ) {
                        connections[ key - 1 ]->Process( cliaddr, buffer );
                    }
                }
                break;
                case STATE: {
                    
                    if( _debug ) {
                        std::cout<<"Key: "<<key<<std::endl;
                        std::cout<<"Size: "<<connections.size()<<std::endl;
                    }

                    /*if( key <= connections.size() ) {
                        std::cout<<"IP: "<<cliaddr.sin_addr.s_addr<<std::endl;
                        std::cout<<"Port: "<<cliaddr.sin_port<<std::endl;                        
                        connections[ key - 1 ]->Process( cliaddr, buffer );
                    }
                }
                break;
                case ACCELERATE: 
                case TURN: {
                    if( key != 0 && key <= connections.size() ) {
                        connections[ key - 1 ]->Process( cliaddr, buffer );
                        if( _debug) std::cout<<"DID WE PROCESS"<<std::endl;
                    }
                }                
                break;
                default: if( _debug ) printf( "Process Unknown: %d", buffer[ 0 ] ); break;
            } 

            //printf( "\n" );

            n = recvfrom(sockfd, buffer, MAXLINE, MSG_DONTWAIT, ( struct sockaddr *) &cliaddr, &len);
        }

        // Clear out the current positions
        positions.clear();

        // Grab all the current positions        
        for( int i = 0; i < connections.size(); i++ ) {
            if( !connections[ i ]->isConnected() ) continue;

            current.type = connections[ i ]->type;
            current.x = connections[ i ]->x;
            current.y = 0;
            current.angle = connections[ i ]->angle;
            current.id = i + 1;

            //printf( "X: %d\n", current.x );

            if( _debug ) printf( "TYPE: %d\n", connections[ i ]->type );
            positions.push_back( current );
        }

        // Build up the current State buffer        
        state = buildState( positions, stateSize );

        // Send the state to all the clients
        for( int i = 0; i < connections.size(); i++ ) {
            if( connections[ i ] != NULL ) {
                connections[ i ]->SendState( state, stateSize );
            }
        }

        // Grab our time at the end, then figure out how long we need to sleep to line up with the tick rate
        clock_gettime( CLOCK_REALTIME, &end );
        diff = BILLION * ( end.tv_sec - start.tv_sec ) + end.tv_nsec - start.tv_nsec;

        //printf( "%llu\n", (long long unsigned int)( (diff / 1000.0) / 16000 ) );

        // Line sleep up with 1s
        //usleep( 1000000 - ( diff / 1000 ) );

        //printf( "SLEEP FOR: %lu\n", ( 16666 - ( diff / 1000 ) ) );
        if( ( diff / 1000 ) < 16666 ) {
            // Line sleep up with 16.66 MS
            delay = 16666 - ( diff / 1000 );
            usleep( delay );
            sleepTimes += delay;
        }
        
        clock_gettime( CLOCK_REALTIME, &end );
        diff = BILLION * ( end.tv_sec - start.tv_sec ) + end.tv_nsec - start.tv_nsec;
        //printf( "TICK - %llu nanoseconds\n", (long long unsigned int) diff );

        //sendto(sockfd, (const char *)hello, strlen(hello),MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len); 
        //if( i == 1 ) printf( "No Packets\n" );

    	/*sendto(sockfd, (const char *)hello, strlen(hello),MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len); 
    	printf("Hello message sent.\n"); 
	    //printf( "IP: %s:%d\n", inet_ntoa( cliaddr.sin_addr ), ntohs( cliaddr.sin_port ) ); 
    }

    return 0; 
} */
