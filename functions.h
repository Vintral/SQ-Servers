uint8* createBuffer( int length ) {
    uint8* ret = new uint8[ length ];
    //memset( ret, '\0', length );
}

const char* convertAddressToKey( sockaddr_in address ) {
    char ip[INET_ADDRSTRLEN];
    int port;

    inet_ntop( AF_INET, &( address.sin_addr ), ip, INET_ADDRSTRLEN );
    port = ntohs( address.sin_port );

    std::string key = ip;
    key.append( ":" );
    key.append( std::to_string( port ) );

    return key.c_str();
}