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
	this->notifyStop();	
	this->listeningSocketThread.join();
}

void AltTabAppInfoSocketTransmitterClass::notifyStop() {

	this->activeClient.store(false);
	
	//add notification of STOP to interact with the background threads!!!
	std::unique_lock<std::mutex> queueLock(this->monitor->notificationQueueMutex);
	if (this->monitor->transmitterConnectedToNotificationQueue.load()) {
		Notification notification;
		notification.notificationEvent = notification_event_type::STOP;
		this->monitor->notificationQueue.push_back(notification);
		this->monitor->newNotificationInQueue.notify_one();
	}	
	queueLock.unlock();


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
				std::this_thread::sleep_for(std::chrono::milliseconds(this->CHECK_PENDING_CLIENT_RATE));
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

//let's start the activities to execute on the socket
void AltTabAppInfoSocketTransmitterClass::serveClient(SOCKET clientSocket)
{	
	this->activeClient.store(true);
	
	std::thread notificationSenderThread = std::thread(&AltTabAppInfoSocketTransmitterClass::sendApplicationMonitorNotificationToClient, this, clientSocket);
	std::thread keysReceiverThread = std::thread(&AltTabAppInfoSocketTransmitterClass::receiveKeys, this, clientSocket);
	std::thread connectionCheckerThread = std::thread(&AltTabAppInfoSocketTransmitterClass::checkConnectionStatus, this, clientSocket);

	//wait the conclusion of the activies
	notificationSenderThread.join();
	keysReceiverThread.join();
	connectionCheckerThread.join();

	//close the client socket
	closesocket(clientSocket);
	return;

}

void AltTabAppInfoSocketTransmitterClass::sendApplicationMonitorNotificationToClient(SOCKET clientSocket)
{
	//get actual list of opened alt tab app
	std::unique_lock<std::mutex> mapLock(this->monitor->mapMutex);
	std::unique_lock<std::mutex> queueLock(this->monitor->notificationQueueMutex);
	
	std::vector<AltTabAppClass> altTabAppVector = this->monitor->getAltTabAppVector();
	//"register" transmitter thread on the queue event
	this->monitor->transmitterConnectedToNotificationQueue.store(true);
	mapLock.unlock();
	queueLock.unlock();

	//create initial message to send to the client
	web::json::value jMsg = createJsonAppListMessage(altTabAppVector);
	std::string msgString = utility::conversions::to_utf8string(jMsg.serialize());

	//send message to the client
	bool success = sendMsgToClient(clientSocket, msgString);
	if (!success) {
		return;
	}	

	while (this->activeClient.load()) {
		
		queueLock.lock();

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

		if (notification.notificationEvent == notification_event_type::STOP) {
			break;
		}

		//create the right message depending on the event
		jMsg = createJsonNotificationMessage(notification);

		//send the message created
		msgString = utility::conversions::to_utf8string(jMsg.serialize());
		bool success = sendMsgToClient(clientSocket, msgString);
		if (!success) {
			break;
		}

	}

	if (this->activeClient.load()) {
		this->activeClient.store(false);
	}
	//"unregister" transmitter thread on the queue event
	queueLock.lock();
	this->monitor->transmitterConnectedToNotificationQueue.store(false);
	this->monitor->notificationQueue.clear();
	queueLock.unlock();
	
	return;

}

void AltTabAppInfoSocketTransmitterClass::receiveKeys(SOCKET clientSocket) {

	while (this->activeClient.load()) {
		try {
			std::string message = this->readMsgFromClient(clientSocket); //THE SOCKET IS NON BLOCKING (BECAUSE IT IS SET IN THIS WAY)

			if (!message.empty()) {
				//extract the json message from the string				
				web::json::value keysReceived = json::value::parse(utility::conversions::to_string_t(message));
				//send the json message to the method that invoke the key shortcut
				this->executeKeys(keysReceived);
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(this->CHECK_NEW_MESSAGE_RATE));
		}
		catch (std::exception e) {
			this->notifyStop();
			break;
		}
	}

	return;
}

void AltTabAppInfoSocketTransmitterClass::executeKeys(web::json::value keys) {

	int numberOfActions = keys.at(U("shortcut_actions_number")).as_number().to_int32();
	json::array actions = keys.at(U("shortcut_actions")).as_array();

	std::shared_ptr<INPUT> inputs = std::shared_ptr<INPUT>(new INPUT[numberOfActions], [](INPUT* ptr) { delete[] ptr; return; });
	for (int i = 0; i < numberOfActions; i++) {
		KEYBDINPUT keyboardInput;
		keyboardInput.wVk = actions[i].at(U("key_virtual_code")).as_number().to_int32();
		if (!actions[i].at(U("is_down")).as_bool()) {
			keyboardInput.dwFlags = KEYEVENTF_KEYUP;
		}
		else {
			keyboardInput.dwFlags = 0;
		}
		INPUT input;
		input.type = INPUT_KEYBOARD;
		input.ki = keyboardInput;

		*(inputs.get() + i) = input;
	}


	int actionsExecuted = SendInput(numberOfActions, (LPINPUT)inputs.get(), sizeof(INPUT));

	return;

}

void AltTabAppInfoSocketTransmitterClass::checkConnectionStatus(SOCKET clientSocket) {

	while (this->activeClient.load()) {
		std::string emptyMsg = "";
		bool connected = emptyMsg.empty();
		connected = this->sendMsgToClient(clientSocket, emptyMsg);
		if (connected) {
			std::this_thread::sleep_for(std::chrono::milliseconds(this->CHECK_CONNECTION_STATUS_RATE));
		}
		else {
			this->notifyStop();
			break;
		}
	}

	return;

}

bool AltTabAppInfoSocketTransmitterClass::sendMsgToClient(SOCKET clientSocket, std::string msg) {

	int32_t msgLen;
	std::shared_ptr<char> msgBuf;

	if (!msg.empty()) {
		msgLen = msg.length();
		/* put the message into the char buffer */
		msgBuf = std::shared_ptr<char>(new char[msgLen + 1], [](char* ptr) {delete[] ptr; });
		strcpy_s(msgBuf.get(), msgLen + 1, msg.c_str());
	}
	else {
		msgLen = 0;
	}
	 	
	{
		std::lock_guard<std::mutex> socketLock(this->clientSocketMutex);

		/* send the msgLen => 4B = 32bit */
		int iResult = send(clientSocket, (char*)&msgLen, sizeof(int32_t), 0);
		if (iResult == SOCKET_ERROR) {
			return false;
		}

		if (msgLen > 0) {
			/* send the char buffer */
			iResult = send(clientSocket, msgBuf.get(), msgLen, 0);
			if (iResult == SOCKET_ERROR) {
				return false;
			}
		}		
	}	
	
	return true;
}

std::shared_ptr<char> AltTabAppInfoSocketTransmitterClass::readNBytesFromClient(SOCKET clientSocket, int N) {

	int result = 0;
	std::shared_ptr<char> buffer(new char[N], [](char* ptr) {delete[] ptr; });
	int bytesRead = 0;
	int bytesToRead = N;
	char* firstByteToRead = buffer.get();

	//read bytes from the socket
	while (bytesRead < N) {		 		
		result = recv(clientSocket, firstByteToRead, bytesToRead, 0);
		if (result > 0) {
			bytesRead += result;
			firstByteToRead += result;
			bytesToRead -= result;			
		}
		else {
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				*(buffer.get()) = 'Z';
				break;
			}
			else {
				throw std::exception::exception("connection error or connection closed");
			}			
		}		
	}

	return buffer;
}

