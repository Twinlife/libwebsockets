/*
 *  Copyright (c) 2025 twinlife SA.
 *
 *  All Rights Reserved.
 *
 *  Contributor:
 *   Stephane Carrez (Stephane.Carrez@skyrock.com)
 */

#import "TLWebSocket.mm"
#import "TLWebSocket+Private.h"

// #include "rtc_base/logging.h"

namespace websocket {
class SessionObserverDelegateAdapter : public websocket::SessionObserver {
  public:
    SessionObserverDelegateAdapter(TLWebSocket *socket) {
        socket_ = socket;
    }
    void OnConnect(Session *session) override {
        lwsl_notice("OnConnect %ld", session->GetSessionId());

        TLWebSocket *webSocket = socket_;
	if (webSocket) {
	    NSArray<TLConnectionStats *> *stats = [webSocket getStats];
            [webSocket.delegate onConnect:webSocket stats:stats active:session->GetActiveSocket()];
	}
    }

    long OnConnectError(Session *session, Error error) override {
        lwsl_notice("OnConnectError %ld error: %d", session->GetSessionId(), error);

        TLWebSocket *webSocket = socket_;
	if (webSocket) {
	    NSArray<TLConnectionStats *> *stats = [webSocket getStats];
            [webSocket.delegate onConnectError:webSocket stats:stats error:error];
	}
	session->Close();
	return -1;
    }
  
    bool OnWritable(Session *session) override {
        lwsl_notice("OnWritable %ld", session->GetSessionId());

        TLWebSocket *webSocket = socket_;
	if (webSocket) {
            [webSocket.delegate onWritable:webSocket];
	}
	return true;
    }

    void OnReceive(Session *session, void* message, size_t length, bool binary) override {
        TLWebSocket *webSocket = socket_;
	if (webSocket) {
	    [webSocket.delegate onMessage:webSocket message:[[NSData alloc] initWithBytesNoCopy:message length:length freeWhenDone:NO] binary:binary];
	}
    }

    void OnClose(Session *session) override {
        lwsl_notice("OnClose %ld", session->GetSessionId());

        TLWebSocket *webSocket = socket_;
	if (webSocket) {
	    [webSocket.delegate onClose:webSocket];
	}
    }

    void OnDestroy(Session *session) override {
        lwsl_notice("OnDestroy %ld", session->GetSessionId());
    }

  private:
    __weak TLWebSocket *socket_;
};
}

@implementation TLConnectionStats

- (nonnull instancetype)initWithStats:(nonnull const websocket::ConnectionStats *)stats {

    self = [super init];
    if (self) {
        _index = stats->index;
	_dnsTime = stats->dnsTime;
	_tcpConnectTime = stats->tcpConnectTime;
	_txnResponseTime = stats->txnResponseTime;
	_connectCount = stats->connectCount;
	_lastError = stats->lastError;
    }
    return self;
}

@end

@implementation TLWebSocket {
  std::unique_ptr<websocket::SessionObserverDelegateAdapter> _observer;
}

- (nonnull instancetype)initWithDelegate:(nonnull id<TLWebSocketDelegate>)delegate {

    self = [super init];
    if (self) {
        _delegate = delegate;
        _observer.reset(new websocket::SessionObserverDelegateAdapter(self));
    }
    return self;
}

- (websocket::SessionObserverDelegateAdapter *)getObserver {

    return _observer.get();
}

- (long)sessionId {

    websocket::Session* s = self.session;
    return s ? s->GetSessionId() : -1;
}

- (BOOL)isConnected {

    websocket::Session* s = self.session;
    return s ? s->GetActiveSocket() >= 0 : NO;
}

