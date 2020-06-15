#pragma once

#include <string>
#include <memory>

#include "entity.h"

using namespace std;

class Bullet : public Entity {
public:
    float speed;

    Bullet( int );
    ~Bullet();

    void Dump() const override;    
protected:    
    void setDefaults();
    void debug( std::string msg );
};