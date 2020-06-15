#pragma once

#include <string>

enum EntityType {
    SHIP,
    BULLET
};

class Entity {
public:
    int id;
    int type;

    float x;
    float vx;
    
    float y;
    float vy;

    float width;
    float height;

    unsigned char status;
    
    float angle;

    Entity();
    
    virtual bool CheckCollide( Entity* ) const;
    virtual void Dump() const;
protected:
    bool _debug;

    void debug( std::string ) const;
};
