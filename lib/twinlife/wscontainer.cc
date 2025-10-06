/*
 *  Copyright (c) 2018-2025 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 *   Stephane Carrez (Stephane.Carrez@twin.life)
 */

#include <libwebsockets.h>

#include <private-lib-core.h>
#include <private-lib-tls.h>

#include "wscontainer.h"

// issuer: Common Name: Go Daddy Root Certificate Authority - G2; Organization: GoDaddy.com, Inc.; Locality: Scottsdale; State/Province: Arizona; Country: US
static const unsigned char GoDaddyG2_SHA256[32] = {
  0x98, 0xdf, 0xb8, 0x6e, 0x81, 0x4a, 0x28, 0x43,
  0x3c, 0xbe, 0x7f, 0xe0, 0x4e, 0x73, 0x6f, 0x63,
  0x97, 0xa3, 0x2d, 0xf2, 0xc9, 0xb0, 0xa0, 0x8b,
  0xd5, 0x0a, 0x32, 0x51, 0x87, 0xe0, 0xc7, 0x70
};

// issuer: Common Name: ISRG Root X1, Organization: Internet Security Research Group, Country: US
static const unsigned char ISRGRootX1_SHA256[32] = {
  0xf4, 0x59, 0x3a, 0x1e, 0x07, 0xcc, 0x9c, 0xce,
  0xff, 0xbe, 0xd9, 0xc1, 0x1d, 0xc5, 0x21, 0x83,
  0x56, 0xf7, 0x81, 0x4d, 0x9b, 0x22, 0x94, 0x9d,
  0xe7, 0x45, 0xe6, 0x29, 0x99, 0x0c, 0x6c, 0x60
};

// issuer: Common Name: ISRG Root X2, Organization: Internet Security Research Group, Country: US
static const unsigned char ISRGRootX2_SHA256[32] = {
  0xf9, 0x01, 0xed, 0xd2, 0x3d, 0x48, 0x80, 0x1a,
  0xfc, 0xf0, 0x2b, 0x22, 0x48, 0x6d, 0x7d, 0xec,
  0xa4, 0x6c, 0x6c, 0x09, 0x69, 0xad, 0x00, 0xe8,
  0x85, 0xcb, 0xe8, 0x7b, 0x56, 0x5a, 0xe3, 0x96
};

// List of SHA256 fingerprints of root CA certificats public keys that we trust.
static const unsigned char* const rootCertFingerprints[3] = {
  GoDaddyG2_SHA256,
  ISRGRootX1_SHA256,
  ISRGRootX2_SHA256
};

#define NAMESPACE namespace websocket {

NAMESPACE

#define BUFFER_SIZE (256 * 1024)

#define TCP_KEEP_ALIVE 60
#define TCP_KEEP_ALIVE_PROBES 6
#define TCP_KEEP_ALIVE_INTERVAL 10

static int callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);

static struct lws_protocols protocols[] = {
    {
    "callback",
    callback,
    sizeof(Session),
    BUFFER_SIZE,
  },
  { NULL, NULL, 0, 0 } // end of list
};

