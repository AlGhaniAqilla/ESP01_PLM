#define tes 0
#define VHMS_UNIT 0
#define VHMS_DOWNLOADER 0
#define PLM_HOULER 1
#define PLM_LOADER 0

#if tes
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Ticker.h>

String unitID = "HD 4123";

// uint8_t macTerpilih[] = {0x78, 0x1C, 0x3C, 0xA5, 0x14, 0x05};
uint8_t macTerpilih[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t macCek[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
int sinyal;
bool scan = false;

typedef struct struct_message {
  char kodeHD[10];
  float TONASE;
} struct_message;
struct_message dataOut;

Ticker TesLoader;

void scanKomplit(int numWifi){
	int cekSinyal = -200;
	byte jmlTerpilih = 0;
	for (int i = 0; i < numWifi; ++i){
		if(WiFi.SSID(i).startsWith("EX")){
			jmlTerpilih += 1;
			if(WiFi.RSSI(i) > cekSinyal){
				cekSinyal = WiFi.RSSI(i);
				memcpy(macCek, WiFi.BSSID(i), 6);
			}
		}
	}

	if(jmlTerpilih > 0){
		memcpy(macTerpilih, macCek, 6);
		sinyal = cekSinyal;
		
		esp_now_send(macTerpilih, (uint8_t *) &dataOut, sizeof(dataOut));
		scan = true;
	}
	// scan = true;
}

void sendTonase() {
	strcpy(dataOut.kodeHD, unitID.c_str()); 
	float tonase = random(1299);
	dataOut.TONASE = tonase / 10;
	// esp_now_send(macTerpilih, (uint8_t *) &dataOut, sizeof(dataOut));

	if(scan){
		esp_now_send(macTerpilih, (uint8_t *) &dataOut, sizeof(dataOut));
	} else {
		WiFi.scanNetworksAsync(scanKomplit);
	}
}

void onDataSend(uint8_t * mac, uint8_t sendStatus){
	if (sendStatus == 0){
		// Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		// Serial.println(esp_now_get_peer_channel(mac));
	}
	else{
		// Serial.println("Delivery fail"); // Kirim ulang
		esp_now_send(macTerpilih, (uint8_t *) &dataOut, sizeof(dataOut));
	}
}

// void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
// 	char buf[len];
// 	memcpy(buf, incomingData, len);

// 	Serial.print("MAC: "); Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
// 	Serial.println(buf); Serial.println();
// }

void setup(){
	Serial.begin(19200);

	WiFi.mode(WIFI_STA);
	
	esp_now_init();
	esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
	esp_now_register_send_cb(onDataSend);
	// esp_now_register_recv_cb(OnDataRecv);

	TesLoader.attach(2, sendTonase);
}

void loop(){ delay(1); }
#endif

#if VHMS_DOWNLOADER
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncUDP.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Ticker.h>

String unitID = "VHMS_DOWNLOADER";
String targetUnit = "kodeUnit";
String broadcastStat;
const char * ssid = "RML-NA";
const char * password = "integrity";

AsyncUDP udp;
uint16_t VHMSport = 61993;

AsyncClient *client = new AsyncClient;
bool statTCP = false;


uint32_t timeSerial;
String dataSerialHEX;
const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

Ticker broadcastUDP;

void infoUDP() {
	if(statTCP){
		broadcastStat = unitID + " <=> " + targetUnit;
	} else {
		broadcastStat = unitID + " => " + targetUnit;
	}
	udp.broadcastTo((uint8_t *)broadcastStat.c_str(), broadcastStat.length(), VHMSport);
}

static void replyToServer(void *arg, const char *data) {
	AsyncClient* client = reinterpret_cast<AsyncClient*>(arg);

	// send reply
	if (client->canSend()){
		client->write(data, strlen(data));
		client->send();
	}
}

/* event callbacks */
static void handleData(void* arg, AsyncClient* client, void *data, size_t len) {
	char *charData = static_cast<char*>(data); //simpan data tcp ke char

	size_t byteCount = len / 2;	//var jumlah byte dari data yaitu setengahnya karna hex menyimpan 2 huruf
	uint8_t *byteArray = new uint8_t[byteCount];

	for(size_t i=0; i < byteCount; i++){	// perulangan untuk mengambil data ke byte
		char hexPair[3] = { charData[i*2], charData[i*2+1], '\0' }; //memasukan array data  ke penggabung dengan \0 sebagai treminator
		byteArray[i] = (uint8_t) strtol(hexPair, NULL, 16);
		Serial.write(byteArray[i]);
	}
	delete[] byteArray; // untuk mengahapus setelah di tampilkan
}

void onConnect(void* arg, AsyncClient* client) {
	statTCP = true;
}

void onDisconnect(void* arg, AsyncClient* client) {
	statTCP = false;
	String namaHost = targetUnit + ".local";
	client->connect(namaHost.c_str() , VHMSport);
}

void serialEventRun(){	// membaca serial
	while (Serial.available()){
		byte serialIn = Serial.read();
		dataSerialHEX += hex[serialIn / 16];	// data dari serial dalam byte di simpan ke hex pertama
		dataSerialHEX += hex[serialIn % 16];	// data dari serial dalam byte di simpan ke hex kedua
		timeSerial = millis();						// pewaktu di samakan dengan milis untung menghitung timer
	}
}

void setup() {
  Serial.begin(9600);

  LittleFS.begin();
  File file = LittleFS.open("/unit.txt", "r");
  if (file) {
  unitID = file.readString();
  }
  file.close();

  file = LittleFS.open("/CONNECT.txt", "r");
  if (file) {
  targetUnit = file.readString();
  }
  file.close();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);

  if(udp.listen(VHMSport)) {
        udp.onPacket([](AsyncUDPPacket packet) {
			if(!packet.isMulticast() && !packet.isBroadcast()){
				String dataIn = String((char *)packet.data()).substring(0, packet.length());
				if(dataIn.startsWith("CHANGE: ")){
					unitID = dataIn.substring(8);

					File file = LittleFS.open("/unit.txt", "w");
					if (file) {
						file.print(unitID);
					}
					file.close();
				}

				if(dataIn.startsWith("CONNECT: ")){
					targetUnit = dataIn.substring(9);

					File file = LittleFS.open("/CONNECT.txt", "w");
					if (file) {
						file.print(targetUnit);
					}
					file.close();
				}
			}
		});
	}

	broadcastUDP.attach(2, infoUDP);
	MDNS.begin(unitID);
	MDNS.addService("VHMS_DOWNLOADER", "tcp", VHMSport);
	MDNS.addService("VHMS_IoT", "udp", VHMSport);

	String HostName = targetUnit + ".local";
	client->onData(&handleData, client);
	client->onConnect(&onConnect, client);
	client->connect(HostName.c_str(), VHMSport);
	client->onDisconnect(&onDisconnect, client);
}

void loop() {
	MDNS.update();

	if (millis() - timeSerial > 100 && dataSerialHEX.length() > 0){	// mengirim pesan ke TCP server
		replyToServer(client, dataSerialHEX.c_str());	// dikirim dalam bentuk string berformat hex
		dataSerialHEX = "";
	}
	delay(1);
}
#endif

#if VHMS_UNIT
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncUDP.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Ticker.h>

String unitID = "VHMS_UNIT";

const char * ssid = "RML-NA";
const char * password = "integrity";

AsyncUDP udp;
uint16_t VHMSport = 61993;
AsyncClient *CLIENT = NULL;
AsyncServer* server = NULL;
bool terhubung = false;

uint32_t timeSerial;
String dataSerialHEX;
const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};


