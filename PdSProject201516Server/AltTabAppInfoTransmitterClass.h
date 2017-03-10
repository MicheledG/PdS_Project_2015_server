#pragma once
#include "AltTabAppMonitorClass.h"
#include <cpprest\http_listener.h>
#include <cpprest\json.h>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;


class AltTabAppInfoTransmitterClass
{
	AltTabAppMonitorClass* monitor;
	http_listener listener;
	static const std::tstring URL;
	void handle_get(http_request request);
	json::value fromAltTabAppObjToJsonObj(AltTabAppClass altTabAppObj);
	std::tstring fromPNGToBase64(AltTabAppClass altTabAppObj);
public:
	AltTabAppInfoTransmitterClass(AltTabAppMonitorClass* monitor);
	~AltTabAppInfoTransmitterClass();
	void setMonitor(AltTabAppMonitorClass* monitor);
	void start();
	void stop();
};

