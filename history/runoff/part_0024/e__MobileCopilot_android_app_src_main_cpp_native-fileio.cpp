#include <jni.h>
#include <string>
#include <fstream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <android/log.h>
#include <memory>

#define LOG_TAG "NativeFileIO"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT jlong JNICALL
Java_com_rawr_mobilecopilot_core_NativeFileManager_nativeOpenFile(
        JNIEnv *env,
        jobject /* this */,
        jstring path,
        jint mode) {
    
    const char *filePath = env->GetStringUTFChars(path, 0);
    
    int flags = 0;
    switch (mode) {
        case 0: flags = O_RDONLY; break;
        case 1: flags = O_WRONLY | O_CREAT | O_TRUNC; break;
        case 2: flags = O_RDWR | O_CREAT; break;
        default: flags = O_RDONLY; break;
    }
    
    int fd = open(filePath, flags, 0644);
    
    env->ReleaseStringUTFChars(path, filePath);
    
    if (fd == -1) {
        LOGE("Failed to open file: %s", filePath);
        return -1;
    }
    
    LOGI("Successfully opened file: %s, fd: %d", filePath, fd);
    return static_cast<jlong>(fd);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_rawr_mobilecopilot_core_NativeFileManager_nativeReadFile(
        JNIEnv *env,
        jobject /* this */,
        jlong fd,
        jbyteArray buffer,
        jint offset,
        jint length) {
    
    jbyte* bufferPtr = env->GetByteArrayElements(buffer, nullptr);
    if (bufferPtr == nullptr) {
        LOGE("Failed to get buffer pointer");
        return -1;
    }
    
    ssize_t bytesRead = read(static_cast<int>(fd), bufferPtr + offset, static_cast<size_t>(length));
    
    env->ReleaseByteArrayElements(buffer, bufferPtr, 0);
    
    if (bytesRead == -1) {
        LOGE("Failed to read file");
        return -1;
    }
    
    return static_cast<jint>(bytesRead);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_rawr_mobilecopilot_core_NativeFileManager_nativeWriteFile(
        JNIEnv *env,
        jobject /* this */,
        jlong fd,
        jbyteArray buffer,
        jint offset,
        jint length) {
    
    jbyte* bufferPtr = env->GetByteArrayElements(buffer, nullptr);
    if (bufferPtr == nullptr) {
        LOGE("Failed to get buffer pointer");
        return -1;
    }
    
    ssize_t bytesWritten = write(static_cast<int>(fd), bufferPtr + offset, static_cast<size_t>(length));
    
    env->ReleaseByteArrayElements(buffer, bufferPtr, JNI_ABORT);
    
    if (bytesWritten == -1) {
        LOGE("Failed to write file");
        return -1;
    }
    
    return static_cast<jint>(bytesWritten);
}

extern "C" JNIEXPORT void JNICALL
Java_com_rawr_mobilecopilot_core_NativeFileManager_nativeCloseFile(
        JNIEnv *env,
        jobject /* this */,
        jlong fd) {
    
    if (close(static_cast<int>(fd)) == -1) {
        LOGE("Failed to close file descriptor: %ld", fd);
    } else {
        LOGI("Successfully closed file descriptor: %ld", fd);
    }
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_rawr_mobilecopilot_core_NativeFileManager_nativeGetFileSize(
        JNIEnv *env,
        jobject /* this */,
        jstring path) {
    
    const char *filePath = env->GetStringUTFChars(path, 0);
    
    struct stat fileStat;
    if (stat(filePath, &fileStat) == -1) {
        LOGE("Failed to get file size: %s", filePath);
        env->ReleaseStringUTFChars(path, filePath);
        return -1;
    }
    
    env->ReleaseStringUTFChars(path, filePath);
    return static_cast<jlong>(fileStat.st_size);
}