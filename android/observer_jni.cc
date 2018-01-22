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
    j_on_writable_(webrtc::jni::GetMethodID(jni, *j_observer_class_, "onWritable", "(J)V")),    
    j_on_message_(webrtc::jni::GetMethodID(jni, *j_observer_class_,
						"onMessage", "(JLjava/nio/ByteBuffer;Z)V")) {
}

ObserverJni::~ObserverJni() {
}  

void ObserverJni::OnConnect(struct lws* wsi) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  env->CallVoidMethod(*j_observer_global_, j_on_connect_, webrtc::jni::jlongFromPointer(wsi));
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";  
}

void ObserverJni::OnWritable(struct lws* wsi) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  env->CallVoidMethod(*j_observer_global_, j_on_writable_, webrtc::jni::jlongFromPointer(wsi));
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";  
}  

void ObserverJni::OnReceive(struct lws* wsi, void* in, size_t len, bool binary) {

  JNIEnv* env = webrtc_jni::AttachCurrentThreadIfNeeded();
  webrtc::jni::ScopedLocalRefFrame local_ref_frame(env);
  jobject j_buffer = env->NewDirectByteBuffer(in, len);
  env->CallVoidMethod(*j_observer_global_, j_on_message_,
		      webrtc::jni::jlongFromPointer(wsi), j_buffer, binary);
  CHECK_EXCEPTION(env) << "error during CallVoidMethod";    
}
  
}  // namespace jni
}  // namespace websocket
