/*
 *  Copyright (c) 2018-2019 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 *   Stephane Carrez (Stephane.Carrez@twin.life)
 */

#if defined(WEBRTC_ANDROID)
#  include <android/log.h>
#endif

#include <libwebsockets.h>

#include <private-lib-core.h>
#include <private-lib-tls.h>

#include "rtc_base/third_party/base64/base64.h"

#include "container.h"
#include "observer_jni.h"

namespace websocket {
namespace jni {

#define BUFFER_SIZE 1024 * 1024 * 64

#define TCP_KEEP_ALIVE 60
#define TCP_KEEP_ALIVE_PROBES 6
#define TCP_KEEP_ALIVE_INTERVAL 10

struct userdata {
  Container* container;
  jlong session_id;
  bool root_certificate_verified;
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

  lwsl_debug("callback wsi=%p reason=%d user=%p in=%p len=%lu\n", wsi, reason, user, in, (unsigned long)len);
  if (wsi && wsi->user_space && reason != LWS_CALLBACK_EVENT_WAIT_CANCELLED) {
    struct userdata* userdata = (struct userdata*)wsi->user_space;
    if (userdata->container) {
      return userdata->container->Callback(wsi, reason, user, in, len);
    }
  }
  return 0;
}

static void emit_log(int level, const char* msg) {
#if defined(WEBRTC_ANDROID)  
  if (level == LLL_NOTICE || level == LLL_INFO) {
    __android_log_write(ANDROID_LOG_INFO, "lws", msg);
  } else if (level == LLL_WARN) {
    __android_log_write(ANDROID_LOG_WARN, "lws", msg);
  } else if (level == LLL_ERR) {
    __android_log_write(ANDROID_LOG_ERROR, "lws", msg);
  } else {
    __android_log_write(ANDROID_LOG_DEBUG, "lws", msg);
  }
#endif
  static long start;
  struct timeval tv;

  (void)gettimeofday(&tv, 0);
  if (start == 0)
    start = tv.tv_sec;
  fprintf(stderr, "lws[%ld.%06ld] %d: %s", tv.tv_sec - start, tv.tv_usec, level, msg);
}

Container::Container(ObserverJni* observer) {

  observer_ = observer;
  
  lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE, emit_log);
  
  memset(&info_, 0, sizeof(info_));
  info_.port = CONTEXT_PORT_NO_LISTEN;
  info_.protocols = protocols;
  info_.gid = -1;
  info_.uid = -1;
  info_.options |= LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
  info_.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  info_.ka_time = TCP_KEEP_ALIVE;
  info_.ka_probes = TCP_KEEP_ALIVE_PROBES;
  info_.ka_interval = TCP_KEEP_ALIVE_INTERVAL;
  info_.connect_timeout_secs = 10;
  info_.timeout_secs = 3;
  info_.ssl_client_options_set = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2;
  info_.client_ssl_cipher_list = "EDH+aRSA+AES256:EECDH+aRSA+AES256:!SSLv3";

  context_ = lws_create_context(&info_);

