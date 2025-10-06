/*
 *  Copyright (c) 2018-2025 twinlife SA.
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
#include "wscontainer.h"
#include "container_jni.h"

namespace websocket {
namespace jni {

class Observer : public SessionObserver{
 public:
  Observer(JNIEnv* jni, jobject j_observer);

  ~Observer();

  void OnConnect(Session *session) override;

  long OnConnectError(Session *session, Error error) override;
  
  void OnReceive(Session *session, void* message, size_t length, bool binary) override;

  void OnClose(Session *session) override;

  void OnDestroy(Session *session) override;

 private:
  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const jmethodID j_on_connect_;
  const jmethodID j_on_connect_error_;
  const jmethodID j_on_message_;
  const jmethodID j_on_close_;
  const ScopedGlobalRef<jclass> j_connection_stats_;
  const jmethodID j_connection_stats_ctor_;

  jobjectArray GetConnectionStats(JNIEnv *env, websocket::Session *session);
};

}  // namespace jni
}  // namespace websocket

#endif // WEBSOCKET_ANDROID_OBSERVER_JNI_H_
