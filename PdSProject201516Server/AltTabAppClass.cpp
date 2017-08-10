#include "stdafx.h"
#include "AltTabAppClass.h"

#define MAX_FILENAMEEXLEN 1024
#define MAX_WNDTEXTLEN 1024

AltTabAppClass::AltTabAppClass()
{
}

//IF SOMETHING GOES WRONG DURING CONSTRUCTION RETURNS AltTabAppClass with hWnd == NULL
AltTabAppClass::AltTabAppClass(HWND hWnd, bool empty)
{
	
	if (empty) {
		this->hWnd = hWnd;
		return;
	}
	
	if (!isAltTabApp(hWnd)) return; //let hWnd to NULL!

	this->hWnd = hWnd;

	//retrieve all AltTabApp attributes
	//get threadId and processId
	dwThrdId = GetWindowThreadProcessId(this->hWnd, &(this->dwProcId));

	//get thread handle
	hThrd = GetThreadHandle(dwThrdId);
	if (hThrd == NULL) {
		this->hWnd = NULL;
		return;
	}

	//get process handle
	hProc = GetProcHandle(dwProcId);
	if (hProc == NULL) {
		this->hWnd = NULL;
		return;
	}

	//get application name
	tstrAppName.assign(GetAppName(hProc));
	if (tstrAppName.empty()) {
		//impossible to retrieve app name
		this->hWnd = NULL;
		return;
	}

	//get application icon in png format into a byte[]
	pPngIcon = std::shared_ptr<byte>(GetAppIconPng(hProc, &iPngIconSize), [](byte* ptr) {delete[] ptr; });

	//default focus status
	focus = false;

	return;
}

AltTabAppClass::~AltTabAppClass()
{
}

void AltTabAppClass::SetFocus(bool hasFocus)
{
	focus = hasFocus;

	return;
}

bool AltTabAppClass::GetFocus()
{
	return focus;
}

HWND AltTabAppClass::GethWnd()
{
	return hWnd;
}

HANDLE AltTabAppClass::GethThrd()
{
	return hThrd;
}

DWORD AltTabAppClass::GetdwThrdId()
{
	return dwThrdId;
}

HANDLE AltTabAppClass::GethProc()
{
	return hProc;
}

DWORD AltTabAppClass::GetdwProcId()
{
	return dwProcId;
}

std::shared_ptr<byte> AltTabAppClass::GetPngIcon()
{
	return this->pPngIcon;
}

int AltTabAppClass::GetPngIconSize() {
	if (pPngIcon.get() == nullptr)
		return -1;
	else
		return iPngIconSize;
}

std::tstring AltTabAppClass::GettstrAppName()
{
	return tstrAppName;
}

std::tstring AltTabAppClass::GettstrWndText()
{
	//obtain window text
	DWORD length = GetWindowTextLength(hWnd);
	std::shared_ptr<TCHAR> wndTextBuffer(new TCHAR[length + 1], [](TCHAR* ptr) {delete[] ptr; });

	if (length == 0) {
		//empty window text
		*wndTextBuffer = L'\0';
	}
	else {
		if (!GetWindowText(hWnd, wndTextBuffer.get(), length + 1)) *wndTextBuffer = L'\0'; //error retrieving window text => set empty window text
	}

	return std::tstring(wndTextBuffer.get());
}

//check if the application (window application) is "alt-tab"
bool AltTabAppClass::isAltTabApp(HWND hWnd)
{		
	
	HWND hWndTry, hWndWalk = NULL;

	if (!IsWindowVisible(hWnd)) {
		return false;
	}
		

	//check if hWnd is the last active popup of the ancestor, if not discard
	hWndTry = GetAncestor(hWnd, GA_ROOTOWNER);
	while (hWndTry != hWndWalk)
	{
		hWndWalk = hWndTry;
		hWndTry = GetLastActivePopup(hWndWalk);
		if (IsWindowVisible(hWndTry)) {
			break;
		}
			
	}
	if (hWndWalk != hWnd) {
		return false;
	}		

	bool isToolWindow = (GetWindowLong(hWnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) == WS_EX_TOOLWINDOW;
	bool isNotAppWindow = (GetWindowLong(hWnd, GWL_EXSTYLE) & WS_EX_APPWINDOW) != WS_EX_APPWINDOW;
	int cloakedVal;
	HRESULT hr = DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloakedVal, sizeof(int));
	if (hr != S_OK)
	{
		cloakedVal = 0;
	}
	bool isWin10Visible = cloakedVal != 0 ? false : true;
	if (!(isToolWindow && isNotAppWindow) && isWin10Visible) {
		return true;
	}		
	
	return false;
}

