#include "stdafx.h"
#include "SocketCommunicationThread.h"
#include "AltTabApp.h"
#include "AltTabAppMonitor.h"

#define DEFAULT_PORT "27015"

enum msg_type
{
	LIST = 1,
	APP_CREATION = 2,
	APP_DESTRUCTION = 3,
	FOCUS_CHANGE = 4
};

int ManageClient(SOCKET clientSocket);
int SendAppListMsg(SOCKET destSocket);
int SendAppCreationMsg(HWND hwndAppId, SOCKET destSocket);
int SendAppDestructionMsg(HWND hwndAppId, SOCKET destSocket);
int SendFocusChangeMsg(HWND hwndAppId, SOCKET destSocket);
int SendMsg(web::json::value jMsg, SOCKET destSocket);
web::json::value CreateJAppObject(HWND hwnd, AltTabApp app, bool emptyField = false);
std::tstring CreateJIconValue(AltTabApp app);

void SocketCommunicationThread(void)
{
	WSADATA wsaData;
	int iResult;
	std::tstringstream outputStream;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		outputStream.str(std::tstring());
		outputStream << "WSAStartup failed with error: " << iResult << std::endl;
		OutputDebugString(outputStream.str().c_str());
		return;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		outputStream.str(std::tstring());
		outputStream << "getaddrinfo failed with error: " << iResult << std::endl;
		OutputDebugString(outputStream.str().c_str());
		WSACleanup();
		return;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		outputStream.str(std::tstring());
		outputStream << "socket failed with error: " << WSAGetLastError() << std::endl;
		OutputDebugString(outputStream.str().c_str());
		freeaddrinfo(result);
		WSACleanup();
		return;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		outputStream.str(std::tstring());
		outputStream << "bind failed with error: " << WSAGetLastError() << std::endl;
		OutputDebugString(outputStream.str().c_str());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		outputStream.str(std::tstring());
		outputStream << "listen failed with error: " << WSAGetLastError() << std::endl;
		OutputDebugString(outputStream.str().c_str());
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}

	// Accept a client connection
	while (active.load()) {

		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			outputStream.str(std::tstring());
			outputStream << "accept failed with error: " << WSAGetLastError() << std::endl;
			OutputDebugString(outputStream.str().c_str());
			break;
		}

		//CONNECTED!
		outputStream.str(std::tstring());
		outputStream << "Connected with client!" << std::endl;
		OutputDebugString(outputStream.str().c_str());
		
		ManageClient(ClientSocket);

		// close connection with the client
		// shutdown the connection since we're done
		iResult = shutdown(ClientSocket, SD_BOTH);
		if (iResult == SOCKET_ERROR) {
			outputStream.str(std::tstring());
			outputStream << "shutdown failed with error : " << WSAGetLastError() << std::endl;
			OutputDebugString(outputStream.str().c_str());
		}

		//close the socket
		closesocket(ClientSocket);
		outputStream.str(std::tstring());
		outputStream << "Client disconnected!" << std::endl;
		OutputDebugString(outputStream.str().c_str());

	}
	
	//cleanup
	closesocket(ListenSocket);
	WSACleanup();

	return;
}

//Manage client connection
int ManageClient(SOCKET ClientSocket) {
	
	int iResult;
	std::tstringstream outputStream;

	//send the list of the actual opened windows/apps
	iResult = SendAppListMsg(ClientSocket);
	if (iResult == -1) {
		outputStream.str(std::tstring());
		outputStream << "Error sending initial app list" << std::endl;
		OutputDebugString(outputStream.str().c_str());
		return -1;
	}


	//check if some events happen (CREATION-DESTRUCTION-FOCUS) and notify the client with the right message
	std::unique_lock<std::mutex> ul(mMapWndAltTabApp);
	while (active.load()) {

		cvMapChanged.wait(ul, []() {
			if (notifyEvent == event_type::NO_EVENT || hwndEvent == NULL)
				return false; //spurious notification
			else
				return true;
		});

		//reset immediately the flag
		HWND hwndAppId = hwndEvent;
		hwndEvent = NULL;
		event_type localNotifyEvent = notifyEvent;
		notifyEvent = event_type::NO_EVENT;

		//ATTENTION: YOU ALREADY HAVE THE LOCK ON THE APP LIST!
		switch (localNotifyEvent) {
		case event_type::APP_CREATION_EVENT:
			iResult = SendAppCreationMsg(hwndAppId, ClientSocket);
			break;
		case event_type::APP_DESTRUCTION_EVENT:
			iResult = SendAppDestructionMsg(hwndAppId, ClientSocket);
			break;
		case event_type::FOCUS_CHANGE_EVENT:
			iResult = SendFocusChangeMsg(hwndAppId, ClientSocket);
			break;
		default: //error
			iResult = -1;
			break;
		}

		if (iResult == -1) return -1;
	}
	
	return 1;
}

//put the app list into a json object object and send it to the destination socket
int SendAppListMsg(SOCKET destSocket) {
																
	web::json::value jMsg = web::json::value::object();
	web::json::value jAppList = web::json::value::array();
	int iAppNumber = 0;
	for each (pairWndAltTabApp pair in mapWndAltTabApp)
	{
		jAppList[iAppNumber++] = CreateJAppObject(pair.first, pair.second);
	}
	jMsg[U("app_list")] = jAppList;
	jMsg[U("app_number")] = web::json::value::number(iAppNumber);
	jMsg[U("msg_type")] = web::json::value::number(msg_type::LIST);


	//send message to the destination socket
	int iSuccess = SendMsg(jMsg, destSocket);
	if (iSuccess == -1)
		return -1;

	return 1; //success

}

