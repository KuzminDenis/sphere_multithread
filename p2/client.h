#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <cstdlib>
#include <fcntl.h>
#include <vector>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

class Client
{

public:
    
    Client();
    ~Client();

    void connect_to_server(const char *ip, int port);
    void use_chat();

private:
    
    int sockfd;
    std::vector<int> fds;

    int recieve_message(int fd);
    void send_message();
};