static int callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {

  lwsl_info("callback wsi=%p reason=%d user=%p in=%p len=%lu\n", wsi, reason, user, in, (unsigned long)len);

  // Callback execution order:
  // Setup:
  // - LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS
  // - LWS_CALLBACK_PROTOCOL_INIT
  // - LWS_CALLBACK_CLIENT_HTTP_BIND_PROTOCOL
  // Connecting:
  // - LWS_CALLBACK_CONNECTING
  // - LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED
  // - LWS_CALLBACK_GET_THREAD_ID
  // - LWS_CALLBACK_EVENT_WAIT_CANCELLED <several>
  // - LWS_CALLBACK_WSI_CREATE
  // - LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION
  // - LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER (iff SSL ok)
  // - LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP
  // - LWS_CALLBACK_CLIENT_ESTABLISHED
  // - LWS_CALLBACK_CLIENT_CONNECTION_ERROR (=1)
  // Shutdown:
  // - LWS_CALLBACK_CLIENT_HTTP_DROP_PROTOCOL
  // - LWS_CALLBACK_WSI_DESTROY

  if (wsi && reason != LWS_CALLBACK_EVENT_WAIT_CANCELLED) {
    Session* session = (Session *)wsi->user_space;
    if (session) {
      switch(reason) {
      case LWS_CALLBACK_WSI_CREATE:
        lwsl_debug("%s: create wsi %p\n", __func__, wsi);
        break;

      case LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION:
        return session->OnVerifyCert(wsi, (X509_STORE_CTX *)user, len);

      case LWS_CALLBACK_CLIENT_ESTABLISHED:
        session->OnConnect(wsi);
        break;

      case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        return session->OnConnectError(wsi, (const char*)in, len);

      case LWS_CALLBACK_CLIENT_WRITEABLE:
        return session->OnWritable(wsi);
    
      case LWS_CALLBACK_CLIENT_RECEIVE:
        session->OnReceive(wsi, in, len);
        break;

      case LWS_CALLBACK_CLIENT_CLOSED:
      case LWS_CALLBACK_CLOSED:
        session->OnClose(wsi);
        break;

      case LWS_CALLBACK_TIMER:
        return session->OnTimer(wsi);

      case LWS_CALLBACK_WSI_DESTROY:
        session->OnDestroy(wsi);
        break;

      default:
        break;
      }
    }
  }
  return 0;
}

const struct ConnectionStats *Session::GetStats() {

  return GetStats(active_);
}

const struct ConnectionStats *Session::GetStats(int index) {

  if (index < 0 || index >= socketCount_) {
    return nullptr;
  }

  WebSocket& webSocket = sockets_[index];
  pthread_mutex_lock(&lock_);
  struct lws *wsi = webSocket.wsi_;
  if (wsi) {
    struct lws_conmon cm;

    lws_conmon_wsi_take(wsi, &cm);

    memset(&webSocket.stats_, 0, sizeof(webSocket.stats_));
    webSocket.stats_.dnsTime = cm.ciu_dns;
    webSocket.stats_.tcpConnectTime = cm.ciu_sockconn;
    webSocket.stats_.tlsConnectTime = cm.ciu_tls;
    webSocket.stats_.txnResponseTime = cm.ciu_txn_resp;
    if (cm.peer46.sa4.sin_family != 0) {
      webSocket.stats_.ipv6 = cm.peer46.sa4.sin_family == AF_INET6;
      lws_sa46_write_numeric_address(&cm.peer46, webSocket.stats_.ip_addr, sizeof(webSocket.stats_.ip_addr));
    }
    lws_conmon_release(&cm);
  }
  pthread_mutex_unlock(&lock_);
  return &webSocket.stats_;
}

Session::Session(Container* container, SessionObserver *observer, long sessionId,
                 int port, const char* host, const char* path, int method, long timeout)
  : sessionId_(sessionId),
    startTime_(lws_now_usecs()),
    connectDeadlineTime_(startTime_ + timeout),
    container_(*container),
    observer_(*observer)
{
  socketCount_ = 0;
  wsiCount_ = 0;
  active_ = -1;
  status_ = CONNECTING;
  port_ = port;
  method_ = method;
  hostname_ = strdup(host);
  path_ = strdup(path);
  packets_ = nullptr;

  pthread_mutex_init(&lock_, NULL);
}

