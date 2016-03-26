#include "client.h"

Client::Client()
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        throw "socket";
    }

    fds.clear();
    fds.push_back(sockfd);
    fds.push_back(0);

}

Client::~Client()
{ 
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    printf("Client closed\n");
}

void Client::connect_to_server(const char *ip, int port)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (!inet_aton(ip, &(addr.sin_addr)))
    {
        throw "inet_aton";
    }

    if (0 != connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)))
    {
        throw "connect";
    }
}

void Client::send_message()
{
    char buff[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; i++)
        buff[i] = '\0';

    fgets(buff, BUFFER_SIZE, stdin);
    send(sockfd, buff, BUFFER_SIZE, 0);  

}

int Client::recieve_message(int fd)
{
    char buff[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; i++)
        buff[i] = '\0';

    int bytes_recieved = 
        recv(fd, &buff, BUFFER_SIZE, 0);
    if (bytes_recieved > 0)
    {
        buff[bytes_recieved] = '\0';
        printf("%s", buff);
    }
    else if (bytes_recieved == 0)
    {
        printf("Server closed\n");
        return 1;
    }
    else
    {
        throw "recv";
    }

    return 0;
}

void Client::use_chat()
{
    while (true)
    {
        fd_set readfds;
        int max_d = sockfd;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        
        for (size_t i = 0; i < fds.size(); i++)
        {
            FD_SET(fds[i], &readfds);
            if (fds[i] > max_d) 
                max_d = fds[i];
        }
        
        int res = select(max_d+1, &readfds, NULL, NULL, NULL);
        if (res < 1)
        {
            throw "select";
        }
        
        for (size_t i = 0; i < fds.size(); i++)
        {
            if (FD_ISSET(fds[i], &readfds))
            {
                if (fds[i] != 0)
                {
                    int state = recieve_message(fds[i]);
                    if (state != 0)
                        return;
                }
                else
                {
                    send_message();
                }
            }
        }            
    }   
}

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
        Client client;
        client.connect_to_server("127.0.0.1", 3100);
        client.use_chat();
    }
    catch (const char *err)
    {
        printf("Error occured in: %s\n", err);
    }
    catch(int s) { }

    return 0;
}
