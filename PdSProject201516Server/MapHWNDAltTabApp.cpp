#include "stdafx.h"
#include "MapHWNDAltTabApp.h"

MapHWNDAltTabApp::MapHWNDAltTabApp()
{
}

MapHWNDAltTabApp::~MapHWNDAltTabApp()
{
}

bool MapHWNDAltTabApp::containsHWND(HWND hwnd)
{
	std::lock_guard<std::mutex> lg(this->mapMutex);
	if (this->map.find(hwnd) == this->map.end())
		return false;
	
	return true;
}

std::map<HWND,AltTabApp>::iterator MapHWNDAltTabApp::findHWND(HWND hwnd)
{
	std::lock_guard<std::mutex> lg(this->mapMutex); 
	return this->map.find(hwnd);
}

bool MapHWNDAltTabApp::insertAltTapApp(HWND hwnd, AltTabApp app)
{
	std::lock_guard<std::mutex> lg(this->mapMutex);
	/* insert the new app into the map */
	std::pair<std::map<HWND, AltTabApp>::iterator, bool> insertResult = this->map.insert(pairWndAltTabApp(hwnd,app));
	return insertResult.second;
}

bool MapHWNDAltTabApp::eraseAltTabApp(HWND hwnd)
{
	std::lock_guard<std::mutex> lg(this->mapMutex);
	if (this->map.erase(hwnd) == 0)
		/* no app removed */
		return false;
	return true;
}

void MapHWNDAltTabApp::clearMap()
{
	std::lock_guard<std::mutex> lg(this->mapMutex);
	this->map.clear();
	return;
}


