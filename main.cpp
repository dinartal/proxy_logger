#include "proxy_logger_linux.h"
int main (int argc, char** argv)
{
	proxylog::ProxyLoggerLinux p;
	p.init();
	while(true)
	{
		p.loop();
	}

	return 0;
}