void Session::CreateSocket(const struct ProxyDescriptor *proxy) {

  // Ignore if we have filled all possible sockets.
  if (socketCount_ >= NB_SOCKETS) {
    return;
  }

  WebSocket& webSocket = sockets_[socketCount_];
  webSocket.proxy_address_ = nullptr;
  webSocket.port_ = port_;
  webSocket.method_ = 0;
  webSocket.stats_.index = socketCount_;
  socketCount_++;

  struct lws_vhost* vhost = lws_create_vhost(container_.context_, &container_.info_);
  if (vhost) {
    if (proxy && proxy->proxy_address) {
	strncpy(vhost->http.http_proxy_address, proxy->proxy_address, sizeof(vhost->http.http_proxy_address) - 1);
	vhost->http.http_proxy_address[sizeof(vhost->http.http_proxy_address) - 1] = '\0';

        webSocket.proxy_address_ = vhost->http.http_proxy_address;
        webSocket.proxy_port_ = proxy->proxy_port;
        webSocket.method_ = proxy->method;
        if (proxy->method & CONFIG_SNI_PASSTHROUGH) {
          vhost->http.http_proxy_port = 0;
        } else {
          vhost->http.http_proxy_port = proxy->proxy_port;
        }
	if (proxy->proxy_username && proxy->proxy_password) {
	  char *auth_token = (char *)lws_malloc(strlen(proxy->proxy_username) + strlen(proxy->proxy_password) + 2,
						"container");
	  strcpy(auth_token, proxy->proxy_username);
	  strcat(auth_token, ":");
	  strcat(auth_token, proxy->proxy_password);
	  //std::string base64_auth_token = rtc::Base64::Encode(auth_token);
	  //strncpy(vhost->proxy_basic_auth_token, base64_auth_token.c_str(),
          //	  sizeof(vhost->proxy_basic_auth_token) - 1);
	  vhost->proxy_basic_auth_token[sizeof(vhost->proxy_basic_auth_token) - 1] = '\0';
	  lws_free(auth_token);
	  vhost->proxy_path[0] = '\0';
          lwsl_debug("Connect through proxy %s port %d token %s\n", vhost->http.http_proxy_address, vhost->http.http_proxy_port, auth_token);
	} else if (proxy->proxy_path) {
	  vhost->proxy_basic_auth_token[0] = '\0';
	  strncpy(vhost->proxy_path, proxy->proxy_path, sizeof(vhost->proxy_path) - 1);
	  vhost->proxy_path[sizeof(vhost->proxy_path) - 1] = '\0';
          lwsl_debug("Connect through proxy %s port %d path %s\n", vhost->http.http_proxy_address, vhost->http.http_proxy_port, vhost->proxy_path);
	} else {
	  vhost->proxy_basic_auth_token[0] = '\0';
	  vhost->proxy_path[0] = '\0';
	}
      
    } else {
      vhost->http.http_proxy_port = 0;
      vhost->http.http_proxy_address[0] = '\0';
      vhost->proxy_basic_auth_token[0] = '\0';
      vhost->proxy_path[0] = '\0';
    }
  }

  webSocket.vhost_ = vhost;
}

int Session::Connect(WebSocket& webSocket, long timeout) {

  struct lws_client_connect_info info_ws;
  memset(&info_ws, 0, sizeof(info_ws));
  info_ws.port = webSocket.method_ & CONFIG_SNI_PASSTHROUGH ? webSocket.proxy_port_ : webSocket.port_;
  info_ws.address = webSocket.method_ & CONFIG_SNI_PASSTHROUGH ? webSocket.proxy_address_ : hostname_;
  info_ws.path = path_;
  info_ws.context = container_.context_;
  info_ws.ssl_connection = method_ & CONFIG_SECURE ? LCCSCF_USE_SSL : 0;
  info_ws.host = hostname_;
  info_ws.origin = hostname_;
  info_ws.ietf_version_or_minus_one = -1;
  info_ws.protocol = protocols[0].name;
  info_ws.userdata = this;
  info_ws.vhost = webSocket.vhost_;

  lwsl_notice("Connecting %ld.%d to %s:%d for host %s", sessionId_, webSocket.stats_.index, info_ws.address, info_ws.port, info_ws.host);

  struct lws *wsi = lws_client_connect_via_info(&info_ws);
  webSocket.wsi_ = wsi;
  if (wsi) {
    wsiCount_++;
    webSocket.startTime_ = lws_now_usecs();
    webSocket.stats_.connectCount++;
    lws_set_timer_usecs(wsi, timeout * 1000L);
  }
  return 0;
}

Session::~Session() {

  lwsl_notice("Destroy session %ld", sessionId_);

  for (int i = 0; i < socketCount_; i++) {
    if (sockets_[i].vhost_) {
      lws_vhost_destroy(sockets_[i].vhost_);
      sockets_[i].vhost_ = nullptr;
    }
  }
  free(hostname_);
  free(path_);
}

