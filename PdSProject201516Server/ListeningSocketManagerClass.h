#pragma once
#include "AltTabAppMonitorClass.h"

class ListeningSocketManagerClass
{
	const PCSTR DEFAULT_PORT = "27015";
	std::atomic<bool> active; //describe if the server application is still active
	void manageListeningSocket();
	std::thread listeningSocketThread;
	AltTabAppMonitorClass* monitor;
	std::vector<std::shared_ptr<int>> clientThreadVector;
public:
	ListeningSocketManagerClass(AltTabAppMonitorClass* monitor);
	~ListeningSocketManagerClass();
	void start();
	void stop();
};

