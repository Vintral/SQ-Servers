#include <iostream>
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <signal.h>
#include <netinet/in.h>

#include <thread>

#include "packet.h"
#include "server.h"
#include "socket.h"

#define PORT 8050
#define BUFSIZE 2048

using namespace std;

typedef struct {
	uint8 type;
	uint16 key;
	uint8 data;
} PACKET;

void dumpBytes( string preface, vector<uint8> data, int length ) {
    cout<<preface<<": ";
	for( int i = 0; i < data.size(); i++ )
		cout<<static_cast<unsigned>((unsigned char)data[ i ])<<" ";
	cout<<endl;
}

bool active = false;
bool exiting = false;
int sockfd;
uint8* packet;
int key = 0;
struct sockaddr_in servaddr, cliaddr;

vector<uint8> buffer( 1024 );

Socket *connection;
char* serverIP;
int serverPort = -1;

thread thinkerThread;

unsigned char packetID = 0;

void initialize( int );
void moveThread();
void sendLogin();
void sendJoin();
void sendFire();
void sendAccelerate( char );
void sendTurn( char );
void sendSelectShip( int );
void sendDisconnect();
void sig_term_handler( int, siginfo_t*, void* );

int main( int argc, char *argv[] ) {
	/*uint8 buffer[ BUFSIZE ];
	void *buff;

	if( ( sockfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
		cout<<"Socket Creation Failed"<<endl;
		return -1;
	}

	std::thread first( moveThread );

	memset( &servaddr, 0, sizeof( servaddr ) );

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons( PORT );	

	inet_aton( "172.31.11.230", &servaddr.sin_addr );
	
	int n;
	int i = 1;
	unsigned int len;

	packet = new uint8[ 1 ];
	packet[ 0 ] = 1;
	sendto( sockfd, packet, sizeof( packet ), MSG_CONFIRM, (const struct sockaddr *)&servaddr, sizeof( servaddr ) );
	cout<<"Sent Message"<<endl;

	/*packet = new uint8[255];
	for( i = 0; i < 255; i++ )
		packet[ i ] = i;
	dumpBytes( packet, 255 );

	i = 1;

	while( true ) {
		while( true ) {
			n = recvfrom(sockfd, buffer, BUFSIZE, MSG_DONTWAIT, ( struct sockaddr *) &servaddr, &len);
			if( n == -1 ) break;

			buffer[ n ] = '\0';

			switch( buffer[ 0 ] ) {
				case INITIALIZE: 
					key = ( buffer[ 1 ] << 8 ) + buffer[ 2 ];
					cout<<"KEY: "<<key<<endl;
					break;
				case STATE:
					/*cout<<"Size: "<<sizeof( unsigned char )<<endl;
					cout<<"Server("<<n<<"): ";
					for( int i = 0; i < n; i++ )
						cout<<static_cast<unsigned>((unsigned char)buffer[ i ])<<" ";
					cout<<endl;
					dumpBytes( "Server", buffer, n );
					break;
			}
		}

		usleep( 16666 );
	}

	close( sockfd );*/

	int port;

	// Make sure we have a port to initialize to
    if( argc > 1 ) {        
        port = atoi( argv[ 1 ] );        
    } else {
        cout<<"ERROR: NO PORT"<<endl;
        return -1;
    }
    initialize( port );

	int n;
	int frames = 0;
	
	sendLogin();

	while( !exiting ) {
		 // Grab a packet
        n = connection->getPacket( buffer, cliaddr );
        while( n != -1 ) {
            /*cout<<"GOT A PACKET"<<endl;

            cout<<"Server("<<n<<"): ";
            for( int l = 0; l < n; l++ )
                cout<<static_cast<int>(buffer[ l ])<<" | ";
            cout<<endl;*/

			switch( buffer[ COMMAND ] ) {
				case ADDRESS: {
					//unsigned char *buff = new unsigned char[ n - 2 ];					
					vector<uint8> buff( n - 2 );
					memcpy( &buff[ 0 ], &buffer[ 2 ], n - 2 );

					dumpBytes( "Stuff", buff, n-2 );

					string addr( buff.begin(), buff.end() );
					cout<<"Address: "<<addr<<endl;
					auto index = addr.rfind( ":" ); // Find the rightmost :
					//cout<<"Buffer: "<<buffer<<endl;				
					cout<<"Index: "<<index<<endl;
					cout<<"SUBSTRING: "<<addr.substr( index + 1, n - ( index + 1 ) )<<endl;
					serverPort = stoi( addr.substr( index + 1, n - ( index + 1 ) ) );
					auto temp = addr.substr( 0, index );
					cout<<"TEMP:"<<temp<<endl;
					serverIP = new char[ temp.length() + 1 ];
					strcpy( serverIP, temp.c_str() );
					
					cout<<"ADDRESS:"<<serverIP<<":"<<serverPort<<endl;

					sendJoin();
				} break;
				case JOINED: {
					cout<<"WE HAVE JOINED A SERVER"<<endl;
					sendSelectShip( 4 );
				} break;
				case READY: {
					active = true;
					cout<<"OUR SERVER IS READY"<<endl;
					thinkerThread = thread( moveThread );
				} break;
				case STATE: {
					//cout<<"Processed "<<frames<<" frames"<<endl;
					frames = 0;
				} break;
			}   

			// Loop until we run out of packets
			n = connection->getPacket( buffer, cliaddr );   

            /*key = convertAddressToKey( cliaddr );
            currentUser = getUser( key );

            switch( buffer[ 0 ] ) {
                case 1: initialized = true; break;
                case ADDRESS: {
                    cout<<"WE HAVE AN ADDRESS"<<endl;
                    processAddressResponse( buffer, n );                  
                }
                break;
                case JOIN: {
                    cout<<"WE HAVE A USER"<<endl;

                    if( getUser( key ) != NULL ) {
                        cout<<"BUT WE ALREADY HAVE ONE?"<<endl;
                        continue;
                    }
                    createUserEndpoint( cliaddr );                                  
                }
                break;
                case ID: {
                    cout<<"WE HAVE AN ID"<<endl;
                    processID( buffer, n );
                }
                break;
                case DISCONNECT:
                    cout<<"WE LOST A USER"<<endl;
                    removeUserEndpoint( cliaddr );
                    break;
                case SELECT_SHIP: {                  
                    cout<<"SELECT SHIP"<<endl;
                    int ship = buffer[ 1 ] + ( buffer[ 2 ] << 8 );
                    sendSelectShip( key, ship );
                }
                break;
                case TURN: {
                    cout<<"TURN"<<endl; 
                    uint8 dir = buffer[ 1 ];
                    sendTurn( key, dir );
                }
                break;
                case ACCELERATE: {
                    cout<<"ACCELERATE"<<endl;
                    uint8 dir = buffer[ 1 ];
                    sendAccelerate( key, dir );
                }
                break;
                case FIRE: {
                    cout<<"FIRE"<<endl;
                    sendFire( key );
                }
                break;
                case USE: {
                    cout<<"USE"<<endl;
                    uint8 item = buffer[ 1 ];
                    sendUse( key, item );
                }
                break;*/
		}

		if( active ) {
			frames++;
			//sendTurn( 1 );
		}

		usleep( 16666 );
	}

	return 0;
}

