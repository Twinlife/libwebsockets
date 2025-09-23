/*
 *  Copyright (c) 2025 twinlife SA.
 *
 *  All Rights Reserved.
 *
 *  Contributor:
 *   Stephane Carrez (Stephane.Carrez@skyrock.com)
 */
#import <Foundation/Foundation.h>

#import "../../sdk/objc/base/RTCMacros.h"

#define TL_CONFIG_SECURE           0x01 // Use TLS
#define TL_CONFIG_DIRECT_CONNECT   0x02 // Start a direct connection
#define TL_CONFIG_FIRST_PROXY      0x04 // Start a connection by using the first proxy
#define TL_CONFIG_KEEP_OTHERS      0x08 // Keep other websocket running even if we are connected
#define TL_CONFIG_NO_DIRECT        0x10 // Don't make a direct connection

// #define MAX_PROXIES   32
// #define NB_SOCKETS    (MAX_PROXIES + 1)

#define TL_CONFIG_SNI_PASSTHROUGH  0x08

typedef NS_ENUM(NSUInteger, TLConnectionError) {
  TLConnectionErrorNone,
    TLConnectionErrorDNS,
    TLConnectionErrorConnect,
    TLConnectionErrorTLS,
    TLConnectionErrorTLSHostname,
    TLConnectionErrorInvalidCA,
    TLConnectionErrorTCP,
    TLConnectionErrorProxy,
    TLConnectionErrorWebSocket,
    TLConnectionErrorResource,
    TLConnectionErrorIO,
    TLConnectionErrorTimeout
};

RTC_OBJC_EXPORT
@interface TLConnectionStats : NSObject

@property (readonly) int index;
@property (readonly) int64_t dnsTime;
@property (readonly) int64_t tcpConnectTime;
@property (readonly) int64_t txnResponseTime;
@property (readonly) int connectCount;
@property (readonly) int lastError;

@end

RTC_OBJC_EXPORT
@interface TLSocketProxyDescriptor : NSObject

@property (readonly, nonnull) NSString *proxyAddress;
@property (readonly, nullable) NSString *proxyUsername;
@property (readonly, nullable) NSString *proxyPassword;
@property (readonly, nullable) NSString *proxyPath;
@property (readonly) int proxyPort;
@property (readonly) int method;

@end

@protocol TLWebSocketDelegate;

RTC_OBJC_EXPORT
@interface TLWebSocket : NSObject

@property (nullable) id<TLWebSocketDelegate> delegate;

- (BOOL)isConnected;

- (BOOL)sendWithMessage:(nonnull NSData *)buffer binary:(BOOL)binary;

- (void)close;

- (long)sessionId;

- (void)triggerWritable;

@end

RTC_OBJC_EXPORT
@protocol TLWebSocketDelegate<NSObject>

- (void)onConnect:(nonnull TLWebSocket *)websocket stats:(nonnull NSArray<TLConnectionStats *> *)stats active:(int)active;

- (void)onConnectError:(nonnull TLWebSocket *)websocket stats:(nonnull NSArray<TLConnectionStats *> *)stats error:(int)error;

- (void)onClose:(nonnull TLWebSocket *)websocket;

- (void)onMessage:(nonnull TLWebSocket *)websocket message:(nonnull NSData *)data binary:(BOOL)binary;

- (void)onWritable:(nonnull TLWebSocket *)websocket;

@end

RTC_OBJC_EXPORT
@interface TLWebSocketContainer : NSObject

- (nonnull instancetype)init;

- (void)serviceWithTimeout:(int)timeout;

- (nullable TLWebSocket *)createWithSession:(int64_t)sessionId delegate:(nullable id<TLWebSocketDelegate>)delegate port:(int)port host:(nonnull NSString*)host path:(nullable NSString *)path method:(int)method timeout:(int)timeout proxies:(nullable NSArray<TLSocketProxyDescriptor *> *)proxies;

- (void)triggerWorker;

@end
