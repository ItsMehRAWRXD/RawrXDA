#include <jni.h>
#include <string>
#include <vector>
#include <memory>
#include <android/log.h>
#include "tensorflow/lite/interpreter.h"
#include <map>

// Minimal GGUF shim: record metadata pairs and expose via JNI
struct GGUFShim {
    std::map<std::string, std::string> meta;
};

extern "C" JNIEXPORT jlong JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeParseGgufMeta(
        JNIEnv* env,
        jobject,
        jbyteArray ggufBytes) {
    jsize len = env->GetArrayLength(ggufBytes);
    if (len < 16) return 0;
    jbyte* data = env->GetByteArrayElements(ggufBytes, nullptr);
    // very light check: starts with 'GGUF'
    if (!(data[0]=='G' && data[1]=='G' && data[2]=='U' && data[3]=='F')) {
        env->ReleaseByteArrayElements(ggufBytes, data, JNI_ABORT);
        return 0;
    }
    auto* shim = new GGUFShim();
    shim->meta["magic"] = "GGUF";
    shim->meta["size"] = std::to_string(len);
    env->ReleaseByteArrayElements(ggufBytes, data, JNI_ABORT);
    return reinterpret_cast<jlong>(shim);
}

extern "C" JNIEXPORT void JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeReleaseGgufMeta(
        JNIEnv* env,
        jobject,
        jlong ptr) {
    auto* shim = reinterpret_cast<GGUFShim*>(ptr);
    delete shim;
}
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"

#define LOG_TAG "ModelInference"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct TFLiteModelWrapper {
    std::unique_ptr<tflite::FlatBufferModel> model;
    std::unique_ptr<tflite::Interpreter> interpreter;
    tflite::ops::builtin::BuiltinOpResolver resolver;
};

