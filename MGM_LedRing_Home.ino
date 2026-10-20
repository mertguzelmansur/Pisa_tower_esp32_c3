/*
  MGM_LedRing_Home.ino  -  v1.1
  ---------------------------------------------------------------
  ESP32-C3 0.42" OLED kart + 12'li WS2812B LED halka
  Apple Home (HomeKit) + Web paneli + kart butonu + OLED
  + iPhone BILDIRIM ISIGI (Bluetooth / ANCS)

  Donanim:
    Halka VCC -> 5V, GND -> GND, DIN -> GPIO4 (~330 ohm seri)
    OLED      -> dahili (SDA=GPIO5, SCL=GPIO6)
    Buton     -> BOOT butonu (GPIO9)

  Ilk kurulum (Wi-Fi + Apple Home):
    1) Kart "MGM-LED-Kurulum" Wi-Fi agini acar (sifre: mgm12345)
    2) Telefonla baglan, ev Wi-Fi'ni ve 8 haneli kurulum kodunu gir
    3) Apple Home > Aksesuar Ekle > Daha fazla secenek > "MGM LED Halka"

  iPhone bildirim isigi (bir kerelik eslestirme):
    iPhone > Ayarlar > Bluetooth > "MGM LED" e dokun > Eslestir >
    "Bildirimleri goster" sorusuna IZIN VER.
    Sonrasinda iPhone yakindayken otomatik baglanir. Bildirim gelince
    halka hangi efektte olursa olsun (kapali olsa bile) araya girer,
    uygulamanin renginde yanar, sonra kaldigi yere doner.
    Gelen aramada arama bitene kadar doner.
    Ac/kapat: Apple Home "Bildirim Isigi" anahtari veya web paneli.

  Web paneli: http://mgm-led.local  (Test butonu ile bildirim denenebilir)

  Buton:
    Tek tik     -> sonraki efekt (bildirim sirasinda: bildirimi kapatir)
    Cift tik    -> ac / kapat
    Uzun basma  -> parlaklik kademesi
    10 sn basma -> fabrika ayari (Wi-Fi, Home ve Bluetooth eslesmeleri silinir)

  Kutuphaneler: HomeSpan 2.x, NimBLE-Arduino 2.x, Adafruit NeoPixel, U8g2
  Kart: ESP32C3 Dev Module, Partition: "Huge APP (3MB No OTA)"

  MGM - 2026
  ---------------------------------------------------------------
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include <NimBLEDevice.h>
#include "HomeSpan.h"

// ================= Donanim =================
constexpr uint8_t  PIN_LED_DATA  = 4;
constexpr uint8_t  PIN_BUTTON    = 9;
constexpr uint8_t  PIN_OLED_SDA  = 5;
constexpr uint8_t  PIN_OLED_SCL  = 6;
constexpr uint8_t  PIN_BOARD_LED = 8;
constexpr uint16_t LED_COUNT     = 12;

// ================= Ayarlar =================
constexpr uint8_t  MAX_BRIGHT     = 130;      // %100 = 130/255 (USB akim siniri)
constexpr uint8_t  BTN_LEVELS[]   = {10, 30, 50, 75, 100};
constexpr uint8_t  NUM_BTN_LEVELS = sizeof(BTN_LEVELS);
constexpr uint32_t FRAME_MS        = 16;
constexpr uint32_t DEBOUNCE_MS     = 25;
constexpr uint32_t DOUBLE_CLICK_MS = 300;
constexpr uint32_t LONG_PRESS_MS   = 700;
constexpr uint32_t RESET_PRESS_MS  = 10000;
constexpr uint32_t SAVE_DELAY_MS   = 3000;
constexpr float    FADE_RATE       = 0.15f;

constexpr uint32_t NOTIF_MS        = 4500;    // normal bildirim suresi
constexpr uint32_t CALL_MAX_MS     = 45000;   // gelen arama en fazla
constexpr uint8_t  NOTIF_MIN_BRIGHT = 60;     // bildirim en az %60 parlaklikta

const char* AP_SSID     = "MGM-LED-Kurulum";
const char* AP_PASSWORD = "mgm12345";
const char* HOST_NAME   = "mgm-led";
const char* BLE_NAME    = "MGM LED";
constexpr uint16_t HAP_PORT = 8080;

// ================= Nesneler =================
Adafruit_NeoPixel ring(LED_COUNT, PIN_LED_DATA, NEO_GRB + NEO_KHZ800);
U8G2_SSD1306_72X40_ER_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA);
Preferences prefs;
WebServer web(80);

// ================= Durum =================
enum : uint8_t { M_SOLID = 0, M_RAINBOW, M_COMET, M_BREATHE, M_FIRE, M_SPARKLE, MODE_COUNT };

const char* const MODE_OLED[MODE_COUNT] = {"Sabit", "Gokkusagi", "Kuyruklu", "Nefes", "Ates", "Pirilti"};
const char* const MODE_HOME[MODE_COUNT] = {"Sabit", "Gökkuşağı", "Kuyruklu", "Nefes", "Ateş", "Pırıltı"};

struct State {
  bool    on     = true;
  uint8_t mode   = M_SOLID;
  uint8_t bright = 50;      // %
  float   hue    = 30.0f;   // 0-360
  float   sat    = 60.0f;   // 0-100
  bool    notif  = true;    // iPhone bildirim isigi
} st;

HS_STATUS hsStatus   = HS_WIFI_NEEDED;
bool      webStarted = false;
bool      oledDirty  = true;
bool      saveDirty  = false;
uint32_t  dirtySince = 0;
uint32_t  frameCount = 0;
uint32_t  lastFrame  = 0;
float     curBright  = 0.0f;
uint32_t  ipShowUntil = 0;

void markChanged() {
  oledDirty  = true;
  saveDirty  = true;
  dirtySince = millis();
}

// ================= Renk yardimcilari =================
uint32_t baseColor() {
  return ring.gamma32(ring.ColorHSV((uint16_t)(st.hue / 360.0f * 65535.0f),
                                    (uint8_t)(st.sat * 2.55f), 255));
}

uint32_t scaleColor(uint32_t c, float k) {
  k = constrain(k, 0.0f, 1.0f);
  return ring.Color((uint8_t)(((c >> 16) & 0xFF) * k),
                    (uint8_t)(((c >> 8) & 0xFF) * k),
                    (uint8_t)((c & 0xFF) * k));
}

// ================= Efektler =================
void fxSolid() { ring.fill(baseColor()); }

void fxRainbow() {
  const uint16_t base = (uint16_t)(frameCount * 256);
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    uint16_t hue = base + (uint16_t)((uint32_t)i * 65536UL / LED_COUNT);
    ring.setPixelColor(i, ring.gamma32(ring.ColorHSV(hue)));
  }
}

void fxComet() {
  static float head = 0.0f;
  constexpr float SPEED = 0.18f, TAIL = 6.0f;
  head += SPEED;
  if (head >= LED_COUNT) head -= LED_COUNT;
  const uint32_t c = baseColor();
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    float d = head - i;
    if (d < 0) d += LED_COUNT;
    float k = (d < TAIL) ? (1.0f - d / TAIL) : 0.0f;
    ring.setPixelColor(i, scaleColor(c, k * k));
  }
}

void fxBreathe() {
  const float t = millis() / 1000.0f;
  float k = (expf(sinf(t * PI / 2.0f)) - 0.36787944f) / 2.35040238f;
  ring.fill(scaleColor(baseColor(), 0.05f + 0.95f * k));
}

void fxFire() {
  static float   lvl[LED_COUNT] = {0};
  static uint8_t tgt[LED_COUNT] = {0};
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    if (random(6) == 0) tgt[i] = random(70, 256);
    lvl[i] += (tgt[i] - lvl[i]) * 0.3f;
    float k = lvl[i] / 255.0f;
    ring.setPixelColor(i, ring.Color((uint8_t)(255 * k), (uint8_t)(110 * k * k), 0));
  }
}

void fxSparkle() {
  static float v[LED_COUNT] = {0};
  for (uint16_t i = 0; i < LED_COUNT; i++) v[i] *= 0.90f;
  if (random(5) == 0) v[random(LED_COUNT)] = 1.0f;
  const uint32_t c = baseColor();
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    uint32_t bg = scaleColor(c, 0.06f);
    uint32_t fg = scaleColor(0xFFFFFF, v[i]);
    ring.setPixelColor(i, ring.Color(max((uint8_t)(bg >> 16), (uint8_t)(fg >> 16)),
                                     max((uint8_t)(bg >> 8),  (uint8_t)(fg >> 8)),
                                     max((uint8_t)bg,         (uint8_t)fg)));
  }
}

void (*const FX[MODE_COUNT])() = {fxSolid, fxRainbow, fxComet, fxBreathe, fxFire, fxSparkle};

// ================= Bildirim katmani =================
struct AppInfo { const char* id; const char* name; uint32_t color; };

const AppInfo APPS[] = {
  {"net.whatsapp.WhatsApp",     "WhatsApp",  0x25D366},
  {"net.whatsapp.WhatsAppSMB",  "WhatsApp",  0x25D366},
  {"com.apple.MobileSMS",       "Mesaj",     0x30D158},
  {"com.burbn.instagram",       "Instagram", 0xE1306C},
  {"ph.telegra.Telegraph",      "Telegram",  0x229ED9},
  {"com.apple.mobilephone",     "Telefon",   0x0A84FF},
  {"com.apple.facetime",        "FaceTime",  0x30D158},
  {"com.google.Gmail",          "Gmail",     0xEA4335},
  {"com.apple.mobilemail",      "Mail",      0x0A84FF},
  {"com.microsoft.Office.Outlook", "Outlook", 0x0078D4},
  {"com.apple.mobilecal",       "Takvim",    0xFF3B30},
  {"com.apple.reminders",       "Animsatici",0xFF9500},
  {"com.toyopagroup.picaboo",   "Snapchat",  0xFFE600},
  {"com.zhiliaoapp.musically",  "TikTok",    0x25F4EE},
  {"com.atebits.Tweetie2",      "X",         0xFFFFFF},
  {"com.facebook.Facebook",     "Facebook",  0x1877F2},
  {"com.facebook.Messenger",    "Messenger", 0x0084FF},
  {"com.google.ios.youtube",    "YouTube",   0xFF0000},
  {"com.spotify.client",        "Spotify",   0x1DB954},
  {"com.linkedin.LinkedIn",     "LinkedIn",  0x0A66C2},
  {"com.hepsiburada.ios",       "Hepsiburada", 0xFF6000},
  {"com.trendyol.app",          "Trendyol",  0xF27A1A},
};

// ANCS kategorileri: uygulama tanınmazsa bu renkler kullanilir
const AppInfo CATS[] = {
  {nullptr, "Bildirim",  0xFFFFFF},   // 0 Other
  {nullptr, "Arama",     0x0A84FF},   // 1 IncomingCall
  {nullptr, "Cevapsiz",  0xFF3B30},   // 2 MissedCall
  {nullptr, "Sesli Msj", 0xBF5AF2},   // 3 Voicemail
  {nullptr, "Sosyal",    0xBF5AF2},   // 4 Social
  {nullptr, "Takvim",    0xFF9500},   // 5 Schedule
  {nullptr, "E-posta",   0x0A84FF},   // 6 Email
  {nullptr, "Haber",     0xFFFFFF},   // 7 News
  {nullptr, "Saglik",    0xFF2D55},   // 8 Health
  {nullptr, "Finans",    0xFFD60A},   // 9 Finance
  {nullptr, "Konum",     0x30D158},   // 10 Location
  {nullptr, "Eglence",   0xFF375F},   // 11 Entertainment
};
constexpr uint8_t CAT_COUNT = sizeof(CATS) / sizeof(CATS[0]);
constexpr uint8_t CAT_CALL  = 1;

struct Overlay {
  bool     active = false;
  bool     call   = false;
  bool     ending = false;
  uint32_t uid    = 0;
  uint32_t start  = 0;
  uint32_t endAt  = 0;
  uint32_t color  = 0xFFFFFF;
  char     name[14] = "";
} ov;

void overlayStart(uint8_t cat, uint32_t uid) {
  if (ov.active && ov.call && cat != CAT_CALL) return;   // arama suruyorsa araya girme
  if (cat >= CAT_COUNT) cat = 0;
  ov.active = true;
  ov.call   = (cat == CAT_CALL);
  ov.ending = false;
  ov.uid    = uid;
  ov.start  = millis();
  ov.endAt  = ov.start + (ov.call ? CALL_MAX_MS : NOTIF_MS);
  ov.color  = CATS[cat].color;
  strlcpy(ov.name, CATS[cat].name, sizeof(ov.name));
  oledDirty = true;
}

void overlaySetApp(uint32_t uid, const char* appId) {
  if (!ov.active || ov.uid != uid) return;
  for (const AppInfo& a : APPS) {
    if (strcmp(appId, a.id) == 0) {
      ov.color = a.color;
      strlcpy(ov.name, a.name, sizeof(ov.name));
      oledDirty = true;
      return;
    }
  }
}

void overlayStop() {          // yumusak bitis
  if (!ov.active || ov.ending) return;
  ov.ending = true;
  ov.endAt  = millis() + 500;
}

// donus degeri: katman cizildiyse true
bool renderOverlay() {
  if (!ov.active) return false;
  const uint32_t now = millis();

  if (now >= ov.endAt && !ov.ending) { ov.ending = true; ov.endAt = now + 500; }
  if (ov.ending && now >= ov.endAt) {
    ov.active = false;
    curBright = 0.0f;           // normal efekt sifirdan yumusakca geri gelsin
    oledDirty = true;
    return false;
  }

  const uint8_t pct = max(st.bright, NOTIF_MIN_BRIGHT);
  ring.setBrightness((uint8_t)(pct * MAX_BRIGHT / 100));
  const uint32_t c = ring.gamma32(ov.color);
  const uint32_t t = now - ov.start;
  float master = 1.0f;
  if (ov.ending) master = (float)(ov.endAt - now) / 500.0f;

  if (ov.call) {
    // gelen arama: donen kuyruklu yildiz + nabiz
    const float head = fmodf(t / 70.0f, LED_COUNT);
    const float pulse = 0.55f + 0.45f * sinf(t / 1000.0f * 2.0f * PI * 1.2f);
    for (uint16_t i = 0; i < LED_COUNT; i++) {
      float d = head - i;
      if (d < 0) d += LED_COUNT;
      float k = (d < 5.0f) ? (1.0f - d / 5.0f) : 0.0f;
      k = max(k * k, 0.12f) * pulse;
      ring.setPixelColor(i, scaleColor(c, k * master));
    }
  } else if (t < 600) {
    // halka etrafinda dolma
    const float n = t / 600.0f * LED_COUNT;
    for (uint16_t i = 0; i < LED_COUNT; i++) {
      float k = constrain(n - i, 0.0f, 1.0f);
      ring.setPixelColor(i, scaleColor(c, k * master));
    }
  } else {
    // 2 nefes
    const float p = (t - 600) / 1700.0f;
    const float k = 0.25f + 0.75f * (0.5f + 0.5f * cosf(p * 2.0f * PI));
    ring.fill(scaleColor(c, k * master));
  }
  return true;
}

void renderFrame() {
  if (renderOverlay()) { ring.show(); return; }

  const float target = st.on ? (st.bright * MAX_BRIGHT / 100.0f) : 0.0f;
  curBright += (target - curBright) * FADE_RATE;
  if (fabsf(target - curBright) < 0.5f) curBright = target;
  ring.setBrightness((uint8_t)curBright);
  if (curBright < 1.0f) ring.clear();
  else                  FX[st.mode]();
  ring.show();
}

// ================= Bluetooth / ANCS =================
// Apple Notification Center Service UUID'leri
static const NimBLEUUID ANCS_SVC("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
static const NimBLEUUID ANCS_NS ("9FBF120D-6301-42D9-8C58-25E699A21DBD");  // Notification Source
static const NimBLEUUID ANCS_CP ("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");  // Control Point
static const NimBLEUUID ANCS_DS ("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");  // Data Source

enum BleState : uint8_t { BLE_IDLE = 0, BLE_CONNECTED, BLE_READY };
volatile BleState bleState = BLE_IDLE;

NimBLEServer*               bleServer = nullptr;
NimBLERemoteCharacteristic* ancsCP    = nullptr;
volatile uint16_t           bleConn   = 0xFFFF;
volatile bool               bleNeedSetup = false;
uint32_t                    bleSetupAt   = 0;
uint8_t                     bleSetupTries = 0;

struct NcEvt { uint8_t evt, flags, cat; uint32_t uid; };
QueueHandle_t ncQueue = nullptr;

portMUX_TYPE appMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool appReady = false;
uint32_t appUid = 0;
char     appId[64] = "";

uint8_t  dsBuf[160];
size_t   dsLen = 0;

void onNotifSource(NimBLERemoteCharacteristic*, uint8_t* d, size_t len, bool) {
  if (len < 8 || !ncQueue) return;
  NcEvt e{d[0], d[1], d[2], (uint32_t)d[4] | ((uint32_t)d[5] << 8) | ((uint32_t)d[6] << 16) | ((uint32_t)d[7] << 24)};
  xQueueSend(ncQueue, &e, 0);
}

// Get Notification Attributes cevabi (parca parca gelebilir)
void onDataSource(NimBLERemoteCharacteristic*, uint8_t* d, size_t len, bool) {
  if (dsLen + len > sizeof(dsBuf)) dsLen = 0;
  memcpy(dsBuf + dsLen, d, len);
  dsLen += len;
  if (dsLen < 8 || dsBuf[0] != 0x00) { if (dsLen >= 1 && dsBuf[0] != 0x00) dsLen = 0; return; }
  const uint16_t alen = dsBuf[6] | (dsBuf[7] << 8);
  if (dsLen < 8u + alen) return;                       // devami bekleniyor
  const uint32_t uid = dsBuf[1] | (dsBuf[2] << 8) | (dsBuf[3] << 16) | ((uint32_t)dsBuf[4] << 24);
  portENTER_CRITICAL(&appMux);
  appUid = uid;
  size_t n = min((size_t)alen, sizeof(appId) - 1);
  memcpy(appId, dsBuf + 8, n);
  appId[n] = 0;
  appReady = true;
  portEXIT_CRITICAL(&appMux);
  dsLen = 0;
}

class BleCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& ci) override {
    bleConn  = ci.getConnHandle();
    bleState = BLE_CONNECTED;
    NimBLEDevice::startSecurity(ci.getConnHandle());
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    bleConn = 0xFFFF;
    bleState = BLE_IDLE;
    bleNeedSetup = false;
    ancsCP = nullptr;
    oledDirty = true;
  }
  void onAuthenticationComplete(NimBLEConnInfo& ci) override {
    if (ci.isEncrypted()) {
      bleNeedSetup  = true;
      bleSetupTries = 0;
      bleSetupAt    = millis() + 800;
    }
  }
} bleCallbacks;

void bleBegin() {
  ncQueue = xQueueCreate(8, sizeof(NcEvt));
  NimBLEDevice::init(BLE_NAME);
  NimBLEDevice::setMTU(185);
  NimBLEDevice::setSecurityAuth(true, false, true);              // bonding, Just Works, Secure Connections
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(&bleCallbacks);
  bleServer->advertiseOnDisconnect(true);
  bleServer->start();

  // Reklam: ANCS "service solicitation" -> iPhone bildirim saglayicisi oldugunu anlar
  NimBLEAdvertisementData adv;
  adv.setFlags(0x06);
  const uint8_t solicit[] = {0x11, 0x15,
    0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79};
  adv.addData(solicit, sizeof(solicit));
  NimBLEAdvertisementData scan;
  scan.setName(BLE_NAME);

  NimBLEAdvertising* a = NimBLEDevice::getAdvertising();
  a->setAdvertisementData(adv);
  a->setScanResponseData(scan);
  a->enableScanResponse(true);
  a->start();
}

bool bleSetupAncs() {
  NimBLEClient* c = bleServer->getClient(bleConn);
  if (!c) return false;
  NimBLERemoteService* s = c->getService(ANCS_SVC);
  if (!s) return false;
  NimBLERemoteCharacteristic* ns = s->getCharacteristic(ANCS_NS);
  NimBLERemoteCharacteristic* cp = s->getCharacteristic(ANCS_CP);
  NimBLERemoteCharacteristic* ds = s->getCharacteristic(ANCS_DS);
  if (!ns || !cp || !ds) return false;
  if (!ds->subscribe(true, onDataSource)) return false;
  if (!ns->subscribe(true, onNotifSource)) return false;
  ancsCP   = cp;
  bleState = BLE_READY;
  oledDirty = true;
  return true;
}

void requestAppId(uint32_t uid) {
  if (!ancsCP) return;
  const uint8_t cmd[] = {0x00, (uint8_t)uid, (uint8_t)(uid >> 8), (uint8_t)(uid >> 16), (uint8_t)(uid >> 24), 0x00};
  ancsCP->writeValue(cmd, sizeof(cmd), true);
}

void bleLoop() {
  if (bleNeedSetup && bleState == BLE_CONNECTED && millis() >= bleSetupAt) {
    if (bleSetupAncs()) {
      bleNeedSetup = false;
    } else if (++bleSetupTries < 5) {
      bleSetupAt = millis() + 2000;
    } else {
      bleNeedSetup = false;
    }
  }

  NcEvt e;
  while (ncQueue && xQueueReceive(ncQueue, &e, 0) == pdTRUE) {
    const bool preExisting = e.flags & 0x04;
    if (e.evt == 0 && !preExisting && st.notif) {            // yeni bildirim
      overlayStart(e.cat, e.uid);
      requestAppId(e.uid);
    } else if (e.evt == 2 && ov.active && ov.uid == e.uid) {  // kaldirildi (arama bitti vb.)
      if (ov.call) overlayStop();
    }
  }

  if (appReady) {
    uint32_t uid;
    char id[64];
    portENTER_CRITICAL(&appMux);
    uid = appUid;
    memcpy(id, appId, sizeof(id));
    appReady = false;
    portEXIT_CRITICAL(&appMux);
    overlaySetApp(uid, id);
  }
}

// ================= Kalici ayarlar =================
void loadState() {
  prefs.begin("ledring", true);
  st.on     = prefs.getBool("on", true);
  st.mode   = prefs.getUChar("mode", M_SOLID);
  st.bright = prefs.getUChar("bright", 50);
  st.hue    = prefs.getFloat("hue", 30.0f);
  st.sat    = prefs.getFloat("sat", 60.0f);
  st.notif  = prefs.getBool("notif", true);
  prefs.end();
  if (st.mode >= MODE_COUNT) st.mode = M_SOLID;
  st.bright = constrain(st.bright, 1, 100);
}

void saveState() {
  prefs.begin("ledring", false);
  prefs.putBool("on", st.on);
  prefs.putUChar("mode", st.mode);
  prefs.putUChar("bright", st.bright);
  prefs.putFloat("hue", st.hue);
  prefs.putFloat("sat", st.sat);
  prefs.putBool("notif", st.notif);
  prefs.end();
  saveDirty = false;
}

// ================= OLED =================
void drawCentered(const char* s, int y) {
  int x = (72 - oled.getStrWidth(s)) / 2;
  oled.drawStr(x < 0 ? 0 : x, y, s);
}

const char* statusShort() {
  switch (hsStatus) {
    case HS_WIFI_NEEDED:
    case HS_AP_STARTED:
    case HS_AP_CONNECTED:    return "AP";
    case HS_WIFI_CONNECTING: return "...";
    case HS_PAIRING_NEEDED:  return "ESLE";
    case HS_PAIRED:          return "HOME";
    default:                 return "";
  }
}

void drawOled() {
  oled.clearBuffer();

  if (hsStatus == HS_FACTORY_RESET) {
    oled.setFont(u8g2_font_7x13B_tf);
    drawCentered("SIFIRLA", 25);
    oled.sendBuffer();
    return;
  }

  if (ov.active) {
    oled.setFont(u8g2_font_5x7_tf);
    drawCentered(ov.call ? "GELEN ARAMA" : "BILDIRIM", 8);
    oled.setFont(strlen(ov.name) > 9 ? u8g2_font_6x10_tf : u8g2_font_7x13B_tf);
    drawCentered(ov.name, 26);
    oled.drawHLine(16, 34, 40);
    oled.sendBuffer();
    return;
  }

  if (hsStatus == HS_AP_STARTED || hsStatus == HS_AP_CONNECTED || hsStatus == HS_WIFI_NEEDED) {
    oled.setFont(u8g2_font_5x7_tf);
    drawCentered("Wi-Fi kurulum", 8);
    drawCentered("MGM-LED-", 20);
    drawCentered("Kurulum", 28);
    drawCentered("sifre:mgm12345", 39);
    oled.sendBuffer();
    return;
  }

  if (millis() < ipShowUntil) {
    oled.setFont(u8g2_font_5x7_tf);
    drawCentered("Web paneli:", 10);
    drawCentered(WiFi.localIP().toString().c_str(), 22);
    drawCentered("mgm-led.local", 34);
    oled.sendBuffer();
    return;
  }

  oled.setFont(u8g2_font_5x7_tf);
  char hdr[8];
  snprintf(hdr, sizeof(hdr), "%u/%u", st.mode + 1, MODE_COUNT);
  oled.drawStr(0, 7, hdr);
  const char* ss = statusShort();
  oled.drawStr(72 - oled.getStrWidth(ss), 7, ss);
  // iPhone bagli + bildirim acik -> ortada nokta
  if (bleState == BLE_READY && st.notif) oled.drawDisc(36, 3, 2);
  else if (bleState == BLE_READY)        oled.drawCircle(36, 3, 2);

  oled.setFont(u8g2_font_7x13B_tf);
  drawCentered(st.on ? MODE_OLED[st.mode] : "KAPALI", 23);

  oled.drawFrame(0, 31, 72, 8);
  if (st.on) oled.drawBox(2, 33, (68 * st.bright) / 100, 4);
  oled.sendBuffer();
}

// ================= HomeKit servisleri =================
struct RingLight : Service::LightBulb {
  SpanCharacteristic *power, *level, *hue, *sat;

  RingLight() : Service::LightBulb() {
    new Characteristic::ConfiguredName("LED Halka");
    power = new Characteristic::On(st.on);
    level = new Characteristic::Brightness(st.bright);
    level->setRange(1, 100, 1);
    hue   = new Characteristic::Hue(st.hue);
    sat   = new Characteristic::Saturation(st.sat);
  }

  boolean update() override {
    if (power->updated()) st.on     = power->getNewVal();
    if (level->updated()) st.bright = level->getNewVal();
    if (hue->updated())   st.hue    = hue->getNewVal<float>();
    if (sat->updated())   st.sat    = sat->getNewVal<float>();
    markChanged();
    return true;
  }

  void loop() override {
    if ((bool)power->getVal() != st.on)          power->setVal(st.on);
    if (level->getVal() != st.bright)            level->setVal(st.bright);
    if (fabsf(hue->getVal<float>() - st.hue) > 0.5f) hue->setVal(st.hue);
    if (fabsf(sat->getVal<float>() - st.sat) > 0.5f) sat->setVal(st.sat);
  }
};

struct EffectSwitch : Service::Switch {
  uint8_t mode;
  SpanCharacteristic* power;

  explicit EffectSwitch(uint8_t m) : Service::Switch(), mode(m) {
    new Characteristic::ConfiguredName(MODE_HOME[m]);
    power = new Characteristic::On(st.on && st.mode == m);
  }

  boolean update() override {
    if (power->getNewVal()) { st.mode = mode; st.on = true; }
    else if (st.mode == mode) st.mode = M_SOLID;
    markChanged();
    return true;
  }

  void loop() override {
    const bool should = st.on && st.mode == mode;
    if ((bool)power->getVal() != should) power->setVal(should);
  }
};

struct NotifSwitch : Service::Switch {
  SpanCharacteristic* power;

  NotifSwitch() : Service::Switch() {
    new Characteristic::ConfiguredName("Bildirim Işığı");
    power = new Characteristic::On(st.notif);
  }

  boolean update() override {
    st.notif = power->getNewVal();
    if (!st.notif) overlayStop();
    markChanged();
    return true;
  }

  void loop() override {
    if ((bool)power->getVal() != st.notif) power->setVal(st.notif);
  }
};

// ================= Web paneli =================
const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="tr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MGM LED Halka</title>
<style>
:root{--bg:#0f1115;--card:#1a1d24;--fg:#e8eaf0;--mut:#8a90a0;--acc:#4f8cff}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:16px system-ui,-apple-system,sans-serif}
.w{max-width:420px;margin:0 auto;padding:16px}
h1{font-size:20px;margin:4px 0 16px;display:flex;justify-content:space-between;align-items:center}
.c{background:var(--card);border-radius:14px;padding:16px;margin-bottom:12px}
.lbl{color:var(--mut);font-size:13px;margin-bottom:10px}
.g{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px}
button{border:0;border-radius:10px;padding:14px 6px;font-size:15px;background:#252a33;color:var(--fg)}
button.on{background:var(--acc);color:#fff}
.pw{width:64px;height:36px;border-radius:18px;background:#333;position:relative;border:0;padding:0;flex:none}
.pw::after{content:"";position:absolute;top:4px;left:4px;width:28px;height:28px;border-radius:50%;background:#fff;transition:.2s}
.pw.on{background:#34c759}.pw.on::after{left:32px}
input[type=range]{width:100%;accent-color:var(--acc)}
input[type=color]{width:100%;height:48px;border:0;border-radius:10px;background:none;padding:0}
.row{display:flex;justify-content:space-between;align-items:center;gap:12px}
.st{font-size:13px;color:var(--mut);margin-top:4px}
.tb{width:100%;margin-top:12px}
</style></head><body><div class="w">
<h1>MGM LED Halka <button id="pw" class="pw" aria-label="Ac/Kapat"></button></h1>
<div class="c"><div class="lbl">Efekt</div><div class="g" id="modes"></div></div>
<div class="c"><div class="row lbl"><span>Parlaklık</span><span id="bv"></span></div>
<input type="range" id="br" min="1" max="100"></div>
<div class="c"><div class="lbl">Renk (Sabit, Kuyruklu, Nefes, Pırıltı)</div><input type="color" id="col"></div>
<div class="c"><div class="row"><div><div>iPhone bildirim ışığı</div><div class="st" id="bs"></div></div>
<button id="nt" class="pw" aria-label="Bildirim isigi"></button></div>
<button class="tb" id="test">Test bildirimi</button></div>
</div><script>
const N=["Sabit","Gökkuşağı","Kuyruklu","Nefes","Ateş","Pırıltı"];let S={},busy=0;
const BS=["iPhone bağlı değil","iPhone bağlanıyor…","iPhone bağlı"];
const $=i=>document.getElementById(i);
N.forEach((n,i)=>{const b=document.createElement('button');b.textContent=n;b.onclick=()=>set({mode:i,on:1});$('modes').appendChild(b)});
function hsv2hex(h,s){s/=100;const f=n=>{const k=(n+h/60)%6;return Math.round(255*(1-s*Math.max(0,Math.min(k,4-k,1))))};return'#'+[f(5),f(3),f(1)].map(x=>x.toString(16).padStart(2,'0')).join('')}
function hex2hs(x){const r=parseInt(x.substr(1,2),16)/255,g=parseInt(x.substr(3,2),16)/255,b=parseInt(x.substr(5,2),16)/255;const mx=Math.max(r,g,b),mn=Math.min(r,g,b),d=mx-mn;let h=0;if(d){h=mx==r?((g-b)/d)%6:mx==g?(b-r)/d+2:(r-g)/d+4;h*=60;if(h<0)h+=360}return{h:Math.round(h),s:Math.round(mx?d/mx*100:0)}}
function show(s){S=s;$('pw').classList.toggle('on',!!s.on);[...$('modes').children].forEach((b,i)=>b.classList.toggle('on',s.on&&s.mode==i));
$('nt').classList.toggle('on',!!s.n);$('bs').textContent=BS[s.ble]||'';
if(!busy){$('br').value=s.b;$('col').value=hsv2hex(s.h,s.s)}$('bv').textContent=s.b+'%'}
function set(p){return fetch('/api/set?'+new URLSearchParams(p)).then(r=>r.json()).then(show).catch(()=>{})}
$('pw').onclick=()=>set({on:S.on?0:1});
$('nt').onclick=()=>set({n:S.n?0:1});
$('test').onclick=()=>fetch('/api/test');
$('br').oninput=e=>{busy=1;$('bv').textContent=e.target.value+'%'};
$('br').onchange=e=>{busy=0;set({b:e.target.value})};
$('col').oninput=()=>busy=1;
$('col').onchange=e=>{busy=0;const c=hex2hs(e.target.value);set({h:c.h,s:c.s})};
function poll(){fetch('/api/state').then(r=>r.json()).then(show).catch(()=>{})}
poll();setInterval(poll,2000);
</script></body></html>)HTML";

void sendState() {
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"on\":%d,\"mode\":%u,\"b\":%u,\"h\":%d,\"s\":%d,\"n\":%d,\"ble\":%u}",
           st.on ? 1 : 0, st.mode, st.bright, (int)lroundf(st.hue), (int)lroundf(st.sat),
           st.notif ? 1 : 0, (unsigned)bleState);
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", buf);
}

void handleSet() {
  if (web.hasArg("on"))   st.on     = web.arg("on").toInt() != 0;
  if (web.hasArg("mode")) { int m = web.arg("mode").toInt(); if (m >= 0 && m < MODE_COUNT) st.mode = m; }
  if (web.hasArg("b"))    st.bright = constrain(web.arg("b").toInt(), 1, 100);
  if (web.hasArg("h"))    st.hue    = constrain(web.arg("h").toFloat(), 0.0f, 360.0f);
  if (web.hasArg("s"))    st.sat    = constrain(web.arg("s").toFloat(), 0.0f, 100.0f);
  if (web.hasArg("n"))  { st.notif  = web.arg("n").toInt() != 0; if (!st.notif) overlayStop(); }
  markChanged();
  sendState();
}

void handleTest() {          // gosterim icin sahte bildirim (sirayla farkli uygulamalar)
  static uint8_t k = 0;
  const uint8_t pick[] = {0, 2, 3, 4, 7};   // WhatsApp, Mesaj, Instagram, Telegram, Gmail
  overlayStart(4, 0xFFFFFF00u + k);
  overlaySetApp(0xFFFFFF00u + k, APPS[pick[k % sizeof(pick)]].id);
  k++;
  web.send(200, "text/plain", "ok");
}

void startWeb() {
  if (webStarted) return;
  web.on("/", HTTP_GET, [] { web.send_P(200, "text/html", PAGE); });
  web.on("/api/state", HTTP_GET, sendState);
  web.on("/api/set", HTTP_GET, handleSet);
  web.on("/api/test", HTTP_GET, handleTest);
  web.onNotFound([] { web.send(404, "text/plain", "Yok"); });
  web.begin();
  webStarted  = true;
  ipShowUntil = millis() + 5000;
  oledDirty   = true;
}

// ================= HomeSpan geri cagirmalari =================
void onStatus(HS_STATUS s) {
  hsStatus = s;
  drawOled();
}

void onWifi(int) { startWeb(); }

// ================= Buton =================
enum class BtnEvent : uint8_t { None, Click, DoubleClick, LongPress, ResetPress };

class Button {
 public:
  explicit Button(uint8_t pin) : pin_(pin) {}
  void begin() { pinMode(pin_, INPUT_PULLUP); }

  BtnEvent update() {
    const uint32_t now = millis();
    const bool raw = (digitalRead(pin_) == LOW);
    if (raw != lastRaw_) { lastRaw_ = raw; lastChange_ = now; }

    if ((now - lastChange_) >= DEBOUNCE_MS && raw != stable_) {
      stable_ = raw;
      if (stable_) {
        pressStart_ = now; longFired_ = false; resetFired_ = false;
      } else if (!longFired_) {
        if (pendingClick_ && (now - lastRelease_) <= DOUBLE_CLICK_MS) {
          pendingClick_ = false;
          return BtnEvent::DoubleClick;
        }
        pendingClick_ = true;
        lastRelease_  = now;
      }
    }

    if (stable_ && !longFired_ && (now - pressStart_) >= LONG_PRESS_MS) {
      longFired_ = true; pendingClick_ = false;
      return BtnEvent::LongPress;
    }
    if (stable_ && !resetFired_ && (now - pressStart_) >= RESET_PRESS_MS) {
      resetFired_ = true;
      return BtnEvent::ResetPress;
    }
    if (pendingClick_ && !stable_ && (now - lastRelease_) > DOUBLE_CLICK_MS) {
      pendingClick_ = false;
      return BtnEvent::Click;
    }
    return BtnEvent::None;
  }

 private:
  uint8_t  pin_;
  bool     lastRaw_ = false, stable_ = false, pendingClick_ = false;
  bool     longFired_ = false, resetFired_ = false;
  uint32_t lastChange_ = 0, pressStart_ = 0, lastRelease_ = 0;
};

Button button(PIN_BUTTON);

void handleButton() {
  switch (button.update()) {
    case BtnEvent::Click:
      if (ov.active) { overlayStop(); break; }      // bildirimi kapat
      if (!st.on) st.on = true;
      else        st.mode = (st.mode + 1) % MODE_COUNT;
      markChanged();
      break;

    case BtnEvent::DoubleClick:
      st.on = !st.on;
      markChanged();
      break;

    case BtnEvent::LongPress: {
      if (!st.on) break;
      uint8_t next = BTN_LEVELS[0];
      for (uint8_t i = 0; i < NUM_BTN_LEVELS; i++) {
        if (BTN_LEVELS[i] > st.bright) { next = BTN_LEVELS[i]; break; }
      }
      st.bright = next;
      markChanged();
      break;
    }

    case BtnEvent::ResetPress:
      hsStatus = HS_FACTORY_RESET;
      drawOled();
      ring.clear(); ring.show();
      NimBLEDevice::deleteAllBonds();
      delay(800);
      homeSpan.processSerialCommand("F");   // Wi-Fi + eslesme silinir, yeniden baslar
      break;

    default:
      break;
  }
}

// ================= Ana akis =================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOARD_LED, OUTPUT);
  digitalWrite(PIN_BOARD_LED, HIGH);

  randomSeed(esp_random());
  button.begin();
  loadState();

  ring.begin();
  ring.clear();
  ring.show();

  oled.begin();
  oled.clearBuffer();
  oled.setFont(u8g2_font_7x13B_tf);
  drawCentered("MGM", 17);
  oled.setFont(u8g2_font_5x7_tf);
  drawCentered("LED Halka", 32);
  oled.sendBuffer();

  bleBegin();

  homeSpan.setLogLevel(0)
          .setPortNum(HAP_PORT)
          .setHostNameSuffix("")
          .setApSSID(AP_SSID)
          .setApPassword(AP_PASSWORD)
          .enableAutoStartAP()
          .setStatusCallback(onStatus)
          .setWifiCallbackAll(onWifi)
          .setSketchVersion("1.1");

  homeSpan.begin(Category::Lighting, "MGM LED Halka", HOST_NAME, "MGM-LED-C3");

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("MGM LED Halka");
      new Characteristic::Manufacturer("MGM");
      new Characteristic::Model("ESP32-C3 WS2812 x12");
      new Characteristic::SerialNumber("MGM-0001");
      new Characteristic::FirmwareRevision("1.1");
    new RingLight();
    for (uint8_t m = M_RAINBOW; m < MODE_COUNT; m++) new EffectSwitch(m);
    new NotifSwitch();

  oledDirty = true;
}

void loop() {
  homeSpan.poll();
  if (webStarted) web.handleClient();
  bleLoop();
  handleButton();

  const uint32_t now = millis();
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    frameCount++;
    renderFrame();
  }

  static bool ipWasShown = false;
  if (ipShowUntil && now >= ipShowUntil && !ipWasShown) { ipWasShown = true; oledDirty = true; }

  if (oledDirty) {
    drawOled();
    oledDirty = false;
  }

  if (saveDirty && (now - dirtySince) >= SAVE_DELAY_MS) saveState();
}
