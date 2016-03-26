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

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define NAME_SIZE 32

struct User 
{
    int fd;
    char name[NAME_SIZE];

    User(int _fd);
    User(int _fd, char _name[NAME_SIZE]);
    ~User() { }
};

enum msg_type { COMMON, DISCON, CONNECT };

struct Message
{
    char text[BUFFER_SIZE];
    int fd;

    Message(msg_type _type, char buff[BUFFER_SIZE], User &user);
    ~Message() { }
};

class Server
{

public:
    
    Server(int port = 3100);
    ~Server();

    void manage_chat();
    void print_log(const char* text);

private:
    
    struct epoll_event ev, events[MAX_EVENTS];
    int listen_sock, conn_sock, nfds, epollfd;

    int log_num;

    std::vector<User> users;

    Message *read_from_user(User &user);
    void send_message(Message *msg, std::vector<User> users);
    void set_nonblocking(int fd);
    int id_by_fd(int fd, std::vector<User> &users);
    void manage_connection();
    void manage_data(User &user);
    void manage_disconnect(User &user);
};