Ticker broadcastUDP;

void infoUDP() {
	if(terhubung){
		String stat = unitID + " DOWNLOADING";
		udp.broadcastTo((uint8_t *)stat.c_str(), stat.length(), VHMSport);
	} else {
		udp.broadcastTo((uint8_t *)unitID.c_str(), unitID.length(), VHMSport);
	}
}

/* clients events */
static void handleData(void* arg, AsyncClient* client, void *data, size_t len) {
	char *charData = static_cast<char*>(data); //simpan data tcp ke char
	size_t byteCount = len / 2;	//var jumlah byte dari data yaitu setengahnya karna hex menyimpan 2 huruf
	uint8_t *byteArray = new uint8_t[byteCount];
	for(size_t i=0; i < byteCount; i++){	// perulangan untuk mengambil data ke byte
		char hexPair[3] = { charData[i*2], charData[i*2+1], '\0' }; //memasukan array data  ke penggabung dengan \0 sebagai treminator
		byteArray[i] = (uint8_t) strtol(hexPair, NULL, 16);
		Serial.write(byteArray[i]);
	}
	delete[] byteArray; // untuk mengahapus setelah di tampilkan
}

/* server events */
static void handleNewClient(void* arg, AsyncClient* client) {
	terhubung = true;
	CLIENT = client;

	// register events
	client->onData(&handleData, NULL);
}

