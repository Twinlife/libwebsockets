/*
 *  Copyright (c) 2018-2021 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 *   Stephane Carrez (Stephane.Carrez@twin.life)
 */

#ifndef WEBSOCKET_ANDROID_OBSERVER_JNI_H_
#define WEBSOCKET_ANDROID_OBSERVER_JNI_H_

#include <jni.h>
#include <memory>
#include <utility>

#include "jni_helpers.h"

extern "C" {
  struct lws;
}

namespace websocket {
namespace jni {

class ObserverJni {
 public:
  ObserverJni(JNIEnv* jni, jobject j_observer);

  ~ObserverJni();

  void OnConnect(jlong session_id, jlong websocket_id, const char* ip, const jlong *stats, jsize stats_length);

  void OnConnectError(jlong session_id, jlong websocket_id, const char* diagnostic, size_t length,
                      const jlong *stats, jsize stats_length);
  
  bool OnWritable(jlong session_id, jlong websocket_id);

  void OnReceive(jlong session_id, jlong websocket_id, void* message, size_t length, bool binary);

  void OnClose(jlong session_id, jlong websocket_id);

  bool OnVerify(jlong session_id, jlong websocket_id, const char* common_name, void* bytes, size_t length);

 private:  
  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const jmethodID j_on_connect_;
  const jmethodID j_on_connect_error_;
  const jmethodID j_on_writable_;  
  const jmethodID j_on_message_;
  const jmethodID j_on_close_;
  const jmethodID j_on_verify_;
};

}  // namespace jni
}  // namespace websocket

#endif // WEBSOCKET_ANDROID_OBSERVER_JNI_H_
