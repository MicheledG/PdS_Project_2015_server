#pragma once

#include "stdafx.h"

//typedef
namespace std {
#if defined _UNICODE || defined UNICODE
	typedef wstring tstring;
#else
	typedef string tstring;
#endif

#if defined _UNICODE || defined UNICODE
	typedef wstringstream tstringstream;
#else
	typedef stringstream tstringstream;
#endif

}

typedef enum AttachModeEnum
{
	DETACH = FALSE,
	ATTACH = TRUE
}	AttachModeT;

class AltTabApp
{
	HWND hWnd = NULL;
	HANDLE hThrd;
	DWORD dwThrdId;
	HANDLE hProc;
	DWORD dwProcId;
	std::tstring tstrAppName;
	std::shared_ptr<byte> pPngIcon;
	int iPngIconSize;
	bool focus = false;
	/* tool functions */
	bool isAltTabApp(HWND hWnd);
	HANDLE GetThreadHandle(DWORD dwThrdId);
	HANDLE GetProcHandle(DWORD dwProcId);
	std::tstring GetAppName(HANDLE hProc);
	byte* GetAppIconPng(HANDLE hProc, int *size);
	int CreateStream(LPSTREAM *pIstream);
	int GetAppHIcon(HANDLE hProc, HICON *appHIcon);
	int GetAppBitmapStream(HICON appHIcon, IStream *pPngStream);
	int SaveBitmapToPngStream(Gdiplus::Bitmap *pBitmap, IStream *pPngStream);
	byte* SavePngStreamToPngByte(IStream* pPngStream, int *size);
	int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
public:
	AltTabApp(HWND hWnd);
	~AltTabApp();
	bool AttachAppMsgQueue(AttachModeT mode);
	void SetFocus(bool hasFocus);
	bool GetFocus();
	HWND GetwHnd();
	HANDLE GethThrd();
	DWORD GetdwThrdId();
	HANDLE GethProc();
	DWORD GetdwProcId();
	byte* GetPngIcon();
	int GetPngIconSize();
	std::tstring GettstrAppName();
	std::tstring GettstrWndText();
};

typedef std::pair<HWND, AltTabApp> pairWndAltTabApp;