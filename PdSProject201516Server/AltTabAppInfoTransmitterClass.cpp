#include "stdafx.h"
#include "AltTabAppInfoTransmitterClass.h"
#include "AltTabAppClass.h"

const std::tstring AltTabAppInfoTransmitterClass::URL = std::tstring(L"http://*:60000/pds_server/");

void AltTabAppInfoTransmitterClass::handle_get(http_request request)
{
	
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
	/* create the response */
	http_response response = http_response();
	//response.headers().add(L"content-type", L"application/json");
	response.set_status_code(http::status_codes::OK);
	response.set_body(jBody);
	/* send back the response to the client */
	request.reply(response);

}

json::value AltTabAppInfoTransmitterClass::fromAltTabAppObjToJsonObj(AltTabAppClass altTabAppObj)
{
	
	web::json::value jApp = web::json::value::object();

	jApp[U("app_id")] = json::value::number((uint64_t)altTabAppObj.GethWnd()); //use hwnd as app id both on client both on server
	jApp[U("app_name")] = json::value::string(altTabAppObj.GettstrAppName());
	jApp[U("window_text")] = json::value::string(altTabAppObj.GettstrWndText());
	jApp[U("focus")] = json::value::boolean(altTabAppObj.GetFocus());
	//to do: bug right here! when http response thread has finished we have problem on png pointer -> alttabappclass
	jApp[U("icon_64")] = json::value::string(this->fromPNGToBase64(altTabAppObj));
	
	return jApp;
}

std::tstring AltTabAppInfoTransmitterClass::fromPNGToBase64(AltTabAppClass altTabAppObj)
{
	std::tstring errorString;

	try {

		int iPngIconSize = altTabAppObj.GetPngIconSize();
		std::shared_ptr<byte> pPngIcon = std::shared_ptr<byte>(altTabAppObj.GetPngIcon(), [](byte* ptr) { delete[] ptr; return; });
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

		std::shared_ptr<TCHAR> pBase64Icon = std::shared_ptr<TCHAR>(new TCHAR[iBase64IconSize], [](TCHAR* ptr) { delete[] ptr; return; });
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

AltTabAppInfoTransmitterClass::AltTabAppInfoTransmitterClass(AltTabAppMonitorClass* monitor)
{	
	/* init the alt tab app monitor */
	this->monitor = monitor;

	/* init the http server */
	listener = http_listener(uri(AltTabAppInfoTransmitterClass::URL));
	listener.support(methods::GET, 
		std::tr1::bind(&AltTabAppInfoTransmitterClass::handle_get,
		this,
		std::tr1::placeholders::_1));
}


AltTabAppInfoTransmitterClass::~AltTabAppInfoTransmitterClass()
{
}

void AltTabAppInfoTransmitterClass::setMonitor(AltTabAppMonitorClass * monitor)
{
	this->monitor = monitor;
}

void AltTabAppInfoTransmitterClass::start()
{
	//this->active.store(true);
	//this->communicationThread = std::thread(&AltTabAppInfoTransmitterClass::test, this);
	this->listener.open().wait();
}

void AltTabAppInfoTransmitterClass::stop()
{
	//this->active.store(false);
	//this->communicationThread.join();
	this->listener.close().wait();
}