void initialize( int port ) {
    // Clear out the structs    
    memset( &cliaddr, 0, sizeof( cliaddr ) );

    connection = new Socket();
	connection->setPort( port );
    connection->Bind();

	static struct sigaction _sigact;

    memset(&_sigact, 0, sizeof(_sigact));
    _sigact.sa_sigaction = sig_term_handler;
    _sigact.sa_flags = SA_SIGINFO;

    sigaction( SIGTERM, &_sigact, NULL );
    sigaction( SIGINT, &_sigact, NULL ); 
}

void sendLogin() {
	cout<<"sendLogin"<<endl;
	
	vector<uint8> packet( 3 );
	packet[ 0 ] = 0;
    packet[ 1 ] = INITIALIZE;
    packet[ 2 ] = USER;

    connection->SendReliable( packet, "127.0.0.1", 8034 );
}

void sendJoin() {
	cout<<"sendJoin"<<endl;
	
	vector<uint8> packet( 2 );
	packet[ 0 ] = 0;
    packet[ 1 ] = JOIN;

    connection->SendReliable( packet, serverIP, serverPort );
}

void sendFire() {
	cout<<"sendFire"<<endl;
	
	vector<uint8> packet( 2 );
	packet[ 0 ] = 0;
    packet[ 1 ] = FIRE;

    connection->SendReliable( packet, serverIP, serverPort );
}

