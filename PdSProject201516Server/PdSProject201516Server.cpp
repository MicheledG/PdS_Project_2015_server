// PdSProject201516Server.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "PdSProject201516Server.h"
#include "InitMonitorAltTabApp.h"
#include "SocketCommunicationThread.h"

#define MAX_LOADSTRING 100
#define WM_USER_SHELLICON WM_USER + 1

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
NOTIFYICONDATA nidMyTrayIcon;
HMENU hPopMenu;
std::thread SocketThread;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.
	InitMonitorAltTabApp();
	SocketThread = std::thread(SocketCommunicationThread);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_PDSPROJECT201516SERVER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PDSPROJECT201516SERVER));

    MSG msg;

    // Main message loop:
	    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PDSPROJECT201516SERVER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_PDSPROJECT201516SERVER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   // Initilize the notify icon structure to display the icon in the taskbar
   nidMyTrayIcon.cbSize = sizeof(NOTIFYICONDATA);
   nidMyTrayIcon.hWnd = hWnd;
   nidMyTrayIcon.uID = IDI_PDSPROJECT201516SERVER;
   nidMyTrayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
   nidMyTrayIcon.uCallbackMessage = WM_USER_SHELLICON;
   nidMyTrayIcon.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PDSPROJECT201516SERVER));
   LoadStringW(hInstance, IDS_APP_TITLE, nidMyTrayIcon.szTip, MAX_LOADSTRING);

   Shell_NotifyIcon(NIM_ADD, &nidMyTrayIcon);
   //ShowWindow(hWnd, nCmdShow);
   //UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	POINT pClickPoint;

    switch (message)
    {
	case WM_USER_SHELLICON:
		//systray msg callback
		switch (LOWORD(lParam)) {
			case WM_RBUTTONDOWN:
				GetCursorPos(&pClickPoint);
				hPopMenu = CreatePopupMenu();
				InsertMenu(hPopMenu, 0, MF_BYPOSITION | MF_STRING, IDM_ABOUT, TEXT("About"));
				InsertMenu(hPopMenu, 1, MF_BYPOSITION | MF_STRING, IDM_EXIT, TEXT("Exit"));
				
				TrackPopupMenu(hPopMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, pClickPoint.x, pClickPoint.y, 0, hWnd, NULL);
				return TRUE;
		}
		break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
				Shell_NotifyIcon(NIM_DELETE, &nidMyTrayIcon);
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        // TO DO!
		/* close all the resources here! */
		active.store(false);
		SocketThread.join();
		//still have to delete the entire map!
		StopMonitorAltTabApp();
		PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

