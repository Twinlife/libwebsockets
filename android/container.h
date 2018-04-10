/*
 *  Copyright (c) 2018 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 */

#ifndef WEBSOCKET_ANDROID_CONTAINER_H_
#define WEBSOCKET_ANDROID_CONTAINER_H_

#include "sdk/android/src/jni/jni_helpers.h"

extern "C" {
#include <libwebsockets.h>
}

namespace websocket {
namespace jni {

struct lws_reference {
  struct lws* wsi;
};

class ObserverJni;
 
class Container {
 public:
  Container(ObserverJni* observer);

  ~Container();

  struct lws_reference* CreateWebSocket(jlong session_id, int port, const char* host, const char* path,
					bool secure, const char* proxy_address, int proxy_port,
					const char* proxy_username, const char* proxy_password);

  void Service(int timeout);

  void TriggerWorker();

  void TriggerWritable(struct lws_reference* lws_reference);
  
  void SendMessage(struct lws_reference* lws_reference, void* buffer, size_t length, bool binary);

  void SendCloseMessage(struct lws_reference* lws_reference);
  
  int Callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
  
 private:
  ObserverJni* observer_;
  lws_context_creation_info info_;
  lws_context* context_;
};

}  // namespace jni
}  // namespace websocket

#endif // WEBSOCKET_ANDROID_WEBSOCKETCONTAINER_H_