void sendAccelerate( char dir ) {
	cout<<"sendAccelerate"<<endl;
	
	vector<uint8> packet( 3 );
	packet[ 0 ] = 0;
    packet[ 1 ] = ACCELERATE;
	packet[ 2 ] = dir;

    connection->SendReliable( packet, serverIP, serverPort );
}

void sendTurn( char dir ) {
	cout<<"sendTurn"<<endl;
	
	vector<uint8> packet( 3 );
	packet[ 0 ] = 0;
    packet[ 1 ] = TURN;
	packet[ 2 ] = dir;

    connection->SendReliable( packet, serverIP, serverPort );
}

void sendSelectShip( int ship ) {
	cout<<"sendSelectShip"<<endl;
	
	vector<uint8> packet( 4 );
	packet[ 0 ] = 0;
    packet[ 1 ] = SELECT_SHIP;
	packet[ 2 ] = ship & 0xFF;
	packet[ 3 ] = ( ship >> 8 ) & 0xFF;

    connection->SendReliable( packet, serverIP, serverPort );
}

void sendDisconnect() {
	if( serverIP == NULL || serverPort == -1 ) return;

	cout<<"sendDisconnect"<<endl;
	
	vector<uint8> packet( 2 );
	packet[ 0 ] = 0;
    packet[ 1 ] = DISCONNECT;

    connection->Send( packet, serverIP, serverPort );
}

void sig_term_handler(int signum, siginfo_t *info, void *ptr){
    if( signum == 2 ) {
		sendDisconnect();

		connection->Kill();
		delete connection;

		exiting = true;
		if( thinkerThread.joinable() ) thinkerThread.join();
	};
    exit( 0 );
}

void moveThread() {
    //std::cout<<"Starting up Checker"<<std::endl;

	cout<<"moveThread Started"<<endl;

    struct timespec start, end;
    uint64_t diff;	

    while( !exiting ) {
			//cout<<"Tick"<<endl;
		usleep( 1000000 );
		cout<<"TICK"<<endl;
		//usleep( 16666 );
		
		vector<uint8> packet( 2 );		
		packet[ 0 ] = 0;
		packet[ 1 ] = FIRE;

		connection->SendReliable( packet, serverIP, serverPort );

			/*packet = createBuffer( 4 );
			packet[ 0 ] = TURN;
			packet[ 1 ] = 0;
			packet[ 2 ] = key;
			packet[ 3 ] = 1;
			sendto( sockfd, packet, sizeof( packet ), MSG_CONFIRM, (const struct sockaddr *)&servaddr, sizeof( servaddr ) );

			packet = createBuffer( 4 );
			packet[ 0 ] = ACCELERATE;
			packet[ 1 ] = 0;
			packet[ 2 ] = key;
			packet[ 3 ] = 1;
			//sendto( sockfd, packet, sizeof( packet ), MSG_CONFIRM, (const struct sockaddr *)&servaddr, sizeof( servaddr ) );*/
    }
}
