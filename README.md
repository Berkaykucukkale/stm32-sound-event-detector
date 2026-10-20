# STM32 Bare-Metal Sound Event Detector

Bu proje, **STM32F4** serisi mikrodenetleyiciler kullanılarak ortamdaki spesifik ses olaylarını (el çırpma, yüksek ses, alarm vb.) tespit etmek amacıyla geliştirilmiş bir **Bare-Metal (Register-Level)** gömülü sistem uygulamasıdır. 

Sistem, hantal kütüphaneler (HAL/LL) kullanılmadan doğrudan donanım kaydedicileri (Hardware Registers) seviyesinde kodlanmış olup, ADC, Timer Kesmeleri (Interrupts) ve UART üzerinden loglama mimarisini kullanır.

## 🛠 Donanım ve Araçlar
* **Mikrodenetleyici:** STM32F4 Serisi (Projede `stm32f4xx.h` kullanılmıştır)
* **Girişler:** 
  * Mikrofon Sensörü (PA0 / ADC1 Channel 0)
  * Potansiyometre (PA1 / ADC1 Channel 1) - *Dinamik eşik ayarı için*
* **Çıkışlar:** 
  * Durum LED'i (PA5)
  * UART TX (PA2 - 9600 Baud Rate)
* **Programlama Yaklaşımı:** Bare-Metal C (Register Seviyesi)

## ✨ Öne Çıkan Özellikler ve Algoritmalar

### 1. Hassas Zamanlama (TIM2 ve SysTick)
* **SysTick:** İşletim sistemi (OS) mantığına benzer şekilde global sistem zamanını (Timestamp) tutmak için 1 milisaniyelik kesmelerle çalışır.
* **TIM2 Interrupt (10 kHz Sampling):** Ses dalgasının doğasını kaçırmamak için donanımsal zamanlayıcı (Timer 2) kullanılarak **her 100 mikrosaniyede bir** ADC okuması yapılır. 

### 2. Sinyal Zarflama (Windowing) ve Genlik Hesabı
Ses verisi doğrudan eşikle karşılaştırılmaz. Sistem her **50 ms'lik pencereler** (500 adet okuma) halinde sinyali analiz eder. Bu pencere içerisindeki `Maksimum ADC` - `Minimum ADC` farkı bulunarak sesin o anki net **Genliği (Amplitude)** hesaplanır. Bu sayede DC offset hataları (mikrofonun referans voltajındaki kaymalar) filtrelenmiş olur.

### 3. Dinamik Eşik (Dynamic Threshold) ve Gürültü Tabanı
Tetiklenme hassasiyeti koda gömülmemiştir (Hardcoded değildir). İkinci bir ADC kanalı üzerinden okunan potansiyometre değeri eşik (Threshold) olarak kullanılır. Ayrıca ortamdaki doğal elektronik gürültüyü (Noise Floor) engellemek için yazılımsal olarak minimum 50 birimlik bir "Deadzone" uygulanmıştır.

### 4. Debounce ve Olay Kilitleme (Event Tracking)
Ses olayının yankılardan veya anlık kesintilerden dolayı birden fazla olaymış gibi algılanmasını önlemek için **Silence Timer (Susturma Zamanlayıcısı)** kullanılmıştır. Sinyal eşiğin altına düşse bile, sistem olayı sonlandırmak için **ardışık 250 ms (5 pencere)** boyunca sessizlik bekler. 

Olay bittiğinde; sistemin başlangıç zamanı, geçen süre (Duration) ve olay boyunca ulaşılan "En Yüksek Ses Şiddeti (Peak Amplitude)" gibi veriler paketlenerek UART üzerinden bilgisayara gönderilir.

## 🚀 Kurulum ve Çalıştırma

1. Projeyi bir STM32 geliştirme ortamında (STM32CubeIDE veya Keil uVision) açın.
2. `main.c` dosyasını `src` veya `Core/Src` klasörünüze dahil edin. **Projeyi oluştururken HAL kütüphanelerini dahil etmenize gerek yoktur.**
3. Donanım bağlantılarını yapın:
   * **PA0:** Mikrofonun Analog Out (A0) pini
   * **PA1:** 10K Potansiyometrenin orta bacağı
   * **PA2 (TX):** USB-to-TTL (FTDI) modülünün RX pinine
   * **PA5:** Durum LED'i (Nucleo kartlarında dahili LED'dir)
4. Kodunuzu derleyin ve mikrodenetleyiciye yükleyin.
5. Bilgisayarınızda bir Serial Terminal programı (PuTTY, TeraTerm) açın ve `9600 Baud, 8 Data bits, 1 Stop bit, No Parity` ayarlarıyla COM portunuza bağlanın.
6. Potansiyometre ile ortam hassasiyetini ayarlayın ve ses çıkararak terminal üzerindeki logları (Timestamp, Event #, Peak, Duration) izleyin.

---
*Gömülü yazılım, donanımla aynı dili konuşmaktır.*
