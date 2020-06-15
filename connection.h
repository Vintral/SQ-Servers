#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <vector>
#include "packet.h"
#include "server.h"
#include "ship.h"

class Connection {
private:
    bool connected;
    uint32 ip;
    uint16 port;
    sockaddr_in socket;
    int sockfd;
    int key;
protected:
    bool _debug;

    void send( unsigned char[], int );
    void debug( std::string msg );    
public:
    double angle;
    double x;
    int type;
    Ship *ship;

    //Connection( uint32 ip, uint16 port );
    Connection( int sockfd, sockaddr_in socket );
    void Dump();
    void Process( sockaddr_in, char[] );
    void Disconnect();
    bool isConnected();

    void SendInitialized( int key );
    void SendState( unsigned char *, int );
};