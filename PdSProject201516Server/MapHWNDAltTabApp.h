#pragma once
#include "AltTabApp.h"

typedef std::pair<HWND, AltTabApp> pairWndAltTabApp;

class MapHWNDAltTabApp
{
	std::map<HWND, AltTabApp> map;
	std::mutex mapMutex;
public:
	MapHWNDAltTabApp();
	~MapHWNDAltTabApp();
	bool containsHWND(HWND hwnd);
	std::map<HWND, AltTabApp>::iterator findHWND(HWND hwnd);
	bool insertAltTapApp(HWND hwnd, AltTabApp);
	bool eraseAltTabApp(HWND hwnd);
	void clearMap();
};

