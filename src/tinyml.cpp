#include "tinyml.h"

#include <time.h>
#include <cmath>

namespace
{
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *alert_output = nullptr;

    constexpr int kTensorArenaSize = 8 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];
}

constexpr int kInputFeatureCount = 4;
constexpr int kAlertClassCount = 5;
constexpr long kGmtOffsetSeconds = 7 * 3600;
constexpr int kDaylightOffsetSeconds = 0;
constexpr char kNtpServer1[] = "pool.ntp.org";
constexpr char kNtpServer2[] = "time.nist.gov";

#define TEMP_MEAN 27.55516209f
#define TEMP_STD 2.81862948f

#define HUM_MEAN 78.74067213f
#define HUM_STD 15.23432879f

const char *const kAlertLabels[kAlertClassCount] = {
    "Safe",
    "Caution",
    "Extreme caution",
    "Danger",
    "Extreme danger",
};

float get_output_value(const TfLiteTensor *tensor, int i)
{
    if (tensor->type == kTfLiteFloat32)
    {
        return tensor->data.f[i];
    }

    if (tensor->type == kTfLiteInt8)
    {
        int8_t val = tensor->data.int8[i];
        return (val - tensor->params.zero_point) * tensor->params.scale;
    }

    return 0.0f;
}

int get_tensor_class_count(const TfLiteTensor *tensor)
{
    if (tensor == nullptr || tensor->dims == nullptr || tensor->dims->size <= 0)
    {
        return 0;
    }
    return tensor->dims->data[tensor->dims->size - 1];
}

int get_predicted_class(const TfLiteTensor *tensor, int class_count)
{
    int safe_class_count = get_tensor_class_count(tensor);
    if (safe_class_count <= 0)
    {
        return 0;
    }

    if (class_count < safe_class_count)
    {
        safe_class_count = class_count;
    }

    int predicted = 0;
    float max_val = get_output_value(tensor, 0);
    for (int i = 1; i < safe_class_count; i++)
    {
        float val = get_output_value(tensor, i);
        if (val > max_val)
        {
            max_val = val;
            predicted = i;
        }
    }
    return predicted;
}

const char *alertStatusToText(int label)
{
    if (label < 0 || label >= kAlertClassCount)
    {
        return "Unknown";
    }
    return kAlertLabels[label];
}

void print_tensor_probabilities(const char *title, const TfLiteTensor *tensor, int class_count, const char *const *labels)
{
    Serial.print(title);
    Serial.println(":");
    for (int i = 0; i < class_count; i++)
    {
        Serial.print("  ");
        Serial.print(i);
        Serial.print(" - ");
        Serial.print(labels[i]);
        Serial.print(": ");
        Serial.println(get_output_value(tensor, i), 4);
    }
}

void print_prediction_summary(int alert_status)
{
    Serial.println("Prediction summary:");
    Serial.print("  Alert : ");
    Serial.print(alert_status);
    Serial.print(" - ");
    Serial.println(alertStatusToText(alert_status));
}

bool syncSystemTime()
{
    Serial.println("Syncing system time with NTP...");
    configTime(kGmtOffsetSeconds, kDaylightOffsetSeconds, kNtpServer1, kNtpServer2);

    struct tm timeinfo;
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        if (getLocalTime(&timeinfo, 1000))
        {
            Serial.print("✓ Time synced at attempt ");
            Serial.println(attempt + 1);
            return true;
        }
        Serial.print(".");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    Serial.println("\n✗ NTP sync timeout - using defaults");
    return false;
}

bool getMonthAndHourFromSystemTime(int &month, int &hour)
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 200))
    {
        Serial.println("⚠ getLocalTime() failed - using defaults");
        month = 5;
        hour = 0;
        return false;
    }

    month = timeinfo.tm_mon + 1;
    hour = timeinfo.tm_hour;
    Serial.print("✓ Got time from system: month=");
    Serial.print(month);
    Serial.print(", hour=");
    Serial.println(hour);
    return true;
}

