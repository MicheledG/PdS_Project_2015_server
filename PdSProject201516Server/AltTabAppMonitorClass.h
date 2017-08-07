#pragma once
#include "stdafx.h"
#include "AltTabAppClass.h"

typedef enum NOTIFICATION_EVENT {
	APP_CREATE,
	APP_DESTROY,
	APP_FOCUS,
	STOP
} notification_event_type;

typedef struct NotificationStruct {
	notification_event_type notificationEvent;
	std::list<HWND> appIdList;
	std::list<AltTabAppClass> appList;
} Notification;

class AltTabAppMonitorClass
{
	const static int CHECK_WINDOW_INTERVAL = 500; //ms
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
	std::mutex notificationQueueMutex;
	std::atomic<bool> transmitterConnectedToNotificationQueue;
	std::condition_variable newNotificationInQueue;
	std::deque<Notification> notificationQueue;
	AltTabAppMonitorClass();
	~AltTabAppMonitorClass();
	void start();
	void stop(); //wait the end of the monitor thread
	std::vector<AltTabAppClass> getAltTabAppVector(); //used to read the map value -> NO THREAD SAFE, USE THE MUTEX
};

