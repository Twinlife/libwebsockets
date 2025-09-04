/*
 *  Copyright (c) 2018-2025 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 *   Stephane Carrez (Stephane.Carrez@skyrock.com)
 */

#ifndef WEBSOCKET_ANDROID_CONTAINER_H_
#define WEBSOCKET_ANDROID_CONTAINER_H_

#include "sdk/android/src/jni/jni_helpers.h"

extern "C" {
#include <libwebsockets.h>
}

#define CONFIG_SECURE           0x01
#define CONFIG_SNI_PASSTHROUGH  0x02

namespace websocket {
namespace jni {

class ObserverJni;
 
class Container {
 public:
  Container(ObserverJni* observer);

  ~Container();

  jlong CreateWebSocket(jlong session_id, int port, const char* host, const char* path,
			int method, long timeout, const char* proxy_address, int proxy_port, const char* proxy_username, const char* proxy_password, const char* proxy_path);

  void Service(int timeout);

  void TriggerWorker();

  void TriggerWritable(jlong websocket_id);

  void Close(jlong websocket_id);
  
  void SendMessage(jlong websocket_id, void* buffer, size_t length, bool binary);

  void SendCloseMessage(jlong websocket_id);
  
  int Callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
  
 private:
  ObserverJni* observer_;
  lws_context_creation_info info_;
  lws_context* context_;
  pthread_mutex_t jni_lws_list_mutex_;
  struct lws *jni_lws_list_;

  struct lws *get_lws_from_websocket_id(jlong websocket_id);
  void insert(struct lws* wsi);
  bool destroy(struct lws* wsi);
};

}  // namespace jni
}  // namespace websocket

#endif // WEBSOCKET_ANDROID_WEBSOCKETCONTAINER_H_

