/*
 Name:		WaterPump.ino
 Created:	5/3/2025 16:06:42 PM
 Author:	Ehsan Zeidabadi
*/


#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdate.h>
#include <EEPROM.h>

// متغیرهای سراسری
uint32_t chipId = 0;
int DeviceId = 2;
String SerialNumber = "";

// ساختار تنظیمات WiFi
struct WiFiConfig {
    char ssid[32];           // 31 کاراکتر + null terminator
    char password[64];       // 63 کاراکتر + null terminator
    char staticIP[16];       // xxx.xxx.xxx.xxx + null terminator
    char gateway[16];
    char subnet[16];
    char preferredDNS[16];
    char alternateDNS[16];
};


// تنظیمات وای‌فای
char ssid[32] = "";
char password[64] = "";
IPAddress staticIP, gateway, subnet, preferredDNS, alternateDNS;

// سرور وب برای پیکربندی (در حالت Access Point)
ESP8266WebServer server(80);


const char* host = "esp8266ashatechnic";

unsigned long previousMillis = 0;  // نگهداری زمان آخرین فراخوانی.
const unsigned long interval = 30000;  // فاصله زمانی 30 ثانیه (در میلی‌ثانیه)

unsigned long getStatusPreviousMillis = 0;  // نگهداری زمان آخرین فراخوانی.
const unsigned long getStatusInterval = 8000;  // فاصله زمانی 8 ثانیه (در میلی‌ثانیه)

// تعریف پین‌های خروجی
#define WaterPumpRelay D1

// تعریف پین‌های ورودی
#define WaterPumpPin D3

// تعریف متغیرهای وضعیت برای ارسال به سرور
bool WaterPumpRelayStatus = false;
bool WaterPumpStatus = false;

// متغیرهایی برای دریافت اطلاعات JSON
int deviceId = 0;
bool waterPumpRelayStatus = false;
bool waterPumpStatus = false;
bool waterPumpTimerStatus = false;

// اعلام توابع
bool waitForWiFiConnection();
void startAccessPoint();
void startWiFiConfig();
void saveWiFiConfig();
void loadWiFiConfig();
void sendJsonToServer();
void receiveJsonFromServer();
void getStatus();

// صفحه لاگین
const char* loginIndex =
"<form name='loginForm'>"
"<table width='20%' bgcolor='A09F9F' align='center'>"
"<tr>"
"<td colspan=2>"
"<center><font size=4><b>ESP32_AshaTechnic Login Page</b></font></center>"
"<br>"
"</td>"
"<br>"
"<br>"
"</tr>"
"<td>Username:</td>"
"<td><input type='text' size=25 name='userid'><br></td>"
"</tr>"
"<br>"
"<br>"
"<tr>"
"<td>Password:</td>"
"<td><input type='Password' size=25 name='pwd'><br></td>"
"<br>"
"<br>"
"</tr>"
"<tr>"
"<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
"</tr>"
"</table>"
"</form>"
"<script>"
"function check(form)"
"{"
"if(form.userid.value=='AshaTechnic' && form.pwd.value=='2025Ehsan1983')"
"{"
"window.open('/serverIndex')"
"}"
"else"
"{"
" alert('Error Password or Username')/*displays error message*/"
"}"
"}"
"</script>";



//صفحه موجود در وب سرور برای نمایش

const char* serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update'>"
"<input type='submit' value='Update'>"
"</form>"
"<div id='prg'>progress: 0%</div>"
"<script>"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
" $.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!')"
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"</script>";


