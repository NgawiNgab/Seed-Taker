#pragma once
// Host-gate shim ONLY for <android/log.h> (see jni.h).
enum { ANDROID_LOG_UNKNOWN = 0, ANDROID_LOG_DEFAULT, ANDROID_LOG_VERBOSE,
       ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR,
       ANDROID_LOG_FATAL, ANDROID_LOG_SILENT };
extern "C" int __android_log_print(int prio, const char* tag, const char* fmt, ...);
