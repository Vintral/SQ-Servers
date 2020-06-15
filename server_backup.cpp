#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 8881
#define BUFSIZE 2048

using namespace std;

int main() {
    struct sockaddr_in myaddr;
    struct sockaddr_in remaddr;
    socklen_t addrlen = sizeof( remaddr );    
    int s;
    int recvlen;
    unsigned char buf[ BUFSIZE ];

    if( ( s = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
        cout<<"Failed to create Socket"<<endl;
        return 0;
    } else cout<<"Socket Created"<<endl;

    memset( ( char * )&myaddr, 0, sizeof( myaddr) );
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl( INADDR_ANY );
    myaddr.sin_port = htons( 0 );

    if( bind( s, (struct sockaddr *)&myaddr, sizeof( myaddr ) ) < 0 ) {
        cout<<"Binding Failed"<<endl;
        return 0;
    } else cout<<"Socket Bound"<<endl;



    while( true ) {
        cout<<"Waiting on port: "<<PORT<<endl;
        recvlen = recvfrom( s, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen );
        cout<<"Recieved: "<<recvlen<<" bytes"<<endl;

        if( recvlen > 0 ) {
            buf[ recvlen ] = 0;
            cout<<"Received: "<<buf<<endl;
        }        
    }
}