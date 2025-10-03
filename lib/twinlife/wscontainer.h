/*
 *  Copyright (c) 2018-2025 twinlife SA.
 *
 *  All Rights Reserved.
 *  
 *  Contributors: 
 *   Christian Jacquemot (Christian.Jacquemot@twinlife-systems.com)
 *   Stephane Carrez (Stephane.Carrez@skyrock.com)
 */

#ifndef WEBSOCKET_CONTAINER_H_
#define WEBSOCKET_CONTAINER_H_

#include <vector>

extern "C" {
#include <libwebsockets.h>
}

#define CONFIG_SECURE           0x01 // Use TLS
#define CONFIG_DIRECT_CONNECT   0x02 // Start a direct connection
#define CONFIG_FIRST_PROXY      0x04 // Start a connection by using the first proxy
#define CONFIG_KEEP_OTHERS      0x08 // Keep other websocket running even if we are connected
#define CONFIG_NO_DIRECT        0x10 // Don't make a direct connection

#define MAX_PROXIES   32
#define NB_SOCKETS    (MAX_PROXIES + 1)

#define CONFIG_SNI_PASSTHROUGH  0x08

namespace websocket {

  class Container;
  class Session;

  enum Error {
    ERR_NONE,
    ERR_DNS,
    ERR_CONNECT,
    ERR_TLS,
    ERR_TLS_HOSTNAME,
    ERR_INVALID_CA,
    ERR_TCP,
    ERR_PROXY,
    ERR_WEBSOCKET,
    ERR_RESOURCE,
    ERR_IO,
    ERR_TIMEOUT
  };

  enum Status {
    NONE,
    CONNECTING,
    CONNECTED,
    CLOSED,
    ERROR
  };

  // Statistics collected for a specific websocket connection.
  struct ConnectionStats {
    unsigned index;
    long dnsTime;
    long tcpConnectTime;
    long tlsConnectTime;
    long txnResponseTime;
    long connectCount;
    Error lastError;
    bool ipv6;
    char ip_addr[INET6_ADDRSTRLEN];
  };

  class SessionObserver {
  public:
    virtual void OnConnect(Session *session) = 0;

    virtual long OnConnectError(Session *session, Error error) = 0;
  
    virtual void OnReceive(Session *session, void* message, size_t length, bool binary) = 0;

    virtual void OnClose(Session *session) = 0;

    virtual void OnDestroy(Session *session) = 0;

    virtual ~SessionObserver() {}
  };

  // Information to connect through a proxy.
  struct ProxyDescriptor {
    const char *proxy_address;
    const char *proxy_username;
    const char *proxy_password;
    const char *proxy_path;
    int proxy_port;
    int method;
  };

  // A websocket connection that could be direct or go through a proxy.
  class WebSocket {
    friend class Session;
    friend class Container;
  private:
    WebSocket() : method_(0), port_(0), status_(NONE), address_(0), proxy_address_(0),
                  wsi_(0), vhost_(0), root_certificate_verified_(false), startTime_(0) {
      memset(&stats_, 0, sizeof(stats_));
    }

    int method_;
    int port_;
    Status status_;
    const char* address_;
    const char* proxy_address_;
    int proxy_port_;
    struct lws *wsi_;
    struct lws_vhost *vhost_;
    bool root_certificate_verified_;
    lws_usec_t startTime_;
    struct ConnectionStats stats_;
  };

  struct Packet {
    struct Packet *next;
    int length;
    bool binary;
    unsigned char buffer[1];
  };

  // A session to the server that could contain one or several websockets.
  class Session {
    friend class Container;
  public:
    // Get the connection stats for a given websocket connection.
    const struct ConnectionStats *GetStats();
    const struct ConnectionStats *GetStats(int index);

    void OnConnect(struct lws *wsi);
    void OnConnecting(struct lws *wsi);
    int OnConnectError(struct lws *wsi, const char* message, size_t len);
    int OnVerifyCert(struct lws *wsi, X509_STORE_CTX *x509_store_ctx, int len);
    int OnWritable(struct lws *wsi);
    void OnReceive(struct lws *wsi, void *in, size_t len);
    void OnClose(struct lws *wsi);
    void OnDestroy(struct lws *wsi);
    Error GetError(const char* message);
    int OnTimer(struct lws *wsi);

    // Send a message on the active connected websocket.
    // Returns true if the message was sent and false if some error occurred.
    bool SendMessage(const void* buffer, size_t length, bool binary);

    // Called by the application to close the websocket session and all its connections.
    void Close();

    long GetSessionId() {
      return sessionId_;
    }
    int GetSocketCount() {
      return socketCount_;
    }
    int GetActiveSocket() {
      return active_;
    }
  private:
    const long sessionId_;
    const lws_usec_t startTime_;
    const lws_usec_t connectDeadlineTime_;
    Container& container_;
    SessionObserver& observer_;
    int socketCount_;
    int wsiCount_;
    int active_;
    int status_;
    int port_;
    int method_;
    char* hostname_;
    char* path_;
    pthread_mutex_t lock_;
    WebSocket sockets_[NB_SOCKETS];
    struct Packet *packets_;

    Session(Container* container, SessionObserver *observer, long sessionId,
            int port, const char* host, const char* path, int method, long timeout);

    ~Session();

    // Create a websocket configuration with an optional proxy.
    void CreateSocket(const struct ProxyDescriptor *proxy);

    // Connect the websocket according to its configuration and setup the given timeout.
    int Connect(WebSocket& webSocket, long timeout);
  };

  class Container {
    friend class Session;
  public:
    Container();

    ~Container();

    // Create a session to connect to the given host:port and with an optional list of proxies.
    Session* CreateWebSocket(SessionObserver *observer, long sessionId, int port, const char* host, const char* path,
                             int method, long timeout, const struct ProxyDescriptor *proxies, int proxyCount);

    void Service(int timeout);

    void TriggerWorker();
  private:
    pthread_mutex_t lock_;
    lws_context_creation_info info_;
    lws_context* context_;
    std::vector<Session *> toDelete_;

    void DeleteSessions();
    void Destroy(Session *session);
  };

}  // namespace websocket

#endif // WEBSOCKET_CONTAINER_H_

