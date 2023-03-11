#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <WebSocketsServer.h>
#include <NBY_TwilioArduino.h>

/*Thiết lập tài khoản, mật khẩu cho Wifi*/
#define WIFI_SSID "DucCoding"
#define WIFI_PASSWORD "20082k36"

// #define WIFI_SSID "P407"
// #define WIFI_PASSWORD "17052000"

/*Thiết lập địa chỉ cho Gateway*/
WiFiServer server(80);
WiFiClient client;

IPAddress AP_LOCAL_IP(192, 1, 1, 1);
IPAddress AP_GATEWAY_IP(192, 1, 1, 1);
IPAddress AP_NETWORK_MASK(255, 255, 255, 0);

/*Thiết lập bảo mật cho Firebase*/
#define API_KEY "AIzaSyBwmuegG8iA377Dz97NKz_9UOvhReVQJtk"
#define USER_EMAIL "haiductlhp@gmail.com"
#define USER_PASSWORD "Hai572001@@"
#define DATABASE_URL "https://smartlock-fc05a-default-rtdb.asia-southeast1.firebasedatabase.app/"

using namespace std;

vector<string> pathNode = {"_", "controls", "locks", "sensors"};
vector<bool> statusNodeUpdateData = {false, false, false, false};

// Define Firebase objects
FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;
String listenerPath = "mac";

int numReceiver;

using namespace std;

struct NodeLock
{
	bool isOpen;
	bool hasCameraRequest;
	string password;
	bool isWarning;
	bool isAntiThief;
	int status;
};

NodeLock nodeLock;


uint32_t time1;
uint32_t timeDelayCall;

bool isDataReceiver = false;
string dataReceiverNode = "";
vector<int> idNodeConnect = {-1, -1, -1, -1};
bool isFirstCall = true;

void initWiFi();
void initFirebase();
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void updateDataToFirebase(int nodeNeedUpdate);
void print_string(string str);

string receiveData();
void handleDataReceiverNode(string dataReceiver);

WebSocketsServer webSocket = WebSocketsServer(81);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
	string dataReceiver(payload, payload + length);
	switch (type)
	{
	case WStype_DISCONNECTED:									//Sự kiện khi client ngắt kết nối
		Serial.printf("[%u] Disconnected!\n", num);
		break;
	case WStype_CONNECTED:										//Sự kiện khi client kết nối
	{
		IPAddress ip = webSocket.remoteIP(num);
		numReceiver = num;
		Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
		webSocket.sendTXT(num, "Connected");
	}
	break;
	case WStype_TEXT:											//Sự kiện nhận được khi nhận được thông điệp dạng Text
		// Serial.printf("[%u] Message: %s\n", num, payload);

		dataReceiverNode = dataReceiver;
		isDataReceiver = true;
		print_string(dataReceiver);
		//handleDataReceiverNode(dataReceiver);
		// bool isDataReceiver = true;
		// webSocket.sendTXT(num, "Echo: " + String((char *)payload));
		break;
	}
}
void setup()
{
	Serial.begin(9600);
	WiFi.mode(WIFI_MODE_APSTA);
	WiFi.softAPdisconnect();
	Serial.println(WiFi.macAddress());
	initWiFi();
	initFirebase();

	WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY_IP, AP_NETWORK_MASK);
	WiFi.softAP("Gateway", "password");
	server.begin();

	delay(3000);
	time1 = millis();

	webSocket.begin();
	webSocket.onEvent(webSocketEvent);
	idNodeConnect.push_back(0);
	// updateDataToFirebase(2);
	// Serial.println(ESP.getFreeHeap());
}

void loop()
{
	webSocket.loop();
	if(isDataReceiver == true)
	{
		handleDataReceiverNode(dataReceiverNode);
		
		updateDataToFirebase(2);
		dataReceiverNode  = "";
		isDataReceiver = false;
	}
	if (nodeLock.isWarning == true)
	{
		if (millis() - timeDelayCall > 120000 || isFirstCall == true)
		{
			isFirstCall = false;
			timeDelayCall = millis();
		}
	}
}

void initWiFi()											//Khởi tạo Wifi 
{
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	Serial.print("Connecting to WiFi ..");
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print('.');
		delay(1000);
	}
	Serial.println(WiFi.localIP());
	Serial.println();
}

void initFirebase()										//Khởi tạo Firebase
{
	config.api_key = API_KEY;
	auth.user.email = USER_EMAIL;
	auth.user.password = USER_PASSWORD;
	config.database_url = DATABASE_URL;
	Firebase.reconnectWiFi(true);

	config.token_status_callback = tokenStatusCallback; 		// bắt trạng thái được phản hồi của token
	config.max_token_generation_retry = 5;

	Firebase.begin(&config, &auth);

	if (!Firebase.RTDB.beginStream(&stream, listenerPath.c_str()))
		Serial.printf("stream begin error, %s\n\n", stream.errorReason().c_str());

	Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
}

/*Lấy data từ Firebase gửi về cho Client*/
void streamCallback(FirebaseStream data)
{
	string path = data.dataPath().c_str();
	print_string(path);
	Serial.println(data.stringData());
	String dataToSend = data.stringData();
	webSocket.sendTXT(numReceiver, dataToSend);
	Serial.println();
}

void streamTimeoutCallback(bool timeout)
{
	if (timeout)
		Serial.println("stream timeout, resuming...\n");
	if (!stream.httpConnected())
		Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

/*Update data từ Client lên Firebase*/
void updateDataToFirebase(int nodeNeedUpdate)
{
	FirebaseData fbdo;
	FirebaseJson json;
	string path = "mac"; // add

	json.add("hasCameraRequest", nodeLock.hasCameraRequest);
	json.add("isAntiThief", nodeLock.isAntiThief);
	json.add("isOpen", nodeLock.isOpen);
	json.add("isWarning", nodeLock.isWarning);
	json.add("password", nodeLock.password);
	Firebase.RTDB.setJSON(&fbdo, path, &json);
	fbdo.~FirebaseData();
}

/*Phân tách Json nhận được để lấy data bỏ vào JsonDocument*/
void handleDataReceiverNode(string dataReceiver)
{
	DynamicJsonDocument dataReceiverJson(1024);
	deserializeJson(dataReceiverJson, dataReceiver);

	nodeLock.isWarning = dataReceiverJson[F("isWarning")].as<bool>();
	nodeLock.password = dataReceiverJson[F("password")].as<string>();
	nodeLock.isOpen = dataReceiverJson[F("isOpen")].as<bool>();
	nodeLock.hasCameraRequest = dataReceiverJson[F("hasCameraRequest")].as<bool>();
	nodeLock.isAntiThief = dataReceiverJson[F("isAntiThief")].as<bool>();
	//updateDataToFirebase(2);
}

void print_string(string str)
{
	for (auto i : str)
	{
		Serial.print(i);
	}
	Serial.println();
}