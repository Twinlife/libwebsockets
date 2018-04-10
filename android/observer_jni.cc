/*
 *  Copyright (c) 2018 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 */

#include "observer_jni.h"

extern "C" {
#include <libwebsockets.h>
#include <private-libwebsockets.h>
}

namespace websocket {
namespace jni {

ObserverJni::ObserverJni(JNIEnv* jni, jobject j_observer)
  : j_observer_global_(jni, j_observer),
    j_observer_class_(jni, webrtc::jni::GetObjectClass(jni, *j_observer_global_)),
    j_on_connect_(webrtc::jni::GetMethodID(jni, *j_observer_class_, "onConnect", "(JJ)V")),
    j_on_connect_error_(webrtc::jni::GetMethodID(jni, *j_observer_class_, "onConnectError",
						 "(JJLjava/lang/String;)V")),
    j_on_writable_(webrtc::jni::GetMethodID(jni, *j_observer_class_, "onWritable", "(J)V")),
    j_on_message_(webrtc::jni::GetMethodID(jni, *j_observer_class_,
					   "onMessage", "(JLjava/nio/ByteBuffer;Z)V")),
    j_on_close_(webrtc::jni::GetMethodID(jni, *j_observer_class_, "onClose", "(J)V")),
    j_on_verify_(webrtc::jni::GetMethodID(jni, *j_observer_class_, "onVerify",
					  "(JLjava/lang/String;[B)Z")) {
}

ObserverJni::~ObserverJni() {
}  

void ObserverJni::OnConnect(long session_id, struct lws_reference* lws_reference) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  env->CallVoidMethod(*j_observer_global_, j_on_connect_, session_id, webrtc::jni::jlongFromPointer(lws_reference));
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

void ObserverJni::OnConnectError(long session_id, struct lws_reference* lws_reference, const char* diagnostic, size_t length) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  jstring j_diagnostic = NULL;
  if (diagnostic) {
    j_diagnostic = webrtc::jni::NativeToJavaString(env, std::string(diagnostic));
  }
  env->CallVoidMethod(*j_observer_global_, j_on_connect_error_, session_id, webrtc::jni::jlongFromPointer(lws_reference), j_diagnostic);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

void ObserverJni::OnWritable(long session_id) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  env->CallVoidMethod(*j_observer_global_, j_on_writable_, session_id);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";  
}  

void ObserverJni::OnReceive(long session_id, void* message, size_t length, bool binary) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  jobject j_message = env->NewDirectByteBuffer(message, length);
  env->CallVoidMethod(*j_observer_global_, j_on_message_, session_id, j_message, binary);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

void ObserverJni::OnClose(long session_id) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  env->CallVoidMethod(*j_observer_global_, j_on_close_, session_id);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

bool ObserverJni::OnVerify(long session_id, const char* common_name, void* bytes, size_t length) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  jstring j_common_name = env->NewStringUTF(common_name);
  CHECK_EXCEPTION(env) << "error during NewStringUTF";
  jbyteArray j_bytes = env->NewByteArray(length);
  env->SetByteArrayRegion(j_bytes, 0, length, (const jbyte*)bytes);
  bool verify = env->CallBooleanMethod(*j_observer_global_, j_on_verify_, session_id, j_common_name, j_bytes);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
  return verify;
}
  
}  // namespace jni
}  // namespace websocket
