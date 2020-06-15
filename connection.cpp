#include "connection.h"
#include "server.h"
#include <iostream>
#include <stdio.h>
#include <string>

Connection::Connection( int sockfd, sockaddr_in socket ) {
    this->_debug = false;
    this->debug( "Created" );    

    this->sockfd = sockfd;
    this->socket = socket;
    this->ip = socket.sin_addr.s_addr;
    this->port = socket.sin_port;
    this->connected = true;    

    this->angle = 0.00;
    this->type = 1;
    this->x = 0;
    this->ship = new Ship( this->type );
}

void Connection::SendInitialized( int key ) {
    this->debug( "SendInitialized" );

    this->key = key;
    if( this->_debug ) std::cout<<"KEY: "<<key<<std::endl;
    unsigned char payload[ 3 ] = { 0x01, (unsigned char)((key >> 8) & 0xFF), (unsigned char)(key & 0xFF) };    
    this->send( payload, 3 );
}

void Connection::send( unsigned char payload[], int size ) {
    if( !this->connected ) return;
    if( this->_debug ) {
        std::cout<<"send: ";
        for( int i = 0; i < size; i++ ) std::cout<<static_cast<unsigned>(payload[ i ] )<<" ";
        std::cout<<std::endl;
    }

    int sent = sendto( this->sockfd, payload, size, MSG_CONFIRM, (const struct sockaddr *) &this->socket, sizeof( this->socket ) );
    if( sent == -1 ) this->debug( "ERROR SENDING" );
    else if( sent != size ) this->debug( "MISMATCHED SIZE" );
    else this->debug( "VALID" );
}

void Connection::Process( sockaddr_in source, char data[] ) {
    this->debug( "Process" );

    if( this->ip == source.sin_addr.s_addr && this->port == source.sin_port ) {
        switch( data[ 0 ] ) {
            case DISCONNECT: {                
                this->Disconnect();
            } break;
            case TURN: {                 
                this->debug( "PROCESS TURN" ); 
                switch( data[ 3 ] ) {
                    case 1:
                        this->ship->TurnRight();
                        this->angle += 2; 
                        break;
                    case 2: 
                        this->ship->TurnLeft();
                        this->angle -= 2; 
                        break;
                }

                while( this->angle >= 360 ) this->angle -= 360;
                while( this->angle < 0 ) this->angle += 360;
            } break;
            case ACCELERATE: {
                this->debug( "PROCESS ACCELERATE" );
                switch( data[ 3 ] ) {                    
                    case 1:
                        this->ship->Accelerate();
                        this->x++; 
                        break;
                    case 2: 
                        this->ship->Decelerate();
                        this->x--; 
                        break;
                }
            } break;
            default: { this->debug( "Invalid Packet" ); } break;
        }
    } else {
        if( this->_debug ) {
            std::cout<<"IP: "<<source.sin_addr.s_addr<<" vs. "<<this->ip<<std::endl;
            std::cout<<"Port: "<<source.sin_port<<" vs. "<<this->port<<std::endl;
        }
        //this->debug( "INVALID SOURCE" );
    }
}

void Connection::SendState( unsigned char *buffer, int length ) {
    this->debug( "SendState" );
    this->send( buffer, length );
}

void Connection::Dump() {
    if( this->_debug ) {
        std::cout<<"=================="<<std::endl;
        std::cout<<"IP: "<<this->ip<<std::endl;
        std::cout<<"Port: "<<this->port<<std::endl;
        std::cout<<"=================="<<std::endl;
    }
}

void Connection::Disconnect() {
    this->debug( "Disconnect" );

    this->connected = false;

    this->sockfd = 0;    
    memset(&this->socket, 0, sizeof(this->socket));
    this->ip = 0;
    this->port = 0;

    this->angle = 0;
    this->x = 0;
}

bool Connection::isConnected() {
    //this->debug( "isConnected" );
    return this->connected;
}

void Connection::debug( std::string msg ) {
    if( this->_debug )
        std::cout<<"Connection: "<<msg<<std::endl;
}