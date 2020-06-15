#pragma once

#include <string>
#include <memory>

#include "entity.h"

class FactoryEntities {
public:
    FactoryEntities();
    std::shared_ptr<Entity> Create( EntityType, int );
protected:
    bool _debug;
    void debug( std::string );
};
