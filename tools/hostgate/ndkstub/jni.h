#pragma once
// Host-gate shim ONLY: lets the REAL preloader headers be parsed on a host
// without the NDK sysroot. Minimal surface used by pl/Mod.hpp.
typedef void* jobject;
typedef void* jclass;
typedef void* JNIEnv;
typedef void* JavaVM;
typedef int jint;
typedef void* jstring;
