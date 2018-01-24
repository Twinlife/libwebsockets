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
    j_on_connect_(webrtc::jni::GetMethodID(jni, *j_observer_class_, "onConnect", "(J)V")),
    j_on_connect_error_(webrtc::jni::GetMethodID(jni, *j_observer_class_, "onConnectError",
						 "(JLjava/lang/String;)V")),
    j_on_writable_(webrtc::jni::GetMethodID(jni, *j_observer_class_, "onWritable", "(J)V")),
    j_on_message_(webrtc::jni::GetMethodID(jni, *j_observer_class_,
					   "onMessage", "(JLjava/nio/ByteBuffer;Z)V")),
    j_on_close_(webrtc::jni::GetMethodID(jni, *j_observer_class_, "onClose", "(J)V")) {
}

ObserverJni::~ObserverJni() {
}  

void ObserverJni::OnConnect(jlong session_id) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  env->CallVoidMethod(*j_observer_global_, j_on_connect_, session_id);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";  
}

void ObserverJni::OnConnectError(jlong session_id, void* in, size_t len) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  jstring diagnostic_j = NULL;
  if (in) {
    std::string name((const char*)in, len);
    diagnostic_j = webrtc::jni::NativeToJavaString(env, name);
  }
  env->CallVoidMethod(*j_observer_global_, j_on_connect_error_, session_id, diagnostic_j);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

void ObserverJni::OnWritable(jlong session_id) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  env->CallVoidMethod(*j_observer_global_, j_on_writable_, session_id);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";  
}  

void ObserverJni::OnReceive(jlong session_id, void* in, size_t len, bool binary) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  jobject j_buffer = env->NewDirectByteBuffer(in, len);
  env->CallVoidMethod(*j_observer_global_, j_on_message_, session_id, j_buffer, binary);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";    
}

void ObserverJni::OnClose(jlong session_id) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  env->CallVoidMethod(*j_observer_global_, j_on_close_, session_id);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}
  
}  // namespace jni
}  // namespace websocket
