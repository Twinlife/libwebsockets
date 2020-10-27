#include <memory>
#include <utility>

#if defined(WEBRTC_ANDROID)
#  include <android/log.h>
#endif

#include "sdk/android/src/jni/jni_helpers.h"

#include <libwebsockets.h>
#include <private-lib-core.h>
#include <private-lib-tls.h>
#include <private-lib-core-net.h>

#include "container.h"
#include "observer_jni.h"

#undef JNI_FUNCTION_DECLARATION
#define JNI_FUNCTION_DECLARATION(rettype, name, ...) \
  extern "C" JNIEXPORT rettype JNICALL Java_org_libwebsockets_##name(__VA_ARGS__)

namespace websocket {
namespace jni {

jlong CreateContainerForJava(JNIEnv* jni,
			     jobject j_observer) {

  return webrtc::jni::jlongFromPointer(new Container(new ObserverJni(jni, j_observer)));
}
  
JNI_FUNCTION_DECLARATION(jlong,
			 ContainerImpl_nativeCreateContainer,
			 JNIEnv* jni,
			 jclass,
			 jobject j_observer) {

  return CreateContainerForJava(jni, j_observer);
}

JNI_FUNCTION_DECLARATION(jlong,
			 ContainerImpl_nativeCreateWebSocket,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p,
			 jlong sessionId,
			 jint port,
			 jstring j_host,
			 jstring j_path,
			 jboolean secure,
			 jstring j_proxy_address,
			 jint proxy_port,
			 jstring j_proxy_username,
			 jstring j_proxy_password) {

  Container* container = reinterpret_cast<Container*>(container_p);
  const char* host = j_host ? jni->GetStringUTFChars(j_host, NULL) : NULL;
  CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  const char* path = NULL;
  if (!IsNull(jni, j_path)) {
    path = jni->GetStringUTFChars(j_path, NULL);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  const char* proxy_address = NULL;
  if (!IsNull(jni, j_proxy_address)) {
    proxy_address = jni->GetStringUTFChars(j_proxy_address, NULL);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  const char* proxy_username = NULL;
  if (!IsNull(jni, j_proxy_username)) {
    proxy_username = jni->GetStringUTFChars(j_proxy_username, NULL);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  const char* proxy_password = NULL;
  if (!IsNull(jni, j_proxy_password)) {
    proxy_password = jni->GetStringUTFChars(j_proxy_password, NULL);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  jlong websocket_id = container->CreateWebSocket(sessionId, port, host, path, secure,
						  proxy_address, proxy_port, proxy_username, proxy_password);
  if (host) {
    jni->ReleaseStringUTFChars(j_host, host);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  if (path) {
    jni->ReleaseStringUTFChars(j_path, path);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  if (proxy_address) {
    jni->ReleaseStringUTFChars(j_proxy_address, proxy_address);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  if (proxy_username) {
    jni->ReleaseStringUTFChars(j_proxy_username, proxy_username);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  if (proxy_password) {
    jni->ReleaseStringUTFChars(j_proxy_password, proxy_password);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  return websocket_id;
}

JNI_FUNCTION_DECLARATION(void,
			 ContainerImpl_nativeService,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p,
			 jint timeout) {

  Container* container = reinterpret_cast<Container*>(container_p);
  container->Service(timeout);
}

JNI_FUNCTION_DECLARATION(void,
			 ContainerImpl_nativeTriggerWritable,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p,
			 jlong websocket_id) {

  Container* container = reinterpret_cast<Container*>(container_p);
  container->TriggerWritable(websocket_id);
}

JNI_FUNCTION_DECLARATION(void,
			 ContainerImpl_nativeTriggerWorker,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p) {

  Container* container = reinterpret_cast<Container*>(container_p);
  container->TriggerWorker();
}

JNI_FUNCTION_DECLARATION(void,
			 ContainerImpl_nativeSendMessage,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p,
			 jlong websocket_id,
			 jbyteArray message,
			 jboolean binary) {

  Container* container = reinterpret_cast<Container*>(container_p);
  jbyte* bytes = jni->GetByteArrayElements(message, nullptr);
  CHECK_EXCEPTION(jni) << "error during GetByteArrayElements";
  size_t length = jni->GetArrayLength(message);
  void* buffer = lws_malloc(length + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING,
			    "container_jni");
  void* data_buffer = (char *)buffer + LWS_SEND_BUFFER_PRE_PADDING;
  std::memcpy(data_buffer, bytes, length);
  jni->ReleaseByteArrayElements(message, bytes, JNI_ABORT);
  CHECK_EXCEPTION(jni) << "error during ReleaseByteArrayElements";  
  container->SendMessage(websocket_id, data_buffer, length, binary);
  lws_free(buffer);
}

JNI_FUNCTION_DECLARATION(void,
			 ContainerImpl_nativeSendCloseMessage,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p,
			 jlong websocket_id) {

  Container* container = reinterpret_cast<Container*>(container_p);
  container->SendCloseMessage(websocket_id);
}

}
}

