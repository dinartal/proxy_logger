#ifndef PROXY_LOGGER_H
#define PROXY_LOGGER_H

#include <map>

class ProxyLogger
{
	public:
		virtual ~ProxyLogger() {}
		virtual void init() = 0;
        virtual void loop() = 0;
		virtual void log(const char* data, int len) = 0;
	protected:
        int ServerSockFd;
        int fdmax;
		std::map<int, int> ProxyPair;
};

#endif
