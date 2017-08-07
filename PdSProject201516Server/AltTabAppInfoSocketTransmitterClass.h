#pragma once
#include <cpprest\json.h>
#include "AltTabAppMonitorClass.h"

using namespace web;

class AltTabAppInfoSocketTransmitterClass
{
	const PCSTR DEFAULT_PORT = "27015";
	std::atomic<bool> active; //describe if the server application is still active
	void manageListeningSocket();
	std::thread listeningSocketThread;
	AltTabAppMonitorClass* monitor;
	std::vector<std::shared_ptr<std::thread>> threadVector;
	void serveClient(SOCKET clientSocket);	
	void sendApplicationMonitorNotificationToClient(SOCKET clientSocket);
	bool sendMsgToClient(SOCKET clientSocket, std::string msg);
	json::value createJsonAppListMessage(std::vector<AltTabAppClass> altTabAppVector);
	json::value createJsonNotificationMessage(Notification notification);
	json::value fromAltTabAppObjToJsonObj(AltTabAppClass altTabAppObj, bool empty = false);
	std::tstring fromPNGToBase64(std::shared_ptr<byte> pPNGByte, int iPNGSize);
	std::tstring fromNotificationEventEnumToTString(notification_event_type notificationEvent);
	void notifyStop();
public:
	AltTabAppInfoSocketTransmitterClass(AltTabAppMonitorClass* monitor);
	~AltTabAppInfoSocketTransmitterClass();
	void start();
	void stop();	
};