void setupTinyML()
{
    Serial.println("TensorFlow Lite Init...");
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    if (!syncSystemTime())
    {
        Serial.println("Time sync not ready yet; month/hour will be skipped until NTP is available.");
    }

    model = tflite::GetModel(tinyml_weather_model);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        error_reporter->Report("Model schema mismatch!");
        return;
    }

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);

    interpreter = &static_interpreter;
    if (interpreter->AllocateTensors() != kTfLiteOk)
    {
        error_reporter->Report("AllocateTensors() failed");
        return;
    }

    input = interpreter->input(0);
    // Model now exports single head: alert (e.g., 4 classes) at output(0)
    alert_output = interpreter->output(0);

    if (input == nullptr || alert_output == nullptr)
    {
        error_reporter->Report("Tensor binding failed");
        interpreter = nullptr;
        return;
    }

    if (input->dims->size < 2 || input->dims->data[1] != kInputFeatureCount)
    {
        Serial.print("Model input mismatch. Expected [1, ");
        Serial.print(kInputFeatureCount);
        Serial.println("]");
        interpreter = nullptr;
        return;
    }

    Serial.print("Input shape: [");
    Serial.print(input->dims->data[0]);
    Serial.print(", ");
    Serial.print(input->dims->data[1]);
    Serial.println("]");

    Serial.print("Input scale: ");
    Serial.println(input->params.scale);
    Serial.print("Input zero_point: ");
    Serial.println(input->params.zero_point);
    Serial.print("Input type: ");
    Serial.println(input->type == kTfLiteFloat32 ? "float32" : "not-float32");
    Serial.print("Output classes [alert]: [");
    Serial.print(get_tensor_class_count(alert_output));
    Serial.println("]");
    Serial.println("TinyML ready.");
}

void tiny_ml_task(void *pvParameters)
{
    setupTinyML();

    while (1)
    {
        if (!interpreter)
        {
            Serial.println("Interpreter not ready!");
            vTaskDelay(1000);
            continue;
        }

        float temp = 0;
        float hum = 0;
        int month = 1;
        int hour = 0;

        if (takeSystemContext(portMAX_DELAY))
        {
            temp = systemContext.temperature;
            hum = systemContext.humidity;
            giveSystemContext();
        }

        if (!getMonthAndHourFromSystemTime(month, hour))
        {
            Serial.println("System time not available yet; using default month/hour.");
        }

        if (input->type != kTfLiteFloat32)
        {
            Serial.println("Unexpected input tensor type, expected float32");
            vTaskDelay(1000);
            continue;
        }

        // temp = 30;
        // hum = 62.25;
        // month = 9;
        // hour = 21;

        float temp_norm = (temp - TEMP_MEAN) / TEMP_STD;
        float hum_norm = (hum - HUM_MEAN) / HUM_STD;

        // Model expects 4 inputs: temp_norm, hum_norm, month, hour
        input->data.f[0] = temp_norm;
        input->data.f[1] = hum_norm;
        input->data.f[2] = (float)month;
        input->data.f[3] = (float)hour;

        if (interpreter->Invoke() != kTfLiteOk)
        {
            error_reporter->Report("Invoke failed");
            vTaskDelay(1000);
            continue;
        }

        int alert_status = get_predicted_class(alert_output, kAlertClassCount);

        if (takeSystemContext(portMAX_DELAY))
        {
            systemContext.alert_status = alert_status;
            giveSystemContext();
        }

        Serial.println("----------------------");
        Serial.print("Input -> temp=");
        Serial.print(temp, 2);
        Serial.print(", hum=");
        Serial.print(hum, 2);
        Serial.print(", month=");
        Serial.print(month);
        Serial.print(", hour=");
        Serial.println(hour);

        print_tensor_probabilities("Alert output", alert_output, kAlertClassCount, kAlertLabels);
        print_prediction_summary(alert_status);

        vTaskDelay(5000);
    }
}