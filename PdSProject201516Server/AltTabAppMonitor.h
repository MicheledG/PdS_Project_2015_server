#pragma once

#include "stdafx.h"
#include "AltTabApp.h"
#include "MapHWNDAltTabApp.h"

enum event_type {
	APP_CREATION_EVENT,
	APP_DESTRUCTION_EVENT,
	FOCUS_CHANGE_EVENT,
	NO_EVENT
};

//declare global variables
extern MapHWNDAltTabApp mapWndAltTabApp;
extern std::atomic<bool> active; //describe if the server application is still active

//extern std::condition_variable cvMapChanged; //used to notify event to the socket thread
//extern event_type notifyEvent; //specify the event that generated the notification
//extern HWND hwndEvent; //specify the window that generated the event

// functions' declarations of AltTabAppMonitor module
void AltTabAppMonitor();
BOOL CALLBACK HandleWinDetected(HWND hWnd, LPARAM ptr);

//void InitAltTabAppMonitor();
//void StopAltTabAppMonitor();
//void CALLBACK HandleWinEvent(
//	HWINEVENTHOOK hWinEventHook,
//	DWORD         dwEvent,
//	HWND          hwnd,
//	LONG          idObject,
//	LONG          idChild,
//	DWORD         dwEventThread,
//	DWORD         dwmsEventTime
//	);