// the setup function runs once when you press reset or power the board
void setup() {
    //////delay(120000);// wait 2 minuts for wifi conecting
    Serial.begin(115200);
    EEPROM.begin(512);
    // تنظیم پین‌های خروجی
    pinMode(WaterPumpRelay, OUTPUT);

    // تنظیم پین‌های ورودی با پول‌آپ داخلی
    pinMode(WaterPumpPin, INPUT_PULLUP);

    // تنظیم حالت اولیه رله‌ها
    digitalWrite(WaterPumpRelay, LOW); // غیر فعال

    chipId = ESP.getChipId();
    SerialNumber = String(chipId);
    Serial.println(SerialNumber);

    // بارگذاری اطلاعات ذخیره‌شده از ایپرام
    loadWiFiConfig();

    // اگر اطلاعات IP از قبل تنظیم شده باشد از آنها استفاده کنیم
    if (staticIP.toString() != String("0.0.0.0")) {
        WiFi.config(staticIP, gateway, subnet, preferredDNS, alternateDNS);
    }

    WiFi.begin(ssid, password);

    if (!waitForWiFiConnection()) {
        // در صورت عدم اتصال به وای‌فای، حالت Access Point را شروع می‌کنیم تا کاربر بتواند اطلاعات را تنظیم کند
        startAccessPoint();
    }
    else {
        Serial.println("اتصال موفق به وای‌فای برقرار شد.");
        Serial.print("آی‌پی دستگاه: ");
        Serial.println(WiFi.localIP());
    }

    // از اسم زیر برای نام هاست استفاده می کنیم
    if (!MDNS.begin(host)) { //http://esp8266ashatechnic.local
        Serial.println("Error setting up MDNS responder!");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("mDNS responder started");

    server.on("/", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", loginIndex);
        });
    server.on("/serverIndex", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", serverIndex);
        });

    // سرور برای به‌روزرسانی از طریق HTTP POST
    server.on("/update", HTTP_POST, []() {
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
        }, []() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                Serial.printf("Update: %s\n", upload.filename.c_str());
                uint32_t maxSketchSpace = ESP.getFreeSketchSpace();
                if (!Update.begin(maxSketchSpace)) {
                    Update.printError(Serial);
                }
            }
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                }
            }
            else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) {
                    Serial.printf("Update Success: %u bytes\nRebooting...\n", upload.totalSize);
                }
                else {
                    Update.printError(Serial);
                }
            }
            });
        startWiFiConfig();
        server.begin();
}

// the loop function runs over and over again until power down or reset
void loop() {
    server.handleClient();

    // خواندن زمان حال حاضر با استفاده از millis()
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        if (WiFi.status() == WL_CONNECTED) {
            receiveJsonFromServer(); // دریافت اطلاعات از سرور (GET)
        }
    }

    // بررسی وضعیت پین‌ها؛ ارسال اطلاعات به سرور تنها در صورت تغییر وضعیت
    unsigned long getStatusCurrentMillis = millis();
    if (getStatusCurrentMillis - getStatusPreviousMillis >= getStatusInterval) {
        getStatusPreviousMillis = getStatusCurrentMillis;
        getStatus();
    }
}

bool waitForWiFiConnection() {
    for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
        delay(1000);
        Serial.println("در حال تلاش برای اتصال به وای‌فای...");
    }
    return (WiFi.status() == WL_CONNECTED);
}

