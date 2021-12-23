#include "common.h"
#include "TCPreqchannel.h"
using namespace std;

// 2 parameter constructor
TCPRequestChannel::TCPRequestChannel(const string hostname, const string port_no){

    if (hostname != "") { // client side

        struct addrinfo hints, *res;

        // first, load up address structs with getaddrinfo():
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET; // use IPv4
        hints.ai_socktype = SOCK_STREAM; // TCP

        int addrStatus = getaddrinfo(hostname.c_str(), port_no.c_str(), &hints, &res);
        cout << "client getaddrinfo: " << addrStatus << endl;

        // socket()
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        cout << "client socket: " << sockfd << endl;

        // connect()
        int connectStatus = connect(sockfd, res->ai_addr, res->ai_addrlen);
        cout << "client connect: " << connectStatus << endl;


    } else { // server side

        struct addrinfo hints, *res;

        // first, load up address structs with getaddrinfo():
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;  // use IPv4
        hints.ai_socktype = SOCK_STREAM; // TCP
        hints.ai_flags = AI_PASSIVE; // fill in my IP for me

        int addrStatus = getaddrinfo(NULL, port_no.c_str(), &hints, &res);
        cout << "server getaddrinfo: " << addrStatus << endl;

        // make a socket:
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        cout << "server socket: " << sockfd << endl;

        // bind it to the port we passed in to getaddrinfo():
        int bindStatus = bind(sockfd, res->ai_addr, res->ai_addrlen);
        cout << "server bind: " << bindStatus << endl;

        // mark socket to listen
        int listenStatus = listen(sockfd, 100); // set backlog to 100 ?
        cout << "server listen: " << listenStatus << endl;
    }
}

// 1 parameter constructor
TCPRequestChannel::TCPRequestChannel(int new_sockfd) {
    // this is a channel to work with the file descriptor that gets returned from accept()

    // update sockfd to the new sockfd
    sockfd = new_sockfd;

    // done :)

}

// destructor
TCPRequestChannel::~TCPRequestChannel(){ 
    close(sockfd);
}

// read
int TCPRequestChannel::cread(void* msgbuf, int bufcapacity) {
    int nbytes = recv(sockfd, msgbuf, bufcapacity, 0);
    return nbytes;
}

// write
int TCPRequestChannel::cwrite(void* msgbuf, int len){
    int nbytes = send(sockfd, msgbuf, len, 0);
    return nbytes;
}

int TCPRequestChannel::getfd() {
    return sockfd;
}

