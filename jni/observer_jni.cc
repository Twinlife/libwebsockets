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

namespace websocket {
namespace jni {

Observer::Observer(JNIEnv* jni, jobject j_observer)
  : j_observer_global_(jni, j_observer),
    j_observer_class_(jni, GetObjectClass(jni, *j_observer_global_)),
    j_on_connect_(GetMethodID(jni, *j_observer_class_, "onConnect", "(J[Lorg/libwebsockets/ConnectionStats;I)V")),
    j_on_connect_error_(GetMethodID(jni, *j_observer_class_, "onConnectError", "(J[Lorg/libwebsockets/ConnectionStats;I)V")),
    j_on_message_(GetMethodID(jni, *j_observer_class_, "onReceive", "(JLjava/nio/ByteBuffer;Z)V")),
    j_on_close_(GetMethodID(jni, *j_observer_class_, "onClose", "(J)V")),
    j_connection_stats_(jni, jni->FindClass("org/libwebsockets/ConnectionStats")),
    j_connection_stats_ctor_(jni->GetMethodID(*j_connection_stats_, "<init>", "(IIZIJJJJIZLjava/lang/String;)V")) {
}

Observer::~Observer() {
}  

jobjectArray Observer::GetConnectionStats(JNIEnv *env, websocket::Session *session)
{
  int count = session->GetSocketCount();
  if (count <= 0) {
    return nullptr;
  }
  jobjectArray result = env->NewObjectArray(count, *j_connection_stats_, nullptr);
  for (int i = 0; i < count; i++) {
    const websocket::ConnectionStats *stats = session->GetStats(i);
    jstring ipAddr = env->NewStringUTF(stats->ip_addr);

    lwsl_notice("Connection stats: proxyIndex=%d connectCount=%d", stats->proxyIndex, stats->connectCount);

    // new ConnectionStats(int index, int proxyIndex, boolean sniOverride, long dnsTime, long tcpConnectTime, long txnResponseTime,
    //                     long tlsConnectTime, int connectCount, int lastError, boolean ipv6, String ipAddr);
    jobject obj = env->NewObject(*j_connection_stats_, j_connection_stats_ctor_, (jint) i, (jint) stats->proxyIndex,
                                 (jboolean) stats->sniOverride, (jint) stats->connectCount,
				 (jlong) stats->dnsTime,
                                 (jlong) stats->tcpConnectTime, (jlong) stats->txnResponseTime,
                                 (jlong) stats->tlsConnectTime, 
                                 (jint) stats->lastError, (jboolean) stats->ipv6,
                                 ipAddr);
    env->SetObjectArrayElement(result, i, obj);
    env->DeleteLocalRef(obj);
    env->DeleteLocalRef(ipAddr);
  }
  return result;
}

void Observer::OnConnect(Session *session) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jlong session_id = session->GetSessionId();
  int active = session->GetActiveSocket();

  jobjectArray j_stats = GetConnectionStats(env, session);
  env->CallVoidMethod(*j_observer_global_, j_on_connect_, session_id, j_stats, active);
  env->DeleteLocalRef(j_stats);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

long Observer::OnConnectError(Session *session, Error error) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jlong session_id = session->GetSessionId();

  jobjectArray j_stats = GetConnectionStats(env, session);
  env->CallVoidMethod(*j_observer_global_, j_on_connect_error_, session_id, j_stats, (int)error);
  env->DeleteLocalRef(j_stats);
  session->Close();
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
  return -1;
}

void Observer::OnReceive(Session *session, void* message, size_t length, bool binary) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jlong session_id = session->GetSessionId();
  jobject j_message = env->NewDirectByteBuffer(message, length);
  env->CallVoidMethod(*j_observer_global_, j_on_message_, session_id, j_message, binary);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

void Observer::OnClose(Session *session) {

  JNIEnv* env = webrtc::jni::AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jlong session_id = session->GetSessionId();
  env->CallVoidMethod(*j_observer_global_, j_on_close_, session_id);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";
}

void Observer::OnDestroy(Session *session) {

  delete this;
}

}  // namespace jni
}  // namespace websocket
