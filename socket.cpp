#include "socket.h"
#include "packet.h"
#include <iostream>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <mutex>
#include <fcntl.h>
#include <netinet/in.h>

#define CONNECTION_LIMIT 10

using namespace std;

mutex mtx;

Socket::Socket() {
    this->_debug = false;
    this->debug( "Created" );
    
    this->setDefaults();
    this->create();
}

Socket::~Socket() {
    this->debug( "Deconstruct" );
}

void Socket::setDefaults() {
    this->debug( "setDefaults" );

    memset( &this->_address, 0, sizeof( this->_address ) );
    memset( &this->_client, 0, sizeof( this->_client ) );

    this->_handle = -1;
    this->exiting = false;

    this->len = sizeof( this->_client );

    this->domain = AF_INET;
    this->type = SOCK_DGRAM;
    this->port = 0;

    this->trackByKey = true;

    this->packetID = 1;

    //this->senderThread( Socket::reliableDaemon );
    this->reliableThread = thread( &Socket::reliableDaemon, this );

    // Filling server information 
    this->_address.sin_family = this->domain;
    this->_address.sin_addr.s_addr = INADDR_ANY; // From anyone
    this->_address.sin_port = this->port;    
}

void Socket::setDebug( bool val ) {
    this->debug( "setDebug" );

    this->_debug = val;
}

void Socket::setPort( int port ) {
    this->debug( "setPort" );
    cout<<"PORT:"<<port<<endl;

    this->_address.sin_port = htons( port );    
}

void Socket::setIP( std::string ip ) {
    this->debug( "setIP" );
    inet_aton( ip.c_str(), &this->_address.sin_addr );
}

int Socket::getPacket( vector<uint8> &buffer, sockaddr_in& client ) {
    char *v;
    //auto n = recvfrom( this->_handle, &buffer[ 0 ], MAX_PACKET_SIZE, MSG_PEEK | MSG_DONTWAIT, (struct sockaddr *) &client, &this->len );
    auto n = recvfrom( this->_handle, &buffer[ 0 ], MAX_PACKET_SIZE, MSG_PEEK | MSG_DONTWAIT, (struct sockaddr *) &client, &this->len );
    if( n > 0 ) {
        buffer.resize( n );
        n = recvfrom( this->_handle, &buffer[ 0 ], MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr *) &client, &this->len );

        if( buffer[ COMMAND ] == ACK ) {
            //cout<<"RECEIVED ACK FOR"<<(unsigned char)buffer[ PACKET_ID ]<<endl;
            this->_reliablePackets.erase( (unsigned char)buffer[ PACKET_ID ] );
        } else {
            if( buffer[ PACKET_ID ] > 0 ) {
                string key;
                
                if( this->trackByKey ) key = this->convertAddressToKey( client );
                else {
                    key = to_string( buffer[ CLIENT_ID_1 ] + ( buffer[ CLIENT_ID_2 ] << 8 ) );
                }

                auto check = this->_receivedPackets.find( key );
                if( check == this->_receivedPackets.end() ) {
                    mtx.lock();
                    this->_receivedPackets[ key ] = unordered_map<int, timespec>();
                    mtx.unlock();
                } else {
                    auto check = this->_receivedPackets[ key ].find( buffer[ PACKET_ID ] );
                    if( check != this->_receivedPackets[ key ].end() ) {
                        //cout<<"DUPE PACKET"<<endl;
                        return this->getPacket( buffer, client );
                    } //else cout<<"NEW PACKET: "<<key<<endl;
                }

                mtx.lock();
                clock_gettime( CLOCK_REALTIME, &this->_receivedPackets[ key ][ buffer[ 0 ] ] );
                mtx.unlock();
                
                vector<uint8> packet( 2 );
                packet[ 0 ] = buffer[ 0 ];
                packet[ 1 ] = ACK;
                
                auto n = sendto( this->_handle, &packet[ 0 ], packet.size(), MSG_CONFIRM, (const struct sockaddr *)&client, sizeof( client ) );   
                //cout<<"SENT: "<<n<<endl;             
            }
        }
    }
    
    return n;
}

