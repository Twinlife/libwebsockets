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

extern "C" {
#include <libwebsockets.h>
}

namespace websocket {
namespace jni {

class ObserverJni;
 
class Container {
 public:
  Container(ObserverJni* observer);

  ~Container();

  struct lws* CreateWebSocket(int port, const char* host, const char* path, bool secure);

  void Service(int timeout);

  void TriggerWritable(struct lws* websocket);
  
  void SendBuffer(struct lws* websocket, void* buffer, size_t length, bool binary);
  
  int Callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
  
 private:
  ObserverJni* observer_;
  lws_context_creation_info info_;
  lws_context* context_;
};

}  // namespace jni
}  // namespace websocket

#endif // WEBSOCKET_ANDROID_WEBSOCKETCONTAINER_H_