HANDLE AltTabAppClass::GetThreadHandle(DWORD dwThrdId) {

	HANDLE hThrd = OpenThread(
		THREAD_ALL_ACCESS,
		//READ_CONTROL,
		TRUE, //children can inherit the handle
		dwThrdId
		);

	return hThrd;

}

HANDLE AltTabAppClass::GetProcHandle(DWORD dwProcId) {

	HANDLE hProc = OpenProcess(
		PROCESS_ALL_ACCESS,
		//READ_CONTROL,
		TRUE, //children can inherit the handle
		dwProcId
		);

	return hProc;
}

std::tstring AltTabAppClass::GetAppName(HANDLE hProc)
{
	std::tstring tstrAppName;

	//obtain name of the executable file
	TCHAR fileNameEx[MAX_FILENAMEEXLEN];
	DWORD length = GetModuleFileNameEx(
		hProc,
		NULL,
		fileNameEx,
		MAX_FILENAMEEXLEN
		);

	if (length == 0) {
		tstrAppName.assign(TEXT("Error retrieving executable file name"));
		return tstrAppName;
	}

	//obtain the version-info resource of that file
	DWORD fileVersionInfoLen;
	fileVersionInfoLen = GetFileVersionInfoSize(fileNameEx, NULL);

	if (fileVersionInfoLen == 0) {
		tstrAppName.assign(TEXT("Error retrieving information size of the the executable file"));
		return tstrAppName;
	}

	std::shared_ptr<VOID> fileVersionInfoPtr(new char[fileVersionInfoLen],
		[](VOID* ptr) { delete[] ptr; }); //avoid leak of memory on unexpected termination
	if (!GetFileVersionInfo(fileNameEx, NULL, fileVersionInfoLen, fileVersionInfoPtr.get())) {
		tstrAppName.assign(TEXT("Error retrieving information about the executable file"));
		return tstrAppName;
	}

	//obtain the language-codepage pair to interpret the file-version resource
	WORD* translationInfo;
	PUINT ptrCbInfo = NULL;
	if (!VerQueryValue(fileVersionInfoPtr.get(), L"\\VarFileInfo\\Translation", (LPVOID*)&translationInfo, ptrCbInfo)) {
		tstrAppName.assign(TEXT("Error getting the language and codepage of information about the executable file"));
		return tstrAppName;
	}

	//create the query to extract file description (application name) from the resource file with the right language-codepage
	TCHAR queriedValue[MAX_FILENAMEEXLEN];
	if (FAILED(StringCchPrintf(queriedValue, MAX_FILENAMEEXLEN, L"\\StringFileInfo\\%04x%04x\\FileDescription", translationInfo[0], translationInfo[1]))) {
		tstrAppName.assign(TEXT("Error creating the query to obtain the file description"));
		return tstrAppName;
	}
	
	//extract file description (application name)
	TCHAR* appName = NULL;
	PUINT ptrAppNameBufferLen = NULL;
	if (!VerQueryValue(fileVersionInfoPtr.get(), queriedValue, (LPVOID*)&appName, ptrAppNameBufferLen)) {
		// if file desciption is not available use the ".exe" file name as appName
		std::tstring temp(fileNameEx);
		size_t pos = temp.find_last_of('\\', MAX_FILENAMEEXLEN);
		appName = &fileNameEx[pos + 1];
	}

	tstrAppName.assign(appName);
	
	//filter out the Application Frame Host windows
	int comparison = tstrAppName.compare(L"Application Frame Host");
	if (comparison == 0) {
		tstrAppName.assign(std::tstring());
	}

	return tstrAppName;
}

