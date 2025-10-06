#include <memory>
#include <utility>
#include <stdlib.h>

#if defined(WEBRTC_ANDROID)
#  include <android/log.h>
#endif

#include "sdk/android/src/jni/jni_helpers.h"

#include "container_jni.h"
#include "observer_jni.h"

#undef JNI_FUNCTION_DECLARATION
#define JNI_FUNCTION_DECLARATION(rettype, name, ...) \
  extern "C" JNIEXPORT rettype JNICALL Java_org_libwebsockets_##name(__VA_ARGS__)


namespace websocket {
namespace jni {

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

Container::Container(JNIEnv* jni) :
  j_proxy_class_(jni, jni->FindClass("org/libwebsockets/SocketProxyDescriptor")),
  j_connection_stats_class_(jni, jni->FindClass("org/libwebsockets/ConnectionStats"))
{
  f_proxy_port_ = jni->GetFieldID(*j_proxy_class_, "proxyPort", "I");
  f_proxy_method_ = jni->GetFieldID(*j_proxy_class_, "method", "I");
  f_proxy_address_ = jni->GetFieldID(*j_proxy_class_, "proxyAddress", "Ljava/lang/String;");
}

struct websocket::ProxyDescriptor *Container::GetProxies(JNIEnv *env, jobjectArray j_proxies)
{
  int proxyCount = env->GetArrayLength(j_proxies);
  if (proxyCount <= 0) {
    return nullptr;
  }

  struct websocket::ProxyDescriptor *result = (struct websocket::ProxyDescriptor *)malloc (proxyCount * sizeof(struct websocket::ProxyDescriptor));
  if (!result) {
    return nullptr;
  }
  for (jsize i = 0; i < proxyCount; i++) {
    struct websocket::ProxyDescriptor *current = &result[i];
    jobject obj = env->GetObjectArrayElement(j_proxies, i);

    jstring jstr = (jstring) env->GetObjectField(obj, f_proxy_address_);
    if (jstr) {
      const char *p = env->GetStringUTFChars(jstr, NULL);
      current->proxy_address = strdup(p);
      env->ReleaseStringUTFChars(jstr, p);
    } else {
      current->proxy_address = nullptr;
    }
    current->proxy_username = nullptr;
    current->proxy_password = nullptr;
    current->proxy_path = nullptr;
    current->proxy_port = env->GetIntField(obj, f_proxy_port_);
    current->method = env->GetIntField(obj, f_proxy_method_);
    env->DeleteLocalRef(obj);
  }
  return result;
}

jlong CreateContainerForJava(JNIEnv* jni) {

  return webrtc::jni::jlongFromPointer(new websocket::jni::Container(jni));
}

JNI_FUNCTION_DECLARATION(jlong,
			 Container_nativeCreateContainer,
			 JNIEnv* jni,
			 jclass,
			 jint j_level) {

  if (j_level == 0) {
    lws_set_log_level(LLL_ERR, emit_log);    
  } else if (j_level == 1) {
    lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE, emit_log);
  } else {
    lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO, emit_log);    
  }
  return CreateContainerForJava(jni);
}

JNI_FUNCTION_DECLARATION(jlong,
			 Container_nativeCreateSession,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p,
                         jobject j_observer,
			 jlong sessionId,
			 jint port,
			 jstring j_host,
			 jstring j_path,
			 jint method,
                         jlong timeout,
			 jobjectArray j_proxies) {

  const char* host = j_host ? jni->GetStringUTFChars(j_host, NULL) : NULL;
  CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  const char* path = NULL;
  if (!IsNull(jni, j_path)) {
    path = jni->GetStringUTFChars(j_path, NULL);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }

  jlong websocket_id = 0;
  if (container_p) {
    websocket::jni::Container* container = reinterpret_cast<websocket::jni::Container*>(container_p);

    websocket::jni::Observer *observer = new websocket::jni::Observer(jni, j_observer);
    int proxyCount = jni->GetArrayLength(j_proxies);
    struct websocket::ProxyDescriptor *proxies = container->GetProxies(jni, j_proxies);
    websocket::Session *session = container->CreateWebSocket(observer, sessionId, port, host, path, method, timeout, proxies, proxyCount);
    websocket_id = webrtc::jni::jlongFromPointer(session);
    if (proxies) {
      for (int i = 0; i < proxyCount; i++) {
        if (proxies[i].proxy_address) {
          free((void*)proxies[i].proxy_address);
        }
      }
      free(proxies);
    }
  }
  if (host) {
    jni->ReleaseStringUTFChars(j_host, host);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  if (path) {
    jni->ReleaseStringUTFChars(j_path, path);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  return websocket_id;
}

JNI_FUNCTION_DECLARATION(void,
			 Container_nativeService,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p,
			 jint timeout) {

  if (container_p) {
    websocket::jni::Container* container = reinterpret_cast<websocket::jni::Container*>(container_p);
    container->Service(timeout);    
  }
}

JNI_FUNCTION_DECLARATION(void,
			 Container_nativeTriggerWorker,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p) {

  if (container_p) {
    websocket::jni::Container* container = reinterpret_cast<websocket::jni::Container*>(container_p);
    container->TriggerWorker();    
  }
}

JNI_FUNCTION_DECLARATION(void,
			 Container_nativeDispose,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p) {

  if (container_p) {
    websocket::jni::Container* container = reinterpret_cast<websocket::jni::Container*>(container_p);
    delete container;
  }
}

JNI_FUNCTION_DECLARATION(void,
			 Session_nativeSendMessage,
			 JNIEnv* jni,
                         jclass,
                         jlong session_p,
			 jbyteArray message,
			 jboolean binary) {

  jbyte* bytes = jni->GetByteArrayElements(message, nullptr);
  CHECK_EXCEPTION(jni) << "error during GetByteArrayElements";
  if (session_p) {
    websocket::Session *session = reinterpret_cast<websocket::Session*>(session_p);
    size_t length = jni->GetArrayLength(message);
    session->SendMessage(bytes, length, binary);
  }
  jni->ReleaseByteArrayElements(message, bytes, JNI_ABORT);
  CHECK_EXCEPTION(jni) << "error during ReleaseByteArrayElements";  
}

JNI_FUNCTION_DECLARATION(void,
			 Session_nativeClose,
			 JNIEnv* jni,
                         jclass,
                         jlong session_p) {

  if (session_p) {
    websocket::Session *session = reinterpret_cast<websocket::Session*>(session_p);
    session->Close();
  }
}

JNI_FUNCTION_DECLARATION(jlong,
			 Session_nativeGetSessionId,
			 JNIEnv* jni,
                         jclass,
                         jlong session_p) {

  if (session_p) {
    websocket::Session *session = reinterpret_cast<websocket::Session*>(session_p);
    return session->GetSessionId();
  } else {
    return 0;
  }
}
  
JNI_FUNCTION_DECLARATION(jlong,
			 Session_nativeActiveSocket,
			 JNIEnv* jni,
                         jclass,
                         jlong session_p) {

  if (session_p) {
    websocket::Session *session = reinterpret_cast<websocket::Session*>(session_p);
    return session->GetActiveSocket();
  } else {
    return 0;
  }
}
  
}
}

