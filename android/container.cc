/*
 *  Copyright (c) 2018 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 */

#include <android/log.h>

extern "C" {
#include <libwebsockets.h>
#include <private-libwebsockets.h>
}

#include "container.h"
#include "observer_jni.h"

namespace websocket {
namespace jni {

#define BUFFER_SIZE 1024 * 1024 * 64
 
struct per_session_data {
};

static int callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
 
static struct lws_protocols protocols[] = {
    {
    "callback",
    callback,
    sizeof( struct per_session_data ),
    BUFFER_SIZE,
  },
  { NULL, NULL, 0, 0 } // end of list
};

static const struct lws_extension exts[] = {
  { NULL, NULL, NULL }
};

  // TBD - to be removed
static Container* container;
  
static int callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {

  return container->Callback(wsi, reason, user, in, len);
}
  
static void emit_log(int level, const char* msg) {
  
  __android_log_write(ANDROID_LOG_INFO, "lws", msg);
}

Container::Container(ObserverJni* observer) {

  container = this;
  observer_ = observer;
  
  lws_set_log_level(LLL_ERR | LLL_WARN, emit_log);
  
  memset(&info_, 0, sizeof(info_));
  info_.port = CONTEXT_PORT_NO_LISTEN;
  info_.protocols = protocols;
  info_.gid = -1;
  info_.uid = -1;
  info_.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  context_ = lws_create_context(&info_);
}

Container::~Container() {
}  

  struct lws* Container::CreateWebSocket(int port, const char* host, const char* path, bool secure) {

  if (context_) {
    struct lws_client_connect_info info_ws;
    memset(&info_ws, 0, sizeof(info_ws));
    info_ws.port = port;
    info_ws.address = host;
    info_ws.path = path;
    info_ws.context = context_;
    info_ws.ssl_connection = secure;
    info_ws.host = host;
    info_ws.origin = host;
    info_ws.ietf_version_or_minus_one = -1;
    info_ws.client_exts = exts;
    info_ws.protocol = protocols[0].name;
      
    return lws_client_connect_via_info(&info_ws);
  }

  return NULL;
}

void Container::Service(int timeout) {
  
  if (context_) {
    lws_service(context_, timeout);    
  }
}

void Container::TriggerWritable(struct lws* websocket) {

  if (context_) {  
    lws_callback_on_writable(websocket);
  }
}  

void Container::SendBuffer(struct lws* websocket, void* buffer, size_t length, bool binary) {

  lws_write(websocket, (unsigned char *)buffer, length, LWS_WRITE_TEXT);
}
  
int Container::Callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {

  switch(reason) {

  case LWS_CALLBACK_ESTABLISHED:
    __android_log_print(ANDROID_LOG_ERROR, "CJ", "LWS_CALLBACK_ESTABLISHED");    
    break;
    
  case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    __android_log_print(ANDROID_LOG_ERROR, "CJ", "LWS_CALLBACK_CLIENT_CONNECTION_ERROR");    
    break;

  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    __android_log_print(ANDROID_LOG_ERROR, "CJ", "LWS_CALLBACK_CLIENT_ESTABLISHED");        
    observer_->OnConnect(wsi);
    break;

  case LWS_CALLBACK_CLIENT_WRITEABLE:
    __android_log_print(ANDROID_LOG_ERROR, "CJ", "LWS_CALLBACK_CLIENT_WRITEABLE");            
    observer_->OnWritable(wsi);
    break;
    
  case LWS_CALLBACK_CLIENT_RECEIVE:
    __android_log_print(ANDROID_LOG_ERROR, "CJ", "LWS_CALLBACK_CLIENT_RECEIVE");    
    observer_->OnReceive(wsi, in, len, false);
    break;

  default:
    break;
  }

  return 0;
}
  
}  // namespace jni
}  // namespace websocket
