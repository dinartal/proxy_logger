#ifndef PROXY_LPGGER_LINUX
#define PROXY_LOGGER_LINUX

#include "proxy_logger.h"
#include <sys/select.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/in.h>

class ProxyLoggerLinux : private ProxyLogger
{
	public:
		virtual void init();
        virtual void loop();
        ~ProxyLoggerLinux();
    
    private:
        fd_set ServerFdSet;
        fd_set ClientFdSet;
        struct timeval tv;
        int newfd;        // descriptor for new connections after accept()
        struct sockaddr_storage remoteaddr; // client addr
        socklen_t addrlen;
        char buf[256];    // client data buffer
        int nbytes;
        char remoteIP[INET6_ADDRSTRLEN];
};


#endif
