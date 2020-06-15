#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <spawn.h>

#include <thread>

using namespace std;

void spawnThread() {
    printf( "Firing up ship\n" );

    execl( "./client.o", "client", (char *)0 );
}

int main(int argc,char* argv[]){

    int status;
    int num;
    const int CLIENTS = 3;
    
    std::thread *threads = new std::thread[ CLIENTS ];
    for( int i = 0; i < CLIENTS; i++ ) {
        cout<<"Spawning: "<<( i + 1 )<<" of "<<CLIENTS<<endl;
        //threads[ i ] = std::thread ( spawnThread );
        //spawnl( P_NOWAIT, "./client.o", "client", (char*) 0 );
        //system( "./client.o &" );
        execl( "./client.o", "client", (char *)0 );
    }
    cout<<"Done Spawning Clients"<<endl;

    while( true ) {};
    return 0;
}