int Socket::getConnection( sockaddr_in& client ) {
    this->debug( "getConnection" );
    return accept( this->_handle, (struct sockaddr *)&client, &this->len );
}

void Socket::setIP( char* ip ) {
    this->debug( "setIP" );
    inet_aton( ip, &this->_address.sin_addr );
}

void Socket::Connect() {
    this->debug( "Connect" );

    if( connect( this->_handle, (struct sockaddr *)&this->_address, sizeof( this->_address ) ) < 0 ) { 
        std::cout<<"Connection Failed"<<std::endl;
        exit( EXIT_FAILURE );
    } 
}

void Socket::dumpBytes( unsigned char *data, int length ) {    
	for( int i = 0; i < length; i++ )
		std::cout<<static_cast<unsigned>((unsigned char)data[ i ])<<" ";
	std::cout<<std::endl;
}

void Socket::dumpBytes( vector<uint8> data, int length ) {
    for( int i = 0; i < length; i++ )
        cout<<static_cast<unsigned>( (unsigned char) data[ i ] )<<" ";
    cout<<endl;
}

void Socket::dumpBytes( vector<uint8> data ) {
    for( vector<uint8>::iterator it = data.begin(); it != data.end(); ++it )
        cout<<static_cast<unsigned>( (unsigned char) *it )<<" ";
    cout<<endl;
}

void Socket::SendReliable( vector<uint8> data, const char* ip, int port ) {
    this->debug( "SendReliable" );

    data[ 0 ] = this->packetID++;
    if( data[ 0 ] == 0 ) data[ 0 ] = this->packetID++;

    Packet *packet = new Packet();
    packet->data = data;    
    packet->ip = ip;
    packet->port = port;
    this->_reliablePackets[ data[ 0 ] ] = *packet;
    delete packet;    

    this->Send( data, ip, port );    
}

void Socket::Send( vector<uint8> data, const char* ip, int port ) {
    if( this->exiting ) return;
    
    this->debug( "Send" );

    if( this->_debug ) this->dumpBytes( data );

    struct sockaddr_in addr;
    memset( &addr, 0, sizeof( addr ) );

    inet_aton( ip, &addr.sin_addr );
    addr.sin_port = htons( port );

    //std::cout<<"SIZE: "<<strlen( (const char*)data )<<std::endl;
    //std::cout<<"SIZE: "<<size<<std::endl;
    auto n = sendto( this->_handle, &data[ 0 ], data.size(), MSG_CONFIRM, (const struct sockaddr *)&addr, sizeof( addr ) );
    //std::cout<<"SENT: "<<n<<std::endl;
}

void Socket::Bind() {
    this->debug( "Bind" );

    std::cout<<"Size: "<<sizeof( this->_address )<<std::endl;
    std::cout<<"Handle: "<<this->_handle<<std::endl;

    // Bind the socket with the server address 
    if ( bind( this->_handle, (const struct sockaddr*)&this->_address, sizeof( this->_address ) ) < 0 ) { 
        std::cout<<"Binding Failed"<<std::endl;

        std::cout<<"ERROR: ";
        switch( errno ) {
            case EACCES: std::cout<<"Address is Protected"; break;
            case EADDRINUSE: std::cout<<"Address is in use"; break;
            case EBADF: std::cout<<"Bad Socket Descriptor"; break;
            case ENOTSOCK: std::cout<<"Descriptor is not a socket"; break;
            default: std::cout<<errno;
        }
        std::cout<<std::endl;

        exit( EXIT_FAILURE ); 
    }
    this->debug( "Socket Bound" );

    if( this->type == SOCK_STREAM ) {
        int flags = fcntl( this->_handle, F_GETFL );
        fcntl( this->_handle, F_SETFL, flags | O_NONBLOCK );

        if( listen( this->_handle, CONNECTION_LIMIT ) != 0 ) {
            std::cout<<"Socket Can't Listen"<<std::endl;
            exit( EXIT_FAILURE );
        }    
    }    
}

