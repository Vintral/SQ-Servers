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
#include <signal.h>
#include <thread>
#include <unordered_map>
#include <vector>
#include <cpp_redis/cpp_redis>
#include <tacopie/tacopie>

#include "socket.h"
#include "packet.h"
#include "server.h"

using namespace std;

cpp_redis::subscriber sub;
cpp_redis::client redis;

int connections = 0;
struct timespec start;
struct timespec endTime;
struct sockaddr_in servaddr, cliaddr;
int sockfd;
uint64 sleepTimes = 0;
int counter = 0;

bool exiting = false;
bool initialized = false;
Socket *connection;

typedef struct {
    sockaddr_in client;
    int port;
    timespec time;
    int test;
} ServerRequest;

queue<ServerRequest> requests;

void reporterDaemon();
void sendRequestServer();
void initRedis();
void initialize();
void sendNotifyActive();
void cleanUp();
void sig_term_handler( int, siginfo_t*, void* );
void initializeKillHandler();

ServerRequest request;

thread *reportThread;
vector<uint8> buffer;

using namespace std;

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
    initialize();

    connection = new Socket();
    connection->setPort( port );    
    connection->Bind();

    sendNotifyActive();

    // Fire up thread to communicate with the master
    reportThread =  new thread( reporterDaemon );

    int delay;
    uint64_t diff;
    //unsigned char buffer[1024];
    int n;
    unsigned char *addr;    
    char ip[ INET_ADDRSTRLEN ];

    buffer = vector<uint8>( 1024 );

    while( !exiting ) {
        clock_gettime( CLOCK_REALTIME, &start );
        
        n = connection->getPacket( buffer, cliaddr );        
        while( n != -1 ) {
            int port = ntohs( cliaddr.sin_port );
            cout<<"ADDRESS:"<<ip<<":"<<port<<endl;

            cout<<"GOT A PACKET"<<endl;

            cout<<"Server("<<n<<"): ";
            for( int l = 0; l < n; l++ )
                cout<<static_cast<int>(buffer[ l ])<<" | ";
            cout<<endl;

            /*if( buffer[ 0 ] > 0 ) {
                uint8 *packet = new uint8[ 3 ];
                packet[ 1 ] = ACK;
                packet[ 2 ] = buffer[ 0 ];
                connection->SendReliable( packet, 3, ip, port );
                cout<<"SENT ACK PACKET"<<endl;
            }*/
                        
            switch( buffer[ 1 ] ) {
                case INITIALIZE:                
                    cout<<"INITIALIZED"<<endl;
                    cout<<"WE NEED AN ADDRESS"<<endl;

                    // Setup our client info
                    clock_gettime( CLOCK_REALTIME, &request.time );
                    request.client = cliaddr;
                    request.port = ntohs( cliaddr.sin_port );

                    inet_ntop( AF_INET, &( cliaddr.sin_addr ), ip, INET_ADDRSTRLEN );
                    cout<<"REQUEST FROM "<<ip<<":"<<ntohs(cliaddr.sin_port)<<endl;

                    initialized = true;

                    // Store it in our queue to be processed
                    requests.push( request );

                    // Dispatch the request
                    sendRequestServer();
                    break;
                case ADDRESS:
                    cout<<"WE HAVE AN ADDRESS"<<endl;
                    
                    // Strip out the address from the packet header
                    /*addr = new unsigned char[ n - 1 ];
                    for( int i = 0; i < n - 1; i++ ) {
                        addr[ i ] = buffer[ i + 1 ];
                    }

                    cout<<"ADDRESS:"<<addr<<endl;*/

                    // Make sure we have something pending, if so pull out the first one
                    if( !requests.empty() ) {
                        cout<<"SEND ADDRESS"<<endl;
                        request = requests.front();
                        requests.pop();
                                                
                        inet_ntop( AF_INET, &( request.client.sin_addr ), ip, INET_ADDRSTRLEN );
                        cout<<"SEND ADDRESS TO "<<ip<<":"<<request.port<<endl;
                        connection->setDebug( true );
                        connection->Send( buffer, ip, request.port );
                    }
                    break;
                default: cout<<"UNHANDLED: "<<buffer[ 0 ]<<endl;
            }

            n = connection->getPacket( buffer, cliaddr );
        }

        clock_gettime( CLOCK_REALTIME, &endTime );
        diff = BILLION * ( endTime.tv_sec - start.tv_sec ) + endTime.tv_nsec - start.tv_nsec;

        usleep( 1000000 );
        /*if( ( diff / 1000 ) < 16666 ) {
            delay = 16666 - ( diff / 1000 );
            usleep( delay );
            sleepTimes += delay;
        }*/

        counter++;
    }

    return 0; 
} 

void reporterDaemon() {
    cout<<"Starting up Reporter"<<endl;

    struct timespec start;
    struct timespec end;
    uint64_t diff;
    float32 usage;
    ServerRequest request;

    while( !exiting ) {
        usleep( 1000000 * 5 ); // Waits for 5 seconds

        if( initialized ) {            
            // Send status to the master server
            uint8 *packet = new uint8[ 2 ];
            packet[ 0 ] = STATE;
            packet[ 1 ] = 5;

            //connection->Send( packet, masterAddress, masterPort );
            delete[] packet;

            // Check for packages still pending
            if( !requests.empty() ) {
                request = requests.front();

                clock_gettime( CLOCK_REALTIME, &end );
                diff = BILLION * ( end.tv_sec - request.time.tv_sec ) + end.tv_nsec - request.time.tv_nsec;

                //cout<<"DIFF: "<<diff<<endl;
            }
        }

        counter = 0;
        sleepTimes = 0;
    }
}

void initialize() {
    initRedis();
    initializeKillHandler();
}

void initRedis() {
    cout<<"Initializing Redis"<<endl;    

    // Connecting our subscriber
    cout<<"Trying to connect...";
    sub.connect( "game.fbfafa.0001.usw1.cache.amazonaws.com", 6379, [](const string& host, size_t port, cpp_redis::subscriber::connect_state status) {
        if (status == cpp_redis::subscriber::connect_state::dropped) {
            cout<<"client disconnected from "<<host<<":"<<port<<endl;            
        }
    } );
    cout<<"Success!"<<endl;

    // Subscribe to our channel
    sub.subscribe( "login_server", [](const string& chan, const string& msg) {
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
}

void sendNotifyActive() {
    cout<<"sendNotifyActive"<<endl;
    
    vector<uint8> packet( 3 );
    packet[ 0 ] = 0;
    packet[ 1 ] = INITIALIZE;
    packet[ 2 ] = LOGIN;

    connection->Send( packet, masterAddress, masterPort );    
}

void sendRequestServer() {
    vector<uint8> packet( 3 );
    packet[ 0 ] = 0;
    packet[ 1 ] = REQUEST_ADDRESS;
    packet[ 2 ] = USER;

    connection->Send( packet, masterAddress, masterPort );    
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