bool Session::SendMessage(const void* buffer, size_t length, bool binary) {

  struct Packet* pkt = (struct Packet*) lws_malloc(sizeof(struct Packet) + length + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING, "SendMessage");
  if (!pkt) {
    return false;
  }

  pkt->next = nullptr;
  pkt->length = length;
  pkt->binary = binary;

  unsigned char* data = &pkt->buffer[LWS_SEND_BUFFER_PRE_PADDING];
  memcpy(data, buffer, length);

  struct lws *wsi;
  pthread_mutex_lock(&lock_);
  if (active_ >= 0) {
    wsi = sockets_[active_].wsi_;
    struct Packet *next = packets_;
    if (!next) {
      packets_ = pkt;
    } else {
      while (next->next) {
	next = next->next;
      }
      next->next = pkt;
    }
  } else {
    wsi = nullptr;
  }
  pthread_mutex_unlock(&lock_);

  if (wsi) {
    lws_callback_on_writable(wsi);
    return true;
  } else {
    // Packet was not queued because there was no active wsi.
    free(pkt);
    return false;
  }
}

void Session::Close() {

  struct lws *toClose[NB_SOCKETS];
  int closeCount = 0;
  struct Packet *pkt;

  pthread_mutex_lock(&lock_);
  for (int i = 0; i < socketCount_; i++) {
    WebSocket& webSocket = sockets_[i];    
    struct lws *wsi = webSocket.wsi_;
    if (wsi) {
      toClose[closeCount] = wsi;
      webSocket.wsi_ = nullptr;
      webSocket.status_ = CLOSED;
      closeCount++;

      // Mark the suppress error because we don't want the OnConnectError() callback
      // to be executed due to the LWS_TO_KILL_SYNC we are doing.
      wsi->client_suppress_CONNECTION_ERROR = 1;
    }
  }
  pkt = packets_;
  packets_ = nullptr;
  pthread_mutex_unlock(&lock_);

  // Release any pending packet.
  while (pkt) {
    struct Packet *next = pkt->next;
    free(pkt);
    pkt = next;
  }

  for (int i = 0; i < closeCount; i++) {
    lws_set_timeout(toClose[i], PENDING_TIMEOUT_AWAITING_PROXY_RESPONSE, LWS_TO_KILL_SYNC);
  }
}

void Session::OnConnect(struct lws *wsi) {

  struct lws *toClose[NB_SOCKETS];
  int closeCount = 0;

  // Identify the websockets which are still trying to connect to invalidate and close them.
  pthread_mutex_lock(&lock_);
  for (int i = 0; i < socketCount_; i++) {
    WebSocket& webSocket = sockets_[i];    
    struct lws *w = webSocket.wsi_;
    if (w) {
      if (w == wsi) {
        if (active_ < 0 || status_ != CONNECTED) {
          active_ = i;
        }
        status_ = CONNECTED;
        webSocket.status_ = CONNECTED;
      } else if ((method_ & CONFIG_KEEP_OTHERS) == 0) {
        toClose[closeCount] = w;
        closeCount++;
        webSocket.wsi_ = nullptr;
        webSocket.status_ = CLOSED;

        // Mark the suppress error because we don't want the OnConnectError() callback
        // to be executed due to the LWS_TO_KILL_SYNC we are doing.
        w->client_suppress_CONNECTION_ERROR = 1;
      }
    } else if (webSocket.status_ == NONE) {
      webSocket.status_ = CLOSED;
    }
  }
  pthread_mutex_unlock(&lock_);

  // Close the other websockets if they are opened.
  for (int i = 0; i < closeCount; i++) {
    lws_set_timeout(toClose[i], PENDING_TIMEOUT_AWAITING_PROXY_RESPONSE, LWS_TO_KILL_SYNC);    
  }

  observer_.OnConnect(this);
  //  lws_callback_on_writable(wsi);
}

bool startsWith(const char *message, const char* prefix) {

  while (*prefix) {
    if (*message++ != *prefix++) {
      return 0;
    }
  }
  return 1;
}

