#ifndef PROXY_LOGGER_H
#define PROXY_LOGGER_H

class ProxyLogger
{
	public:
		virtual ~ProxyLogger() {}
		virtual void init() = 0;
        virtual void loop() = 0;
        int ServerSockFd;
        int fdmax;
};



#endif