//this function does not need the lock on the socket because there is only one thread reading on it! => keysReceiverThread!
std::string AltTabAppInfoSocketTransmitterClass::readMsgFromClient(SOCKET clientSocket) {

	try {
		//read message body length
		int N = 4; //int32
		std::shared_ptr<char> bytesRead = this->readNBytesFromClient(clientSocket, N);
		if (*bytesRead.get() == 'Z') {
			return "";
		}
		N = *((int*)bytesRead.get());		

		//read message body (if present)
		std::string messageBody;
		if (N > 0) {
			//WARNING!!!
			std::shared_ptr<char> receivedMessageBody;
			while (this->activeClient.load()) {
				receivedMessageBody = this->readNBytesFromClient(clientSocket, N);
				if (*receivedMessageBody.get() == 'Z') {
					continue;
				}
				else {
					break;
				}
			} 
			std::shared_ptr<char> messageBodyChar = std::shared_ptr<char>(new char[N + 1], [](char* ptr) {delete[] ptr; });
			strncpy_s(messageBodyChar.get(), N+1, receivedMessageBody.get(), N);
			*(messageBodyChar.get() + N) = '\0';
			messageBody = messageBodyChar.get();
		}
		else {
			messageBody = "";
		}
		//return message body
		return messageBody;
	}	
	catch (std::exception e) {
		//rethrow
		throw e;
	}	
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
	
	if (altTabAppObj.GethWnd() != NULL) {
		jApp[U("app_id")] = json::value::number((uint64_t)altTabAppObj.GethWnd()); //use hwnd as app id both on client both on server
	}
	else {
		jApp[U("app_id")] = json::value::number(-1);
	}
		
	if (empty) {
		return jApp;
	}
	jApp[U("process_id")] = json::value::number((uint64_t)altTabAppObj.GetdwProcId());
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
			throw std::exception::exception("impossible to create base 64 representation of the application icon");
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
	case notification_event_type::STOP:
		notificationEventString = std::tstring(L"STOP");
		break;
	default:
		notificationEventString = std::tstring(L"unknown notification type");
		break;
	}
	
	return notificationEventString;
}