Error Session::GetError(const char* message) {

  if (strcmp(message, "DNS NXDOMAIN") == 0) {
    return ERR_DNS;
  }
  if (strcmp(message, "Timed out waiting SSL") == 0) {
    return ERR_TLS;
  }
  if (strcmp(message, "Timed out waiting server reply") == 0) {
    return ERR_TCP;
  }
  if (strcmp(message, "tls: Hostname mismatch") == 0) {
    return ERR_TLS_HOSTNAME;
  }
  if (strcmp(message, "Unable to connect") == 0
      || strcmp(message, "closed before established") == 0
      || strcmp(message, "Closed before conn") == 0) {
    return ERR_CONNECT;
  }
  if (strcmp(message, "bio_create failed") == 0
      || strcmp(message, "unable to bind socket") == 0
      || strcmp(message, "tls restriction limit") == 0
      || strcmp(message, "waiting for event loop watcher to close") == 0
      || strcmp(message, "first service failed") == 0) {
    return ERR_RESOURCE;
  }
  if (strcmp(message, "proxy write failed") == 0) {
    return ERR_IO;
  }
  if (startsWith(message, "tls: ") != 0) {
    return ERR_TLS;
  }
  if (startsWith(message, "server's cert didn't look good") != 0) {
    return ERR_TLS_HOSTNAME;
  }
  if (startsWith(message, "http_proxy") != 0) {
    return ERR_PROXY;
  }
  if (startsWith(message, "HS:") != 0) {
    return ERR_WEBSOCKET;
  }
  if (startsWith(message, "conn fail:") != 0) {
    return ERR_IO;
  }
  return ERR_NONE;
}

int Session::OnConnectError(struct lws *wsi, const char* message, size_t len) {

  Error error = GetError(message);
  int running = 0;
  int toConnectCount = 0;
  struct lws_conmon cm;
  WebSocket *toConnect[NB_SOCKETS];
  int failedSocket = -1;

  lws_conmon_wsi_take(wsi, &cm);

  pthread_mutex_lock(&lock_);
  for (int i = 0; i < socketCount_; i++) {
    WebSocket *webSocket = &sockets_[i];    
    if (webSocket->wsi_ == wsi) {
      // Invalidate this websocket and record the stats.
      webSocket->wsi_ = nullptr;
      if (webSocket->stats_.lastError != ERR_NONE) {
        error = webSocket->stats_.lastError;
      }
      webSocket->status_ = ERROR;
      webSocket->stats_.lastError = error;
      webSocket->stats_.dnsTime = cm.ciu_dns;
      webSocket->stats_.tcpConnectTime = cm.ciu_sockconn;
      webSocket->stats_.tlsConnectTime = cm.ciu_tls;
      webSocket->stats_.txnResponseTime = cm.ciu_txn_resp;
      failedSocket = i;
    } else if (webSocket->wsi_) {
      running++;
    } else if (webSocket->status_ == NONE) {
      webSocket->status_ = CONNECTING;
      toConnect[toConnectCount] = webSocket;
      toConnectCount++;
    }
  }
  pthread_mutex_unlock(&lock_);

  lwsl_notice("OnConnectError %ld.%d dns=%d tcp=%d tls=%d txn=%d error=%d",
              sessionId_, failedSocket, cm.ciu_dns, cm.ciu_sockconn,
              cm.ciu_tls, cm.ciu_txn_resp, error);

  lws_conmon_release(&cm);

  if (toConnectCount > 0) {
    for (int i = 0; i < toConnectCount; i++) {
      Connect(*toConnect[i], 5000);
    }

  } else if (running == 0) {
    observer_.OnConnectError(this, error);
  }
  return -1; // Close this wsi connection.
}

int Session::OnWritable(struct lws *wsi) {

  // The lws_write() can be called only from the LWS_CALLBACK_CLIENT_WRITEABLE callback.
  while (1) {
    struct Packet *pkt;

    pthread_mutex_lock(&lock_);
    pkt = packets_;
    if (pkt) {
      packets_ = pkt->next;
    }
    pthread_mutex_unlock(&lock_);
    if (!pkt) {
      return 0;
    }

    int result = lws_write(wsi, &pkt->buffer[LWS_SEND_BUFFER_PRE_PADDING], pkt->length, pkt->binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);
    free(pkt);
    if (result < 0) {
      return -1;
    }	
  }
}

void Session::OnReceive(struct lws *wsi, void *in, size_t len) {

  observer_.OnReceive(this, in, len, lws_frame_is_binary(wsi));
}

