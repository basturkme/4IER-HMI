#include <Arduino.h>
#include <TensorFlowLite_ESP32.h> 

// MODEL VERİSİ (Header dosyanızın adı neyse onu include edin)
#include "emg_model_data.h"     
// TEST VERİSİ 
#include "test_vectors.h"     
// TFLite Kütüphaneleri
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// --- GLOBALLER ---
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

const int kTensorArenaSize = 4 * 1024; 
uint8_t tensor_arena[kTensorArenaSize];

void setup() {
  Serial.begin(115200);
  
  // 1. Model Yükle
  model = tflite::GetModel(emg_model_data);
  static tflite::MicroErrorReporter micro_error_reporter;
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, &micro_error_reporter);
  interpreter = &static_interpreter;
  interpreter->AllocateTensors();
  
  input = interpreter->input(0);
  output = interpreter->output(0);
  
  Serial.println("--- SINYAL TARAMA TESTI BASLIYOR ---");
  
  // TANI: Modelin beklediği veri tipi nedir?
  // 1 = FLOAT32, 9 = INT8
  Serial.print("Model Input Tipi (1=FLOAT, 9=INT8): ");
  Serial.println(input->type);
}
// Global değişken olarak sayacı ekleyelim (loop'un dışına veya static olarak içine)
int current_sample_index = 0; 

void loop() {
  // 1. TEST VERİSİNİ SEÇ
  // test_data'dan o anki satırı okuyoruz
  
  // 2. INPUT TENSORUNU DOLDUR
  // S1_A1_E1 verisi 10 kanallı olduğu için input->dims->data[1] kullanıyoruz
  for (int i = 0; i < input->dims->data[1]; i++) { 
    float ham_veri = test_data[current_sample_index][i];
    float islenmis_veri = ham_veri * 10.0;
    // Model tipine göre (INT8 veya FLOAT) veriyi yerleştir
    if (input->type == kTfLiteInt8) {
      int8_t quant_value = (int8_t)(ham_veri / input->params.scale + input->params.zero_point);
      input->data.int8[i] = quant_value;
    } else {
       input->data.f[i] = ham_veri;
    }
  }

  // 3. TAHMİN (Inference)
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    Serial.println("Invoke failed!");
    return;
  }

  // 4. ÇIKTILARI AL (CLASSIFICATION LOGIC - DÜZELTİLEN KISIM)
  float max_probability = 0.0;
  int predicted_class = -1;
  
  // Çıktı katmanının boyutunu (sınıf sayısını) al
  int num_classes = output->dims->data[1]; 

  // Tüm sınıfları tara ve en yüksek olasılığı bul
  for (int i = 0; i < num_classes; i++) {
    float probability;
    
    if (output->type == kTfLiteInt8) {
       probability = (output->data.int8[i] - output->params.zero_point) * output->params.scale;
    } else {
       probability = output->data.f[i];
    }

    if (probability > max_probability) {
      max_probability = probability;
      predicted_class = i;
    }
  }

  // 5. SERİ PORTA YAZDIR (HATANIN ÇÖZÜMÜ)
  // Artık prob_0 veya prob_1 yok. "Tahmin Edilen Sınıf" ve "Güven Oranı" var.
  
  // 1. Değer: Sinyal (Görsel referans için)
  Serial.print(test_data[current_sample_index][0] * 10); 
  Serial.print(",");
  
  // 2. Değer: Tahmin Edilen Hareket (0, 1, 2, ... 12)
  Serial.print(predicted_class); 
  Serial.print(",");
  
  // 3. Değer: Güven Oranı (0.0 - 1.0 arası)
  Serial.println(max_probability);

  // 6. BİR SONRAKİ VERİYE GEÇ
  current_sample_index++;
  
  if (current_sample_index >= TEST_DATA_LEN) {
    current_sample_index = 0;
  }

  delay(50); 
}