void startAccessPoint() {
    WiFi.softAP("AshaTechnicAP");

    server.on("/", HTTP_GET, []() {
        String page = "<!DOCTYPE html><html><head>";
        page += "<meta charset='UTF-8'>";
        page += "<style>";
        page += "body { direction: rtl; text-align: right; background-color: #f4f4f4; padding: 20px; }";
        page += "form { background: white; padding: 15px; border-radius: 5px; }";
        page += "label { display: block; margin: 10px 0 5px; font-size: 16px; }";
        page += "input, select { width: 100%; padding: 8px; margin-bottom: 10px; }";
        page += "button { padding: 10px; background-color: #4CAF50; color: white; border: none; cursor: pointer; }";
        page += "</style></head><body>";
        page += "<h1>تنظیمات وای‌فای</h1>";
        page += "<br>";
        page += "<tr><td colspan=2><center><fontsize=3><b>Serial Number: " + SerialNumber + "</b></font></center><br></td><tr>";
        page += "<br>";
        //page += "<tr><td colspan=2><center><fontsize=3><b>MAC Address: " + MacAddress + "</b></font></center><br></td><tr>";
        page += "<br>";
        page += "<form method='POST' action='/save'>";
        page += "<label for='ssid'>نام شبکه (SSID):</label>";
        page += "<select id='ssid' name='ssid'>";

        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; i++) {
            page += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
        }

        page += "</select>";
        page += "<label for='password'>رمز عبور:</label>";
        page += "<input type='password' id='password' name='password'>";
        page += "<label for='staticIP'>آی‌پی استاتیک:</label>";
        page += "<input type='text' id='staticIP' name='staticIP'>";
        page += "<label for='gateway'>گیت‌وی:</label>";
        page += "<input type='text' id='gateway' name='gateway'>";
        page += "<label for='subnet'>زیرشبکه:</label>";
        page += "<input type='text' id='subnet' name='subnet'>";
        page += "<label for='preferredDNS'>Preferred DNS:</label>";
        page += "<input type='text' id='preferredDNS' name='preferredDNS'>";
        page += "<label for='alternateDNS'>Alternate DNS:</label>";
        page += "<input type='text' id='alternateDNS' name='alternateDNS'>";
        page += "<button type='submit'>ذخیره اطلاعات</button>";
        page += "</form></body></html>";

        server.send(200, "text/html", page);
        });

    server.on("/save", HTTP_POST, []() {
        strcpy(ssid, server.arg("ssid").c_str());
        strcpy(password, server.arg("password").c_str());
        staticIP.fromString(server.arg("staticIP"));
        gateway.fromString(server.arg("gateway"));
        subnet.fromString(server.arg("subnet"));
        preferredDNS.fromString(server.arg("preferredDNS"));
        alternateDNS.fromString(server.arg("alternateDNS"));

        saveWiFiConfig();

        server.send(200, "text/plain", "اطلاعات ذخیره شدند. لطفاً دستگاه را ری‌استارت کنید.");
        delay(1000);
        ESP.restart();
        });

    server.begin();
    Serial.println("Access point started.");
    Serial.print("آی‌پی دستگاه: ");
    Serial.println(WiFi.localIP());
}

void startWiFiConfig() {
    server.on("/wificonfig", HTTP_GET, []() {
        String page = "<!DOCTYPE html><html><head>";
        page += "<meta charset='UTF-8'>";
        page += "<style>";
        page += "body { direction: rtl; text-align: right; background-color: #f4f4f4; padding: 20px; }";
        page += "form { background: white; padding: 15px; border-radius: 5px; }";
        page += "label { display: block; margin: 10px 0 5px; font-size: 16px; }";
        page += "input, select { width: 100%; padding: 8px; margin-bottom: 10px; }";
        page += "button { padding: 10px; background-color: #4CAF50; color: white; border: none; cursor: pointer; }";
        page += "</style></head><body>";
        page += "<h1>تنظیمات وای‌فای</h1>";
        page += "<br>";
        page += "<tr><td colspan=2><center><fontsize=3><b>Serial Number: " + SerialNumber + "</b></font></center><br></td><tr>";
        page += "<br>";
        //page += "<tr><td colspan=2><center><fontsize=3><b>MAC Address: " + MacAddress + "</b></font></center><br></td><tr>";
        page += "<br>";
        page += "<form method='POST' action='/save'>";
        page += "<label for='ssid'>نام شبکه (SSID):</label>";
        page += "<select id='ssid' name='ssid'>";

        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; i++) {
            page += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
        }

        page += "</select>";
        page += "<label for='password'>رمز عبور:</label>";
        page += "<input type='password' id='password' name='password'>";
        page += "<label for='staticIP'>آی‌پی استاتیک:</label>";
        page += "<input type='text' id='staticIP' name='staticIP'>";
        page += "<label for='gateway'>گیت‌وی:</label>";
        page += "<input type='text' id='gateway' name='gateway'>";
        page += "<label for='subnet'>زیرشبکه:</label>";
        page += "<input type='text' id='subnet' name='subnet'>";
        page += "<label for='preferredDNS'>Preferred DNS:</label>";
        page += "<input type='text' id='preferredDNS' name='preferredDNS'>";
        page += "<label for='alternateDNS'>Alternate DNS:</label>";
        page += "<input type='text' id='alternateDNS' name='alternateDNS'>";
        page += "<button type='submit'>ذخیره اطلاعات</button>";
        page += "</form></body></html>";

        server.send(200, "text/html", page);
        });

    server.on("/save", HTTP_POST, []() {
        strcpy(ssid, server.arg("ssid").c_str());
        strcpy(password, server.arg("password").c_str());
        staticIP.fromString(server.arg("staticIP"));
        gateway.fromString(server.arg("gateway"));
        subnet.fromString(server.arg("subnet"));
        preferredDNS.fromString(server.arg("preferredDNS"));
        alternateDNS.fromString(server.arg("alternateDNS"));

        saveWiFiConfig();

        server.send(200, "text/plain", "اطلاعات ذخیره شدند. لطفاً دستگاه را ری‌استارت کنید.");
        delay(1000);
        ESP.restart();
        });
}

