#ifndef PROXY_LPGGER_LINUX
#define PROXY_LOGGER_LINUX

#include "proxy_logger.h"
#include <sys/select.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <set>

class ProxyLoggerLinux : private ProxyLogger
{
public:
	virtual void init();
	virtual void loop();
	virtual void log(const char* data, int len);
	~ProxyLoggerLinux();

	int ClientPort = 5430;
	int ServerPort = 5432;
	std::string ServerAddr = "127.0.0.1";
	int ClientCntMax = 100;


private:
	int check(int ret, std::string msg);
	int ServerSockFd;
	int fdmax;
	std::map<int, int> ProxyPair;
	fd_set ServerFdSet;
	fd_set ClientFdSet;
	std::set<int> ClientFdSetV;
	//std::vector<int>::iterator it;
	struct timeval tv;
	int newfd;        // descriptor for new connections after accept()
	struct sockaddr_storage remoteaddr; // client addr
	socklen_t addrlen;
	char buf[256];    // client data buffer
	int nbytes;
	char remoteIP[INET6_ADDRSTRLEN];
	struct sockaddr PosgreAddr;
};


#endif
