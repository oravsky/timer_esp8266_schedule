  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <LittleFS.h>
  #include <ArduinoJson.h>
  #include <time.h>
  #include <ESP8266mDNS.h>
  
  // Konfigurasi WiFi
  const char* ssid = "YourSsid"; //input ssid
  const char* password = "YourPassword"; //input ssid password
  
  
  // NTP Server
  const char* ntpServer = "pool.ntp.org";
  const long  gmtOffset_sec = 7 * 3600;  // GMT+7
  const int   daylightOffset_sec = 0;
  const int   RELAY_PIN = D1;
  unsigned long lastCheck = 0;
  const unsigned long interval = 10000; // 10 detik
  // Web server ESP8266
  ESP8266WebServer server(80);
  
  // Fungsi untuk membaca jadwal dari LittleFS
  String readSchedules() {
      File file = LittleFS.open("/schedules.txt", "r");
      if (!file) return "[]"; 
      String schedules = file.readString();
      file.close();
      return schedules;
  }
  
  // Fungsi untuk menyimpan jadwal ke LittleFS
  void saveSchedules(String schedules) {
      File file = LittleFS.open("/schedules.txt", "w");
      if (file) {
          file.print(schedules);
          file.close();
      }
  }
  
  // Halaman HTML utama
  void handleRoot() {
      server.send(200, "text/html", R"rawliteral(
        <!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Jadwal Pompa Kebun Lombok</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            background-color: #f8f9fa;
            padding: 20px;
        }
        .container {
            max-width: 500px;
            margin: auto;
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        }
        h2, h3 {
            color: #333;
        }
        #currentTime {
            font-size: 1.5em;
            margin-bottom: 10px;
            color: #007bff;
        }
        ul {
            list-style: none;
            padding: 0;
        }
        li {
            background: #e9ecef;
            padding: 10px;
            margin: 5px 0;
            border-radius: 5px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        button {
            background: red;
            color: white;
            border: none;
            padding: 5px 10px;
            border-radius: 5px;
            cursor: pointer;
            transition: 0.3s;
        }
        button:hover {
            background: darkred;
        }
        input {
            width: 50px;
            padding: 5px;
            margin: 5px;
            border: 1px solid #ccc;
            border-radius: 5px;
            text-align: center;
        }
        .btn-add {
            background: green;
            padding: 8px 12px;
            border-radius: 5px;
            color: white;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>TIMER OTOMATIS</h2>
        <h3> Waktu Saat Ini </h3>
        <p id="currentTime"></p>
        <form onsubmit="addSchedule(event)">
            <label>Mulai: </label>
            <input type="number" id="startHour" min="0" max="23" required placeholder="HH"> :
            <input type="number" id="startMinute" min="0" max="59" required placeholder="MM">
            <br>
            <label>Selesai: </label>
            <input type="number" id="endHour" min="0" max="23" required placeholder="HH"> :
            <input type="number" id="endMinute" min="0" max="59" required placeholder="MM">
            <br>
            <button type="submit" class="btn-add">Tambahkan Jadwal</button>
        </form>
        <h3>Jadwal Aktif</h3>
        <ul id="scheduleList"></ul>
    </div>

    <script>
        function updateCurrentTime() {
            const now = new Date();
            document.getElementById("currentTime").innerText = now.toLocaleTimeString();
        }
        setInterval(updateCurrentTime, 1000);
        updateCurrentTime();

        function loadSchedules() {
            fetch("/getSchedules").then(response => response.json()).then(data => {
                const list = document.getElementById("scheduleList");
                list.innerHTML = "";
                data.forEach((schedule, index) => {
                    const item = document.createElement("li");
                    item.innerHTML = `<strong>${schedule.startHour}:${schedule.startMinute} - ${schedule.endHour}:${schedule.endMinute}</strong>
                    <button onclick="deleteSchedule(${index})">Hapus</button>`;
                    list.appendChild(item);
                });
            });
        }

        function addSchedule(event) {
            event.preventDefault();
            const startHour = document.getElementById("startHour").value;
            const startMinute = document.getElementById("startMinute").value;
            const endHour = document.getElementById("endHour").value;
            const endMinute = document.getElementById("endMinute").value;
            
            fetch("/addSchedule", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ startHour, startMinute, endHour, endMinute })
            }).then(() => {
                loadSchedules();
            });
        }

        function deleteSchedule(index) {
            fetch(`/deleteSchedule?index=${index}`).then(() => {
                loadSchedules();
            });
        }

        window.onload = loadSchedules;
    </script>