  pthread_mutex_init(&jni_lws_list_mutex_, NULL);
  jni_lws_list_ = NULL;
}

Container::~Container() {

  pthread_mutex_destroy(&jni_lws_list_mutex_);
}  

jlong Container::CreateWebSocket(jlong session_id, int port, const char* host, const char* path, bool secure,
				 const char* proxy_address, int proxy_port, const char* proxy_username, const char* proxy_password) {

  if (context_) {
    struct lws_vhost* vhost = lws_create_vhost(context_, &info_);
    if (vhost) {
      if (proxy_address && proxy_port != 0) {
	vhost->http.http_proxy_port = proxy_port;
	strncpy(vhost->http.http_proxy_address, proxy_address, sizeof(vhost->http.http_proxy_address) - 1);
	vhost->http.http_proxy_address[sizeof(vhost->http.http_proxy_address) - 1] = '\0';
	if (proxy_username && proxy_password) {
	  char *auth_token = (char *)lws_malloc(strlen(proxy_username) + strlen(proxy_password) + 2,
						"container");
	  strcpy(auth_token, proxy_username);
	  strcat(auth_token, ":");
	  strcat(auth_token, proxy_password);
	  std::string base64_auth_token = rtc::Base64::Encode(auth_token);
	  strncpy(vhost->proxy_basic_auth_token, base64_auth_token.c_str(),
		  sizeof(vhost->proxy_basic_auth_token) - 1);
	  vhost->proxy_basic_auth_token[sizeof(vhost->proxy_basic_auth_token) - 1] = '\0';
	  lws_free(auth_token);
          lwsl_debug("Connect through proxy %s port %d token %s\n", vhost->http.http_proxy_address, vhost->http.http_proxy_port, auth_token);
	} else {
	  vhost->proxy_basic_auth_token[0] = '\0';
	}
      } else {
	vhost->http.http_proxy_port = 0;
	vhost->http.http_proxy_address[0] = '\0';
	vhost->proxy_basic_auth_token[0] = '\0';
      }
    }

    struct userdata* userdata = (struct userdata*)lws_malloc(sizeof(struct userdata), "container");
    userdata->container = this;
    userdata->session_id = session_id;
    userdata->root_certificate_verified = false;

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
    info_ws.vhost = vhost;

    struct lws *wsi = lws_client_connect_via_info(&info_ws);
    if (wsi) {
      return webrtc::jni::jlongFromPointer(wsi);
    }
    return 0;
  }

  return 0;
}

void Container::Service(int timeout) {

  lwsl_debug("%s: service with timeout=%d ms\n", __func__, timeout);
  if (context_) {
    lws_service(context_, timeout);
  }
}

void Container::TriggerWorker() {

  if (context_) {
    lws_cancel_service(context_);
  }
}

void Container::TriggerWritable(jlong websocket_id) {

  if (context_) {
    struct lws* wsi = reinterpret_cast<struct lws*>(websocket_id);
    //
    // Called in the notification thread
    // Lock mutex to avoid race condition with wsi deletion in the service thread
    //
    pthread_mutex_lock(&jni_lws_list_mutex_);
    struct lws* current_wsi = jni_lws_list_;
    while (current_wsi) {
      if (current_wsi == wsi) {
	lws_callback_on_writable(wsi);
	break;
      }
      current_wsi = current_wsi->jni_lws_list;
    }
    pthread_mutex_unlock(&jni_lws_list_mutex_);
  }
}  

void Container::SendMessage(jlong websocket_id, void* buffer, size_t length, bool binary) {

  if (context_) {
    struct lws* wsi = get_lws_from_websocket_id(websocket_id);
    if (wsi) {
      lws_write(wsi, (unsigned char *)buffer, length, binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);
    }
  }
}

void Container::SendCloseMessage(jlong websocket_id) {

  if (context_) {
    struct lws* wsi = get_lws_from_websocket_id(websocket_id);
    if (wsi) {
      lws_close_reason(wsi, LWS_CLOSE_STATUS_GOINGAWAY, NULL, 0);
    }
  }
}

int Container::Callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {

  struct userdata* userdata = NULL;
  jlong session_id = 0;
  jlong websocket_id = 0;
  if (wsi) {
    websocket_id = webrtc::jni::jlongFromPointer(wsi);
    userdata = (struct userdata*)wsi->user_space;
    if (userdata) {
      session_id = userdata->session_id;
    }
  } else {
    return 0;
  }

  switch(reason) {

  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    observer_->OnConnect(session_id, websocket_id);
    break;

  case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    observer_->OnConnectError(session_id, websocket_id, (const char *)in, len);
    break;

  case LWS_CALLBACK_CLIENT_WRITEABLE:
    return observer_->OnWritable(session_id, websocket_id) ? 0 : -1;
    
  case LWS_CALLBACK_CLIENT_RECEIVE:
    observer_->OnReceive(session_id, websocket_id, in, len, lws_frame_is_binary(wsi));
    break;

  case LWS_CALLBACK_CLIENT_CLOSED:
  case LWS_CALLBACK_CLOSED:
    observer_->OnClose(session_id, websocket_id);
    break;

  case LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION:
    if (!userdata->root_certificate_verified) {
      userdata->root_certificate_verified = true;

      bool preverify_ok = len;
      bool verify_ok = false;
      if (preverify_ok) {
	X509_STORE_CTX* x509_store_ctx = (X509_STORE_CTX *)user;
	X509* x509 = X509_STORE_CTX_get_current_cert(x509_store_ctx);
	if (x509) {
	  const char* common_name = NULL;
	  X509_NAME* name = X509_get_subject_name(x509);
	  int entry_count = X509_NAME_entry_count(name);
	  for (int i = 0; i < entry_count; i++) {
	    X509_NAME_ENTRY* name_entry = X509_NAME_get_entry(name, i);
	    int nid = OBJ_obj2nid(X509_NAME_ENTRY_get_object(name_entry));
	    if (nid == NID_commonName) {
	      common_name = (const char*)ASN1_STRING_get0_data(X509_NAME_ENTRY_get_data(name_entry));
	      break;
	    }
	  }
	  uint8_t* bytes = NULL;
	  int length = 0;
	  EVP_PKEY* pkey = X509_get_pubkey(x509);
	  if (pkey) {
	    length = i2d_PublicKey(pkey, &bytes);
	  }

	  if (common_name && bytes && length > 0) {
	    verify_ok = observer_->OnVerify(session_id, websocket_id, common_name, bytes, length);
	  }
	  if (bytes) {
	    OPENSSL_free(bytes);
	  }
	  if (pkey) {
	    EVP_PKEY_free(pkey);
	  }
	  if (!verify_ok) {
	    return -1;
	  }
	  break;
	}
      }
    }
    break;

  case LWS_CALLBACK_WSI_CREATE:
    lwsl_debug("%s: create wsi %p\n", __func__, wsi);
    pthread_mutex_lock(&jni_lws_list_mutex_);
    wsi->jni_lws_list = jni_lws_list_;
    jni_lws_list_ = wsi;
    pthread_mutex_unlock(&jni_lws_list_mutex_);
    break;

  case LWS_CALLBACK_WSI_DESTROY: {
    lwsl_debug("%s: destroy wsi %p\n", __func__, wsi);
    if (wsi->a.vhost) {
      lws_vhost_destroy(wsi->a.vhost);
      wsi->a.vhost = NULL;
    }

    pthread_mutex_lock(&jni_lws_list_mutex_);
    struct lws* current_wsi = jni_lws_list_;
    struct lws* previous_wsi = NULL;
    while (current_wsi) {
      if (current_wsi == wsi) {
	if (previous_wsi) {
	  previous_wsi->jni_lws_list = current_wsi->jni_lws_list;
	} else {
	  jni_lws_list_ = current_wsi->jni_lws_list;
	}
	break;
      }
      previous_wsi = current_wsi;
      current_wsi = current_wsi->jni_lws_list;
    }
    pthread_mutex_unlock(&jni_lws_list_mutex_);
    break;
  }

  default:
    break;
  }

  return 0;
}

struct lws *Container::get_lws_from_websocket_id(jlong websocket_id) {

  struct lws* wsi = reinterpret_cast<struct lws*>(websocket_id);
  pthread_mutex_lock(&jni_lws_list_mutex_);
  struct lws* current_wsi = jni_lws_list_;
  bool found = 0;
  while (current_wsi) {
    if (current_wsi == wsi) {
      found = 1;
      break;
    }
    current_wsi = current_wsi->jni_lws_list;
  }
  pthread_mutex_unlock(&jni_lws_list_mutex_);
  return found ? wsi : NULL;
}
  
}  // namespace jni
}  // namespace websocket