void saveWiFiConfig() {
    // شروع و اختصاص حافظه برای EEPROM
    EEPROM.begin(EEPROM_SIZE);

    // انتقال اطلاعات تنظیمات به ساختار، با اطمینان از قطع شدن رشته (null termination)
    strncpy(wifiConfig.ssid, ssid.c_str(), sizeof(wifiConfig.ssid));
    wifiConfig.ssid[sizeof(wifiConfig.ssid) - 1] = '\0';

    strncpy(wifiConfig.password, password.c_str(), sizeof(wifiConfig.password));
    wifiConfig.password[sizeof(wifiConfig.password) - 1] = '\0';

    String ipStr = staticIP.toString();
    strncpy(wifiConfig.staticIP, ipStr.c_str(), sizeof(wifiConfig.staticIP));
    wifiConfig.staticIP[sizeof(wifiConfig.staticIP) - 1] = '\0';

    String gwStr = gateway.toString();
    strncpy(wifiConfig.gateway, gwStr.c_str(), sizeof(wifiConfig.gateway));
    wifiConfig.gateway[sizeof(wifiConfig.gateway) - 1] = '\0';

    String subnetStr = subnet.toString();
    strncpy(wifiConfig.subnet, subnetStr.c_str(), sizeof(wifiConfig.subnet));
    wifiConfig.subnet[sizeof(wifiConfig.subnet) - 1] = '\0';

    String pDnsStr = preferredDNS.toString();
    strncpy(wifiConfig.preferredDNS, pDnsStr.c_str(), sizeof(wifiConfig.preferredDNS));
    wifiConfig.preferredDNS[sizeof(wifiConfig.preferredDNS) - 1] = '\0';

    String aDnsStr = alternateDNS.toString();
    strncpy(wifiConfig.alternateDNS, aDnsStr.c_str(), sizeof(wifiConfig.alternateDNS));
    wifiConfig.alternateDNS[sizeof(wifiConfig.alternateDNS) - 1] = '\0';

    // ذخیره کل ساختار در EEPROM از آدرس 0
    EEPROM.put(0, wifiConfig);

    // اطمینان از انتقال داده به حافظه فلاش
    EEPROM.commit();

    // پایان کار با EEPROM
    EEPROM.end();
}


void loadWiFiConfig() {
    // شروع کار با EEPROM با تخصیص حافظه لازم
    EEPROM.begin(EEPROM_SIZE);

    WiFiConfig wifiConfig;
    // خواندن اطلاعات ساختار ذخیره‌شده از آدرس 0
    EEPROM.get(0, wifiConfig);

    // خاتمه کار با EEPROM
    EEPROM.end();

    // انتقال اطلاعات خوانده شده به متغیرهای global
    strcpy(ssid, wifiConfig.ssid);
    strcpy(password, wifiConfig.password);

    // توجه کنید که toString() نیاز به نوع String دارد. از سازنده String برای تبدیل آرایه کاراکتری استفاده می‌کنیم.
    staticIP.fromString(String(wifiConfig.staticIP));
    gateway.fromString(String(wifiConfig.gateway));
    subnet.fromString(String(wifiConfig.subnet));
    preferredDNS.fromString(String(wifiConfig.preferredDNS));
    alternateDNS.fromString(String(wifiConfig.alternateDNS));
}


