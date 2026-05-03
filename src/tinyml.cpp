#include "tinyml.h"

namespace
{
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;

    constexpr int kTensorArenaSize = 8 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];
}

#define TEMP_MEAN 25.0780196
#define TEMP_STD 4.29019663
#define HUM_MEAN 76.77308542
#define HUM_STD 11.34869143

int8_t float_to_int8(float value, float scale, int zero_point)
{
    int32_t result = (int32_t)(value / scale + zero_point);
    if (result > 127)
        result = 127;
    if (result < -128)
        result = -128;
    return (int8_t)result;
}

float get_output_value(TfLiteTensor *output, int i)
{
    int8_t val = output->data.int8[i];
    return (val - output->params.zero_point) * output->params.scale;
}

int get_predicted_class(TfLiteTensor *output)
{
    int predicted = 0;
    float max_val = get_output_value(output, 0);
    for (int i = 1; i < 4; i++)
    {
        float val = get_output_value(output, i);
        if (val > max_val)
        {
            max_val = val;
            predicted = i;
        }
    }
    return predicted;
}

void setupTinyML()
{
    Serial.println("TensorFlow Lite Init...");
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(dht_weather_prediction_model_tflite);
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
    output = interpreter->output(0);

    Serial.print("Input scale: ");
    Serial.println(input->params.scale);
    Serial.print("Input zero_point: ");
    Serial.println(input->params.zero_point);
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

        if (takeSystemContext(portMAX_DELAY))
        {
            temp = systemContext.temperature;
            hum = systemContext.humidity;
            giveSystemContext();
        }

        float temp_norm = (temp - TEMP_MEAN) / TEMP_STD;
        float hum_norm = (hum - HUM_MEAN) / HUM_STD;

        input->data.int8[0] = float_to_int8(temp_norm, input->params.scale, input->params.zero_point);
        input->data.int8[1] = float_to_int8(hum_norm, input->params.scale, input->params.zero_point);

        if (interpreter->Invoke() != kTfLiteOk)
        {
            error_reporter->Report("Invoke failed");
            vTaskDelay(1000);
            continue;
        }

        int predicted = get_predicted_class(output);
        if (takeSystemContext(portMAX_DELAY))
        {
            systemContext.weather_status = predicted;
            giveSystemContext();
        }

        Serial.print("Temp: ");
        Serial.print(temp);
        Serial.print(" | Hum: ");
        Serial.println(hum);

        Serial.print("Probabilities: ");
        for (int i = 0; i < 4; i++)
        {
            Serial.print(get_output_value(output, i), 4);
            Serial.print(" ");
        }
        Serial.println();

        Serial.print("Predicted: ");
        Serial.print(predicted);
        Serial.print(" => ");
        Serial.println(get_weather_label(predicted));
        Serial.println("----------------------");

        vTaskDelay(5000);
    }
}