byte* AltTabAppClass::GetAppIconPng(HANDLE hProc, int *size) {
	

	IStream *pPngStream = nullptr;
	if (CreateStream(&pPngStream) == -1)
		return nullptr;

	HICON appHIcon = NULL;

	if (GetAppHIcon(hProc, &appHIcon) == -1) {
		pPngStream->Release();
		//always to do
		DestroyIcon(appHIcon);
		return nullptr;
	}

	if (GetAppBitmapStream(appHIcon, pPngStream) == -1) {
		pPngStream->Release();
		//always to do
		DestroyIcon(appHIcon);
		return nullptr;
	}

	//always to do
	DestroyIcon(appHIcon);

	byte *pPngByte = SavePngStreamToPngByte(pPngStream, size);
	if (pPngByte == nullptr) {
		pPngStream->Release();
		return nullptr;
	}
	
	pPngStream->Release();

	return pPngByte;
}

int AltTabAppClass::CreateStream(LPSTREAM *pIstream) {

	HRESULT result = CreateStreamOnHGlobal(
		NULL,
		TRUE,
		pIstream
		);

	if (result != S_OK)
		return -1;
	else
		return 1;
}

int AltTabAppClass::GetAppHIcon(HANDLE hProc, HICON *appHIcon)
{
	//obtain name of the executable file
	TCHAR fileNameEx[MAX_FILENAMEEXLEN];
	DWORD length = GetModuleFileNameEx(
		hProc,
		NULL,
		fileNameEx,
		MAX_FILENAMEEXLEN);
	
	*appHIcon = ExtractIcon(
		NULL,
		fileNameEx,
		0);

	if (*appHIcon == nullptr)
		return -1;

	return 1;





}

int AltTabAppClass::GetAppBitmapStream(HICON appHIcon, IStream *pPngStream) {
	
	/* startup Gdiplus to extract bitmap bytes from video memory */
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::Status status = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	if (status != Gdiplus::Status::Ok) {
		/* close Gdiplus */
		Gdiplus::GdiplusShutdown(gdiplusToken);
		return -1;
	}
		
	/* extract the bitmap */	
	Gdiplus::Bitmap *pBitmap = Gdiplus::Bitmap::FromHICON(appHIcon);

	if (SaveBitmapToPngStream(pBitmap, pPngStream) == -1) {
		delete pBitmap;
		/* close Gdiplus */
		Gdiplus::GdiplusShutdown(gdiplusToken);
		return -1;
	}

	/* close Gdiplus */
	Gdiplus::GdiplusShutdown(gdiplusToken);

	return 1;
}


int AltTabAppClass::SaveBitmapToPngStream(Gdiplus::Bitmap *pBitmap, IStream *pPngStream) {

	CLSID pngClsid;
	if (GetEncoderClsid(L"image/png", &pngClsid) == -1)
		return -1;

	HRESULT result = pBitmap->Save(pPngStream, &pngClsid);
	if (result != S_OK)
		return -1;

	return 1;
}

byte* AltTabAppClass::SavePngStreamToPngByte(IStream* pPngStream, int *size) {

	STATSTG stat;
	HRESULT result = pPngStream->Stat(&stat, STATFLAG_NONAME);
	if (result != S_OK)
		return nullptr;

	ULONGLONG big =  stat.cbSize.QuadPart;
	DWORD small1 = stat.cbSize.LowPart;
	DWORD small2 = stat.cbSize.HighPart;

	*size = stat.cbSize.LowPart;
	byte* pPngByte = new byte[*size];
	if (pPngByte == nullptr)
		return nullptr;

	unsigned long int readByte;
	LARGE_INTEGER zero;
	zero.QuadPart = 0;
	pPngStream->Seek(zero, STREAM_SEEK_SET, NULL);
	result = pPngStream->Read(pPngByte, *size, &readByte);
	if (result != S_OK) {
		delete[] pPngByte;
		return nullptr;
	}
		

	return pPngByte;
}

int AltTabAppClass::GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}