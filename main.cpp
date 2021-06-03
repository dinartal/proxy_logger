/*

#include <iostream>
#include <sys/socket.h>
#include <cstring>
#include <netinet/in.h>

int main(int argc, char** argv)
{
	int sockfd;
	sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (sockfd < 0)
	{
		throw std::runtime_error("ERROR opening socket: " + std::string(std::strerror(errno)));
	}
	else
	{
		std::cout << "sockfd: " << sockfd << std::endl;
	}
	
	struct sockaddr_in serv_addr;
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(5430);
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		throw std::runtime_error("ERROR on binding. Pid: " + std::string(std::strerror(errno)));
	}
	if (listen(sockfd, 5) < 0)
	{
		throw std::runtime_error("ERROR on listen: " + std::string(std::strerror(errno)));
	}

	while(1) {}	

	return 0;
}

*/

#include "proxy_logger_linux.h"
int main (int argc, char** argv)
{
	ProxyLoggerLinux p;
	p.init();

	return 0;

}
