// Архитектура: Linux + libevent
// Формат файла конфигурации:
// Одна строка вида "<listen_port>, <ip1:port1>, <ip2:port2>..."

#include <iostream>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <arpa/inet.h>
#include <event.h>

int random_int(int min, int max)
{
    return min + (rand() % (int)(max - min + 1));
}

struct Destination
{
    char *ip;
    int port;

    Destination(char buff[1024], int _port)
    {
        ip = new char[strlen(buff)]; 
        int i = 0;
        while (buff[i])
        {
            ip[i] = buff[i];
            i++;
        }
        ip[i] = '\0';

        port = _port;
    }
    
    ~Destination()
    {
        delete [] ip;
    }
    
    void show()
    {
        std::cout << ip << ':' << port << std::endl;
    } 
};

#define N_SERV 128
struct Config
{
    int proxy_port;
    Destination *servers[N_SERV];
    int n;

    Config()
    {
        proxy_port = -1;
        for (int i = 0; i < N_SERV; i++)
            servers[i] = NULL;
        n = 0;
    }

    Config(const char *filename)
    {
        for (int i = 0; i < N_SERV; i++)
            servers[i] = NULL;
     
        char buff[1024];
        char res[1024];  
         
        FILE *f = fopen(filename, "r");
        if (f == NULL)
            throw "fopen()";
                   
        // read proxy port
        fscanf(f, "%d", &proxy_port);
        fscanf(f, ",");
        n = 0;   
        int ip0, ip1, ip2, ip3, port;

        // read servers info
        while ((fscanf(f, " %[^ ]", buff) != 0) && (strlen(buff) > 1))
        {
            sscanf(buff, "%d.%d.%d.%d:%d", 
                   &ip0, &ip1, &ip2, &ip3, &port);
            
            sprintf(res, "%d.%d.%d.%d%c", 
                    ip0, ip1, ip2, ip3, '\0');
            
            servers[n] = new Destination(res, port);
            n++;
               
            buff[0] = '\0';

            fscanf(f, ",");
        }
         
        fclose(f);
    }       
         
    ~Config()
    {
        for (int i = 0; i < n; i++)
            delete servers[i];
    }

    void show()
    {
        std::cout << "Proxy port: " << proxy_port << std::endl;
        std::cout << "Servers count: " << n << std::endl;
        for (int i = 0; i < n; i++)
            servers[i]->show();
    }    
    
    void random_server(char **_ip, int *_port)
    {
        int i = random_int(0, n-1);
        *_ip = servers[i]->ip;
        *_port = servers[i]->port;
    }
        
} *cfg;

int set_nonblock(int fd)
{
	int flags;
#if defined(O_NONBLOCK)
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
} 

struct Session
{
    struct bufferevent *ev_cl_read;
    struct bufferevent *ev_sv_read;
    int sock_sv;
    int sock_cl;

    bool cl_closed;
    bool sv_closed;

    int closed_sides;
};

// CALLBACK FUNCTIONS ===============================================

void special_cb(struct bufferevent *event, short ev, void *arg)
{
    if (ev & BEV_EVENT_EOF)
    {
        Session *ssn = (Session*)arg;
        struct evbuffer *buf_in = bufferevent_get_input(event);
        struct evbuffer *buf_out;
        int remain_fd;
        if (event == ssn->ev_cl_read)
        {
            remain_fd = ssn->sock_sv;
            buf_out = bufferevent_get_output(ssn->ev_sv_read);
        }
        else
        {
            remain_fd = ssn->sock_cl;
            buf_out = bufferevent_get_output(ssn->ev_cl_read);
        }

        evbuffer_remove_buffer(buf_in, buf_out, 
                               evbuffer_get_length(buf_in));
        shutdown(remain_fd, SHUT_WR);
        ssn->closed_sides++;
        if (ssn->closed_sides == 2)
        {
            shutdown(ssn->sock_sv, SHUT_RDWR);
            shutdown(ssn->sock_cl, SHUT_RDWR);
            close(ssn->sock_sv);
            close(ssn->sock_cl);
            bufferevent_free(ssn->ev_sv_read);
            bufferevent_free(ssn->ev_cl_read);
            delete ssn;
        }
    }
    else if (ev & BEV_EVENT_ERROR)
    {
        std::cout << "<Error>" << std::endl;
        Session *ssn = (Session*)arg;
        bufferevent_free(ssn->ev_sv_read);
        bufferevent_free(ssn->ev_cl_read);
        delete ssn;
    }
}

