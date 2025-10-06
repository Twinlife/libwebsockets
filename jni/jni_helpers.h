/*
 *  Copyright (c) 2019 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 */

#ifndef WEBSOCKET_ANDROID_JNI_HELPERS_H_
#define WEBSOCKET_ANDROID_JNI_HELPERS_H_

//
// Based on sdk/android/src/jni/jni_helpers.h (WebRTC 64.8)
//  693e067740fbd6da62d7359e5d92ea520f3aa27d
//

#include <jni.h>
#include <string>

#include "rtc_base/checks.h"
#include "sdk/android/src/jni/jvm.h"

#define CHECK_EXCEPTION(jni)        \
  RTC_CHECK(!jni->ExceptionCheck()) \
      << (jni->ExceptionDescribe(), jni->ExceptionClear(), "")

namespace webrtc {
namespace jni {

  void DeleteGlobalRef(JNIEnv* jni, jobject o);
}}

namespace websocket {
namespace jni {

// JNIEnv-helper methods that RTC_CHECK success: no Java exception thrown and
// found object/class/method/field is non-null.
jmethodID GetMethodID(
    JNIEnv* jni, jclass c, const std::string& name, const char* signature);

jclass GetObjectClass(JNIEnv* jni, jobject object);

// Returns true if |obj| == null in Java.
bool IsNull(JNIEnv* jni, jobject obj);

// Given a UTF-8 encoded |native| string return a new (UTF-16) jstring.
jstring NativeToJavaString(JNIEnv* jni, const std::string& native);

// Scope Java local references to the lifetime of this object.  Use in all C++
// callbacks (i.e. entry points that don't originate in a Java callstack
// through a "native" method call).
class ScopedLocalRefFrame {
 public:
  explicit ScopedLocalRefFrame(JNIEnv* jni);
  ~ScopedLocalRefFrame();

 private:
  JNIEnv* jni_;
};

// Scoped holder for global Java refs.
template<class T>  // T is jclass, jobject, jintArray, etc.
class ScopedGlobalRef {
 public:
  ScopedGlobalRef(JNIEnv* jni, T obj)
      : obj_(static_cast<T>(jni->NewGlobalRef(obj))) {}
  ~ScopedGlobalRef() {
    webrtc::jni::DeleteGlobalRef(webrtc::jni::AttachCurrentThreadIfNeeded(), obj_);
  }
  T operator*() const {
    return obj_;
  }
 private:
  T obj_;
};

}  // namespace jni
}  // namespace websocket

#endif // WEBSOCKET_ANDROID_JNI_HELPERS_H_
