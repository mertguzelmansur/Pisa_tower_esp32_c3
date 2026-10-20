# MGM LED Halka

**ESP32-C3 + 12'li WS2812 LED halka ile Apple Home uyumlu, iPhone bildirim ışığına dönüşebilen masaüstü lamba.**

![Montaj görünümü](docs/images/montaj.png)

Halka kendi efektlerini oynatırken iPhone'a bir bildirim gelirse anında **bildirim ışığına dönüşür**. Uygulamanın renginde yanar, ardından kaldığı efekte geri döner. Apple Home'dan, Siri'den, tarayıcıdan ya da kart üzerindeki butondan kontrol edilir.

---

## İçindekiler

- [Özellikler](#özellikler)
- [Donanım](#donanım)
- [Bağlantı şeması](#bağlantı-şeması)
- [Kutu (3D baskı)](#kutu-3d-baskı)
- [Firmware yükleme](#firmware-yükleme)
- [İlk kurulum](#ilk-kurulum)
- [Kullanım](#kullanım)
- [iPhone bildirim ışığı](#iphone-bildirim-ışığı)
- [Web paneli ve API](#web-paneli-ve-api)
- [Kaynak koddan derleme](#kaynak-koddan-derleme)
- [Sorun giderme](#sorun-giderme)
- [Dosya yapısı](#dosya-yapısı)
- [Yol haritası](#yol-haritası)

---

## Özellikler

| | |
|---|---|
| 🏠 **Apple Home (HomeKit)** | Renkli lamba (aç/kapa, parlaklık, renk) + her efekt için ayrı anahtar + "Bildirim Işığı" anahtarı. Siri ile çalışır. Hub gerekmez (uzaktan erişim ve otomasyon için Apple TV / HomePod gerekir). |
| 📱 **iPhone bildirim ışığı** | Bluetooth (Apple ANCS) üzerinden. Uygulamaya göre renk, gelen aramada dönen ışık, OLED'de uygulama adı. |
| 🌐 **Web paneli** | `http://mgm-led.local` üzerinden telefon/PC'den kontrol. Uygulama kurmak gerekmez. |
| 🎨 **6 efekt** | Sabit, Gökkuşağı, Kuyruklu, Nefes, Ateş, Pırıltı |
| 🖥️ **OLED durum ekranı** | Efekt, parlaklık, Wi-Fi/Home durumu, iPhone bağlantısı, bildirim |
| 🔘 **Tek buton kontrol** | Tık, çift tık, uzun basma, 10 sn fabrika ayarı |
| 💾 **Kalıcı ayarlar** | Elektrik gidip gelince kaldığı yerden devam eder |
| 📶 **Kolay Wi-Fi kurulumu** | İlk açılışta kendi ağını açar, telefondan Wi-Fi seçilir. Şifre koda gömülmez. |
| 🖨️ **3D baskı kutu** | Ø59,4 mm, lehim paylı, USB-C kablo kanallı, parametrik OpenSCAD |

---

## Donanım

| Parça | Adet | Not |
|---|---|---|
| ESP32-C3 0.42" OLED geliştirme kartı | 1 | 24,95 × 20,7 mm, USB-C, dahili 72×40 OLED |
| WS2812 12'li RGB LED halka | 1 | Dış çap 50 mm, iç çap 35 mm, 5 V |
| 330 Ω direnç | 1 | Data hattına seri |
| 470 µF / 6,3 V+ kondansatör | 1 | Önerilir, halka besleme girişine |
| USB-C kablo | 1 | Kablo Ø ≤ 3 mm, plastik kafa ≤ 21 mm (kutuya sığması için) |
| İnce kablo (26–28 AWG) | ~30 cm | |

**Güç:** 12 LED tam beyazda ~720 mA çeker. Firmware USB'den beslemeye uygun olarak maksimum parlaklığı **%51 (130/255)** ile sınırlar. Daha yüksek parlaklık için `MAX_BRIGHT` değeri artırılabilir; bu durumda en az 1 A'lik adaptör kullanın.

---

## Bağlantı şeması

```
 ESP32-C3 OLED                     WS2812 Halka
┌──────────────┐                  ┌────────────┐
│          5V  ├──────────────────┤ VCC / 5V   │
│         GND  ├─────────┬────────┤ GND        │
│       GPIO4  ├──[330Ω]─┼────────┤ DIN / IN   │
│              │         │        │ DOUT (boş) │
│  GPIO5/6 OLED│      [470µF]     └────────────┘
│  GPIO9 BOOT  │         │
└──────────────┘        GND
```

| ESP32-C3 pini | Görev |
|---|---|
| GPIO4 | LED halka data |
| GPIO5 / GPIO6 | Dahili OLED (I²C SDA / SCL) — kullanmayın |
| GPIO9 | BOOT butonu (kontrol butonu olarak kullanılır) |
| GPIO8 | Kart üzerindeki LED (kapalı tutulur) |
| GPIO2, 8, 9 | Boot pinleri — LED data için kullanmayın |

> 3,3 V data sinyali 5 V halkada genelde sorunsuz çalışır. Titreme olursa halkanın beslemesini bir diyotla ~4,4 V'a düşürün veya seviye dönüştürücü kullanın.

---

## Kutu (3D baskı)

![Gövde iç görünüm](docs/images/govde_ic.png)

Kutu, **Base-Tilted** standının ortasındaki **Ø60 mm** boşluğa oturacak şekilde tasarlandı. USB-C kablosu standın **3 mm** kablo kanalından çıkar. Stand modeli bu repoda yoktur, ayrı bir modeldir.

| Dosya | Malzeme | Baskı |
|---|---|---|
| `enclosure/stl/MGM_LedHalka_Govde.stl` | PLA/PETG, opak | Düz tabanı tablaya, destek yok |
| `enclosure/stl/MGM_LedHalka_Kapak.stl` | **Beyaz / yarı saydam** PLA/PETG | Dosyadaki gibi (üst yüzey tablada), destek yok |
| `enclosure/stl/MGM_LedHalka_TusPimleri.stl` | Herhangi | 2 adet pim, destek yok |

**Ölçüler:** Ø59,4 × 16,5 mm (stand tabanından ~8,5 mm yükselir)

**Tasarım detayları**
- Kart ölçüleri, OLED ve buton konumları üreticinin **STEP modelinden** alındı. Kapakta OLED aktif alanına (9,5 × 5,7 mm) pencere vardır.
- **Lehim payları:** Halka sürekli bir çıkıntıya değil, **3 küçük ayağa** (60°/180°/300°) oturur. Halkanın altında yaklaşık 6 mm boşluk vardır. Kartın altında 2,5 mm boşluk var, USB ucundaki 5V/GND pedlerinin altında destek yoktur.
- Kapaktaki iç boru halkayı yerinde tutar ve ışığın OLED tarafına sızmasını engeller.
- BOOT ve RST butonlarına kapaktaki iki pim ile basılır. Pimlerin yakası kapağın altında kalır, düşmez.

**Montaj**
1. Kartı köşe kılavuzlarına oturtun, USB-C tünele baksın. Gerekirse bir damla sıcak silikon kullanın.
2. Halkanın kablolarını lehimleyin, halkayı **lehim pedleri ayaklara denk gelmeyecek şekilde** çevirip yuvasına oturtun.
3. Pimleri kapağın deliklerine alttan takın, kapağı bastırarak kapatın.

**Parametrik düzenleme:** `enclosure/MGM_LedRing_Stand60.scad` dosyasını OpenSCAD ile açın. Halka, kart, kablo ve tolerans ölçüleri dosyanın başındadır. `part` değişkeni `"base" | "lid" | "pins" | "assembly"` olabilir.

---

## Firmware yükleme

### Yöntem 1: Hazır .bin dosyası (önerilen, bilgisayar gerekmez)

`firmware/bin/MGM_LedRing_Home_v1.1.merged.bin` tek parça bir dosyadır: bootloader, bölümleme ve uygulama birlikte.

1. **Chrome / Edge** ile [ESP Web Flasher](https://espressif.github.io/esptool-js/) sayfasını açın.
   *Android'de OTG kablo ile çalışır. iPhone'da Web Serial olmadığı için PC/Mac gerekir.*
2. Kartı USB ile bağlayın, **Connect**'e basın. Port görünmezse **BOOT'a basılı tutarak** USB'yi takın.
3. İlk yüklemede **Erase Flash** yapın.
4. **Flash Address: `0x0`**, dosyayı seçin, **Program**'a basın.
5. Bitince **RST**'ye basın.

> Sonraki güncellemelerde Erase yapmazsanız Wi-Fi, Apple Home ve Bluetooth eşleşmeleri korunur.

### Yöntem 2: Arduino IDE

[Kaynak koddan derleme](#kaynak-koddan-derleme) bölümüne bakın.

---

## İlk kurulum

### 1. Wi-Fi

1. Kart ilk açılışta OLED'de **"Wi-Fi kurulum"** yazar ve **`MGM-LED-Kurulum`** ağını açar (şifre: `mgm12345`).
2. Telefonla bu ağa bağlanın, otomatik açılan sayfada:
   - Ev Wi-Fi ağınızı seçip şifresini girin
   - **8 haneli bir kurulum kodu** belirleyin (ör. `46637726`). Bu kod Apple Home eşleştirmesinde kullanılacak.
3. Kart yeniden başlar ve ağa bağlanır. OLED'de 5 saniye IP adresi görünür.

### 2. Apple Home

**Ev** uygulaması → **+** → **Aksesuar Ekle** → **Daha fazla seçenek…** → **MGM LED Halka** → kurulum kodunu girin.

Home'da görünenler:

| Aksesuar | İşlev |
|---|---|
| **LED Halka** (lamba) | Aç/kapa, parlaklık, renk |
| **Gökkuşağı / Kuyruklu / Nefes / Ateş / Pırıltı** (anahtar) | Efekt seçimi. Biri açılınca diğerleri kapanır, hepsi kapalıysa halka sabit renkte yanar. |
| **Bildirim Işığı** (anahtar) | iPhone bildirim ışığını aç/kapat |

Siri örnekleri: *"Halkayı maviye çevir"*, *"Gökkuşağını aç"*, *"Bildirim ışığını kapat"*

### 3. iPhone bildirim ışığı (isteğe bağlı)

1. iPhone → **Ayarlar** → **Bluetooth** → **MGM LED** → **Eşleştir**
2. *"MGM LED bildirimlerinizi alsın mı?"* sorusuna **İzin Ver**
3. OLED'in üst ortasında **dolu nokta** görünür: iPhone bağlı ve bildirim ışığı açık.

---

## Kullanım

### Buton (kartın BOOT butonu / kapaktaki pim)

| Hareket | İşlev |
|---|---|
| Tek tık | Sonraki efekt (kapalıysa açar). Bildirim sırasında: bildirimi kapatır. |
| Çift tık | Aç / kapat |
| Uzun basma (0,7 sn) | Parlaklık kademesi (%10 → 30 → 50 → 75 → 100) |
| **10 sn basılı** | **Fabrika ayarı:** Wi-Fi, Apple Home ve Bluetooth eşleşmeleri silinir |

### Efektler

| # | Efekt | Renk ayarını kullanır |
|---|---|---|
| 1 | Sabit | ✔ |
| 2 | Gökkuşağı | — |
| 3 | Kuyruklu (dönen ışık) | ✔ |
| 4 | Nefes | ✔ |
| 5 | Ateş | — |
| 6 | Pırıltı | ✔ (arka plan) |

### OLED ekran

```
┌────────────────────┐
│ 3/6     ●     HOME │  efekt no · iPhone bağlı · Home durumu
│     Kuyruklu       │  efekt adı (veya KAPALI)
│ [██████░░░░░░░░░]  │  parlaklık
└────────────────────┘
```

Sağ üst köşedeki durum yazıları: `AP` Wi-Fi kurulumu bekliyor · `...` bağlanıyor · `ESLE` Home eşleşmesi bekliyor · `HOME` eşleşmiş
Ortadaki işaret: `●` iPhone bağlı ve bildirim açık · `○` iPhone bağlı ama bildirim kapalı

---

## iPhone bildirim ışığı

Kart, Apple'ın **ANCS (Apple Notification Center Service)** protokolünü Bluetooth Low Energy üzerinden kullanır. iPhone'da ek uygulama gerekmez.

**Davranış**
- Yeni bildirim gelince halka **hangi efektte olursa olsun, kapalı olsa bile** araya girer.
- Uygulamanın renginde halka etrafında dolar, iki kez nefes alır (~4,5 sn), sonra önceki efekte yumuşakça döner.
- **Gelen arama:** Arama bitene kadar (en fazla 45 sn) dönen ışık gösterir. Arama sürerken gelen diğer bildirimler aramayı kesmez.
- Telefonda önceden bekleyen bildirimler ışığı tetiklemez.
- Bildirim en az **%60** parlaklıkta gösterilir.
- OLED'de "BILDIRIM" / "GELEN ARAMA" ve uygulama adı yazar.

**Uygulama renkleri**

| Uygulama | Renk | | Uygulama | Renk |
|---|---|---|---|---|
| WhatsApp | 🟢 `#25D366` | | Gmail | 🔴 `#EA4335` |
| Mesajlar | 🟢 `#30D158` | | Mail / Outlook | 🔵 `#0A84FF` |
| Instagram | 🩷 `#E1306C` | | Takvim | 🔴 `#FF3B30` |
| Telegram | 🔵 `#229ED9` | | Anımsatıcılar | 🟠 `#FF9500` |
| Telefon | 🔵 `#0A84FF` | | Snapchat | 🟡 `#FFE600` |
| Messenger | 🔵 `#0084FF` | | TikTok | 🩵 `#25F4EE` |
| Facebook | 🔵 `#1877F2` | | YouTube | 🔴 `#FF0000` |
| X | ⚪ `#FFFFFF` | | Spotify | 🟢 `#1DB954` |
| LinkedIn | 🔵 `#0A66C2` | | Trendyol / Hepsiburada | 🟠 |

Listede olmayan uygulamalar **kategorilerine göre** renk alır: e-posta mavi, takvim turuncu, sosyal mor, sağlık pembe, finans sarı, cevapsız arama kırmızı, diğerleri beyaz.

**Yeni uygulama eklemek:** `.ino` dosyasındaki `APPS[]` tablosuna paket kimliği, ad ve renk ekleyin:

```cpp
{"com.ornek.uygulama", "Ornek", 0xRRGGBB},
```

> OLED fontu Türkçe karakter içermediği için adlarda ASCII kullanın.

**Göstermek için:** Web panelindeki **Test bildirimi** butonu, iPhone bağlı olmasa bile sırayla WhatsApp, Mesaj, Instagram, Telegram ve Gmail bildirimlerini canlandırır.

---

## Web paneli ve API

Aynı Wi-Fi ağında tarayıcıdan **`http://mgm-led.local`** (veya OLED'de görünen IP) adresini açın.

Panelde: aç/kapa, efekt butonları, parlaklık, renk seçici, bildirim ışığı anahtarı, iPhone bağlantı durumu ve test butonu. Panel her 2 saniyede bir durumu yeniler; Apple Home, panel ve buton birbiriyle senkron çalışır.

### HTTP API

| Uç nokta | Açıklama |
|---|---|
| `GET /api/state` | Durumu JSON olarak döner |
| `GET /api/set?...` | Bir veya birkaç değeri değiştirir, yeni durumu döner |
| `GET /api/test` | Test bildirimi oynatır |

**`/api/set` parametreleri**

| Parametre | Değer | Açıklama |
|---|---|---|
| `on` | `0` / `1` | Aç / kapat |
| `mode` | `0`–`5` | Efekt (0 Sabit … 5 Pırıltı) |
| `b` | `1`–`100` | Parlaklık (%) |
| `h` | `0`–`360` | Renk tonu |
| `s` | `0`–`100` | Doygunluk |
| `n` | `0` / `1` | Bildirim ışığı |

**Örnek**

```bash
curl "http://mgm-led.local/api/set?on=1&mode=1&b=40"
```

```json
{"on":1,"mode":1,"b":40,"h":30,"s":60,"n":1,"ble":2}
```

`ble`: `0` iPhone bağlı değil · `1` bağlanıyor · `2` bağlı ve bildirimler aktif

> Bu API'yi Home Assistant, Node-RED, PLC/SCADA veya bir kısayol (iOS Kestirmeler → "URL içeriğini al") ile de kullanabilirsiniz.

---

## Kaynak koddan derleme

### Gereksinimler

| | Sürüm |
|---|---|
| Arduino IDE | 2.x |
| ESP32 kart paketi (Espressif) | **3.0.2 veya üzeri** (3.0.4 ile test edildi) |
| [HomeSpan](https://github.com/HomeSpan/HomeSpan) | 2.x |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | 2.x (2.3.9 ile test edildi) |
| [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) | güncel |
| [U8g2](https://github.com/olikraus/u8g2) | güncel |

### Kart ayarları

| Ayar | Değer |
|---|---|
| Kart | **ESP32C3 Dev Module** |
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** ← zorunlu, uygulama ~1,8 MB |
| Flash Mode | DIO |
| USB CDC On Boot | Disabled (seri log gerekiyorsa Enabled) |

`firmware/MGM_LedRing_Home/MGM_LedRing_Home.ino` dosyasını açıp yükleyin.

### Tek parça .bin üretmek (isteğe bağlı)

Arduino IDE → **Sketch → Export Compiled Binary**. Oluşan `*.merged.bin` dosyası web flasher ile `0x0` adresine yüklenebilir.

### Ayarlanabilir sabitler (`.ino` başında)

| Sabit | Varsayılan | Açıklama |
|---|---|---|
| `PIN_LED_DATA` | `4` | Halka data pini |
| `LED_COUNT` | `12` | LED sayısı |
| `MAX_BRIGHT` | `130` | %100 parlaklığın karşılığı (0–255) |
| `NOTIF_MS` | `4500` | Bildirim gösterim süresi (ms) |
| `CALL_MAX_MS` | `45000` | Gelen arama en fazla süre (ms) |
| `NOTIF_MIN_BRIGHT` | `60` | Bildirim minimum parlaklığı (%) |
| `AP_SSID` / `AP_PASSWORD` | `MGM-LED-Kurulum` / `mgm12345` | Kurulum ağı |
| `HOST_NAME` | `mgm-led` | `mgm-led.local` adresi |
| `BLE_NAME` | `MGM LED` | iPhone'da görünen Bluetooth adı |

---

## Sorun giderme

| Sorun | Çözüm |
|---|---|
| Web flasher kartı görmüyor | BOOT'a basılı tutarak USB'yi takın. Şarj kablosu değil, data kablosu kullanın. |
| Arduino IDE `exit status 1` | Asıl hata çıktı penceresinde üstteki kırmızı satırlardadır. En sık sebepler: kütüphane eksik, Partition Scheme "Huge APP" değil. |
| LED'ler titriyor / yanlış renk | 330 Ω seri direnç ve 470 µF kondansatörü kontrol edin. 3,3 V data seviyesi için yukarıdaki nota bakın. |
| Kart sürekli yeniden başlıyor | Parlaklık çok yüksek ve USB portu yetersiz olabilir. Daha güçlü bir adaptör kullanın veya `MAX_BRIGHT` değerini düşürün. |
| Home'da "Yanıt Yok" | Kart ve iPhone aynı ağda mı? Router'da mDNS/Bonjour engelli olabilir. |
| `mgm-led.local` açılmıyor | Android bazı sürümlerde `.local` adreslerini çözmez. OLED'de görünen IP'yi kullanın. |
| iPhone Bluetooth listesinde "MGM LED" görünmüyor | Kartı yeniden başlatın. Daha önce eşleştiyse iPhone'da "Bu aygıtı unut" deyip tekrar deneyin. |
| iPhone bağlanıyor ama bildirim gelmiyor | Ayarlar → Bluetooth → MGM LED → (i) → **Sistem Bildirimlerini Paylaş** açık olmalı. Odak / Rahatsız Etmeyin modu bildirimleri engelleyebilir. |
| Her şeyi sıfırlamak | Butona **10 saniye** basılı tutun. iPhone'da da "Bu aygıtı unut" yapın, Home'dan aksesuarı kaldırın. |

---

## Dosya yapısı

```
MGM-LED-Halka/
├── README.md
├── firmware/
│   ├── MGM_LedRing_Home/
│   │   └── MGM_LedRing_Home.ino       # Arduino kaynak kodu
│   └── bin/
│       └── MGM_LedRing_Home_v1.1.merged.bin   # 0x0 adresine yüklenir
├── enclosure/
│   ├── MGM_LedRing_Stand60.scad       # Parametrik kutu (OpenSCAD)
│   └── stl/
│       ├── MGM_LedHalka_Govde.stl
│       ├── MGM_LedHalka_Kapak.stl
│       └── MGM_LedHalka_TusPimleri.stl
└── docs/
    └── images/
        ├── montaj.png
        └── govde_ic.png
```

---

## Yol haritası

- [ ] OTA (kablosuz) firmware güncelleme
- [ ] Bildirim filtreleme (sadece seçili uygulamalar)
- [ ] Web panelinden uygulama renklerini düzenleme
- [ ] JK BMS batarya göstergesi modu (BLE)
- [ ] Bluetooth ile Wi-Fi kurulumu

---

## Sürüm geçmişi

| Sürüm | Değişiklikler |
|---|---|
| **v1.1** | iPhone bildirim ışığı (ANCS), Home'da "Bildirim Işığı" anahtarı, web panelinde test butonu ve iPhone durumu, fabrika ayarı Bluetooth eşleşmelerini de siler |
| **v1.0** | Apple Home, web paneli, 6 efekt, OLED, buton kontrolü, Wi-Fi kurulum ağı |

---

## Teşekkür

[HomeSpan](https://github.com/HomeSpan/HomeSpan) · [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) · [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) · [U8g2](https://github.com/olikraus/u8g2)

## Lisans

Lisans henüz seçilmedi. Açık kaynak paylaşmak için yaygın seçenek **MIT**'tir. Kullanılan kütüphanelerin kendi lisansları geçerlidir.

---

<sub>**MGM** · 2026</sub>
