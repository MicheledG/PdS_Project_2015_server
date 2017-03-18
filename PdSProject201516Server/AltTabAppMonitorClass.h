#pragma once

#include "stdafx.h"
#include "AltTabAppClass.h"

typedef enum NOTIFICATION_EVENT {
	APP_CREATE,
	APP_DESTROY,
	APP_FOCUS
} notification_event_type;

class AltTabAppMonitorClass
{
	const static int CHECK_WINDOW_INTERVAL = 1000; //ms
	std::atomic<bool> active; //describe if the server application is still active
	std::map<HWND, AltTabAppClass> map;
	HWND focusAppId;
	notification_event_type notificationEvent;
	HWND notificationAppId;
	void monitor();
	void getActualOpenedApp(std::map<HWND, AltTabAppClass>* appMap, HWND* focusAppId);
	static BOOL CALLBACK HandleWinDetected(HWND hWnd, LPARAM ptr);
	std::thread monitorThread;
public:
	std::mutex mapMutex;
	std::mutex eventQueueMutex;
	std::condition_variable newEventInQueue;
	std::deque<std::pair<notification_event_type, HWND>> eventQueue;
	AltTabAppMonitorClass();
	~AltTabAppMonitorClass();
	void start();
	void stop(); //wait the end of the monitor thread
	std::vector<AltTabAppClass> getAltTabAppVector(); //used to read the map value -> NO THREAD SAFE, USE THE MUTEX
};

