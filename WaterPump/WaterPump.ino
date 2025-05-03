/*
 Name:		WaterPump.ino
 Created:	5/3/2025 1:22:42 PM
 Author:	Ehsan Zeidabadi
*/


#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <esp_mac.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <Update.h>

// متغیرهای سراسری
Preferences preferences;
uint32_t chipId = 0;
int DeviceId = 1;
String MacAddress = "";
String SerialNumber = "";

// تنظیمات وای‌فای
char ssid[32] = "";
char password[64] = "";
IPAddress staticIP, gateway, subnet, preferredDNS, alternateDNS;

// سرور وب برای پیکربندی (در حالت Access Point)
WebServer server(80);

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
String getInterfaceMacAddress(esp_mac_type_t interface);

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

    // تنظیم پین‌های خروجی
    pinMode(WaterPumpRelay, OUTPUT);

    // تنظیم پین‌های ورودی با پول‌آپ داخلی
    pinMode(WaterPumpPin, INPUT_PULLUP);

    // تنظیم حالت اولیه رله‌ها
    digitalWrite(WaterPumpRelay, LOW); // غیر فعال

    MacAddress = getInterfaceMacAddress(ESP_MAC_ETH);
    Serial.print("Mac Address is: ");
    Serial.println(MacAddress);

    // تولید شناسه یکتا برای چیپ با استفاده از MAC
    for (int i = 0; i < 17; i += 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    SerialNumber = String(chipId);
    Serial.print("Chip ID: ");
    Serial.println(SerialNumber);

    // بارگذاری اطلاعات ذخیره‌شده از NVS
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

    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
        }, []() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                Serial.printf("Update: %s\n", upload.filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
                    Update.printError(Serial);
                }
            }
            else if (upload.status == UPLOAD_FILE_WRITE) {
                /* flashing firmware to ESP*/
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                }
            }
            else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) { //true to set the size to the current progress
                    Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
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
        page += "<tr><td colspan=2><center><fontsize=3><b>MAC Address: " + MacAddress + "</b></font></center><br></td><tr>";
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
        page += "<tr><td colspan=2><center><fontsize=3><b>MAC Address: " + MacAddress + "</b></font></center><br></td><tr>";
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
    preferences.begin("wifi", false);
    preferences.putString("ssid", String(ssid));
    preferences.putString("password", String(password));
    preferences.putString("staticIP", staticIP.toString());
    preferences.putString("gateway", gateway.toString());
    preferences.putString("subnet", subnet.toString());
    preferences.putString("preferredDNS", preferredDNS.toString());
    preferences.putString("alternateDNS", alternateDNS.toString());
    preferences.end();
}

void loadWiFiConfig() {
    preferences.begin("wifi", true);

    String tmp = preferences.getString("ssid", "");
    tmp.toCharArray(ssid, sizeof(ssid));

    tmp = preferences.getString("password", "");
    tmp.toCharArray(password, sizeof(password));

    tmp = preferences.getString("staticIP", "");
    staticIP.fromString(tmp);

    tmp = preferences.getString("gateway", "");
    gateway.fromString(tmp);

    tmp = preferences.getString("subnet", "");
    subnet.fromString(tmp);

    tmp = preferences.getString("preferredDNS", "");
    preferredDNS.fromString(tmp);

    tmp = preferences.getString("alternateDNS", "");
    alternateDNS.fromString(tmp);

    preferences.end();
}

void sendJsonToServer() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClient client;
        HTTPClient http;

        http.begin(client, "http://api.coldroom.ashatechnic.com/v1/Api/AddDetails");
        http.addHeader("Content-Type", "application/json");

        StaticJsonDocument<200> doc;
        doc["deviceId"] = DeviceId;
        doc["boardSensorTemperature"] = dht_val[0];
        doc["coldRoomTemperature"] = dht_val[1];
        doc["coldRoomStatus"] = WaterPumpRelayStatus;
        doc["defrostStatus"] = false;
        doc["motorStatus"] = WaterPumpStatus;

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
                    if (doc["coldRoomStatus"] != waterPumpRelayStatus) {
                        waterPumpRelayStatus = doc["coldRoomStatus"];
                        Serial.print("coldRoomStatus: ");
                        Serial.println(waterPumpRelayStatus ? "true" : "false");
                        if (waterPumpRelayStatus) {
                            digitalWrite(WaterPumpRelay, LOW); // فعال
                        }
                        else {
                            digitalWrite(WaterPumpRelay, HIGH); // غیر فعال
                        }
                    }
                    if (doc["motorStatus"] != waterPumpStatus) {
                        waterPumpStatus = doc["motorStatus"];
                        Serial.print("motorStatus: ");
                        Serial.println(waterPumpStatus ? "true" : "false");
                    }
                    if (doc["coldRoomTimerStatus"] != waterPumpTimerStatus) {
                        waterPumpTimerStatus = doc["coldRoomTimerStatus"];
                        Serial.print("coldRoomTimerStatus: ");
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

String getInterfaceMacAddress(esp_mac_type_t interface) {
    String mac = "";
    unsigned char mac_base[6] = { 0 };

    if (esp_read_mac(mac_base, interface) == ESP_OK) {
        char buffer[18]; // فرمت: AA:BB:CC:DD:EE:FF + null termination
        sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac_base[0], mac_base[1], mac_base[2],
            mac_base[3], mac_base[4], mac_base[5]);
        mac = buffer;
    }
    return mac;
}