void serialEventRun(){	// membaca serial
	while (Serial.available()){
		byte serialIn = Serial.read();
		dataSerialHEX += hex[serialIn / 16];	// data dari serial dalam byte di simpan ke hex pertama
		dataSerialHEX += hex[serialIn % 16];	// data dari serial dalam byte di simpan ke hex kedua
		timeSerial = millis();
	}
}

void setup() {
  Serial.begin(19200);

  LittleFS.begin();
  File file = LittleFS.open("/unit.txt", "r");
  if (file) {
  unitID = file.readString();
  }
  file.close();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.onStationModeConnected([](const WiFiEventStationModeConnected& event) {
	server->onClient(&handleNewClient, server);
	server->begin();
  });
  WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
	terhubung = false;
	WiFi.begin(ssid, password);
  });

  server = new AsyncServer(VHMSport);
  server->onClient(&handleNewClient, server);
  server->begin();

  if(udp.listen(VHMSport)) {
        udp.onPacket([](AsyncUDPPacket packet) {
			if(!packet.isMulticast() && !packet.isBroadcast()){
				String dataIn = String((char *)packet.data()).substring(0, packet.length());
				if(dataIn.startsWith("CHANGE: ")){
					unitID = dataIn.substring(8);

					File file = LittleFS.open("/unit.txt", "w");
					if (file) {
						file.print(unitID);
					}
					file.close();
				}
			}
		});
	}

	broadcastUDP.attach(2, infoUDP);
	MDNS.begin(unitID);
	MDNS.addService("VHMS_IoT", "tcp", VHMSport);
	MDNS.addService("VHMS_IoT", "udp", VHMSport);
}

void loop() {
	MDNS.update();

	if(terhubung){
		if(timeSerial - millis() > 100){
			if(CLIENT->canSend()  && dataSerialHEX.length() > 0){
				CLIENT->add(dataSerialHEX.c_str(), strlen(dataSerialHEX.c_str()));
				CLIENT->send();
				dataSerialHEX = "";
			}
		}
	}

	delay(1);
}
#endif

#if PLM_HOULER
#include <Arduino.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncUDP.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Ticker.h>

String unitID = "HOULER";

const char * ssid = "RML-NA";
const char * password = "integrity";

AsyncUDP udp;
uint16_t PLMport = 62104;

