#include <memory>
#include <utility>

#include <android/log.h>

#include "sdk/android/src/jni/jni_helpers.h"

#include <libwebsockets.h>
#include <private-libwebsockets.h>

#include "container.h"
#include "observer_jni.h"

#undef JNI_FUNCTION_DECLARATION
#define JNI_FUNCTION_DECLARATION(rettype, name, ...) \
  extern "C" JNIEXPORT rettype JNICALL Java_org_libwebsockets_##name(__VA_ARGS__)

namespace websocket {
namespace jni {

jlong CreateContainerForJava(JNIEnv* jni,
			     jobject joptions,
			     jobject j_observer) {

  // options
  return webrtc::jni::jlongFromPointer(new Container(new ObserverJni(jni, j_observer)));
}
  
JNI_FUNCTION_DECLARATION(jlong,
			 ContainerImpl_nativeCreateContainer,
			 JNIEnv* jni,
			 jclass,
			 jobject joptions,
			 jobject j_observer) {

  return CreateContainerForJava(jni, joptions, j_observer);
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
			 jstring j_proxy_host,
			 jint proxy_port) {

  Container* container = reinterpret_cast<Container*>(container_p);
  const char* host = j_host ? jni->GetStringUTFChars(j_host, NULL) : NULL;
  CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  const char* path = j_path ? jni->GetStringUTFChars(j_path, NULL) : NULL;
  CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  const char* proxy_host = j_proxy_host ? jni->GetStringUTFChars(j_proxy_host, NULL) : NULL;
  CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  struct lws_reference* lws_reference = container->CreateWebSocket(sessionId, port, host, path, secure,
								   proxy_host, proxy_port);
  if (host) {
    jni->ReleaseStringUTFChars(j_host, host);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  if (path) {
    jni->ReleaseStringUTFChars(j_path, path);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  if (proxy_host) {
    jni->ReleaseStringUTFChars(j_proxy_host, proxy_host);
    CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  }
  return webrtc::jni::jlongFromPointer(lws_reference);
}

JNI_FUNCTION_DECLARATION(void,
			 ContainerImpl_nativeFreeReference,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p,
			 jlong lws_reference_p) {

  struct lws_reference* lws_reference = reinterpret_cast<struct lws_reference*>(lws_reference_p);
  lws_free(lws_reference);
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
			 jlong lws_reference_p) {

  Container* container = reinterpret_cast<Container*>(container_p);
  struct lws_reference* lws_reference = reinterpret_cast<struct lws_reference*>(lws_reference_p);
  container->TriggerWritable(lws_reference);
}

JNI_FUNCTION_DECLARATION(void,
			 ContainerImpl_nativeSendMessage,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p,
			 jlong lws_reference_p,
			 jbyteArray message,
			 jboolean binary) {

  Container* container = reinterpret_cast<Container*>(container_p);
  struct lws_reference* lws_reference = reinterpret_cast<struct lws_reference*>(lws_reference_p);
  jbyte* bytes = jni->GetByteArrayElements(message, nullptr);
  CHECK_EXCEPTION(jni) << "error during GetByteArrayElements";
  size_t length = jni->GetArrayLength(message);
  void* buffer = lws_malloc(length + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING,
			    "container_jni");
  void* data_buffer = (char *)buffer + LWS_SEND_BUFFER_PRE_PADDING;
  std::memcpy(data_buffer, bytes, length);
  jni->ReleaseByteArrayElements(message, bytes, JNI_ABORT);
  CHECK_EXCEPTION(jni) << "error during ReleaseByteArrayElements";  
  container->SendMessage(lws_reference, data_buffer, length, binary);
  lws_free(buffer);
}

JNI_FUNCTION_DECLARATION(void,
			 ContainerImpl_nativeSendCloseMessage,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p,
			 jlong lws_reference_p) {

  Container* container = reinterpret_cast<Container*>(container_p);
  struct lws_reference* lws_reference = reinterpret_cast<struct lws_reference*>(lws_reference_p);
  container->SendCloseMessage(lws_reference);
}

}
}

