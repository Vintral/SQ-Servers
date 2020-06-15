#include <iostream>
#include <vector>
#include <memory>
 
using namespace std;

class Entity {
public:
    Entity() { cout<<"Entity Constructed"<<endl; }
    ~Entity() { cout<<"Entity Deconstructed"<<endl; }
};

class Ship : public Entity {
public :
    Ship() { 
        cout<<"========================"<<endl;
        cout<<"Ship Constructed"<<endl;
        cout<<"========================"<<endl;
    }
    ~Ship() { cout<<"Ship Deconstructed"<<endl; }
};

class Factory {
public:
    Factory() { cout<<"Factory Constructed"<<endl; }
    ~Factory() { cout<<"Factory Deconstructed"<<endl; }

    shared_ptr<Entity> CreateEntity() { return make_shared<Entity>(); }
    shared_ptr<Ship> CreateShip() { return make_shared<Ship>(); }
};

void test() {
    auto factory = make_shared<Factory>();
    //shared_ptr<Entity> entity = make_shared<Entity>();
    //Ship *ship = new Ship();

    vector<shared_ptr<Entity>> entities;
    entities.push_back( factory->CreateEntity() );

    /*vector<shared_ptr<Ship>> ships;
    ships.push_back( factory->CreateShip() );
    ships.push_back( factory->CreateShip() );
    ships.push_back( factory->CreateShip() );*/

    //entity = factory->CreateEntity();

    entities.push_back( static_pointer_cast<Entity>( factory->CreateShip() ) );
}

int main(){
    test();
}
