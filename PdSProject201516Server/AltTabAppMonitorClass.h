#pragma once

#include "stdafx.h"
#include "AltTabAppClass.h"

class AltTabAppMonitorClass
{
	std::map<HWND, AltTabAppClass> map;
	std::mutex mapMutex;
	std::atomic<bool> active; //describe if the server application is still active
	void monitor();
	static BOOL CALLBACK HandleWinDetected(HWND hWnd, LPARAM ptr);
	const static int CHECK_WINDOW_INTERVAL = 1000; //ms
	std::thread monitorThread;
public:
	AltTabAppMonitorClass();
	~AltTabAppMonitorClass();
	void start();
	void stop(); //wait the end of the monitor thread
	std::vector<AltTabAppClass> getAltTabAppVector();
	bool mapContainsHwnd(HWND hwnd);
};

