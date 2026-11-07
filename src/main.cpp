#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <time.h>
#include <WiFiUdp.h>

// =========================================================================
// TETAPAN RANGKAIAN WI-FI & FIREBASE (SILA UBAH DI SINI)
// =========================================================================
#define WIFI_SSID      "Wokwi-GUEST"
#define WIFI_PASSWORD  ""

// Rujuk Web Firebase: Buang "https://" dan "/" di hujung URL
#define FIREBASE_HOST "https://lesson-10---rfid-default-rtdb.asia-southeast1.firebasedatabase.app"
// Rujuk Web Firebase: Project Settings -> Service Accounts -> Database Secrets
#define FIREBASE_AUTH "AIzaSyDebrGqhhyhw-FZCEm-mOzEwZ0joDKM9rk"

// =========================================================================
// TAKRIFAN PIN & PERKAKASAN
// =========================================================================
#define SS_PIN    5   // Pin SDA RFID
#define RST_PIN   15  // Pin RST RFID
#define RELAY_PIN 4   // Pin Relay Pintu

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

// Objek Perkakasan
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MFRC522 rfid(SS_PIN, RST_PIN);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Tetapan NTP Server Baharu
const char* ntpServer = "time.google.com"; // Server Google lebih pantas
const long  gmtOffset_sec = 28800;         // Waktu Malaysia (+8 jam)
const int   daylightOffset_sec = 0;

void showOLED(String line1, String line2, String line3) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(line1);
  display.setCursor(0, 22);
  display.println(line2);
  display.setCursor(0, 44);
  display.println(line3);
  display.display();
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); 

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Gagal mengesan OLED!"));
    for (;;);
  }

  SPI.begin(18, 19, 23, 5);
  rfid.PCD_Init();

  showOLED("SISTEM KEHADIRAN", "Menyambung Wi-Fi...", "");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Bersambung!");
  
  // =======================================================
  // KEMAS KINI: Guna configTime (Sangat laju & stabil)
  // =======================================================
  showOLED("SISTEM KEHADIRAN", "Mendapatkan Masa...", "Sila Tunggu");
  Serial.print("Mendapatkan masa...");
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  // getLocalTime akan menahan proses sehingga masa sebenar dimuat turun
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nMasa berjaya dikemas kini!");
  // =======================================================

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  showOLED("SISTEM KEHADIRAN", "Sila Tap Kad Anda", "Status: Sedia");

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String cardUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) cardUID += "0";
    cardUID += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) cardUID += " ";
  }
  cardUID.toUpperCase();
  Serial.println("UID Diimbas: " + cardUID);

  String cleanUID = cardUID;
  cleanUID.replace(" ", "");

  bool isRegistered = false;
  if (Firebase.getBool(fbdo, "/whitelist/" + cleanUID)) {
    isRegistered = fbdo.boolData();
  }

  // =======================================================
  // KEMAS KINI: Dapatkan Tarikh & Masa dari sistem terbina
  // =======================================================
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Ralat: Gagal membaca masa");
    return; // Batal tindakan jika tiba-tiba hilang masa
  }

  time_t epochTime;
  time(&epochTime);

  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
  String masaSemasa = String(timeStringBuff);

  char dateStringBuff[50];
  strftime(dateStringBuff, sizeof(dateStringBuff), "%e-%m-%Y", &timeinfo);
  String tarikh = String(dateStringBuff);
  tarikh.trim(); 

  // =======================================================

  if (isRegistered) {
    String logKey = tarikh + "_" + cleanUID; 
    String logPath = "/log_kehadiran/" + logKey;
    
    // Semak jika pelajar ini SUDAH daftar masuk hari ini
    if (Firebase.getString(fbdo, logPath + "/masaMasuk")) {
      
      // Dapatkan setem masa (Epoch dalam saat) sewaktu tap terakhir
      int epochTerdahulu = 0;
      if (Firebase.getInt(fbdo, logPath + "/timestamp")) {
        epochTerdahulu = fbdo.intData();
      }

      // Kira beza masa sekarang dengan tap sebelumnya
      int bezaMasaSaat = epochTime - epochTerdahulu;

      if (bezaMasaSaat <= 300) {
        // --- LOGIK 1: BAWAH 5 MINIT (Tulis Ganti Masa Masuk) ---
        Serial.println("STATUS: KEMAS KINI MASA MASUK (Abaikan Double Tap)");
        showOLED("MASUK DIKEMAS KINI", "Masa: " + masaSemasa, "Sila Masuk Kelas");
        
        // Kemas kini masaMasuk dan reset semula 'timestamp' ke masa terbaharu
        Firebase.setString(fbdo, logPath + "/masaMasuk", masaSemasa);
        Firebase.setInt(fbdo, logPath + "/timestamp", epochTime);
        
      } else {
        // --- LOGIK 2: LEBIH 5 MINIT (Mula Kira Masa Keluar) ---
        Serial.println("STATUS: DAFTAR KELUAR");
        showOLED("DAFTAR KELUAR", "Masa: " + masaSemasa, "Terima Kasih!");
        
        // Jika pelajar tap berkali-kali sewaktu balik, ia akan sentiasa kemas kini masaKeluar terbaharu
        Firebase.setString(fbdo, logPath + "/masaKeluar", masaSemasa);
      }

    } else {
      // --- LOGIK 3: DAFTAR MASUK BAHARU (Tap Pertama Hari Ini) ---
      Serial.println("STATUS: DAFTAR MASUK");
      showOLED("DAFTAR MASUK", "Masa: " + masaSemasa, "Selamat Datang!");
      
      FirebaseJson jsonLog;
      jsonLog.set("uid", cardUID);
      jsonLog.set("tarikh", tarikh);
      jsonLog.set("masaMasuk", masaSemasa);
      jsonLog.set("masaKeluar", "-"); 
      jsonLog.set("timestamp", epochTime); // Simpan setem masa untuk kiraan 5 minit
      
      Firebase.setJSON(fbdo, logPath, jsonLog);
    }

    // Buka Pintu / Relay
    digitalWrite(RELAY_PIN, LOW); 
    delay(3000); 
    digitalWrite(RELAY_PIN, HIGH);

  } else {
    // --- LOGIK KAD BELUM DIDAFTAR ---
    Serial.println("STATUS: KAD BELUM DIDAFTAR");
    showOLED("AKSES DITOLAK", "Kad Belum Didaftar", "Sila Rujuk Guru");
    
    // Hantar ke Web untuk Auto-Fill (TIDAK rekod dalam jadual)
    Firebase.setString(fbdo, "/kad_terkini", cardUID);
    delay(3000);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}