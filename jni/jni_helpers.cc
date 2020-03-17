/*
 *  Copyright (c) 2019 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 */

#include "jni_helpers.h"

#include <asm/unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

#include "rtc_base/thread_checker.h"

namespace websocket {
namespace jni {

// JNIEnv-helper methods that RTC_CHECK success: no Java exception thrown and
// found object/class/method/field is non-null.
jmethodID GetMethodID(
    JNIEnv* jni, jclass c, const std::string& name, const char* signature) {
  jmethodID m = jni->GetMethodID(c, name.c_str(), signature);
  CHECK_EXCEPTION(jni) << "error during GetMethodID: " << name << ", "
                       << signature;
  RTC_CHECK(m) << name << ", " << signature;
  return m;
}

jclass GetObjectClass(JNIEnv* jni, jobject object) {
  jclass c = jni->GetObjectClass(object);
  CHECK_EXCEPTION(jni) << "error during GetObjectClass";
  RTC_CHECK(c) << "GetObjectClass returned NULL";
  return c;
}

bool IsNull(JNIEnv* jni, jobject obj) {
  return jni->IsSameObject(obj, nullptr);
}

jstring NativeToJavaString(JNIEnv* jni, const std::string& native) {
  jstring jstr = jni->NewStringUTF(native.c_str());
  CHECK_EXCEPTION(jni) << "error during NewStringUTF";
  return jstr;
}

// Scope Java local references to the lifetime of this object.  Use in all C++
// callbacks (i.e. entry points that don't originate in a Java callstack
// through a "native" method call).
ScopedLocalRefFrame::ScopedLocalRefFrame(JNIEnv* jni) : jni_(jni) {
  RTC_CHECK(!jni_->PushLocalFrame(0)) << "Failed to PushLocalFrame";
}

ScopedLocalRefFrame::~ScopedLocalRefFrame() {
  jni_->PopLocalFrame(nullptr);
}

}  // namespace jni
}  // namespace webrtc
