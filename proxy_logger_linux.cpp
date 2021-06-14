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
    ClientFdSetV.insert(ServerSockFd);

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(5432);
    sa.sin_addr.s_addr = inet_addr("127.0.0.1");

    memcpy(&PosgreAddr, &sa, sizeof sa);
}

void ProxyLoggerLinux::loop()
{
    // select modify tv, so set it before each call
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    ClientFdSet = ServerFdSet;
    switch (select(fdmax + 1, &ClientFdSet, NULL, NULL, &tv))
    {
    case -1:
        throw std::runtime_error("ERROR on select: " + std::string(std::strerror(errno)));
        break;
    case 0:
        std::cout << "select timeout "<< "10" <<"s.." << std::endl;
        break;
    default:
        break;
    }

    for(int i = 0; i <= fdmax; i++)
    //auto it = ClientFdSetV.begin();
    //while (it != ClientFdSetV.end())
    //for (auto i : ClientFdSetV)  //maybe better solution, but segfault occure
    //for (it = ClientFdSetV.begin(); it != ClientFdSetV.end(); it++)
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
                    auto ProxySockFd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
                    if (ProxySockFd < 0)
                    {
                        throw std::runtime_error("ERROR opening socket: " + std::string(std::strerror(errno)));
                    }
                    else
                    {
                        std::cout << "Proxy socket created, ProxySockFd: " << ProxySockFd << std::endl;
                    }
                    if (connect(ProxySockFd, &PosgreAddr, sizeof(PosgreAddr)) == 0)
                    {
                        // connected
                        std::cout << "Connected" << std::endl;

                        // add descriptor to ServerFdSet
                        FD_SET(newfd, &ServerFdSet);
                        FD_SET(ProxySockFd, &ServerFdSet);

                        ClientFdSetV.insert(newfd);
                        ClientFdSetV.insert(ProxySockFd);
                        ProxyPair.insert(std::pair<int, int>(newfd, ProxySockFd));
                        //std::map<int, int>::iterator it = ProxyPair.find(i);
                        //if (it != ProxyPair.end()) it->second = newfd;

                        if (newfd > fdmax)
                        {
                            // update max descriptor number
                            fdmax = newfd;
                        }
                        if (ProxySockFd > fdmax)
                        {
                            // update max descriptor number
                            fdmax = ProxySockFd;
                        }

                        std::cout << "selectserver: new connection from: " << inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET6_ADDRSTRLEN)
                            << ", on socket: " << newfd << std::endl;
                    }
                    else
                    {
                        std::cout << "Connect error. Close connected socket." << std::endl;
                        close(newfd);
                    }
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

                        //ClientFdSetV.erase(std::remove(ClientFdSetV.begin(), ClientFdSetV.end(), i), ClientFdSetV.end());
                        //ClientFdSetV.erase()
                    }
                    else
                    {
                        std::cout << "receive error" << std::endl;
                    }
                    close(i);
                    FD_CLR(i, &ServerFdSet);
                    ClientFdSetV.erase(i);
                    if (ProxyPair.find(i) == ProxyPair.end())
                    {
                        std::cout << "not found key in map" << std::endl;

                        bool search_by_val = false;

                        // search by value
                        for (auto it = ProxyPair.begin(); it != ProxyPair.end(); ++it)
                        {
                            if (it->second == i)
                            {
                                std::cout << "found val in map" << std::endl;
                                close(ProxyPair[it->first]);
                                FD_CLR(ProxyPair[it->first], &ServerFdSet);
                                ClientFdSetV.erase(it->first);
                                ProxyPair.erase(it->first);
                                search_by_val = true;
                                break;
                            }
                        }

                        if (!search_by_val)
                        {
                            std::cout << "ERROR: cant found recieved data socket" << std::endl;
                        }
                        // not found
                    }
                    else
                    {
                        std::cout << "found key in map, FD_CLR" << std::endl;
                        close(ProxyPair[i]);
                        FD_CLR(ProxyPair[i], &ServerFdSet);
                        ClientFdSetV.erase(ProxyPair[i]);
                        ProxyPair.erase(i);
                    }
                    
                    fdmax = *(ClientFdSetV.rbegin());
                }
                else
                {
                    log(buf, nbytes);
                    if (ProxyPair.find(i) == ProxyPair.end())
                    {
                        std::cout << "not found" << std::endl;
                        // not found

                        // search by value
                        bool search_by_val = false;
                        for (auto it = ProxyPair.begin(); it != ProxyPair.end(); ++it)
                        {
                            if (it->second == i)
                            {
                                std::cout << "found val in map" << std::endl;
                                if (send(ProxyPair[it->first], buf, nbytes, 0) == -1)
                                {
                                    std::cout << "send error" << std::endl;
                                }
                                else
                                {
                                    std::cout << "send response" << std::endl;
                                }
                                search_by_val = true;
                                break;
                            }
                        }
                        if (!search_by_val)
                        {
                            std::cout << "ERROR: cant found recieved data socket" << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << "found" << std::endl;

                        if (send(ProxyPair[i], buf, nbytes, 0) == -1)
                        {
                            std::cout << "send error" << std::endl;
                        }
                        else
                        {
                            std::cout << "send echo" << std::endl;
                        }
                        // found
                    }

                    // we have some data from client
                    // send it back
                    /*if (send(i, buf, nbytes, 0) == -1)
                    {
                        std::cout << "send error" << std::endl;
                    }
                    else
                    {
                        std::cout << "send echo" << std::endl;
                    }*/
                }
            } // end handle data from client
        } // end event handle
    } // end for loop
}

ProxyLoggerLinux::~ProxyLoggerLinux()
{
    close(ServerSockFd);
}

void ProxyLoggerLinux::log(const char* data, int len)
{

}