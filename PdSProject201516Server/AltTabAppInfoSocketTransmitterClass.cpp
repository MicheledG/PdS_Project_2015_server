#include "stdafx.h"
#include "AltTabAppInfoSocketTransmitterClass.h"
#include "AltTabAppClass.h"

AltTabAppInfoSocketTransmitterClass::AltTabAppInfoSocketTransmitterClass(AltTabAppMonitorClass* monitor)
{
	this->monitor = monitor;
}

AltTabAppInfoSocketTransmitterClass::~AltTabAppInfoSocketTransmitterClass()
{
}

void AltTabAppInfoSocketTransmitterClass::start()
{
	this->active.store(true);
	this->listeningSocketThread = std::thread(&AltTabAppInfoSocketTransmitterClass::manageListeningSocket, this);
}

void AltTabAppInfoSocketTransmitterClass::stop()
{
	this->active.store(false);
	this->listeningSocketThread.join();
}


void AltTabAppInfoSocketTransmitterClass::manageListeningSocket()
{
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, this->DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}

	// Accept a client socket
	while (this->active) {
		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return;
		}
		closesocket(ClientSocket);
		std::shared_ptr<int> testIntPtr = std::shared_ptr<int>(new int[10], [](int* ptr) { delete[] ptr; }); //avoid leak of memory and ensure joining all the child thread
		//pay attention to the destruction of the pointer! -> they will be class with associated thread!
	}

	// No longer need server socket
	closesocket(ListenSocket);
	WSACleanup();

	return;
}


void AltTabAppInfoSocketTransmitterClass::serveClient(SOCKET clientSocket, AltTabAppMonitorClass * monitor, std::atomic<bool>* active)
{

	while (active->load()) {
		/* obtain the list and data of all the opened windows */
		std::vector<AltTabAppClass> altTabAppVector = this->monitor->getAltTabAppVector();
		/* jsonify everything */
		web::json::value jBody = web::json::value::object();
		web::json::value jAppList = web::json::value::array();
		int iAppNumber = 0;
		for each (AltTabAppClass app in altTabAppVector)
		{
			jAppList[iAppNumber++] = this->fromAltTabAppObjToJsonObj(app);
		}
		jBody[U("app_list")] = jAppList;
		jBody[U("app_number")] = web::json::value::number(iAppNumber);
		/* send the message to the client */
		/* compute length of the message */
		//stringfy json
		int jsonLen = 
		int msgLen = sizeof(int)
		/* put the message into the char buffer */
		std::shared_ptr<char> msgBuf = std::shared_ptr<char>(new char[msgLen], [](char* ptr) {delete[] ptr; })
		/* send the char buffer */
		/* check everything is ok */
		/* sleep for a while */
		int iSendResult = send(clientSocket, msgBuf, iResult, 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}
		printf("Bytes sent: %d\n", iSendResult);

	}


}

json::value AltTabAppInfoSocketTransmitterClass::fromAltTabAppObjToJsonObj(AltTabAppClass altTabAppObj)
{

	web::json::value jApp = web::json::value::object();

	jApp[U("app_id")] = json::value::number((uint64_t)altTabAppObj.GethWnd()); //use hwnd as app id both on client both on server
	jApp[U("app_name")] = json::value::string(altTabAppObj.GettstrAppName());
	jApp[U("window_text")] = json::value::string(altTabAppObj.GettstrWndText());
	jApp[U("focus")] = json::value::boolean(altTabAppObj.GetFocus());
	jApp[U("icon_64")] = json::value::string(this->fromPNGToBase64(altTabAppObj.GetPngIcon(), altTabAppObj.GetPngIconSize()));

	return jApp;
}

std::tstring AltTabAppInfoSocketTransmitterClass::fromPNGToBase64(std::shared_ptr<byte> pPNGByte, int iPNGSize)
{
	std::tstring errorString;

	try {

		if (iPNGSize == -1 || pPNGByte.get() == nullptr) {
			errorString.assign(L"error retrieving app icon");
			throw std::exception::exception();
		}

		//convert byte format to base64
		DWORD iBase64IconSize;
		BOOL success = CryptBinaryToString(pPNGByte.get(), iPNGSize, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &iBase64IconSize);
		if (!success) {
			errorString.assign(L"error retrieving info to convert into base64");
			throw std::exception::exception();
		}

		std::shared_ptr<TCHAR> pBase64Icon = std::shared_ptr<TCHAR>(new TCHAR[iBase64IconSize], [](TCHAR* ptr) { delete[] ptr; return; });
		success = CryptBinaryToString(pPNGByte.get(), iPNGSize, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, pBase64Icon.get(), &iBase64IconSize);
		if (!success) {
			errorString.assign(L"error retrieving base64 translation of the bitmap");
			throw std::exception::exception();
		}

		std::tstring tstrBase64Icon;
		tstrBase64Icon.assign(pBase64Icon.get());
		return tstrBase64Icon;
	}
	catch (std::exception e) {
		return errorString;
	}
}