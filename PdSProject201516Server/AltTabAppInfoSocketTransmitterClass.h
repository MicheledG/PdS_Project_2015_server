#pragma once
#include <cpprest\json.h>
#include "AltTabAppMonitorClass.h"

using namespace web;

class AltTabAppInfoSocketTransmitterClass
{
	const PCSTR DEFAULT_PORT = "27015";
	std::atomic<bool> active; //describe if the server application is still active
	void manageListeningSocket();
	std::thread listeningSocketThread;
	AltTabAppMonitorClass* monitor;
	std::vector<std::shared_ptr<std::thread>> threadVector;
	void serveClient(SOCKET clientSocket, AltTabAppMonitorClass* monitor, std::atomic<bool>* active);
	json::value fromAltTabAppObjToJsonObj(AltTabAppClass altTabAppObj);
	std::tstring fromPNGToBase64(std::shared_ptr<byte> pPNGByte, int iPNGSize);
public:
	AltTabAppInfoSocketTransmitterClass(AltTabAppMonitorClass* monitor);
	~AltTabAppInfoSocketTransmitterClass();
	void start();
	void stop();
};