</body>
</html>

      )rawliteral");
  }
  
  // API untuk mengambil jadwal
 void handleGetSchedules() {
    File file = LittleFS.open("/schedules.txt", "r");
    if (!file) {
        server.send(200, "application/json", "[]");
        return;
    }
    server.streamFile(file, "application/json");
    file.close();
}
  
  // API untuk menambahkan jadwal
  void handleAddSchedule() {
      if (server.hasArg("plain")) {
          String body = server.arg("plain");
          String schedules = readSchedules();
          if (schedules == "[]") schedules = "[" + body + "]";
          else schedules = schedules.substring(0, schedules.length() - 1) + "," + body + "]";
          saveSchedules(schedules);
      }
      server.send(200, "text/plain", "OK");
  }
  
  // API untuk menghapus jadwal
  void handleDeleteSchedule() {
      if (server.hasArg("index")) {
          int index = server.arg("index").toInt();
          String schedules = readSchedules();
          DynamicJsonDocument doc(1024);
          deserializeJson(doc, schedules);
          JsonArray arr = doc.as<JsonArray>();
          arr.remove(index);
          String newSchedules;
          serializeJson(arr, newSchedules);
          saveSchedules(newSchedules);
      }
      server.send(200, "text/plain", "Deleted");
  }
  
  // Sinkronisasi waktu dengan NTP
  void syncTimeWithNTP() {
      Serial.println("Menghubungkan ke NTP...");
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
          Serial.println("Waktu berhasil diperbarui dari NTP:");
          Serial.printf("%02d:%02d:%02d %02d-%02d-%d\n",
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                        timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      } else {
          Serial.println("Gagal memperbarui waktu dari NTP.");
      }
  }
  
  // Menampilkan waktu lokal dari RTC/NTP
  
  void checkRelay() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return; // Gagal mendapatkan waktu

    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;

    String schedules = readSchedules();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, schedules);
    JsonArray arr = doc.as<JsonArray>();

    bool relayShouldBeOn = false;

    for (JsonObject schedule : arr) {
        int startHour = schedule["startHour"];
        int startMinute = schedule["startMinute"];
        int endHour = schedule["endHour"];
        int endMinute = schedule["endMinute"];

        if ((currentHour > startHour || (currentHour == startHour && currentMinute >= startMinute)) &&
            (currentHour < endHour || (currentHour == endHour && currentMinute <= endMinute))) {
            relayShouldBeOn = true;
            break;
        }
    }

    digitalWrite(RELAY_PIN, relayShouldBeOn ? LOW : HIGH); // Active Low
}
  
  // Setup ESP8266
  void setup() {
      Serial.begin(115200);
      
      // Mulai WiFi
      WiFi.begin(ssid, password);
      int attempt = 0;
      while (WiFi.status() != WL_CONNECTED && attempt < 99) {
          delay(1000);
          Serial.print(".");
          attempt++;
      }
      Serial.println(WiFi.localIP());
      pinMode(RELAY_PIN, OUTPUT);
      digitalWrite(RELAY_PIN, HIGH);
      
      
      if (WiFi.status() == WL_CONNECTED) {
          Serial.println("\nWiFi Connected!");
          syncTimeWithNTP(); // Sinkronisasi waktu dengan NTP jika terhubung WiFi
      } else {
          Serial.println("\nGagal terhubung ke WiFi, menggunakan waktu RTC lokal.");
      }
  
      // Mulai LittleFS
      if (!LittleFS.begin()) {
          Serial.println("LittleFS Mount Failed");
      }
      if (MDNS.begin("kebunlombok")) {
        Serial.println("mdns responder started, akses di:kebunlombok.local");
        Serial.println(WiFi.localIP());
      } else {
        Serial.println("gagal memulai mDNS");
      }
  
      // Konfigurasi server
      server.on("/", handleRoot);
      server.on("/getSchedules", handleGetSchedules);
      server.on("/addSchedule", HTTP_POST, handleAddSchedule);
      server.on("/deleteSchedule", handleDeleteSchedule);
      server.begin();
  }
  
  void loop() {
      server.handleClient();
      MDNS.update();
  if (millis() - lastCheck >= interval) {
        lastCheck = millis();
        checkRelay();
    }
}

