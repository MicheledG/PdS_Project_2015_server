#pragma once
#include "AltTabAppMonitorClass.h"
#include <cpprest\http_listener.h>

using namespace web::http::experimental::listener;
using namespace web::http;

class AltTabAppInfoTransmitterClass
{
	http_listener listener;
	AltTabAppMonitorClass* monitor;
	//std::thread communicationThread;
	//std::atomic<bool> active;
	void test(http_request request);
	const static int DEFAULT_PORT = 27015;
public:
	AltTabAppInfoTransmitterClass(AltTabAppMonitorClass* monitor);
	~AltTabAppInfoTransmitterClass();
	void setMonitor(AltTabAppMonitorClass* monitor);
	void start();
	void stop();
};

