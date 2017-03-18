#include "stdafx.h"
#include "AltTabAppMonitorClass.h"

AltTabAppMonitorClass::AltTabAppMonitorClass()
{
}

AltTabAppMonitorClass::~AltTabAppMonitorClass()
{
}

void AltTabAppMonitorClass::start()
{
	this->active.store(true);
	this->monitorThread = std::thread(&AltTabAppMonitorClass::monitor, this);
}

void AltTabAppMonitorClass::stop()
{
	this->active.store(false);
	this->monitorThread.join();
}

void AltTabAppMonitorClass::monitor()
{
	//create initial map
	std::unique_lock<std::mutex> mapLock(this->mapMutex);
	this->getActualOpenedApp(&this->map, &this->focusAppId);
	//map in consistent state
	mapLock.unlock();

	while (active.load()) {
		//create temporary map with the actual window opened
		std::map<HWND, AltTabAppClass> tmpMap;
		HWND tmpFocusAppId;
		this->getActualOpenedApp(&tmpMap, &tmpFocusAppId);
		
		//compare temporary and monitor map and update monitor map
		mapLock.lock();
		//remove closed app
		for each (std::pair<HWND, AltTabAppClass> pair in this->map)
		{
			bool tmpMapContains = !(tmpMap.find(pair.first) == tmpMap.end());
			if (!tmpMapContains) {
				//the app considere is no more opened because it is not contained into the tmpMap
				this->map.erase(pair.first);
				//ADD EVENT TO THE QUEUE EVENT
				std::pair<notification_event_type, HWND> eventNotification(notification_event_type::APP_DESTROY, pair.first);
				std::lock_guard<std::mutex> queueLock(this->eventQueueMutex);
				this->eventQueue.push_back(eventNotification);
				this->newEventInQueue.notify_one();
			}
			else {
				//the app is still opened so check the next one
				tmpMap.erase(pair.first);
			}
		}

		//insert new opened app
		for each (std::pair<HWND, AltTabAppClass> tmpPair in tmpMap)
		{
			//the apps still contained in the temporary map are all new
			this->map.insert(tmpPair);
			//ADD EVENT TO THE QUEUE EVENT
			std::pair<notification_event_type, HWND> eventNotification(notification_event_type::APP_CREATE, tmpPair.first);
			std::lock_guard<std::mutex> queueLock(this->eventQueueMutex);
			this->eventQueue.push_back(eventNotification);
			this->newEventInQueue.notify_one();
		}

		//check focus change
		HWND oldFocusAppId = this->focusAppId;
		if (oldFocusAppId != tmpFocusAppId) {
			bool mapContains = !(this->map.find(oldFocusAppId) == this->map.end());
			if(mapContains)
				this->map[oldFocusAppId].SetFocus(false);
			this->map[tmpFocusAppId].SetFocus(true);
			this->focusAppId = tmpFocusAppId;
			//ADD EVENT TO THE QUEUE EVENT
			std::pair<notification_event_type, HWND> eventNotification(notification_event_type::APP_FOCUS, tmpFocusAppId);
			std::lock_guard<std::mutex> queueLock(this->eventQueueMutex);
			this->eventQueue.push_back(eventNotification);
			this->newEventInQueue.notify_one();
		}

		mapLock.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(AltTabAppMonitorClass::CHECK_WINDOW_INTERVAL));
	}

	this->map.clear();

	return;
}

/* store into a given map the actual opened alt tab app and into a HWND the app id of the one with the focus */
 void AltTabAppMonitorClass::getActualOpenedApp(std::map<HWND, AltTabAppClass>* appMap, HWND* focusAppId)
{
	//insert opened Alt-Tab windows into the map
	BOOL result = EnumWindows(AltTabAppMonitorClass::HandleWinDetected, reinterpret_cast<LPARAM>(appMap));
	if (result == FALSE) return;
	//set focusAppId
	HWND tmpFocusAppId = GetForegroundWindow();
	bool mapContainsHwnd = !(appMap->find(tmpFocusAppId) == appMap->end());
	if (tmpFocusAppId != NULL && mapContainsHwnd) {
		appMap->find(tmpFocusAppId)->second.SetFocus(true);
		*focusAppId = tmpFocusAppId;
	}
	DWORD error = GetLastError();
	
	return;
}

/* manage the window returned by EnumWindows -> insert in map or not */
//IT IS ALWAYS CALLED WITH THE LOCK ACQUIRED BY THE THREAD WHICH CALLS THIS CALLBACK -> not acquire again!
BOOL CALLBACK AltTabAppMonitorClass::HandleWinDetected(HWND hwnd, LPARAM ptr)
{
	std::map<HWND, AltTabAppClass>* mapPtr = reinterpret_cast<std::map<HWND, AltTabAppClass>*>(ptr);

	/* check if the app is already into the map*/
	bool mapContainsHwnd = !(mapPtr->find(hwnd) == mapPtr->end());
	if (mapContainsHwnd)
		/*the app is already into the map*/
		return TRUE;

	/* create the new app to insert into the map*/
	AltTabAppClass newApp(hwnd);

	/* check if the creation went well*/
	if (newApp.GethWnd() == NULL)
		return TRUE;

	/* insert into the map */
	mapPtr->insert(std::pair<HWND, AltTabAppClass>(hwnd, newApp));
	return TRUE;
}

//ATTENTION: NOT THREAD SAFE!
std::vector<AltTabAppClass> AltTabAppMonitorClass::getAltTabAppVector()
{
	std::vector<AltTabAppClass> altTabAppVector = std::vector<AltTabAppClass>();
	auto i = this->map.begin();
	auto iEnd = this->map.end();

	for (; i != iEnd; i++) {
		//should be copied by value each AltTabAppClass object
		altTabAppVector.push_back(i->second);
	}

	return altTabAppVector;
}
