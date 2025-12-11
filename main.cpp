#include <Arduino.h>

// MaxGerhardt / Atomic14 kütüphanesi uyumlu başlıklar
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// MODEL VE TEST VERİSİ
// İnen dosyanın adı "robust_emg_model_data.h" ise adını "emg_model_data.h" yap
// ya da buradaki include ismini değiştir.
#include "emg_model_data.h"
#include "test_vectors.h"

// TFLite Global Değişkenler
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

// HAFIZA AYARI
// Dense model hafiftir, 60KB fazlasıyla yeter ve çökmez.
// "alignas(16)" komutu ESP32-S3 için kritik, hafızayı hizalar.
const int kTensorArenaSize = 60 * 1024;
alignas(16) uint8_t tensor_arena[kTensorArenaSize];

// Test sayacı
int current_sample = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("--- ESP32 'Güçlü Dense' Testi Başlıyor ---");

  // 1. Modeli Yükle
  model = tflite::GetModel(emg_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("HATA: Model schema versiyonu uyumsuz!");
    while (1);
  }

  // 2. Operatörleri Çözücü (Resolver)
  static tflite::AllOpsResolver resolver;

  // 3. Yorumlayıcı (Interpreter)
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, nullptr);
  interpreter = &static_interpreter;

  // 4. Bellek Ayırma
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    Serial.println("HATA: Bellek ayrılamadı! (AllocateTensors)");
    while (1);
  }

  // 5. Giriş ve Çıkışları Al
  input = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("Model Başarıyla Yüklendi. Loop başlıyor... 🚀");
}

void loop() {
  // 1. DATA INJECTION: Test verisini modele yükle
  // Dosyadan sıradaki 4 sensör değerini alıyoruz
  for (int i = 0; i < 4; i++) {
    input->data.f[i] = test_data[current_sample][i];
  }

  // 2. TAHMİN (INFERENCE)
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    Serial.println("Tahmin Hatası!");
    return;
  }

  // 3. SONUÇ OKUMA
  // Çıkışımız: [0]=Dinlenme, [1]=Hareket (Yumruk)
  float olasilik_hareket = output->data.f[1];

  // 4. GRAFİK ÇİZDİRME (Serial Plotter)
  // Mavi Çizgi: Giriş sinyali (Simülasyon)
  // Kırmızı Çizgi: Yapay Zekanın kararı (0 ile 5 arası ölçekledik)
  Serial.print(test_data[current_sample][0]); 
  Serial.print(","); // Araya virgül koyuyoruz (CSV Formatı)
  
  // 2. Tahmin Sınıfı (0: Dinlenme, 1: Yumruk)
  // Olasılık > 0.5 ise 1, değilse 0 gönder
  int tahmin = (olasilik_hareket > 0.5) ? 1 : 0;
  Serial.print(tahmin);
  Serial.print(",");

  // 3. Güven Oranı (0.0 - 1.0 arası)
  Serial.println(olasilik_hareket); 
  // (Burada "Sensor_Sim" gibi yazılar YAZDIRMIYORUZ!)

  // Döngü Kontrolü
  current_sample++;
  if (current_sample >= TEST_DATA_LEN) {
    current_sample = 0;
  }
  // Hız ayarı (Çok hızlı akarsa grafik okunmaz)
  delay(20);
}