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
 
struct userdata {
  Container* container;
  jlong session_id;
};

static int callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
 
static struct lws_protocols protocols[] = {
    {
    "callback",
    callback,
    sizeof(struct userdata),
    BUFFER_SIZE,
  },
  { NULL, NULL, 0, 0 } // end of list
};

static int callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {

  if (user) {
    struct userdata* userdata = (struct userdata*)user;
    return userdata->container->Callback(wsi, reason, user, in, len);
  }
  return 0;
}
  
static void emit_log(int level, const char* msg) {

  if (level == LLL_NOTICE || level == LLL_INFO) {
    __android_log_write(ANDROID_LOG_INFO, "lws", msg);
  } else if (level == LLL_WARN) {
    __android_log_write(ANDROID_LOG_WARN, "lws", msg);
  } else if (level == LLL_ERR) {
    __android_log_write(ANDROID_LOG_ERROR, "lws", msg);
  } else {
    __android_log_write(ANDROID_LOG_DEBUG, "lws", msg);
  }
}

Container::Container(ObserverJni* observer) {

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

  struct lws* Container::CreateWebSocket(jlong session_id, int port, const char* host, const char* path,
					 bool secure) {

  if (context_) {
    struct userdata* userdata = (struct userdata*)lws_malloc(sizeof(struct userdata), "userdata");
    userdata->container = this;
    userdata->session_id = session_id;

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
    info_ws.protocol = protocols[0].name;
    info_ws.userdata = userdata;
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

  lwsl_debug("Container::Callback wsi=%p reason=%d user=%p in=%p len=%d", wsi, reason, user, in, len);

  long session_id = -1;
  if (user) {
    struct userdata* userdata = (struct userdata*)user;
    session_id = userdata->session_id;
  }

  switch(reason) {

  case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    observer_->OnConnectError(session_id, in, len);
    break;

  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    observer_->OnConnect(session_id);
    break;

  case LWS_CALLBACK_CLIENT_WRITEABLE:
    observer_->OnWritable(session_id);
    break;
    
  case LWS_CALLBACK_CLIENT_RECEIVE:
    observer_->OnReceive(session_id, in, len, false);
    break;

  default:
    break;
  }

  return 0;
}
  
}  // namespace jni
}  // namespace websocket