uint8_t macTerpilih[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t macCek[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
int sinyal;
bool scan = false;

typedef struct struct_message {
  char kodeHD[10];
  float TONASE;
} struct_message;
struct_message dataOut;

#define STX 0x02 // 2
#define ETX 0x03 // 3
#define DLE 0x10 // 16

uint8_t dataTersimpan[250];
uint8_t dataIndex;

bool readStat = true;
bool stxStat = false;
bool dleStat = false;
bool etxStat = false;

#define lock 0
#define hidup 0
#define mati  1
bool over = false;
float batas = 100.0;
bool lockStat = false;
uint32_t timer;
uint32_t serialTimer;

uint16_t berat2byte(uint8_t high, uint8_t low){
	uint16_t masa;
	masa = (uint16_t)(((low << 8) | high) & 0x3FF);
	return masa;
}

Ticker broadcastUDP;

void infoUDP() { udp.broadcastTo((uint8_t *)unitID.c_str(), unitID.length(), PLMport); }


void scanKomplit(int numWifi){
	int cekSinyal = -200;
	byte jmlTerpilih = 0;
	for (int i = 0; i < numWifi; ++i){
		if(WiFi.SSID(i).startsWith("EX")){
			jmlTerpilih += 1;
			if(WiFi.RSSI(i) > cekSinyal){
				cekSinyal = WiFi.RSSI(i);
				memcpy(macCek, WiFi.BSSID(i), 6);
			}
		}
	}

	if(jmlTerpilih > 0){
		memcpy(macTerpilih, macCek, 6);
		sinyal = cekSinyal;
		
		esp_now_send(macTerpilih, (uint8_t *) &dataOut, sizeof(dataOut));
	}
	scan = true;
	readStat = true;
}

// DATA total muatan
void P4Mode(){
	strcpy(dataOut.kodeHD, unitID.c_str());
	dataOut.TONASE = (float)berat2byte(dataTersimpan[3], dataTersimpan[4]) / 10;

	over = dataOut.TONASE > batas ? true : false;
	// over = berat2byte(dataTersimpan[13], dataTersimpan[14]) / 10 > 100 ? true : false;

	readStat = true;
	esp_now_send(macTerpilih, (uint8_t *) &dataOut, sizeof(dataOut));
	String dataOutUDP = String(dataOut.kodeHD) + ": " + String(dataOut.TONASE, 1) + " Ton";
	udp.broadcastTo((uint8_t *)dataOutUDP.c_str(), dataOutUDP.length(), PLMport);
}

// DATA per Bucket
void M4Mode(){
	strcpy(dataOut.kodeHD, unitID.c_str());
	dataOut.TONASE = (float)berat2byte(dataTersimpan[3], dataTersimpan[4]) / 10;

	over = dataOut.TONASE > batas ? true : false;
	// over = berat2byte(dataTersimpan[13], dataTersimpan[14]) / 10 > 100 ? true : false;

	String dataOutUDP = String(dataOut.kodeHD) + ": " + String(dataOut.TONASE, 1) + " Ton";
	udp.broadcastTo((uint8_t *)dataOutUDP.c_str(), dataOutUDP.length(), PLMport);

	if(scan){
		readStat = true;
		esp_now_send(macTerpilih, (uint8_t *) &dataOut, sizeof(dataOut));
	} else {
		WiFi.scanNetworksAsync(scanKomplit);
	}
}

// DATA DUMPING
void LMode(){
	char dataOutUDP[300];
	sprintf(dataOutUDP,
			"%s Lmode Date: %d / %d %d:%d, Tonase: %.1f, EtravTime: %.1f, EtravDis: %d Km, EmaxSpeed: %d km/j, EaveSpeed: %d km/j, EstopTime: %.1f, Time Load: %.1f, LtravTime: %.1f, LtravDis: %d Km, LmaxSpeed: %d km/j, LaveSpeed: %d km/j, LstopTime: %.1f, DumpTime: %d",
			unitID.c_str(), dataTersimpan[3], dataTersimpan[4],  dataTersimpan[5],  dataTersimpan[6],	// tanggal
			(float)berat2byte(dataTersimpan[9], dataTersimpan[10]) / 10,									// tonase
			(float)berat2byte(dataTersimpan[11], dataTersimpan[12]) / 10,									// empty travel time
			dataTersimpan[13], dataTersimpan[14],  dataTersimpan[15],										// empty distance, max speed, average speed
			(float)berat2byte(dataTersimpan[16], dataTersimpan[17]) / 10,									// empty stop time
			(float)berat2byte(dataTersimpan[18], dataTersimpan[19]) / 10,									// load time
			(float)berat2byte(dataTersimpan[20], dataTersimpan[21]) / 10,									// load travel time
			dataTersimpan[22], dataTersimpan[23],  dataTersimpan[24],										// load distance, max speed, average speed
			(float)berat2byte(dataTersimpan[25], dataTersimpan[26]) / 10, 									// load stop time
			dataTersimpan[27]																				// dump time
			);
	udp.broadcastTo((uint8_t*)dataOutUDP, sizeof(dataOutUDP), PLMport);
}

void olahData(){
	if(dataTersimpan[0] == 80 && dataTersimpan[1] == 52){ //0x50, 0x34
		// Serial.println("P4 Mode");
		P4Mode();
	} else if(dataTersimpan[0] == 77 && dataTersimpan[1] == 52){ //0x4D, 0x34
		// Serial.println("M4 Mode");
		M4Mode();
	} else if(dataTersimpan[0] == 76){ //0x4C
		// Serial.println("L Mode");
		LMode();
	} else{
		readStat = true;
		memset(dataTersimpan, 0, sizeof(dataTersimpan));
	}

	stxStat = false;
	etxStat = false;
}

void processSerialData() {
	// 1. readPaket = true
	if (Serial.available() && readStat) {
		serialTimer = millis();
		uint8_t byteRead = Serial.read();

		// 2. jika byte = 02h, stxPacket = false, byte adalah stx data
		if(byteRead == STX && !stxStat){
			stxStat = true;
			// serialIndex = 0;
			dataIndex = 0;
			// serialBuffer[serialIndex++] = byteRead;
		}
		// 3. setelah stxStat true cek ke eksekusi dle
		else {
			//4. jika dleStat == true, byte adalah data
			if(dleStat){
				// serialBuffer[serialIndex++] = byteRead; // untuk menyimpan semua byte hingga etx
				dataTersimpan[dataIndex++] = byteRead;  // untuk menyimpan data saja
				dleStat = false;
			}
			// dleStat == false
			else {
				// cek jika byte = DLE dan dlePacket = false
				// byte = dle dan ubah dlePacket ke true
				// byte adalah dle
				if(byteRead == DLE && !dleStat){
					dleStat = true;
					// serialBuffer[serialIndex++] = byteRead;
				}
				// jika byte = ETX dan etxPacket = false
				// byte adalah etx
				else if(byteRead == ETX  && !etxStat){
					etxStat = true;
					// serialBuffer[serialIndex++] = byteRead;
				}
				// cek jika etxPacket = true byte adalah bcc, jika bukan byte adalah data
				else {
					// etxStat == true, tanda akhir data dan cek bcc /  ceksum
					if(etxStat){
						// bccRead = byteRead;
						olahData();
						Serial1.write(0x02); Serial1.write(0x06); Serial1.write(0x31); Serial1.write(0x03); Serial1.write(0x36);
					}
					// karna bukan dle bukan etx, maka byte adalah data
					else {
						// serialBuffer[serialIndex++] = byteRead; // untuk menyimpan semua byte hingga etx
						dataTersimpan[dataIndex++] = byteRead;  // untuk byte ke data
					}
				}		 
			}
		}
	}
}

void onDataSend(uint8_t * mac, uint8_t sendStatus){
	if (sendStatus == 0){
		// Serial.println("Delivery success");
	}
	else{
		// Serial.println("Delivery fail"); // Kirim ulang
		esp_now_send(macTerpilih, (uint8_t *) &dataOut, sizeof(dataOut));
	}
}

void setup(){
	Serial.begin(9600);
	pinMode(lock, OUTPUT);
	digitalWrite(lock, HIGH);

	LittleFS.begin();
    File file = LittleFS.open("/unit.txt", "r");
    if (file) {
		unitID = file.readString();
    }
    file.close();

	file = LittleFS.open("/lock.txt", "r");
    if (file) {
		if(file.readString() != "0"){
			lockStat = true;
			batas = file.readString().toFloat();
		} else {
			lockStat = false;
		}
    }
    file.close();

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	WiFi.setAutoReconnect(true);
	
	esp_now_init();
	esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
	esp_now_register_send_cb(onDataSend);

	timer = millis();

	if(udp.listen(PLMport)) {
        udp.onPacket([](AsyncUDPPacket packet) {
			if(!packet.isMulticast() && !packet.isBroadcast()){
				String dataIn = String((char *)packet.data()).substring(0, packet.length());
				if(dataIn.startsWith("CHANGE: ")){
					unitID = dataIn.substring(8);

					File file = LittleFS.open("/unit.txt", "w");
					if (file) {
						file.print(unitID);
					}
					file.close();
				} else if(dataIn.startsWith("over: ")){
					unitID = dataIn.substring(6);

					File file = LittleFS.open("/lock.txt", "w");
					if (file) {
						file.print(unitID);
					}
					file.close();
				}
			}
		});
	}

	broadcastUDP.attach(2, infoUDP);
	MDNS.begin(unitID);
	MDNS.addService("PLM_IoT", "udp", PLMport);
}

void loop() {
	MDNS.update();
	processSerialData();

	if(Serial.available() == 0 && (millis() - serialTimer > 500)){
		// Serial.write(2); Serial.write(77); Serial.write(50); Serial.write(3); Serial.write(126);
		Serial.flush();
		stxStat = false;
		serialTimer = millis();
	}
	
	if(lockStat){
		if(over){
			digitalWrite(lock, hidup);
		} else {
			digitalWrite(lock, mati);
		}
	}

	if(millis() - timer > 15 * 60000){
		scan = false;
	}

	delay(1);
}
#endif

#if PLM_LOADER
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESPAsyncUDP.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Ticker.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

String unitID = "LOADER";
const char * passwordAP = "songolaS";

const char * ssid = "RML-NA";
const char * password = "integrity";

AsyncUDP udp;
uint16_t PLMport = 62104;

uint8_t macLED[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
int sinyal;
bool scan = false;
volatile bool newData = false;

typedef struct struct_message {
  char kodeHD[10];
  float tonase;
} struct_message;
struct_message dataIN;

LiquidCrystal_I2C lcd(0x27, 16, 2);
volatile uint32_t timer;

uint16_t berat2byte(uint8_t high, uint8_t low){
	uint16_t masa;
	masa = (uint16_t)(((low << 8) | high) & 0x3FF);
	return masa;
}

Ticker broadcastUDP;

void infoUDP() { udp.broadcastTo((uint8_t *)unitID.c_str(), unitID.length(), PLMport); }

void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
    memcpy(&dataIN, incomingData, sizeof(dataIN));
	newData = true;

    // lcd.clear();
    // lcd.setCursor(1, 0);
    // lcd.print(dataIN.kodeHD);
    // lcd.setCursor(12, 1);
    // lcd.print("TON");
    // lcd.setCursor(6, 1);
    // lcd.print(dataIN.tonase, 1);

    // esp_now_send(macLED, (uint8_t *) &dataIN, sizeof(dataIN));

	// Serial.print("MAC: "); Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n", macLED[0], macLED[1], macLED[2], macLED[3], macLED[4], macLED[5]);
	// Serial.print(dataIN.kodeHD); Serial.print(" "); Serial.print(dataIN.tonase); Serial.println("Ton");

	timer = millis();
}

void onDataSend(uint8_t * mac, uint8_t sendStatus){
	if (sendStatus == 0){
		// Serial.println("Delivery success");
	}
	else{
		// Serial.println("Delivery fail"); // Kirim ulang
		esp_now_send(macLED, (uint8_t *) &dataIN, sizeof(dataIN));
	}
}

void setup(){
	Serial.begin(19200);

	LittleFS.begin();
    File file = LittleFS.open("/unit.txt", "r");
    if (file) {
		unitID = file.readString();
    }
    file.close();

	file = LittleFS.open("/IP.bin", "r");
    if (file) {
		file.read(macLED, 6);
    }
    file.close();

	WiFi.mode(WIFI_AP_STA);
	WiFi.softAP(unitID, passwordAP);
	WiFi.begin(ssid, password);
	
	WiFi.setAutoReconnect(true);
	
	esp_now_init();
	esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
	esp_now_register_send_cb(onDataSend);
	esp_now_register_recv_cb(OnDataRecv);

	timer = millis();

	if(udp.listen(PLMport)) {
        udp.onPacket([](AsyncUDPPacket packet) {
			if(!packet.isMulticast() && !packet.isBroadcast()){
				String dataIn = String((char *)packet.data()).substring(0, packet.length());
				if(dataIn.startsWith("CHANGE: ")){
					unitID = dataIn.substring(8);

					File file = LittleFS.open("/unit.txt", "w");
					if (file) {
						file.print(unitID);
					}
					file.close();
				} else if(dataIn.startsWith("LED: ")){
					String newMAC = dataIn.substring(5);

					sscanf(newMAC.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &macLED[0], &macLED[1], &macLED[2], &macLED[3], &macLED[4], &macLED[5]);
					File file = LittleFS.open("/IP.bin", "w");
					if (file) {
						file.write(macLED, 6);
					}
					file.close();
				}
			}
		});
	}

	broadcastUDP.attach(2, infoUDP);
	MDNS.begin(unitID);
	MDNS.addService("PLM_IoT", "udp", PLMport);

	Wire.begin(2, 0);
	lcd.begin();
	byte lenUnit = unitID.length();
    lcd.setCursor(8 - (lenUnit / 2), 0);
	lcd.print(unitID.c_str());
    lcd.setCursor(0, 1);
    lcd.print("Created by Ton@y");
}

void loop() {
	MDNS.update();
	if(millis() - timer > (60000 * 10)){
        timer = millis();
        byte lenUnit = unitID.length();
        lcd.setCursor(8 - (lenUnit / 2), 0);
        lcd.print(unitID.c_str());
        lcd.setCursor(0, 1);
        lcd.print("Created by Ton@y");
    }

	if(newData){
		lcd.clear();
		lcd.setCursor(1, 0);
		lcd.print(dataIN.kodeHD);
		lcd.setCursor(12, 1);
		lcd.print("TON");
		lcd.setCursor(6, 1);
		lcd.print(dataIN.tonase, 1);

		esp_now_send(macLED, (uint8_t *) &dataIN, sizeof(dataIN));
		newData = false;
	}
	delay(1);
}
#endif
