#pragma once

#include <string>
#include <memory>

#include "entity.h"

using namespace std;

class Ship : public Entity {
public:    
    float turnSpeed;
    
    float va;

    float maxSpeed;
    float deltaSpeed;
    float accSpeed;

    bool decelerating;
    
    Ship( int );
    ~Ship();

    void Dump() const override;
    bool CheckCollide( Entity* ) const override;

    int Fire( unsigned char );

    void Turn( unsigned char );
    void StopTurn( unsigned char );

    void Accelerate( unsigned char );
    void StopAccelerate( unsigned char );
    void UpdateSpeed();

    void Decelerate();
    void StopDecelerate();
protected:
    float fireRate;
    struct timespec firedTime;

    void setDefaults();
    void debug( string, bool force = false ) const;
};