void Session::OnClose(struct lws *wsi) {

  int wsiCount = 0;
  struct Packet *pkt;

  pthread_mutex_lock(&lock_);
  for (int i = 0; i < socketCount_; i++) {
    if (sockets_[i].wsi_ == wsi) {
      sockets_[i].wsi_ = nullptr;
    } else if (sockets_[i].wsi_ != nullptr) {
      wsiCount++;
    }
  }
  if (wsiCount == 0) {
    pkt = packets_;
    packets_ = nullptr;
  } else {
    pkt = nullptr;
  }
  pthread_mutex_unlock(&lock_);

  // Release any pending packet.
  while (pkt) {
    struct Packet *next = pkt->next;
    free(pkt);
    pkt = next;
  }
  lwsl_notice("%s: OnClose wsi %p\n", __func__, wsi);

  if (wsiCount == 0) {
    observer_.OnClose(this);
  }
}

void Session::OnDestroy(struct lws *wsi) {

  pthread_mutex_lock(&lock_);
  for (int i = 0; i < socketCount_; i++) {
    if (sockets_[i].wsi_ == wsi) {
      sockets_[i].wsi_ = nullptr;
      break;
    }
  }
  wsiCount_--;
  pthread_mutex_unlock(&lock_);

  lwsl_notice("%s: destroy wsi %p\n", __func__, wsi);
  wsi->a.vhost = NULL;

  if (wsiCount_ == 0) {
    container_.Destroy(this);
    observer_.OnDestroy(this); 
  }
}

int Session::OnVerifyCert(struct lws *wsi, X509_STORE_CTX *x509_store_ctx, int len) {

  int pos = -1;
  pthread_mutex_lock(&lock_);
  for (int i = 0; i < socketCount_; i++) {
    if (sockets_[i].wsi_ == wsi) {
      pos = i;
      break;
    }
  }
  pthread_mutex_unlock(&lock_);
  if (pos < 0) {
    return -1;
  }

  WebSocket& webSocket = sockets_[pos];
  if (!webSocket.root_certificate_verified_) {
    webSocket.root_certificate_verified_ = true;

    bool preverify_ok = len;
    bool verify_ok = false;
    if (preverify_ok) {
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
          // Build the SHA-256 of the public key.
          unsigned int digest_len;
          unsigned char digest[EVP_MAX_MD_SIZE];
          EVP_MD_CTX* ctx = EVP_MD_CTX_new();
          const EVP_MD* md = EVP_sha256();

          EVP_MD_CTX_init(ctx);
          EVP_DigestInit_ex(ctx, md, 0);
          EVP_DigestUpdate(ctx, bytes, length);
          EVP_DigestFinal(ctx, digest, &digest_len);
          EVP_MD_CTX_destroy(ctx);

          // Look each root CA fingerprint that we trust until we have a match.
          if (digest_len == 32) {
            for (int i = 0; i < 3; i++) {
              if (memcmp(digest, rootCertFingerprints[i], digest_len) == 0) {
                verify_ok = true;
                break;
              }
            }
          }
        }
        if (bytes) {
          OPENSSL_free(bytes);
        }
        if (pkey) {
          EVP_PKEY_free(pkey);
        }
        if (!verify_ok) {
          pthread_mutex_lock(&lock_);
          webSocket.status_ = ERROR;
          webSocket.stats_.lastError = ERR_INVALID_CA;
          pthread_mutex_unlock(&lock_);

          // lwsl_hexdump_err(digest, digest_len);
          return -1; // Close this wsi connection.
        }
      }
    }
  }
  return 0;
}

