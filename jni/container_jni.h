/*
 *  Copyright (c) 2018-2025 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 *   Stephane Carrez (Stephane.Carrez@twin.life)
 */

#ifndef WEBSOCKET_ANDROID_CONTAINER_JNI_H_
#define WEBSOCKET_ANDROID_CONTAINER_JNI_H_

#include <jni.h>
#include <memory>
#include <utility>

#include "jni_helpers.h"
#include "wscontainer.h"

namespace websocket {
namespace jni {

class Container : public websocket::Container {
public:
  Container(JNIEnv* jni);

  struct websocket::ProxyDescriptor *GetProxies(JNIEnv *env, jobjectArray j_proxies);

private:
  const ScopedGlobalRef<jclass> j_proxy_class_;
  const ScopedGlobalRef<jclass> j_connection_stats_class_;
  jfieldID f_proxy_address_;
  jfieldID f_proxy_port_;
  jfieldID f_proxy_method_;
  jfieldID f_proxy_path_;
};

}  // namespace jni
}  // namespace websocket

#endif // WEBSOCKET_ANDROID_OBSERVER_JNI_H_