void Socket::reliableDaemon() {
    this->debug( "reliableThread" );

    uint64_t diff;
    struct timespec curTime;
    int seconds = 5; // Seconds to hold onto a packet for tracking for
    while( !this->exiting ) {
        // Check for any packets we've sent that are still pending and resend them
        unordered_map<int, Packet>::iterator it = this->_reliablePackets.begin(); 
        while( it != this->_reliablePackets.end() ) {
            cout<<it->first<<endl;

            // Pull out the packet
            Packet packet = (Packet)it->second;            

            // Send it again
            this->Send( packet.data, packet.ip, packet.port );
            it++;
        }

        clock_gettime( CLOCK_REALTIME, &curTime );

        // Clear out old tracked packets
        for( auto tracked = this->_receivedPackets.begin(); tracked != this->_receivedPackets.end(); ) {
            //cout<<tracked->first<<endl;
            //cout<<"Size: "<<tracked->second.size()<<endl;

            for( auto packet = tracked->second.begin(); packet != tracked->second.end(); ) {                
                diff = 1000000000L * ( curTime.tv_sec - packet->second.tv_sec ) + curTime.tv_nsec - packet->second.tv_nsec;
                //cout<<"ID: "<<packet->first<<"   Diff: "<<diff<<endl;


                if( diff > seconds * 1000000000L ) {
                    //cout<<"IT HAS BEEN "<<seconds<<" SECONDS STOP TRACKING"<<endl;

                    //cout<<"Current Size: "<<tracked->second.size()<<endl;
                    if( tracked->second.size() > 0 ) {
                        mtx.lock();
                        if( tracked->second.find( packet->first ) != tracked->second.end() ) {                        
                            packet = tracked->second.erase( packet );
                        }
                        mtx.unlock();
                    } else packet++;
                    //cout<<"Final Size: "<<tracked->second.size()<<endl;
                } else packet++;
            }

            if( tracked->second.size() == 0 ) {
                mtx.lock();
                tracked = this->_receivedPackets.erase( tracked );
                mtx.unlock();
            } else tracked++;
        }

        usleep( 250000 );
    }
}

void Socket::setTCP() {
    this->debug( "setTCP" );
    if( this->type == SOCK_STREAM ) return;

    this->type = SOCK_STREAM;
    this->create();
}

void Socket::setUDP() {
    this->debug( "setUDP" );
    if( this->type == SOCK_DGRAM ) return;
    
    this->type = SOCK_DGRAM;
    this->create();
}

void Socket::setIPv4() {
    this->debug( "setIPv4" );
    if( this->domain == AF_INET ) return;

    this->domain = AF_INET;
    this->create();
}

void Socket::setIPv6() {
    this->debug( "setIPv6" );
    if( this->domain == AF_INET6 ) return;

    this->domain = AF_INET6;
    this->create();
}

void Socket::setTrackByKey() {
    this->debug( "setTrackByKey" );
    this->trackByKey = true;
}

void Socket::setTrackByID() {
    this->debug( "setTrackByID" );
    this->trackByKey = false;
}

void Socket::create() {
    this->debug( "create" );
    
    // Reset if we've been created already
    if( this->_handle >= 0 ) this->reset();

    // Creating socket file descriptor 
    if ( ( this->_handle = socket ( this->domain, this->type, 0 ) ) < 0 ) { 
        std::cout<<"Socket Creation Failed!"<<std::endl;
        exit( EXIT_FAILURE );
    }    
}

void Socket::reset() {
    this->debug( "reset" );

    //close( this->_handle );
}

sockaddr_in Socket::getClient() {
    return this->_client;
}

string Socket::convertAddressToKey( sockaddr_in addr ) {
    char ip[INET_ADDRSTRLEN];
    int port;

    inet_ntop( AF_INET, &( addr.sin_addr ), ip, INET_ADDRSTRLEN );
    port = ntohs( addr.sin_port );

    std::string key = ip;
    key.append( ":" );
    key.append( std::to_string( port ) );

    return key;
}

void Socket::Kill() {
    this->debug( "Kill" );

    this->exiting = true;
    if( this->reliableThread.joinable() ) this->reliableThread.join();
    //delete this->reliableThread;
}

void Socket::debug( string msg ) {
    if( this->_debug )
        cout<<"Socket: "<<msg<<endl;
}
