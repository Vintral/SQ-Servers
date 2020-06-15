#include "entity.h"

#include <iostream>

using namespace std;

Entity::Entity() {
    this->_debug = true;
    this->debug( "Created" );
}

void Entity::Dump() const {
    this->debug( "Dump" );
}

bool Entity::CheckCollide( Entity *e ) const {
    //this->debug( "CheckCollide: NOT IMPLEMENTED" );

    if( ( e->x > this->x ) && ( e->x < this->x + this->width ) &&
        ( e->y > this->y ) && ( e->y < this->y + this->height ) ) {
            return true;        
    }
    
    if( ( e->x + e->width > this->x ) && ( e->x + e->width < this->x + this->width ) &&
        ( e->y + e->height > this->y ) && ( e->y + e->height < this->y + this->height ) ) {
            return true;        
    }

    return false;
}

void Entity::debug( string msg ) const {
    if( this->_debug )
        cout<<"Entity: "<<msg<<endl;
}
