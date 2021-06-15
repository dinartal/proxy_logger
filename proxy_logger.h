#pragma once

#include <map>
namespace proxylog
{
	class ProxyLogger
	{
	public:
		virtual ~ProxyLogger() {}
		virtual void loop() = 0;
		virtual void log(const char* data, int len) = 0;
	};
}