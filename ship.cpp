#include "ship.h"
#include "types.h"
#include <iostream>
#include <string.h>
#include <math.h>

Ship::Ship( int type ) {    
    this->_debug = true;
    this->debug( "Created" );

    this->id = 0;
    
    this->type = type;
    this->x = 0;
    this->vx = 0;

    this->y = 0;
    this->vy = 0;

    this->status = 0;

    this->decelerating = false;

    this->angle = 0;
    this->va = 0;

    this->fireRate = 1000;
    clock_gettime( CLOCK_REALTIME, &this->firedTime );

    this->maxSpeed = .1f;
    this->deltaSpeed = .003f;
    this->accSpeed = 0;

    this->turnSpeed = 2;

    this->width = 1.0f;
    this->height = 1.0f;

    this->setDefaults();

    cout<<"Created Ship("<<this<<")"<<endl;
}

Ship::~Ship() {
    cout<<"Destroying Ship("<<this<<")"<<endl;
}

void Ship::setDefaults() {
    this->debug( "setDefaults" );

    switch( this->type ) {
        case 1: cout<<"WE ARE SHIP #1"<<endl; break;
    }
}

bool Ship::CheckCollide( Entity *e ) const {
    //this->debug( "CheckCollide" );

    if( this->Entity::CheckCollide( e ) ) {
        cout<<"AHHHH WE ARE OVERLAPPING"<<endl;
    }
    //e->Dump();
}

void Ship::Turn( unsigned char dir ) {
    this->debug( "Turn" );
    cout<<"DIR: "<<(int)dir<<endl;

    if( dir == 1 ) this->va += this->turnSpeed;
    if( dir == 2 ) this->va -= this->turnSpeed;

    this->status = this->status | 0b00000001;
}

void Ship::StopTurn( unsigned char dir ) {
    this->debug( "StopTurn" );
    cout<<"DIR: "<<(int)dir<<endl;

    if( dir == 1 ) this->va -= this->turnSpeed;
    if( dir == 2 ) this->va += this->turnSpeed;

    cout<<"ANGLE: "<<this->angle<<endl;

    this->status = this->status & 0b11111110;
}

void Ship::Accelerate( unsigned char dir ) {
    this->debug( "Accelerate" );

    if( dir == 1 ) this->accSpeed += this->deltaSpeed;
    if( dir == 2 ) this->accSpeed -= this->deltaSpeed;

    this->status = this->status | 0b00000010;
}

void Ship::StopAccelerate( unsigned char dir ) {
    this->debug( "StopAccelerate" );

    if( dir == 1 ) this->accSpeed -= this->deltaSpeed;
    if( dir == 2 ) this->accSpeed += this->deltaSpeed;

    this->status = this->status & 0b11111101;
}

int Ship::Fire( unsigned char weapon ) {
    this->debug( "Fire" );

    struct timespec curTime;
    clock_gettime( CLOCK_REALTIME, &curTime );

    auto diff = ( BILLION * ( curTime.tv_sec - this->firedTime.tv_sec ) + curTime.tv_nsec - this->firedTime.tv_nsec ) / MILLION;
    if( diff < this->fireRate ) return -1;

    cout<<"FIRE!!!!"<<endl;

    clock_gettime( CLOCK_REALTIME, &this->firedTime );

    switch( weapon ) {
        case 1: return 13;
        case 2: return 14;
        default: return -1;
    }    
}

void Ship::UpdateSpeed() {
    //this->debug( "UpdateSpeed" );

    // Are we accelerating?  If so let's process it!
    if( this->accSpeed != 0 ) {
        auto ax = sin( this->angle * PI / 180 ) * this->accSpeed;
        auto ay = cos( this->angle * PI / 180 ) * this->accSpeed;

        this->vx += sin( this->angle * PI / 180 ) * this->accSpeed;
        this->vy += cos( this->angle * PI / 180 ) * this->accSpeed;

        if( pow( this->vx, 2 ) + pow( this->vy, 2 ) > pow( this->maxSpeed, 2 ) ) {
            auto vx = this->vx;
            //this->vx = this->maxSpeed / ( this->vx + this->vy ) * this->vx;
            //this->vy = sqrt( pow( this->maxSpeed, 2 ) - pow( this->vx, 2 ) );//this->maxSpeed / ( this->vx + this->vy ) * this->vy;
            //this->vx = this->maxSpeed * ( this->vx / ( this->vx + this->vy ) );
            //this->vy = this->maxSpeed / ( vx + this->vy ) * this->vy;
            //this->vy = this->maxSpeed * ( this->vy / ( vx + this->vy ) );

            float over = sqrt( pow( this->vx, 2 ) + pow( this->vy, 2 ) );
            this->vx = this->vx / over * this->maxSpeed;
            this->vy = this->vy / over * this->maxSpeed;
        }
    }

    // Are we braking?  If so let's process it!
    if( this->decelerating ) {
        cout<<"Velocity: "<<this->vx<<"x"<<this->vy<<endl;
        auto current = sqrt( ( this->vx * this->vx ) + (this->vy * this->vy ) );
        cout<<"Current Speed: "<<current<<endl;
        cout<<"Acceleration: "<<this->deltaSpeed<<endl;
        current -= this->deltaSpeed;
        cout<<"Current Speed: "<<current<<endl;
        if( current < 0 ) current = 0;

        this->vx = sin( this->angle * PI / 180 ) * current;
        this->vy = cos( this->angle * PI / 180 ) * current;
        cout<<"Velocity: "<<this->vx<<"x"<<this->vy<<endl;
    }
}

void Ship::Decelerate() {
    this->debug( "Decelerate" );
    this->decelerating = true;
}

void Ship::StopDecelerate() {
    this->debug( "StopDecelerate" );
    this->decelerating = false;
}

void Ship::Dump() const {    
    cout<<"=========SHIP========"<<endl;
    cout<<"ID: "<<this->id<<endl;
    cout<<"Position: "<<this->x<<","<<this->y<<endl;
    cout<<"Velocity: "<<this->vx<<","<<this->vy<<endl;
    cout<<"Angle: "<<this->angle<<endl;
    cout<<"Dimensions: "<<this->width<<"x"<<this->height<<endl;
    cout<<"====================="<<endl;
}

void Ship::debug( string msg, bool force ) const {
    if( this->_debug || force )
        cout<<"Ship: "<<msg<<endl;
}