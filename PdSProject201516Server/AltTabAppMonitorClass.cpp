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
	
	while (active) {
		//TO DO: WARNING -> WHEN THE CLIENT MUST ACCESS MAP IN CONSISTEN STATE!
		std::unique_lock<std::mutex> ul(this->mapMutex);
		//clear the map
		this->map.clear();
		//find opened Alt-Tab windows
		BOOL result = EnumWindows(AltTabAppMonitorClass::HandleWinDetected, reinterpret_cast<LPARAM>(this));
		if (result == FALSE) return;

		//initialize first focus
		HWND hwndWithFocus = GetForegroundWindow();
		bool mapContainsHwnd = !(this->map.find(hwndWithFocus) == this->map.end());
		if (hwndWithFocus != NULL && mapContainsHwnd)
			this->map.find(hwndWithFocus)->second.SetFocus(true);

		DWORD error = GetLastError();
		//map in consistent state
		ul.unlock();

		std::this_thread::sleep_for(std::chrono::milliseconds(AltTabAppMonitorClass::CHECK_WINDOW_INTERVAL));
	}

	this->map.clear();

	return;
}

/* manage the window returned by EnumWindows -> insert in map or not */
//IT IS ALWAYS CALLED WITH THE LOCK ACQUIRED BY THE THREAD WHICH CALLS THIS CALLBACK -> not acquire again!
BOOL CALLBACK AltTabAppMonitorClass::HandleWinDetected(HWND hwnd, LPARAM ptr)
{
	AltTabAppMonitorClass* myThis = reinterpret_cast<AltTabAppMonitorClass*>(ptr);
	
	/* check if the app is already into the map*/
	bool mapContainsHwnd = !(myThis->map.find(hwnd) == myThis->map.end());
	if (mapContainsHwnd)
		/*the app is already into the map*/
		return TRUE;

	/* create the new app to insert into the map*/
	AltTabAppClass newApp(hwnd);

	/* check if the creation went well*/
	if (newApp.GethWnd() == NULL)
		return TRUE;

	/* insert into the map */
	myThis->map.insert(std::pair<HWND, AltTabAppClass>(hwnd, newApp));
	return TRUE;
}

std::vector<AltTabAppClass> AltTabAppMonitorClass::getAltTabAppVector()
{
	std::vector<AltTabAppClass> altTabAppVector = std::vector<AltTabAppClass>();
	//acquire the lock on the map -> consistent map
	std::lock_guard<std::mutex> lg(this->mapMutex);
	auto i = this->map.begin();
	auto iEnd = this->map.end();

	for (; i != iEnd; i++) {
		//should be copied by value each AltTabAppClass object
		altTabAppVector.push_back(i->second);
	}

	return altTabAppVector;
}

bool AltTabAppMonitorClass::mapContainsHwnd(HWND hwnd)
{
	std::lock_guard<std::mutex> lg(this->mapMutex);
	return this->map.find(hwnd) == this->map.end();
}