//put the new created app into a json object and send it to the destination socket
int SendAppCreationMsg(HWND hwndAppId, SOCKET destSocket) {
	
	web::json::value jMsg = web::json::value::object();
	web::json::value jAppList = web::json::value::array();
	auto iteratorAppCreated = mapWndAltTabApp.find(hwndAppId);
	if (iteratorAppCreated == mapWndAltTabApp.end())
		return -1; //error created app not found into the list
	
	jAppList[0] = CreateJAppObject(iteratorAppCreated->first, iteratorAppCreated->second);
	
	jMsg[U("app_list")] = jAppList;
	jMsg[U("app_number")] = web::json::value::number(1);
	jMsg[U("msg_type")] = web::json::value::number(msg_type::APP_CREATION);

	//send message to the destination socket
	int iSuccess = SendMsg(jMsg, destSocket);
	if (iSuccess == -1)
		return -1;

	return 1; //success

}

//put the destroyed app into a json object and send it to the destination socket
int SendAppDestructionMsg(HWND hwndAppId, SOCKET destSocket) {

	web::json::value jMsg = web::json::value::object();
	web::json::value jAppList = web::json::value::array();

	jAppList[0] = CreateJAppObject(hwndAppId, NULL, true);

	jMsg[U("app_list")] = jAppList;
	jMsg[U("app_number")] = web::json::value::number(1);
	jMsg[U("msg_type")] = web::json::value::number(msg_type::APP_DESTRUCTION);

	//send message to the destination socket
	int iSuccess = SendMsg(jMsg, destSocket);
	if (iSuccess == -1)
		return -1;

	return 1; //success

}

//put the new gaining focus app into a json object and send it to the destination socket
int SendFocusChangeMsg(HWND hwndAppId, SOCKET destSocket) {
	
	web::json::value jMsg = web::json::value::object();
	web::json::value jAppList = web::json::value::array();
	auto iteratorAppGainingFocus = mapWndAltTabApp.find(hwndAppId);
	if (iteratorAppGainingFocus == mapWndAltTabApp.end())
		return -1; //error created app not found into the list

	jAppList[0] = CreateJAppObject(iteratorAppGainingFocus->first, iteratorAppGainingFocus->second, true);

	jMsg[U("app_list")] = jAppList;
	jMsg[U("app_number")] = web::json::value::number(1);
	jMsg[U("msg_type")] = web::json::value::number(msg_type::FOCUS_CHANGE);

	//send message to the destination socket
	int iSuccess = SendMsg(jMsg, destSocket);
	if (iSuccess == -1)
		return -1;

	return 1; //success
}

int SendMsg(web::json::value jMsg, SOCKET destSocket) {

	//create string representation of the json message
	std::stringstream outBuf;
	jMsg.serialize(outBuf);

	//send the lenght of the json message in string format to the destination socket as an int (32bit)
	int iMsgLen = outBuf.str().length();
	int iResult = send(destSocket, (char *)&iMsgLen, sizeof(int), 0);
	if (iResult != sizeof(int))
		return -1;

	//send message to the destination socket
	iResult = send(destSocket, outBuf.str().c_str(), iMsgLen, 0);
	if (iResult != iMsgLen)
		return -1;

	return 1;

}

web::json::value CreateJAppObject(HWND hwnd, AltTabApp app, bool emptyField) {
	
	web::json::value jApp = web::json::value::object();
	
	jApp[U("app_id")] = web::json::value::number((int) hwnd); //use hwnd as app id both on client both on server
	
	if (!emptyField) {
		jApp[U("app_name")] = web::json::value::string(app.GettstrAppName());
		jApp[U("window_text")] = web::json::value::string(app.GettstrWndText());
		jApp[U("focus")] = web::json::value::boolean(app.GetFocus());
		jApp[U("icon_64")] = web::json::value::string(CreateJIconValue(app));
	}

	return jApp;
}

std::tstring CreateJIconValue(AltTabApp app) {

	std::tstring errorString;

	try {
		
		int iPngIconSize = app.GetPngIconSize();
		std::shared_ptr<byte> pPngIcon = std::shared_ptr<byte>(app.GetPngIcon(), [](byte* ptr) { delete[] ptr; return; });
		if (iPngIconSize == -1 || pPngIcon.get() == nullptr) {
			errorString.assign(L"error retrieving app icon");
				throw std::exception::exception();
		}

		//convert byte format to base64
		DWORD iBase64IconSize;
		BOOL success = CryptBinaryToString(pPngIcon.get(), iPngIconSize, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &iBase64IconSize);
		if (!success) {
			errorString.assign(L"error retrieving info to convert into base64");
			throw std::exception::exception();
		}

		std::shared_ptr<TCHAR> pBase64Icon = std::shared_ptr<TCHAR>(new TCHAR[iBase64IconSize],  [](TCHAR* ptr) { delete[] ptr; return; });
		success = CryptBinaryToString(pPngIcon.get(), iPngIconSize, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, pBase64Icon.get(), &iBase64IconSize);
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