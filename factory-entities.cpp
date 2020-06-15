#include "factory-entities.h"
#include <memory>
#include <iostream>

#include "ship.h"
#include "bullet.h"

using namespace std;

FactoryEntities::FactoryEntities() {
    this->_debug = true;
    this->debug( "Created" );
}

shared_ptr<Entity> FactoryEntities::Create( EntityType type, int id ) {
    cout<<"Create: "<<type<<" - "<<id<<endl;

    switch( type ) {
        //case SHIP: return static_pointer_cast<Entity>( make_shared<Ship>( id ) );
        //case BULLET: return static_pointer_cast<Entity>( make_shared<Bullet>( id ) );
        case SHIP: return make_shared<Ship>( id );
        case BULLET: return make_shared<Bullet>( id );
    }
}

void FactoryEntities::debug( string msg ) {
    if( this->_debug )
        cout<<"FactoryEntities: "<<msg<<endl;
}
