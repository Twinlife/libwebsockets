/*
 *  Copyright (c) 2025 twinlife SA.
 *
 *  All Rights Reserved.
 *
 *  Contributor:
 *   Stephane Carrez (Stephane.Carrez@skyrock.com)
 */

#import "TLWebSocket.h"
#include "wscontainer.h"

RTC_OBJC_EXPORT
@interface TLConnectionStats ()

- (nonnull instancetype)initWithStats:(nonnull const websocket::ConnectionStats *)stats;

@end

RTC_OBJC_EXPORT
@interface TLWebSocket ()

@property (nonatomic, nullable) websocket::Session *session;

- (nonnull NSArray<TLConnectionStats *> *)getStats;

@end

RTC_OBJC_EXPORT
@interface TLWebSocketContainer ()

@property (nonatomic, nullable) websocket::Container *container;

@end