extern "C" JNIEXPORT jlong JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeLoadModel(
        JNIEnv *env,
        jobject /* this */,
        jstring modelPath) {
    
    const char *path = env->GetStringUTFChars(modelPath, 0);
    
    TFLiteModelWrapper* wrapper = new TFLiteModelWrapper();
    
    // Load model
    wrapper->model = tflite::FlatBufferModel::BuildFromFile(path);
    if (!wrapper->model) {
        LOGE("Failed to load model from: %s", path);
        delete wrapper;
        env->ReleaseStringUTFChars(modelPath, path);
        return 0;
    }
    
    // Build interpreter
    tflite::InterpreterBuilder builder(*(wrapper->model), wrapper->resolver);
    builder(&(wrapper->interpreter));
    
    if (!wrapper->interpreter) {
        LOGE("Failed to build interpreter for model: %s", path);
        delete wrapper;
        env->ReleaseStringUTFChars(modelPath, path);
        return 0;
    }
    
    // Allocate tensor buffers
    if (wrapper->interpreter->AllocateTensors() != kTfLiteOk) {
        LOGE("Failed to allocate tensors for model: %s", path);
        delete wrapper;
        env->ReleaseStringUTFChars(modelPath, path);
        return 0;
    }
    
    LOGI("Successfully loaded model: %s", path);
    env->ReleaseStringUTFChars(modelPath, path);
    
    return reinterpret_cast<jlong>(wrapper);
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeGetInputShape(
        JNIEnv *env,
        jobject /* this */,
        jlong modelPtr,
        jint inputIndex) {
    
    TFLiteModelWrapper* wrapper = reinterpret_cast<TFLiteModelWrapper*>(modelPtr);
    if (!wrapper || !wrapper->interpreter) {
        LOGE("Invalid model pointer");
        return nullptr;
    }
    
    TfLiteTensor* tensor = wrapper->interpreter->input_tensor(inputIndex);
    if (!tensor) {
        LOGE("Failed to get input tensor at index: %d", inputIndex);
        return nullptr;
    }
    
    int dims = tensor->dims->size;
    jintArray result = env->NewIntArray(dims);
    jint* dimArray = env->GetIntArrayElements(result, nullptr);
    
    for (int i = 0; i < dims; i++) {
        dimArray[i] = tensor->dims->data[i];
    }
    
    env->ReleaseIntArrayElements(result, dimArray, 0);
    return result;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeGetOutputShape(
        JNIEnv *env,
        jobject /* this */,
        jlong modelPtr,
        jint outputIndex) {
    
    TFLiteModelWrapper* wrapper = reinterpret_cast<TFLiteModelWrapper*>(modelPtr);
    if (!wrapper || !wrapper->interpreter) {
        LOGE("Invalid model pointer");
        return nullptr;
    }
    
    TfLiteTensor* tensor = wrapper->interpreter->output_tensor(outputIndex);
    if (!tensor) {
        LOGE("Failed to get output tensor at index: %d", outputIndex);
        return nullptr;
    }
    
    int dims = tensor->dims->size;
    jintArray result = env->NewIntArray(dims);
    jint* dimArray = env->GetIntArrayElements(result, nullptr);
    
    for (int i = 0; i < dims; i++) {
        dimArray[i] = tensor->dims->data[i];
    }
    
    env->ReleaseIntArrayElements(result, dimArray, 0);
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeSetInputData(
        JNIEnv *env,
        jobject /* this */,
        jlong modelPtr,
        jint inputIndex,
        jfloatArray inputData) {
    
    TFLiteModelWrapper* wrapper = reinterpret_cast<TFLiteModelWrapper*>(modelPtr);
    if (!wrapper || !wrapper->interpreter) {
        LOGE("Invalid model pointer");
        return JNI_FALSE;
    }
    
    TfLiteTensor* tensor = wrapper->interpreter->input_tensor(inputIndex);
    if (!tensor) {
        LOGE("Failed to get input tensor at index: %d", inputIndex);
        return JNI_FALSE;
    }
    
    jsize dataSize = env->GetArrayLength(inputData);
    jfloat* data = env->GetFloatArrayElements(inputData, nullptr);
    
    if (tensor->type != kTfLiteFloat32) {
        LOGE("Input tensor type mismatch. Expected float32.");
        env->ReleaseFloatArrayElements(inputData, data, JNI_ABORT);
        return JNI_FALSE;
    }
    
    size_t tensorSize = tensor->bytes / sizeof(float);
    if (dataSize != tensorSize) {
        LOGE("Input data size mismatch. Expected: %zu, Got: %d", tensorSize, dataSize);
        env->ReleaseFloatArrayElements(inputData, data, JNI_ABORT);
        return JNI_FALSE;
    }
    
    memcpy(tensor->data.f, data, tensor->bytes);
    env->ReleaseFloatArrayElements(inputData, data, JNI_ABORT);
    
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeRunInference(
        JNIEnv *env,
        jobject /* this */,
        jlong modelPtr) {
    
    TFLiteModelWrapper* wrapper = reinterpret_cast<TFLiteModelWrapper*>(modelPtr);
    if (!wrapper || !wrapper->interpreter) {
        LOGE("Invalid model pointer");
        return JNI_FALSE;
    }
    
    TfLiteStatus status = wrapper->interpreter->Invoke();
    if (status != kTfLiteOk) {
        LOGE("Model inference failed with status: %d", status);
        return JNI_FALSE;
    }
    
    return JNI_TRUE;
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeGetOutputData(
        JNIEnv *env,
        jobject /* this */,
        jlong modelPtr,
        jint outputIndex) {
    
    TFLiteModelWrapper* wrapper = reinterpret_cast<TFLiteModelWrapper*>(modelPtr);
    if (!wrapper || !wrapper->interpreter) {
        LOGE("Invalid model pointer");
        return nullptr;
    }
    
    TfLiteTensor* tensor = wrapper->interpreter->output_tensor(outputIndex);
    if (!tensor) {
        LOGE("Failed to get output tensor at index: %d", outputIndex);
        return nullptr;
    }
    
    if (tensor->type != kTfLiteFloat32) {
        LOGE("Output tensor type not supported. Only float32 is supported.");
        return nullptr;
    }
    
    size_t tensorSize = tensor->bytes / sizeof(float);
    jfloatArray result = env->NewFloatArray(tensorSize);
    jfloat* resultData = env->GetFloatArrayElements(result, nullptr);
    
    memcpy(resultData, tensor->data.f, tensor->bytes);
    env->ReleaseFloatArrayElements(result, resultData, 0);
    
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeReleaseModel(
        JNIEnv *env,
        jobject /* this */,
        jlong modelPtr) {
    
    TFLiteModelWrapper* wrapper = reinterpret_cast<TFLiteModelWrapper*>(modelPtr);
    if (wrapper) {
        delete wrapper;
        LOGI("Successfully released model");
    }
}