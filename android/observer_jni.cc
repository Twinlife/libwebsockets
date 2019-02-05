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
    j_observer_class_(jni, GetObjectClass(jni, *j_observer_global_)),
    j_on_connect_(GetMethodID(jni, *j_observer_class_, "onConnect", "(JJ)V")),
    j_on_connect_error_(GetMethodID(jni, *j_observer_class_, "onConnectError", "(JJLjava/lang/String;)V")),
    j_on_writable_(GetMethodID(jni, *j_observer_class_, "onWritable", "(JJ)V")),
    j_on_message_(GetMethodID(jni, *j_observer_class_, "onMessage", "(JJLjava/nio/ByteBuffer;Z)V")),
    j_on_close_(GetMethodID(jni, *j_observer_class_, "onClose", "(JJ)V")),
    j_on_verify_(GetMethodID(jni, *j_observer_class_, "onVerify", "(JJLjava/lang/String;[B)Z")) {
}

ObserverJni::~ObserverJni() {
}  

void ObserverJni::OnConnect(jlong session_id, jlong websocket_id) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  env->CallVoidMethod(*j_observer_global_, j_on_connect_, session_id, websocket_id);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

void ObserverJni::OnConnectError(jlong session_id, jlong websocket_id, const char* diagnostic, size_t length) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jstring j_diagnostic = NULL;
  if (diagnostic) {
    j_diagnostic = NativeToJavaString(env, std::string(diagnostic));
  }
  env->CallVoidMethod(*j_observer_global_, j_on_connect_error_, session_id, websocket_id, j_diagnostic);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

void ObserverJni::OnWritable(jlong session_id, jlong websocket_id) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  env->CallVoidMethod(*j_observer_global_, j_on_writable_, session_id, websocket_id);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";  
}  

void ObserverJni::OnReceive(jlong session_id, jlong websocket_id, void* message, size_t length, bool binary) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject j_message = env->NewDirectByteBuffer(message, length);
  env->CallVoidMethod(*j_observer_global_, j_on_message_, session_id, websocket_id, j_message, binary);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

void ObserverJni::OnClose(jlong session_id, jlong websocket_id) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  env->CallVoidMethod(*j_observer_global_, j_on_close_, session_id, websocket_id);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

bool ObserverJni::OnVerify(jlong session_id, jlong websocket_id, const char* common_name, void* bytes, size_t length) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jstring j_common_name = env->NewStringUTF(common_name);
  CHECK_EXCEPTION(env) << "error during NewStringUTF";
  jbyteArray j_bytes = env->NewByteArray(length);
  env->SetByteArrayRegion(j_bytes, 0, length, (const jbyte*)bytes);
  bool verify = env->CallBooleanMethod(*j_observer_global_, j_on_verify_, session_id, websocket_id, j_common_name, j_bytes);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
  return verify;
}
  
}  // namespace jni
}  // namespace websocket
