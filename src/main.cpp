#include <Arduino.h>
#include "model_data.h" 

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

// --- MODEL POINTER ---
// FIXED: Updated name to match your new 'emg_model.tflite' file
const unsigned char* my_model_array = emg_model_tflite; 

// --- GLOBALS (Must be defined first!) ---
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
tflite::ErrorReporter* error_reporter = nullptr; 

// Memory pool
constexpr int kTensorArenaSize = 16 * 1024; 
uint8_t tensor_arena[kTensorArenaSize];

// --- SETTINGS ---
int kInferenceWindowSize = 0;   
float input_buffer[2000];       
const int EMG_PIN = 34;        

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // 1. Load Model
  model = tflite::GetModel(my_model_array);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report("Model version mismatch!");
    while (1);
  }

  // 2. Define Operations
  static tflite::AllOpsResolver resolver;

  // 3. Setup Interpreter 
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  // 4. Allocate Memory
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    error_reporter->Report("AllocateTensors() failed");
    while (1);
  }

  // 5. Get Pointers
  input = interpreter->input(0);
  output = interpreter->output(0);
  
  // 6. Set Window Size dynamically
  kInferenceWindowSize = input->dims->data[1];
  
  error_reporter->Report("Model Loaded! Input Window Size: %d", kInferenceWindowSize);
}

// --- LOOP ---
void loop() {
  // 1. ACQUIRE REAL DATA 
  int raw_val = analogRead(EMG_PIN);
  float normalized_val = raw_val / 4095.0f; 

  // 2. SLIDING WINDOW (Shift Buffer Left)
  for (int i = 0; i < kInferenceWindowSize - 1; i++) {
    input_buffer[i] = input_buffer[i + 1];
  }
  input_buffer[kInferenceWindowSize - 1] = normalized_val;

  // 3. FILL TENSOR
  if (input->type == kTfLiteInt8) {
    for (int i = 0; i < kInferenceWindowSize; i++) {
      input->data.int8[i] = (input_buffer[i] / input->params.scale) + input->params.zero_point;
    }
  } else {
    for (int i = 0; i < kInferenceWindowSize; i++) {
      input->data.f[i] = input_buffer[i];
    }
  }

  // 4. RUN INFERENCE (Every 50ms)
  static unsigned long last_run = 0;
  if (millis() - last_run > 50) { 
    
    if (interpreter->Invoke() != kTfLiteOk) {
      Serial.println("Invoke failed!");
      return;
    }

    // 5. PROCESS OUTPUT
    int num_classes = output->dims->data[1];
    float max_prob = 0.0;
    int best_class = -1;

    Serial.print("EMG: "); Serial.print(raw_val);
    Serial.print(" -> Probs: [");

    for (int i = 0; i < num_classes; i++) {
      float prob = 0;
      if (output->type == kTfLiteInt8) {
        prob = (output->data.int8[i] - output->params.zero_point) * output->params.scale;
      } else {
        prob = output->data.f[i];
      }
      
      Serial.print(prob, 2); 
      if (i < num_classes - 1) Serial.print(", ");

      if (prob > max_prob) {
        max_prob = prob;
        best_class = i;
      }
    }
    Serial.print("] -> PREDICTION: Class "); Serial.println(best_class);
    
    last_run = millis();
  }
}