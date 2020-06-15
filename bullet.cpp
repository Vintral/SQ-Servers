#include "bullet.h"

#include "types.h"
#include <iostream>
#include <string.h>
#include <math.h>

Bullet::Bullet( int type ) {    
    this->_debug = true;
    this->debug( "Created" );

    this->id = 0;
    
    this->type = type;
    this->x = 0;
    this->vx = 0;

    this->speed = 3;

    this->y = 0;
    this->vy = 0;

    this->angle = 0;



    this->width = 1.0f;
    this->height = 1.0f;

    this->setDefaults();

    cout<<"Created Bullet("<<this<<")"<<endl;
}

Bullet::~Bullet() {
    cout<<"Destroying Bullet("<<this<<")"<<endl;
}

void Bullet::setDefaults() {
    this->debug( "setDefaults" );

    switch( this->type ) {
        case 13: {
            cout<<"WE ARE Bullet #1"<<endl;
            this->speed = .15f;            
        } break;
    }
}

void Bullet::Dump() const {    
    cout<<"=======BULLET========"<<endl;
    cout<<"ID: "<<this->id<<endl;
    cout<<"Type: "<<this->type<<endl;
    cout<<"Position: "<<this->x<<","<<this->y<<endl;
    cout<<"Velocity: "<<this->vx<<","<<this->vy<<endl;
    cout<<"Angle: "<<this->angle<<endl;
    cout<<"Dimensions: "<<this->width<<"x"<<this->height<<endl;
    cout<<"====================="<<endl;
}

void Bullet::debug( std::string msg ) {
    if( this->_debug )
        cout<<"Bullet: "<<msg<<endl;
}