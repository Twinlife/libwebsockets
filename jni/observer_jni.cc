/*
 *  Copyright (c) 2018-2025 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 *   Stephane Carrez (Stephane.Carrez@twin.life)
 */

#include "observer_jni.h"

extern "C" {
#include <libwebsockets.h>
}

#include <private-lib-core.h>
#include <private-lib-tls.h>
#include <private-lib-core-net.h>

namespace websocket {
namespace jni {

ObserverJni::ObserverJni(JNIEnv* jni, jobject j_observer)
  : j_observer_global_(jni, j_observer),
    j_observer_class_(jni, GetObjectClass(jni, *j_observer_global_)),
    j_on_connect_(GetMethodID(jni, *j_observer_class_, "onConnect", "(JJLjava/lang/String;[J)V")),
    j_on_connect_error_(GetMethodID(jni, *j_observer_class_, "onConnectError", "(JJLjava/lang/String;[J)J")),
    j_on_writable_(GetMethodID(jni, *j_observer_class_, "onWritable", "(JJ)Z")),
    j_on_message_(GetMethodID(jni, *j_observer_class_, "onMessage", "(JJLjava/nio/ByteBuffer;Z)V")),
    j_on_close_(GetMethodID(jni, *j_observer_class_, "onClose", "(JJ)V")),
    j_on_timer_(GetMethodID(jni, *j_observer_class_, "onTimer", "(JJ)J")) {
}

ObserverJni::~ObserverJni() {
}  

void ObserverJni::OnConnect(jlong session_id, jlong websocket_id, const char* ip, const jlong* stats, jsize stats_length) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jstring j_ip = NativeToJavaString(env, std::string(ip));
  jlongArray j_stats = env->NewLongArray(stats_length);
  env->SetLongArrayRegion(j_stats, 0, stats_length, stats);
  env->CallVoidMethod(*j_observer_global_, j_on_connect_, session_id, websocket_id, j_ip, j_stats);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

jlong ObserverJni::OnConnectError(jlong session_id, jlong websocket_id, const char* diagnostic, size_t length,
                                 const jlong* stats, jsize stats_length) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jstring j_diagnostic = NULL;
  if (diagnostic) {
    j_diagnostic = NativeToJavaString(env, std::string(diagnostic));
  }
  jlongArray j_stats = env->NewLongArray(stats_length);
  env->SetLongArrayRegion(j_stats, 0, stats_length, stats);
  jlong timeout = env->CallLongMethod(*j_observer_global_, j_on_connect_error_, session_id, websocket_id, j_diagnostic, j_stats);
  CHECK_EXCEPTION(env) << "error during CallLongMethod";
  return timeout;
}

bool ObserverJni::OnWritable(jlong session_id, jlong websocket_id) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  bool result = env->CallBooleanMethod(*j_observer_global_, j_on_writable_, session_id, websocket_id);
  CHECK_EXCEPTION(env) << "error during CallBooleanMethod";
  return result;
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

jlong ObserverJni::OnTimer(jlong session_id, jlong websocket_id) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jlong timeout = env->CallLongMethod(*j_observer_global_, j_on_timer_, session_id, websocket_id);
  CHECK_EXCEPTION(env) << "error during CallLongMethod";
  return timeout;
}

}  // namespace jni
}  // namespace websocket
