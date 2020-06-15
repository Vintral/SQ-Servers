g++ -o server.o server.cpp ship.cpp connection.cpp -g -std=c++0x -pthread -L/usr/local/lib -lcpp_redis -ltacopie
./server.o