int Session::OnTimer(struct lws *wsi) {

  WebSocket *toConnect[NB_SOCKETS];
  int toConnectCount = 0;
  lws_usec_t now = lws_now_usecs();
  lws_usec_t delay = now - startTime_;
  lws_usec_t timeout = 0;
  bool canConnect = delay > 5000L * 1000L;
  bool expired = false;
  int curIndex = -1;

  pthread_mutex_lock(&lock_);
  if (status_ == CONNECTING) {
    if (now > connectDeadlineTime_) {
      expired = true;
      canConnect = false;
    }
    for (int i = 0; i < socketCount_; i++) {
      WebSocket *webSocket = &sockets_[i];    
      if (webSocket->wsi_) {
        if (webSocket->wsi_ == wsi) {
          timeout = connectDeadlineTime_ - now;
          curIndex = i;

          // If this is the current websocket and it is now expired, we will
          // return -1 to inform the libwebsocket to close and release that wsi connection.
          // We must not do the close ourselves because Close() could be executed when
          // we run the OnConnectError() callback and the wsi will be freed when we returned.
          if (expired) {
            webSocket->wsi_ = nullptr;
          }
        }
      } else if (canConnect && webSocket->status_ == NONE) {
        webSocket->status_ = CONNECTING;
        toConnect[toConnectCount] = webSocket;
        toConnectCount++;
      }
    }
  } else {
    timeout = 10 * 1000 * 1000;
    curIndex = active_;
  }
  pthread_mutex_unlock(&lock_);

  if (expired) {
    observer_.OnConnectError(this, ERR_TIMEOUT);
    return -1; // Close this wsi connection.
  } else {
    if (toConnectCount > 0) {
      for (int i = 0; i < toConnectCount; i++) {
        Connect(*toConnect[i], 5000);
      }
    }
    if (timeout <= 0) {
      timeout = 10 * 1000 * 1000;
    }
    lwsl_notice("OnTimer %ld.%d new timeout %lld", sessionId_, curIndex, (long long) timeout);
    lws_set_timer_usecs(wsi, timeout);
    return 0;
  }
}

Container::Container() {

  pthread_mutex_init(&lock_, NULL);
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
  info_.connect_timeout_secs = 20;
  info_.timeout_secs = 20;
  info_.ssl_client_options_set = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2;
  info_.client_ssl_cipher_list = "EDH+aRSA+AES256:EECDH+aRSA+AES256:!SSLv3";

  context_ = lws_create_context(&info_);

  lws_protocol_init(context_);
}

Container::~Container() {

  DeleteSessions();
  lws_context_destroy(context_);
}  

void Container::DeleteSessions() {

  pthread_mutex_lock(&lock_);
  while (!toDelete_.empty()) {
    Session *session = toDelete_.back();
    toDelete_.pop_back();
    pthread_mutex_unlock(&lock_);
    delete session;
    pthread_mutex_lock(&lock_);
  }
  pthread_mutex_unlock(&lock_);
}

Session* Container::CreateWebSocket(SessionObserver *observer, long sessionId, int port, const char* host, const char* path, int method, long timeout,
                                    const struct ProxyDescriptor *proxies, int proxyCount) {
  if (!context_ || proxyCount < 0) {
    return nullptr;
  }

  if (proxyCount > MAX_PROXIES) {
    proxyCount = MAX_PROXIES;
  }
  Session* session = new Session(this, observer, sessionId, port, host, path, method, timeout * 1000L);
  if (!session) {
    return nullptr;
  }

  session->CreateSocket(nullptr);
  for (int i = 0; i < proxyCount; i++) {
    session->CreateSocket(&proxies[i]);
  }

  // Start the direct connection if it is enabled.
  if ((method & CONFIG_DIRECT_CONNECT) != 0) {
    session->Connect(session->sockets_[0], 5000);
  } else if ((method & CONFIG_NO_DIRECT) != 0) {
    session->sockets_[0].status_ = CLOSED;
  }

  // Start a connection with the first proxy if it is enabled.
  if ((method & CONFIG_FIRST_PROXY) != 0 && session->socketCount_ > 1) {
    session->Connect(session->sockets_[1], 5000);
  }
  return session;
}

void Container::Service(int timeout) {

  lwsl_debug("%s: service with timeout=%d ms\n", __func__, timeout);
  if (context_) {
    lws_service(context_, timeout);
    DeleteSessions();
  }
}

void Container::Destroy(Session *session) {

  pthread_mutex_lock(&lock_);
  toDelete_.push_back(session);
  pthread_mutex_unlock(&lock_);
}

void Container::TriggerWorker() {

  if (context_) {
    lws_cancel_service(context_);
  }
}

}  // namespace websocket