// ------------------------------------------------------------------

void read_cb(struct bufferevent *event, void *arg)
{
    Session *ssn = (Session*)arg;

    struct evbuffer *buf_in = bufferevent_get_input(event);
    struct evbuffer *buf_out;
    if (event == ssn->ev_cl_read)
        buf_out = bufferevent_get_output(ssn->ev_sv_read);
    else
        buf_out = bufferevent_get_output(ssn->ev_cl_read);

    evbuffer_remove_buffer(buf_in, buf_out, 
                           evbuffer_get_length(buf_in));
}

// ------------------------------------------------------------------

void on_accept(int fd, short ev, void *arg)
{
	struct event_base *base = (struct event_base*)arg;
	int SlaveSocket = accept(fd, 0, 0);
	set_nonblock(SlaveSocket);

	Session *ssn;
    ssn = new Session();
    ssn->sock_cl = SlaveSocket;

    ssn->sock_sv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    set_nonblock(ssn->sock_sv);

    char *ip;
    int port;
    cfg->random_server(&ip, &port);

	struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_aton(ip, &(serv_addr.sin_addr));

	ssn->ev_sv_read = bufferevent_socket_new(base, ssn->sock_sv, 
                                             BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(ssn->ev_sv_read, read_cb, NULL, 
                      special_cb, ssn);

    int con = 
        bufferevent_socket_connect(ssn->ev_sv_read, 
                                   (struct sockaddr *)&serv_addr, 
                                   sizeof(serv_addr));
    if (con < 0)
        throw "connect()";

    ssn->ev_cl_read = bufferevent_socket_new(base, ssn->sock_cl,
                                             BEV_OPT_CLOSE_ON_FREE);

    bufferevent_setcb(ssn->ev_cl_read, read_cb, NULL, 
                      special_cb, ssn);

    bufferevent_enable(ssn->ev_cl_read, 
                       EV_READ | EV_WRITE | EV_PERSIST);

    bufferevent_enable(ssn->ev_sv_read, 
                       EV_READ | EV_WRITE | EV_PERSIST);

    ssn->cl_closed = false;
    ssn->sv_closed = false;
    ssn->closed_sides = 0;
}

// ==================================================================

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Wrong command line" << std::endl;
        return 1;
    }
    else try
    {
        cfg = new Config(argv[1]);
        cfg->show();
        srand(time(NULL));

        event_init();

        struct event_base *base = event_base_new();
        int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(MasterSocket == -1)
            throw "socket()";

        struct sockaddr_in SockAddr;
        SockAddr.sin_family = AF_INET;
        SockAddr.sin_port = htons(cfg->proxy_port);
        SockAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        int opt = 1;
        setsockopt(MasterSocket, SOL_SOCKET, SO_REUSEADDR, 
                   &opt, sizeof(opt));

        int Result = bind(MasterSocket, 
                          (struct sockaddr *)&SockAddr, 
                          sizeof(SockAddr));
        if(Result == -1)
            throw "bind()";
        
        set_nonblock(MasterSocket);

        Result = listen(MasterSocket, SOMAXCONN);
        if(Result == -1)
            throw "listen()";

        struct event *ev_accept = event_new(base, MasterSocket, 
                                            EV_READ | EV_PERSIST, 
                                            on_accept,
                                            base);
        event_add(ev_accept, NULL);
        event_base_loop(base, 0);   
    }
    catch(const char *err)
    {
            std::cout << "Error occured in: " << err << std::endl;
    }

    delete cfg;
	return 0;
}
