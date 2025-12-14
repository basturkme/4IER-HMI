#include <Arduino.h>

// --- LIBRARIES ---
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
// ERROR REPORTER IS REQUIRED FOR MOST TFLITE VERSIONS TO AVOID CRASHES
#include "tensorflow/lite/micro/micro_error_reporter.h"

// Model and Test Data
#include "robust_emg_model_data.h"
#include "test_vectors.h"

// --- SETTINGS ---
const float FILTER_ALPHA = 0.2f; 
const float THRESHOLD = 0.2f; 

// --- GLOBAL VARIABLES ---
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
tflite::ErrorReporter* error_reporter = nullptr; // Added Error Reporter

// --- MEMORY SETTINGS ---
// 25KB is usually safe for EMG models.
// alignas(16) IS MANDATORY for ESP32/TFLite to prevent memory alignment crashes.
constexpr int kTensorArenaSize = 25 * 1024; 
alignas(16) uint8_t tensor_arena[kTensorArenaSize];

TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

float filtered_probability = 0.0f; 

void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println("=== SYSTEM STARTING ===");
  Serial.flush();

  // 1. Setup Error Reporter
  // This is critical. Passing nullptr to interpreter often causes abort() inside the constructor.
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;
  Serial.println("1. Error Reporter Setup... OK");

  // 2. Load Model
  Serial.print("2. Loading Model... ");
  model = tflite::GetModel(emg_model_data);
  if (model == nullptr) {
    Serial.println("ERROR: Model data is NULL!");
    while(1);
  }
  Serial.println("OK.");

  // 3. Schema Check
  Serial.print("3. Checking Schema... ");
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("ERROR: Schema version mismatch!");
    while(1); 
  }
  Serial.println("OK.");

  // 4. Create Resolver
  Serial.print("4. Creating Resolver... ");
  static tflite::AllOpsResolver resolver;
  Serial.println("OK.");
  
  // 5. Setup Interpreter
  Serial.print("5. Setting up Interpreter... ");
  
  // CRITICAL FIX: Passing 'error_reporter' instead of nullptr.
  // Many TFLite versions crash if this is missing.
  interpreter = new tflite::MicroInterpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  
  if (interpreter == nullptr) {
    Serial.println("ERROR: Failed to allocate Interpreter (Heap full?)");
    while(1);
  }
  Serial.println("OK.");
  Serial.flush();

  // 6. Allocate Tensors
  Serial.print("6. Allocating Tensors... ");
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    Serial.println("\nERROR: AllocateTensors() failed!");
    Serial.println("Likely kTensorArenaSize is too small.");
    while(1);
  }
  Serial.println("OK.");
  
  Serial.print("Arena used bytes: ");
  Serial.println(interpreter->arena_used_bytes());
  Serial.flush();

  // 7. Get Input/Output Pointers
  input = interpreter->input(0);
  output = interpreter->output(0);
  
  if (input == nullptr || output == nullptr) {
     Serial.println("ERROR: Input or Output tensor is NULL!");
     while(1);
  }
  
  Serial.println("=== SETUP COMPLETE. STARTING LOOP ===");
  delay(1000);
}

void loop() {

  for (int i = 0; i < TEST_DATA_LEN; i++) {

   

    input->data.f[0] = test_data[i][0];

    input->data.f[1] = test_data[i][1];

    input->data.f[2] = test_data[i][2];

    input->data.f[3] = test_data[i][3];



    // ... inside loop() ...

    if (interpreter->Invoke() != kTfLiteOk) {
      Serial.println("ERROR: Invoke failed!");
      return;
    }

    // Now this works because the model actually has 3 outputs
    float prob_rest   = output->data.f[0]; // Class 0
    float prob_index  = output->data.f[1]; // Class 1
    float prob_middle = output->data.f[2]; // Class 2

    // Logic to find the winner
    float max_prob = prob_rest;
    int predicted_class = 0; 

    if (prob_index > max_prob) {
        max_prob = prob_index;
        predicted_class = 1;
    }
    if (prob_middle > max_prob) {
        max_prob = prob_middle;
        predicted_class = 2;
    }

    // Visualization compatible format
    Serial.print("Test:"); Serial.print(test_data[i][0]);
    Serial.print("Rest:"); Serial.print(prob_rest);
    Serial.print(",Index:"); Serial.print(prob_index);
    Serial.print(",Middle:"); Serial.println(prob_middle);

// ...
       

    delay(50);

  }
  filtered_probability = 0.0f; 
  Serial.println("\n--- Loop Restarting ---\n");
  delay(2000);
}