#include <Arduino.h>

// --- KÜTÜPHANELER ---
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
// version.h ARTIK GEREK YOK (Chirale kutuphanesi icin)

// Model ve Test Verileri
#include "robust_emg_model_data.h"
#include "test_vectors.h"

// --- AYARLAR ---
// Filtre Katsayısı (0.0 ile 1.0 arası)
// 0.2 - 0.3 genelde en iyi dengedir.
const float FILTER_ALPHA = 0.2f; 

// Eşik Değeri
const float THRESHOLD = 0.2f; // %60'ın üzerindeyse "Hareket" kabul et

// --- GLOBAL DEĞİŞKENLER ---
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
// Error reporter kaldırıldı (Gerek yok)

constexpr int kTensorArenaSize = 4 * 1024; 
uint8_t tensor_arena[kTensorArenaSize];

TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

// Filtre için hafıza değişkeni
float filtered_probability = 0.0f; 

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("EMG Model (Filtreli) Baslatiliyor...");

  // 1. Modeli Yükle
  model = tflite::GetModel(emg_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model sema versiyonu uyusmuyor!");
    while(1); // Hata varsa dur
  }

  // 2. Interpreter Kurulumu
  static tflite::AllOpsResolver resolver;
  
  // DÜZELTİLEN KISIM BURASI:
  // error_reporter yerine nullptr, nullptr kullanıyoruz.
  interpreter = new tflite::MicroInterpreter(
      model, resolver, tensor_arena, kTensorArenaSize, nullptr, nullptr);

  // 3. Bellek Tahsisi (Allocate Tensors)
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("Bellek tahsis hatasi (AllocateTensors)!");
    while(1);
  }

  // 4. Giriş/Çıkış İşaretçilerini Al
  input = interpreter->input(0);
  output = interpreter->output(0);
  
  Serial.println("Sistem hazir! Test basliyor...");
}

void loop() {
  // Test verileri üzerinde dönüyoruz
  for (int i = 0; i < TEST_DATA_LEN; i++) {
    
    // 1. Veriyi Hazırla (Test verisinden oku)
    input->data.f[0] = test_data[i][0];
    input->data.f[1] = test_data[i][1];
    input->data.f[2] = test_data[i][2];
    input->data.f[3] = test_data[i][3];

    // 2. Tahmin Yap (Invoke)
    if (interpreter->Invoke() != kTfLiteOk) {
      Serial.println("Invoke hatasi!");
      return;
    }

    // 3. Ham Sonucu Al (Hareket Olasılığı [1])
    // output->data.f[0] -> Dinlenme
    // output->data.f[1] -> Hareket
    float raw_move_prob = output->data.f[1]; 

    // --- 4. FİLTRELEME İŞLEMİ (Low Pass Filter) ---
    // Yeni Değer = (Alpha * Ham) + ((1 - Alpha) * Eski)
    filtered_probability = (FILTER_ALPHA * raw_move_prob) + ((1.0f - FILTER_ALPHA) * filtered_probability);

    // 5. Karar Verme
    bool is_moving = (filtered_probability > THRESHOLD);

    // --- EKRANA YAZDIRMA ---
    // Format: "Ham: 0.XX | Filtreli: 0.XX => DURUM"
    Serial.print("Ham: "); 
    Serial.print(raw_move_prob, 2);
    Serial.print(" | Filtreli: "); 
    Serial.print(filtered_probability, 2);
    
    if (is_moving) {
      Serial.println(" ==> HAREKET (ON) ***");
    } else {
      Serial.println(" ==> Dinlenme (OFF)");
    }
    
    delay(50); // Okumayı kolaylaştırmak için biraz bekle
  }
  
  // Test bitince döngü başa sarar
  filtered_probability = 0.0f; // Filtreyi sıfırla
  Serial.println("\n--- Test Vektorleri Bitti, Basa Donuyor ---\n");
  delay(2000);
}
