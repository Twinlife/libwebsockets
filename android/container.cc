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

//
// Ping messages are sent every 6 minutes by the server
// If no answer is received during 12 minutes the connection is closed by xthe server
//
#define KEEP_ALIVE_TIMEOUT 360 * 2

struct userdata {
  Container* container;
  jlong session_id;
  struct lws_reference* lws_reference;
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
  info_.ws_ping_pong_interval = KEEP_ALIVE_TIMEOUT;
  context_ = lws_create_context(&info_);
}

Container::~Container() {
}  

  struct lws_reference* Container::CreateWebSocket(jlong session_id, int port, const char* host,
						   const char* path, bool secure) {

  if (context_) {
    struct lws_reference* lws_reference = (struct lws_reference*)lws_malloc(sizeof(struct lws_reference),
									    "container");
    struct userdata* userdata = (struct userdata*)lws_malloc(sizeof(struct userdata), "container");
    userdata->container = this;
    userdata->session_id = session_id;
    userdata->lws_reference = lws_reference;

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

    lws_reference->wsi = lws_client_connect_via_info(&info_ws);
    return lws_reference;
  }

  return NULL;
}

void Container::Service(int timeout) {
  
  if (context_) {
    lws_service(context_, timeout);    
  }
}

void Container::TriggerWritable(struct lws_reference* lws_reference) {

  if (context_) {
    struct lws* wsi = lws_reference->wsi;
    if (wsi) {
      lws_callback_on_writable(wsi);
    }
  }
}  

void Container::SendMessage(struct lws_reference* lws_reference, void* buffer, size_t length, bool binary) {

  struct lws* wsi = lws_reference->wsi;
  if (wsi) {
    lws_write(wsi, (unsigned char *)buffer, length, LWS_WRITE_TEXT);
  }
}

void Container::SendCloseMessage(struct lws_reference* lws_reference) {

  if (context_) {
    struct lws* wsi = lws_reference->wsi;
    if (wsi) {
      lws_close_status status = LWS_CLOSE_STATUS_GOINGAWAY;
      unsigned char buffer[2 + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING];
      unsigned char* data_buffer = buffer + LWS_SEND_BUFFER_PRE_PADDING;
      unsigned char* p = data_buffer;
      *p++ = (((int)status) >> 8) & 0xff;
      *p++ = ((int)status) & 0xff;
      lws_write(wsi, data_buffer, 2, LWS_WRITE_CLOSE);
    }
  }
}
  
int Container::Callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {

  lwsl_debug("Container::Callback wsi=%p reason=%d user=%p in=%p len=%d", wsi, reason, user, in, len);

  long session_id = -1;
  if (user && user == wsi->user_space) {
    struct userdata* userdata = (struct userdata*)user;
    session_id = userdata->session_id;
  }

  switch(reason) {

  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    observer_->OnConnect(session_id);
    break;

  case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    observer_->OnConnectError(session_id, in, len);
    break;

  case LWS_CALLBACK_CLIENT_WRITEABLE:
    observer_->OnWritable(session_id);
    break;
    
  case LWS_CALLBACK_CLIENT_RECEIVE:
    observer_->OnReceive(session_id, in, len, false);
    break;

  case LWS_CALLBACK_CLOSED:
    observer_->OnClose(session_id);
    break;

  default:
    break;
  }

  return 0;
}
  
}  // namespace jni
}  // namespace websocket
