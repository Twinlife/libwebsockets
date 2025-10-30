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
            webSocket.session = nil;
	}
	session->Close();
	return -1;
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
            webSocket.session = nil;
	}
    }

    void OnDestroy(Session *session) override {
        lwsl_notice("OnDestroy %ld", session->GetSessionId());

        socket_ = nil;
    }

  private:
    TLWebSocket *socket_;
};
}

@implementation TLConnectionStats

- (nonnull instancetype)initWithStats:(nonnull const websocket::ConnectionStats *)stats {

    self = [super init];
    if (self) {
        _index = stats->index;
        _proxyIndex = stats->proxyIndex;
	_dnsTime = stats->dnsTime;
	_tcpConnectTime = stats->tcpConnectTime;
	_txnResponseTime = stats->txnResponseTime;
	_connectCount = stats->connectCount;
	_lastError = stats->lastError;
	_ipv6 = stats->ipv6;
        _sniOverride = stats->sniOverride;
	if (stats->ip_addr[0]) {
	    _ipAddr = [NSString stringWithUTF8String:stats->ip_addr];
	} else {
	    _ipAddr = nil;
	}
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
	return s->SendMessage((const void*) [buffer bytes], length, binary);
    }
    return false;
}

- (BOOL)close {

    websocket::Session* s = self.session;
    if (!s) {
        return NO;
    }
    self.session = nil;
    lwsl_notice("Close %ld", s ? s->GetSessionId() : -1);
    return s->Close();
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
    if (s) {
        lwsl_notice("dealloc %ld", s ? s->GetSessionId() : -1);
        s->Close();
    }
}

@end

RTC_OBJC_EXPORT
@implementation TLWebSocketContainer

- (nonnull instancetype)initWithLevel:(int)level {

    self = [super init];
    if (self) {
	int l = LLL_ERR | LLL_WARN;
	if (level >= 1) {
	    l |= LLL_NOTICE;
	    if (level >= 2) {
	        l |= LLL_INFO;
		if (level >= 3) {
		    l |= LLL_DEBUG;
		}
	    }
	}
        lws_set_log_level(l, 0);
        _container = new websocket::Container();
    }
    return self;
}

- (void)serviceWithTimeout:(int)timeout {

    websocket::Container *container = self.container;
    if (container) {
        container->Service(timeout);
    }
}

- (nullable TLWebSocket *)createWithSession:(int64_t)sessionId delegate:(id<TLWebSocketDelegate>)delegate port:(int)port host:(nonnull NSString*)host customSNI:(nullable NSString *)customSNI path:(nullable NSString *)path method:(int)method timeout:(int)timeout proxies:(nullable NSArray<TLSocketProxyDescriptor *> *)proxies {

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
    websocket::Session* session = container->CreateWebSocket(observer, sessionId, port, [host UTF8String], [customSNI UTF8String], [path UTF8String], method, timeout, proxyDescriptors, proxyCount);
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

- (void)dealloc {

    websocket::Container *container = self.container;
    self.container = nil;
    if (container) {
        delete container;
    }
}

@end