void sendJsonToServer() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClient client;
        HTTPClient http;

        http.begin(client, "http://api.coldroom.ashatechnic.com/v1/Api/AddDetails");
        http.addHeader("Content-Type", "application/json");

        StaticJsonDocument<200> doc;
        doc["deviceId"] = DeviceId;
        doc["WaterPumpRelayStatus"] = WaterPumpRelayStatus;
        doc["WaterPumpStatus"] = WaterPumpStatus;

        String requestBody;
        serializeJson(doc, requestBody);

        int httpResponseCode = http.POST(requestBody);

        if (httpResponseCode > 0) {
            Serial.printf("پاسخ سرور: %d\n", httpResponseCode);
            Serial.println(http.getString());
        }
        else {
            Serial.printf("خطا: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
    }
}

void receiveJsonFromServer() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = "http://api.coldroom.ashatechnic.com/v1/Api/GetDetails/" + String(DeviceId);
        http.begin(url);

        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
            String payload = http.getString();
            Serial.println("داده دریافتی از سرور:");
            Serial.println(payload);

            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (!error) {
                // استخراج اطلاعات از JSON دریافتی
                deviceId = doc["deviceId"];

                if (deviceId == DeviceId) {
                    Serial.print("deviceId: ");
                    Serial.println(deviceId);
                    if (doc["WaterPumpRelayStatus"] != waterPumpRelayStatus) {
                        waterPumpRelayStatus = doc["WaterPumpRelayStatus"];
                        Serial.print("WaterPumpRelayStatus: ");
                        Serial.println(waterPumpRelayStatus ? "true" : "false");
                        if (waterPumpRelayStatus) {
                            digitalWrite(WaterPumpRelay, LOW); // فعال
                        }
                        else {
                            digitalWrite(WaterPumpRelay, HIGH); // غیر فعال
                        }
                    }
                    if (doc["WaterPumpStatus"] != waterPumpStatus) {
                        waterPumpStatus = doc["WaterPumpStatus"];
                        Serial.print("WaterPumpStatus: ");
                        Serial.println(waterPumpStatus ? "true" : "false");
                    }
                    if (doc["waterPumpTimerStatus"] != waterPumpTimerStatus) {
                        waterPumpTimerStatus = doc["waterPumpTimerStatus"];
                        Serial.print("waterPumpTimerStatus: ");
                        Serial.println(waterPumpTimerStatus ? "true" : "false");
                    }
                }
            }
            else {
                Serial.print("خطا در تجزیه JSON: ");
                Serial.println(error.c_str());
            }
        }
        else {
            Serial.printf("خطا در دریافت اطلاعات، کد خطا: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
    }
}

void getStatus() {
    bool statusChanged = false; // تغییر وضعیت در این حلقه یا خیر؟

    // خواندن یکبار وضعیت پین‌ها
    int motorVal = digitalRead(WaterPumpPin);

    bool currentMotorStatus = (motorVal == LOW);

    // تعیین وضعیت ColdRoomStatus براساس مقادیر خوانده شده
    bool currentColdRoomStatus;
    if (motorVal == HIGH) {
        currentColdRoomStatus = false;
    }
    else {
        currentColdRoomStatus = true;
    }

    // بررسی تغییر وضعیت ColdRoomStatus
    if (currentColdRoomStatus != WaterPumpRelayStatus) {
        WaterPumpRelayStatus = currentColdRoomStatus;
        Serial.print("تغییر وضعیت ColdRoomStatus به: ");
        Serial.println(WaterPumpRelayStatus ? "true" : "false");
        statusChanged = true;
    }

    // بررسی تغییر وضعیت پین MotorPin
    if (currentMotorStatus != WaterPumpStatus) {
        WaterPumpStatus = currentMotorStatus;
        Serial.print("تغییر وضعیت MotorStatus به: ");
        Serial.println(WaterPumpStatus ? "true" : "false");
        statusChanged = true;
    }

    // اگر در طول اجرای تابع وضعیت یکی از موارد تغییر کرده باشد، یکبار sendJsonToServer اجرا شود.
    if (statusChanged) {
        sendJsonToServer();
    }
}
