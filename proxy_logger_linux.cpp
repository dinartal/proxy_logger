#include "proxy_logger_linux.h"
#include <iostream>

#include <sys/socket.h>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <sstream>

namespace proxylog
{
	// get sockaddr, IPv4 or IPv6:
	void* get_in_addr(struct sockaddr* sa)
	{
		if (sa->sa_family == AF_INET)
		{
			return &(((struct sockaddr_in*)sa)->sin_addr);
		}
		return &(((struct sockaddr_in6*)sa)->sin6_addr);
	}

	ProxyLoggerLinux::ProxyLoggerLinux()
	{
		ServerSockFd = check(socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0), "ERROR opening socket: ");

		int SockOptionVal = 1;
		check(setsockopt(ServerSockFd, SOL_SOCKET, SO_REUSEADDR, &SockOptionVal, sizeof(SockOptionVal)), "ERROR on setsockopt SO_REUSEADDR: ");

		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		check(setsockopt(ServerSockFd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv), "ERROR on setsockopt SO_RCVTIMEO: ");

		struct sockaddr_in serv_addr;
		bzero((char*)&serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(ClientPort);
		check(bind(ServerSockFd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "ERROR on binding: ");

		check(listen(ServerSockFd, ClientCntMax), "ERROR on listen: ");
		std::cout << "Server socket listen" << std::endl;

		FD_ZERO(&ServerFdSet);
		FD_ZERO(&ClientFdSet);

		FD_SET(ServerSockFd, &ServerFdSet);

		// we have only one socket at this point, so initialize max value by it
		fdmax = ServerSockFd;
		ClientFdSetV.insert(ServerSockFd);

		struct sockaddr_in sa;
		sa.sin_family = AF_INET;
		sa.sin_port = htons(ServerPort);
		sa.sin_addr.s_addr = inet_addr(ServerAddr.data());

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
			std::cout << "select timeout " << "10" << "s.." << std::endl;
			break;
		default:
			break;
		}

		for (int i = 0; i <= fdmax; i++)
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
					newfd = accept(ServerSockFd, (struct sockaddr*)&remoteaddr, &addrlen);

					if (newfd == -1)
					{
						std::cout << "Accept error" << std::endl;
					}
					else
					{
						auto ProxySockFd = check(socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0), "ERROR opening socket: ");
						std::cout << "Proxy socket created, ProxySockFd: " << ProxySockFd << std::endl;

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
							std::cout << "Select server: socket " << i << " hung up" << std::endl;
						}
						else
						{
							std::cout << "Receive error" << std::endl;
						}
						close(i);
						FD_CLR(i, &ServerFdSet);
						ClientFdSetV.erase(i);
						if (ProxyPair.find(i) == ProxyPair.end())
						{
							std::cout << "Not found key in map" << std::endl;

							bool search_by_val = false;

							// search by value
							for (auto it = ProxyPair.begin(); it != ProxyPair.end(); ++it)
							{
								if (it->second == i)
								{
									std::cout << "Found val in map" << std::endl;
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
						}
						else
						{
							std::cout << "Found key in map, FD_CLR" << std::endl;
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
							std::cout << "Not found by key" << std::endl;

							// search by value
							bool search_by_val = false;
							for (auto it = ProxyPair.begin(); it != ProxyPair.end(); ++it)
							{
								if (it->second == i)
								{
									if (send(ProxyPair[it->first], buf, nbytes, 0) == -1)
									{
										std::cout << "ERROR: can`t send data to pair" << std::endl;
									}
									else
									{
										std::cout << "Send data to pair ok" << std::endl;
									}
									search_by_val = true;
									break;
								}
							}
							if (!search_by_val)
							{
								std::cout << "ERROR: can`t found recieved data socket" << std::endl;
							}
						}
						else
						{
							if (send(ProxyPair[i], buf, nbytes, 0) == -1)
							{
								std::cout << "ERROR: can`t send data to pair" << std::endl;
							}
							else
							{
								std::cout << "Send data to pair ok" << std::endl;
							}
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

	void ProxyLoggerLinux::log(const char* data, int len)
	{

	}

	int ProxyLoggerLinux::check(int ret, const std::string_view msg)
	{
		if (ret < 0)
		{
			std::stringstream ss;
			ss << msg << " " << std::strerror(ret);
			throw std::runtime_error{ ss.str() };
		}
		else
		{
			return ret;
		}
	}
}