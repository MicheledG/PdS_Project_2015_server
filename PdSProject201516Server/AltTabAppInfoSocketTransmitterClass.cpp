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

	//enable non blocking mode for the listening socket
	unsigned long argp = 1;
	ioctlsocket(ListenSocket, FIONBIO, &argp);

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

	// Accept a client socket at a time!!!
	while (this->active.load()) {
		ClientSocket = accept(ListenSocket, NULL, NULL);
		
		if (ClientSocket == INVALID_SOCKET) {
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				//there are no pending connections on the listening socket
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}
			else {
				int error = WSAGetLastError();
				closesocket(ListenSocket);
				WSACleanup();
				return;
			}			
		}
		
		//connection established, serve the client!
		serveClient(ClientSocket);
	}

	// No longer need server socket
	closesocket(ListenSocket);
	WSACleanup();

	return;
}

//mapLock is used to synchronize access on the map and ensure that
//transmitter thread registers on the queue event only when no update is in progress
//thus after the first get of the app list, transmitter thread "notify" to monitor thread
//to start pushing back event in the queue BEFORE the monitor thread start updating the map
void AltTabAppInfoSocketTransmitterClass::serveClient(SOCKET clientSocket)
{
	//get actual list of opened alt tab app
	std::unique_lock<std::mutex> mapLock(this->monitor->mapMutex);
	std::vector<AltTabAppClass> altTabAppVector = this->monitor->getAltTabAppVector();
	//"register" transmitter thread on the queue event
	this->monitor->transmitterConnectedToNotificationQueue.store(true);
	mapLock.unlock();
	
	//create initial message to send to the client
	web::json::value jMsg = createJsonAppListMessage(altTabAppVector);
	std::string msgString = utility::conversions::to_utf8string(jMsg.serialize());

	//send message to the client
	bool success = sendMsgToClient(clientSocket, msgString);
	if (!success) {
		closesocket(clientSocket);
		return;
	}
	
	while (this->active.load()) {
		
		std::unique_lock<std::mutex> queueLock(this->monitor->notificationQueueMutex);

		//WARNING: STOP FIRST THE TRANSMITTER AND THEN THE MONITOR
		while (true) {
			if (!this->monitor->notificationQueue.empty()) {
				//event in the queue
				break;
			}				
			this->monitor->newNotificationInQueue.wait(queueLock);
		}
		
		Notification notification = this->monitor->notificationQueue.front();
		this->monitor->notificationQueue.pop_front();
		queueLock.unlock();

		//create the right message depending on the event
		jMsg = createJsonNotificationMessage(notification);
				
		//send the message created
		msgString = utility::conversions::to_utf8string(jMsg.serialize());
		bool success = sendMsgToClient(clientSocket, msgString);
		if (!success) {
			break;
		}

	}

	//"unregister" transmitter thread on the queue event
	mapLock.lock();
	this->monitor->transmitterConnectedToNotificationQueue.store(false);
	this->monitor->notificationQueue.clear();
	mapLock.unlock();

	//close the client socket
	closesocket(clientSocket);
	return;

}

bool AltTabAppInfoSocketTransmitterClass::sendMsgToClient(SOCKET clientSocket, std::string msg) {

	int32_t msgLen = msg.length();

	/* put the message into the char buffer */
	std::shared_ptr<char> msgBuf = std::shared_ptr<char>(new char[msgLen + 1], [](char* ptr) {delete[] ptr; });
	strcpy_s(msgBuf.get(), msgLen + 1, msg.c_str());

	/* send the msgLen => 4B = 32bit */
	int iResult = send(clientSocket, (char*)&msgLen, sizeof(int32_t), 0);
	if (iResult == SOCKET_ERROR) {		
		return false;
	}

	/* send the char buffer */
	iResult = send(clientSocket, msgBuf.get(), msgLen, 0);
	if (iResult == SOCKET_ERROR) {		
		return false;
	}
	
	return true;
}

json::value AltTabAppInfoSocketTransmitterClass::createJsonAppListMessage(std::vector<AltTabAppClass> altTabAppVector) {

	web::json::value jMsg = web::json::value::object();
	web::json::value jAppList = web::json::value::array();
	int iAppNumber = 0;
	for each (AltTabAppClass app in altTabAppVector)
	{
		jAppList[iAppNumber++] = this->fromAltTabAppObjToJsonObj(app);
	}
	jMsg[U("app_list")] = jAppList;
	jMsg[U("app_number")] = web::json::value::number(iAppNumber);

	return jMsg;
}

json::value AltTabAppInfoSocketTransmitterClass::createJsonNotificationMessage(Notification notification)
{
	web::json::value jMsg = web::json::value::object();
	web::json::value jAppList = web::json::value::array();
	
	//create the right list of application to send into the json according to the notification event	
	switch (notification.notificationEvent)
	{
		case notification_event_type::APP_CREATE: {
			jAppList[0] = this->fromAltTabAppObjToJsonObj(notification.appList.front());
			break;
		}
		case notification_event_type::APP_DESTROY: {
			AltTabAppClass destroyedApp(notification.appIdList.front(), true);
			jAppList[0] = this->fromAltTabAppObjToJsonObj(destroyedApp, true);
			break;
		}
		case notification_event_type::APP_FOCUS: {
			AltTabAppClass oldFocusApp(notification.appIdList.front(), true);
			AltTabAppClass newFocusApp(notification.appIdList.back(), true);
			jAppList[0] = this->fromAltTabAppObjToJsonObj(oldFocusApp, true);
			//jAppList[0] = this->fromAltTabAppObjToJsonObj(newFocusApp, true);
			jAppList[1] = this->fromAltTabAppObjToJsonObj(newFocusApp, true);						
			break;
		}
	default:
		break;
	}

	jMsg[U("app_list")] = jAppList;
	jMsg[U("app_number")] = web::json::value::number(jAppList.size());
	std::tstring eventNotificationTString = this->fromNotificationEventEnumToTString(notification.notificationEvent);
	jMsg[U("notification_event")] = web::json::value::string(eventNotificationTString);

	return jMsg;
}

json::value AltTabAppInfoSocketTransmitterClass::fromAltTabAppObjToJsonObj(AltTabAppClass altTabAppObj, bool empty)
{

	web::json::value jApp = web::json::value::object();

	jApp[U("app_id")] = json::value::number((uint64_t)altTabAppObj.GethWnd()); //use hwnd as app id both on client both on server
	if (empty)
		return jApp;

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

std::tstring AltTabAppInfoSocketTransmitterClass::fromNotificationEventEnumToTString(notification_event_type notificationEvent)
{
	std::tstring notificationEventString;
	switch (notificationEvent)
	{
	case notification_event_type::APP_CREATE:
		notificationEventString = std::tstring(L"APP_CREATE");
		break;
	case notification_event_type::APP_DESTROY:
		notificationEventString = std::tstring(L"APP_DESTROY");
		break;
	case notification_event_type::APP_FOCUS:
		notificationEventString = std::tstring(L"APP_FOCUS");
		break;
	default:
		notificationEventString = std::tstring(L"unknown notification type");
		break;
	}
	
	return notificationEventString;
}
