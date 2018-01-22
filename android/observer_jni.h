/*
 *  Copyright (c) 2018 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 */

#ifndef WEBSOCKET_ANDROID_OBSERVER_JNI_H_
#define WEBSOCKET_ANDROID_OBSERVER_JNI_H_

#include <jni.h>
#include <memory>
#include <utility>

#include "sdk/android/src/jni/jni_helpers.h"

extern "C" {
  struct lws;
}

namespace websocket {
namespace jni {

class ObserverJni {
 public:
  ObserverJni(JNIEnv* jni, jobject j_observer);

  ~ObserverJni();

  void OnConnect(struct lws* wsi);

  void OnWritable(struct lws* wsi);
  
  void OnReceive(struct lws* wsi, void* in, size_t len, bool binary);
  
 private:  
  const webrtc::jni::ScopedGlobalRef<jobject> j_observer_global_;
  const webrtc::jni::ScopedGlobalRef<jclass> j_observer_class_;
  const jmethodID j_on_connect_;
  const jmethodID j_on_writable_;  
  const jmethodID j_on_message_;  
};

}  // namespace jni
}  // namespace websocket

#endif // WEBSOCKET_ANDROID_OBSERVER_JNI_H_