- (BOOL)sendWithMessage:(nonnull NSData *)buffer binary:(BOOL)binary {

    websocket::Session* s = self.session;
    if (s) {
        size_t length = (size_t)[buffer length];
	unsigned char* buf = (unsigned char*) malloc(length + LWS_SEND_BUFFER_PRE_PADDING);
	if (!buf) {
	    return false;
	}
	memcpy(&buf[LWS_SEND_BUFFER_PRE_PADDING], [buffer bytes], length);
	bool result = s->SendMessage(&buf[LWS_SEND_BUFFER_PRE_PADDING], length, binary);
	free(buf);
	return result;
    }
    return false;
}

- (void)close {

    websocket::Session* s = self.session;
    self.session = nil;
    lwsl_notice("Close %ld", s ? s->GetSessionId() : -1);
    if (s) {
        s->Close();
    }
}

- (void)triggerWritable {

    websocket::Session* s = self.session;
    lwsl_notice("TriggerWritable %ld", s ? s->GetSessionId() : -1);
    if (s) {
	s->TriggerWritable();
    }
}

- (nonnull NSArray<TLConnectionStats *> *)getStats {

    NSMutableArray<TLConnectionStats *> *result = [[NSMutableArray alloc] init];
    websocket::Session *s = self.session;
    if (s) {
        for (int i = 0; i < s->GetSocketCount(); i++) {
	    [result addObject:[[TLConnectionStats alloc] initWithStats:s->GetStats(i)]];
	}
    }
    return result;
}

- (nonnull NSString *)description {

    websocket::Session *s = self.session;
    return [NSString stringWithFormat:@"TLWebSocket[%ld]", s ? s->GetSessionId() : -1];
}

- (void)dealloc {

    websocket::Session *s = self.session;
    self.session = nil;
    lwsl_notice("dealloc %ld", s ? s->GetSessionId() : -1);
    if (s) {
        s->Close();
    }
}

@end

RTC_OBJC_EXPORT
@implementation TLWebSocketContainer

- (nonnull instancetype)init {

    self = [super init];
    if (self) {
        _container = new websocket::Container();
        lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG, 0);
    }
    return self;
}

- (void)serviceWithTimeout:(int)timeout {

    websocket::Container *container = self.container;
    if (container) {
        container->Service(timeout);
    }
}

- (nullable TLWebSocket *)createWithSession:(int64_t)sessionId delegate:(id<TLWebSocketDelegate>)delegate port:(int)port host:(nonnull NSString*)host path:(nullable NSString *)path method:(int)method timeout:(int)timeout proxies:(nullable NSArray<TLSocketProxyDescriptor *> *)proxies {

    websocket::Container *container = self.container;
    if (!container) {
        return nil;
    }
    int proxyCount = proxies.count;
    struct websocket::ProxyDescriptor *proxyDescriptors = new websocket::ProxyDescriptor[proxyCount];
    for (int i = 0; i < proxyCount; i++) {
        TLSocketProxyDescriptor *proxy = proxies[i];
        proxyDescriptors[i].proxy_address = [proxy.proxyAddress UTF8String];
	proxyDescriptors[i].proxy_username = [proxy.proxyUsername UTF8String];
	proxyDescriptors[i].proxy_password = [proxy.proxyPassword UTF8String];
	proxyDescriptors[i].proxy_path = [proxy.proxyPath UTF8String];
	proxyDescriptors[i].proxy_port = proxy.proxyPort;
	proxyDescriptors[i].method = proxy.method;
    }

    TLWebSocket *webSocket = [[TLWebSocket alloc] initWithDelegate:delegate];
    websocket::SessionObserverDelegateAdapter *observer = [webSocket getObserver];
    websocket::Session* session = container->CreateWebSocket(observer, sessionId, port, [host UTF8String], [path UTF8String], method, timeout, proxyDescriptors, proxyCount);
    delete [] proxyDescriptors;
    if (!session) {
        return nil;
    }
    webSocket.session = session;
    return webSocket;
}

- (void)triggerWorker {

    websocket::Container *container = self.container;
    if (container) {
        container->TriggerWorker();
    }
}

@end
