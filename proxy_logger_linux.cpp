#include "proxy_logger_linux.h"
#include <iostream>

#include <sys/socket.h>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void ProxyLoggerLinux::init()
{
    tv.tv_sec = 2;
    tv.tv_usec = 500000;
    
	ServerSockFd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (ServerSockFd < 0)
	{
		throw std::runtime_error("ERROR opening socket: " + std::string(std::strerror(errno)));
	}
	else
	{
		std::cout << "Server socket created, ServerSockFd: " << ServerSockFd << std::endl;
	}

    int yes = 1;
    if (setsockopt(ServerSockFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
    {
        throw std::runtime_error("ERROR on setsockopt SO_REUSEADDR: " + std::string(std::strerror(errno)));
    }
    else
    {
        std::cout << "Server socket option REUSEADDR applied" << std::endl;
    }
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(ServerSockFd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0)
    {
        throw std::runtime_error("ERROR on setsockopt SO_RCVTIMEO: " + std::string(std::strerror(errno)));
    }
    else
    {
        std::cout << "Server socket option RCVTIMEO applied: " << tv.tv_sec << "s" << std::endl;
    }
	
	struct sockaddr_in serv_addr;
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(5430);
	if (bind(ServerSockFd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		throw std::runtime_error("ERROR on binding. Pid: " + std::string(std::strerror(errno)));
	}
    else
    {
        std::cout << "Server socket binded" << std::endl;
    }
    
	if (listen(ServerSockFd, 5) < 0)
	{
		throw std::runtime_error("ERROR on listen: " + std::string(std::strerror(errno)));
	}
    else
    {
        std::cout << "Server socket listen" << std::endl;
    }
    
    FD_ZERO(&ServerFdSet);
    FD_ZERO(&ClientFdSet);
    
    FD_SET(ServerSockFd, &ServerFdSet);
    
    // we have only one socket at this point, so initialize max value by it
    fdmax = ServerSockFd;
    ClientFdSetV.push_back(ServerSockFd);
}

void ProxyLoggerLinux::loop()
{
    ClientFdSet = ServerFdSet;
    if(select(fdmax+1, &ClientFdSet, NULL, NULL, &tv) == -1)
    {
        throw std::runtime_error("ERROR on select");
    }
    
    for(int i = 0; i <= fdmax; i++)
    //for (auto i : ClientFdSetV)  //maybe better solution
    {
        if (FD_ISSET(i, &ClientFdSet)) 
        {   
            // we have event and need to handle it
            if (i == ServerSockFd)
            {
                // handle new connection
                addrlen = sizeof remoteaddr;
                newfd = accept(ServerSockFd, (struct sockaddr *)&remoteaddr, &addrlen);

                if (newfd == -1)
                {
                    std::cout << "Accept error" << std::endl;
                }
                else
                {
                    // add descriptor to ServerFdSet
                    FD_SET(newfd, &ServerFdSet);
                    auto ProxySockFd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
                    if (connect(ProxySockFd, &PosgreAddr, sizeof(PosgreAddr)) == 0)
                    {
                        // connected
                        std::cout << "Connected" << std::endl;
                    }
                    else
                    {
                        throw std::runtime_error("ERROR on connect: " + std::string(std::strerror(errno)));
                    }

                    ClientFdSetV.push_back(newfd);
                    if (newfd > fdmax)
                    {   
                        // update max descriptor number
                        fdmax = newfd;
                    }
                    std::cout << "selectserver: new connection from: " << inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET6_ADDRSTRLEN) 
                    << ", on socket: " << newfd << std::endl;
                }
            }
            else
            {
                // handle data from client
                if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0)
                {
                    if (nbytes == 0)
                    {
                        // connection closed
                        std::cout << "selectserver: socket " << i << " hung upn" << std::endl;
                    }
                    else
                    {
                        std::cout << "receive error" << std::endl;
                    }
                    close(i);
                    FD_CLR(i, &ServerFdSet);
                }
                else
                {
                    // we have some data from client
                    // send it back
                    if (send(i, buf, nbytes, 0) == -1)
                    {
                        std::cout << "send error" << std::endl;
                    } 
                }
            } // end handle data from client
        } // end event handle
    } // end for loop
}

ProxyLoggerLinux::~ProxyLoggerLinux()
{
    close(ServerSockFd);
}
