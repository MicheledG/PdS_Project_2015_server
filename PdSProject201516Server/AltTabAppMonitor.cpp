#include "stdafx.h"
#include "AltTabAppMonitor.h"
#include "AltTabApp.h"
#include "MapHWNDAltTabApp.h"

#define WINDOW_CHECK_INTERVAL 1000 //ms

//define global variable
MapHWNDAltTabApp mapWndAltTabApp;
std::atomic<bool> active(true);
HWND hwndWithFocus = NULL;
//event_type notifyEvent = event_type::NO_EVENT;
//HWND hwndEvent = NULL; //value on 4 bytes to identify the window
//HWINEVENTHOOK EventHook;

void AltTabAppMonitor()
{
	
	while (active) {
		//TO DO: WARNING -> WHEN THE CLIENT MUST ACCESS MAP IN CONSISTEN STATE!
		std::unique_lock<std::mutex> ul(mapWndAltTabApp.mapMutex);
		//clear the map
		mapWndAltTabApp.clearMap();
		//find opened Alt-Tab windows
		BOOL result = EnumWindows(HandleWinDetected, NULL);
		if (result == FALSE) return;

		//initialize first focus
		hwndWithFocus = GetForegroundWindow();
		if (hwndWithFocus != NULL && mapWndAltTabApp.containsHWND(hwndWithFocus))
			mapWndAltTabApp.findHWND(hwndWithFocus)->second.SetFocus(true);

		DWORD error = GetLastError();
		//map in consistent state
		ul.unlock();

		std::this_thread::sleep_for(std::chrono::milliseconds(WINDOW_CHECK_INTERVAL));
	}
	
	mapWndAltTabApp.clearMap();

	return;
}

/* manage the window returned by EnumWindows -> insert in map or not */
BOOL CALLBACK HandleWinDetected(HWND hwnd, LPARAM ptr)
{
	/* check if the app is already into the map*/
	if (mapWndAltTabApp.containsHWND(hwnd))
		/*the app is already into the map*/
		return TRUE;

	/* create the new app to insert into the map*/
	AltTabApp newApp(hwnd);

	/* check if the creation went well*/
	if (newApp.GethWnd() == NULL)
		return TRUE;

	/* insert into the map */
	mapWndAltTabApp.insertAltTapApp(hwnd, newApp);
	return TRUE;
}


///* make a first search of the already opened windows
//and install the WinEvent hook to detect window creation, destruction and focus change */
//void InitAltTabAppMonitor()
//{
//	//group already opened Alt-Tab windows
//	BOOL result = EnumWindows(HandleWinDetected, NULL);
//	if (result == FALSE) return;
//
//	//initialize first focus
//	hwndLastFocus = GetForegroundWindow();
//	if (hwndLastFocus != NULL && mapWndAltTabApp.containsHWND(hwndLastFocus))
//		mapWndAltTabApp.findHWND(hwndLastFocus)->second.SetFocus(true);
//
//	//install the win event hook procedure
//	EventHook = SetWinEventHook(
//		EVENT_MIN, //MIN EVENT NUMBER CHECKED
//		EVENT_MAX, //MAX EVENT NUMBER CHECKED
//		NULL,
//		HandleWinEvent,
//		0, 0, //all threads all processes
//		WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS | WINEVENT_SKIPOWNTHREAD);
//
//	DWORD error = GetLastError();
//
//	return;
//}
//
//void StopAltTabAppMonitor() {
//	/* release all the resources!!! */
//	//TODO: uninstall the hook!
//	mapWndAltTabApp.clearMap();
//}
//
///* detect the win events: create, destroy and focus */
//void CALLBACK HandleWinEvent(
//	HWINEVENTHOOK hWinEventHook,
//	DWORD         dwEvent,
//	HWND          hwnd,
//	LONG          idObject,
//	LONG          idChild,
//	DWORD         dwEventThread,
//	DWORD         dwmsEventTime)
//{
//	switch (dwEvent)
//	{
//	case EVENT_OBJECT_CREATE: //creation event
//							  //if (idObject == OBJID_WINDOW && idChild == CHILDID_SELF)
//		HandleWinDetected(hwnd, NULL);
//		break;
//	case EVENT_OBJECT_DESTROY: //destruction event
//							   //if (idObject == OBJID_WINDOW && idChild == CHILDID_SELF) {
//							   /*check if the window is into the map*/
//		if (mapWndAltTabApp.containsHWND(hwnd))
//			/*remove app from the map*/
//			mapWndAltTabApp.eraseAltTabApp(hwnd);
//		//}
//		break;
//	case EVENT_SYSTEM_FOREGROUND: //foreground window changed event
//								  //check if gaining focus window is into the map
//		if (mapWndAltTabApp.containsHWND(hwnd)) {
//			//check if last focus window is different from the gaining focus window and if last focus window is still into the map
//			if (hwndLastFocus != hwnd && mapWndAltTabApp.containsHWND((HWND)hwndLastFocus))
//				mapWndAltTabApp.findHWND(hwndLastFocus)->second.SetFocus(false);
//			//set focus to the gaining window
//			mapWndAltTabApp.findHWND(hwnd)->second.SetFocus(true);
//			//update last focus
//			hwndLastFocus = hwnd;
//			//notifyEvent = event_type::FOCUS_CHANGE_EVENT;
//			//hwndEvent = hwnd;
//			//cvMapChanged.notify_one();
//		}
//		//TO DO
//		/* till now I do simple management for new focus on window not in list -> ignoring, may be problem on last focus */
//		break;
//	default:
//		break;
//	}
//
//	return;
//}
