#include "stdafx.h"
#include "AltTabAppMonitor.h"
#include "AltTabApp.h"

//define global variable
std::map<HWND, AltTabApp> mapWndAltTabApp;
std::mutex mMapWndAltTabApp;
std::condition_variable cvMapChanged;
event_type notifyEvent = event_type::NO_EVENT;
HWND hwndEvent = NULL; //value on 4 bytes to identify the window
std::atomic<bool> active(true);

HWND hwndLastFocus = NULL;
HWINEVENTHOOK EventHookFocus;

/* make a first search of the already opened windows
   and install the WinEvent hook to detect window creation, destruction and focus change */
void InitAltTabAppMonitor()
{	
	
	//group already opened Alt-Tab windows
 	BOOL result = EnumWindows(AddAltTabAppInMap, NULL);
	if (result == FALSE) return;

	//initialize first window with focus
	hwndLastFocus = GetForegroundWindow();
	if (hwndLastFocus != NULL && mapWndAltTabApp.find(hwndLastFocus) != mapWndAltTabApp.end())
		mapWndAltTabApp.find(hwndLastFocus)->second.SetFocus(true);

	//install the win event hook procedure
	EventHookFocus = SetWinEventHook(
		EVENT_MIN, //MIN EVENT NUMBER CHECKED
		EVENT_MAX, //MAX EVENT NUMBER CHECKED
		NULL,
		HandleWinEvent,
		0, 0, //all threads all processes
		WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS | WINEVENT_SKIPOWNTHREAD);

	DWORD error = GetLastError();

	return;
}

/* add a valid alt-tab window to the map */
BOOL CALLBACK AddAltTabAppInMap(HWND hWnd, LPARAM ptr)
{
	std::lock_guard<std::mutex> lg(mMapWndAltTabApp);
	//check if it is window already into map (faster operation)
	if (mapWndAltTabApp.find(hWnd) != mapWndAltTabApp.end()) return TRUE;
	//check if is an Alt-Tab window (more power and resource consuming)
	AltTabApp app(hWnd);
	if (app.GetwHnd() == NULL) return TRUE;

	//add to the map
	mapWndAltTabApp.insert(pairWndAltTabApp(hWnd, app));
	notifyEvent = event_type::APP_CREATION_EVENT;
	hwndEvent = hWnd;
	cvMapChanged.notify_one();

	return TRUE;
}

/* detect the win events: create, destroy and focus */
void CALLBACK HandleWinEvent(
	HWINEVENTHOOK hWinEventHook,
	DWORD         dwEvent,
	HWND          hwnd,
	LONG          idObject,
	LONG          idChild,
	DWORD         dwEventThread,
	DWORD         dwmsEventTime)
{
	std::unique_lock<std::mutex> ul(mMapWndAltTabApp);

	switch (dwEvent)
	{
		case EVENT_OBJECT_CREATE: //add the new window to the map
			if (idObject == OBJID_WINDOW && idChild == CHILDID_SELF)
				//check if it is a missing window into map
				if (mapWndAltTabApp.find(hwnd) == mapWndAltTabApp.end()) {
					//update map of opened window
					ul.unlock();
					EnumWindows(AddAltTabAppInMap, NULL);
				}
			break;
		case EVENT_OBJECT_DESTROY: //delete the window from the map
			if (idObject == OBJID_WINDOW && idChild == CHILDID_SELF) {
				//window destruction event
				//check if window is into the map
				if (mapWndAltTabApp.find(hwnd) != mapWndAltTabApp.end()) {
					mapWndAltTabApp.erase(hwnd);
					notifyEvent = event_type::APP_DESTRUCTION_EVENT;
					hwndEvent = hwnd;
					cvMapChanged.notify_one();
				}	
			}	
			break;
		case EVENT_SYSTEM_FOREGROUND:
			//foreground window changed
			//check if window is into the map
			if (mapWndAltTabApp.find(hwnd) != mapWndAltTabApp.end()) {
				//if last focus window is different from the gaining window focus (hwnd) and last focus window is still into the map
				if (hwndLastFocus != hwnd && mapWndAltTabApp.find((HWND)hwndLastFocus) != mapWndAltTabApp.end())
					mapWndAltTabApp.find(hwndLastFocus)->second.SetFocus(false);
				//set focus to the gaining window
				mapWndAltTabApp.find(hwnd)->second.SetFocus(true);
				//update last focus
				hwndLastFocus = hwnd;
				notifyEvent = event_type::FOCUS_CHANGE_EVENT;
				hwndEvent = hwnd;
				cvMapChanged.notify_one();
			}
			break;
		default:
			break;
	}

	return;
}

void StopAltTabAppMonitor() {
	/* release all the resources!!! */
	//TODO: uninstall the hook!
	std::lock_guard<std::mutex> lg(mMapWndAltTabApp);
	mapWndAltTabApp.clear();
}