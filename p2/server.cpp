#include "server.h"

// USER ==============================================================

User::User(int _fd)
{
    fd = _fd;
    const char *default_name = "Anonymous";
    strcpy(name, default_name);

    for (int i = strlen(default_name); i < NAME_SIZE; i++)
        name[i] = '\0';
}

User::User(int _fd, char _name[NAME_SIZE])
{
    size_t i;
    for (i = 0; i <= strlen(_name); i++)
        name[i] = _name[i];

    for (i = i; i < NAME_SIZE; i++)
        name[i] = '\0';

    fd = _fd;
}

// MESSAGE ===========================================================

Message::Message(msg_type _type, char buff[BUFFER_SIZE], User &user) 
{
    if (_type == COMMON)
    {
        sprintf(text, 
                "<%s>: %s%c", 
                user.name, buff, '\0');
    }
    else if (_type == DISCON)
    {
        sprintf(text, 
                "User <%s> disconnected from the channel (connection terminated)\n%c",
                user.name, '\0');
    }
    else
    {
        sprintf(text, 
                "User <%s> entered your channel (accepted connection)\n%c",
                user.name, '\0');
    }

    for (int i = strlen(text); i < BUFFER_SIZE; i++)
        text[i] = '\0';
 
    fd = user.fd;
}

Message* Server::read_from_user(User &user) 
{
    char buff[BUFFER_SIZE];

    int bytes_recieved = recv(user.fd, &buff, BUFFER_SIZE, 0);
    if (bytes_recieved > 0) 
    {
        buff[bytes_recieved] = '\0';
        Message *msg = new Message(COMMON, buff, user);
        return msg;    
    } 
    else if (bytes_recieved == 0) 
    { 
        return NULL;
    }

    throw "recv";
}

// SERVER ============================================================

Server::Server(int port) 
{
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1)
    {
        throw "socket";
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (0 != bind(listen_sock, (struct sockaddr *) &addr, 
                  sizeof(addr))) 
    {
        throw "bind";
    }
    
    if ( -1 == listen(listen_sock, 100)) 
    {
        throw "listen";
    } 

    epollfd = epoll_create1(0);
    if (epollfd == -1) 
    {
        throw "epoll_create1";
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) 
    {
        throw "epoll_ctl: listen_sock";
    }

    log_num = 0;

    int opt = 1;

    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, 
               &opt, sizeof(opt));
}

Server::~Server()
{
    for (size_t i = 0; i < users.size(); i++)
    {
        shutdown(users[i].fd, SHUT_RDWR);
        close(users[i].fd);
    }

    shutdown(listen_sock, SHUT_RDWR);
    close(listen_sock);

    print_log("Server is shutting down\n");
}

void Server::print_log(const char *text)
{
    char buff[BUFFER_SIZE];
    
    sprintf(buff, 
            "[LOG -%3d-] %s\n%c",
            log_num, text, '\0');

    for (size_t i = strlen(buff); i < BUFFER_SIZE; i++)
        buff[i] = 0;

    printf("%s", buff);
    log_num++;
}

void Server::send_message(Message *msg, std::vector<User> users)
{
    for (size_t i = 0; i < users.size(); i++)
    {
        if (users[i].fd != msg->fd)
            send(users[i].fd, msg->text, BUFFER_SIZE, 0);
    }             
}

void Server::set_nonblocking(int fd) 
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int Server::id_by_fd(int fd, std::vector<User> &users)
{
    for (size_t i = 0; i < users.size(); i++)
        if (users[i].fd == fd)
            return i;
    return -1;
} 

void Server::manage_connection()
{
    int conn_sock = accept(listen_sock, NULL, NULL);
    if (conn_sock == -1) 
    {
        throw "accept";
    }
   
    set_nonblocking(conn_sock);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = conn_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock,
                &ev) == -1) 
    {
        throw "epoll_ctl: conn_sock";
    }

    User user = User(conn_sock);
    users.push_back(user);

    Message *msg = new Message(CONNECT, NULL, user);
    send_message(msg, users);

    const char *greetings = "Welcome!\n";
    send(user.fd, greetings, strlen(greetings), 0);
    print_log(msg->text);
    delete msg;
}

void Server::manage_disconnect(User &user)
{
    Message *msg = new Message(DISCON, NULL, user);
    send_message(msg, users);
    print_log(msg->text);
    delete msg;

    shutdown(user.fd, SHUT_RDWR);
    close(user.fd);
    for (std::vector<User>::iterator it = users.begin(); 
         it != users.end(); ++it)
    {
        if (it->fd == user.fd)
        {
            users.erase(it);
            break;
        }
    }
}

void Server::manage_chat()
{
    Message *msg = NULL;
    while (true) 
    {
        // How many descpitors are ready for interaction
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) 
        {
            throw "epoll_wait";
        }

        // iterating through ready descriptors
        for (int n = 0; n < nfds; ++n) 
        {
            // new connection
            if (events[n].data.fd == listen_sock) 
            {
                manage_connection();
            } 
            else // incoming data 
            {
                int id = id_by_fd(events[n].data.fd, users);
                msg = read_from_user(users[id]);
                if (msg != NULL) // user sent a message
                {
                    send_message(msg, users);
                    print_log(msg->text);
                    delete msg;
                }
                else // user closed connection
                {
                    manage_disconnect(users[id]);
                }
            }
        } 
    }
}

// MAIN ==============================================================

void handler(int)
{
    signal(SIGINT, handler);
    printf("\n");
    throw 1;
}

int main()
{
    signal(SIGINT, handler);

    try
    {
        Server server;
        server.manage_chat();
    }
    catch (const char *error)
    {
        printf("Error occured in: %s\n", error);
    }
    catch (int s) { }

    return 0;
}
