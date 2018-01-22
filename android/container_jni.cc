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
			 jint port,
			 jstring j_host,
			 jstring j_path,
			 jboolean secure) {

  Container* container = reinterpret_cast<Container*>(container_p);
  const char* host = jni->GetStringUTFChars(j_host, NULL);
  CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";  
  const char* path = jni->GetStringUTFChars(j_path, NULL);
  CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  struct lws* wsi = container->CreateWebSocket(port, host, path, secure);
  jni->ReleaseStringUTFChars(j_host, host);
  CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";
  jni->ReleaseStringUTFChars(j_path, path);
  CHECK_EXCEPTION(jni) << "error during GetStringUTFChars";      
  return webrtc::jni::jlongFromPointer(wsi);
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
			 jlong websocket_p) {

  Container* container = reinterpret_cast<Container*>(container_p);
  struct lws* websocket = reinterpret_cast<struct lws*>(websocket_p);  
  container->TriggerWritable(websocket);
}  

JNI_FUNCTION_DECLARATION(void,
			 ContainerImpl_nativeSendMessage,
			 JNIEnv* jni,
                         jclass,
                         jlong container_p,
			 jlong websocket_p,			 
			 jbyteArray message,
			 jboolean binary) {

  Container* container = reinterpret_cast<Container*>(container_p);
  struct lws* websocket = reinterpret_cast<struct lws*>(websocket_p);    
  jbyte* bytes = jni->GetByteArrayElements(message, nullptr);
  CHECK_EXCEPTION(jni) << "error during GetByteArrayElements";
  size_t length = jni->GetArrayLength(message);
  void* buffer = lws_malloc(length + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING,
			    "container_jni");
  void* data_buffer = (char *)buffer + LWS_SEND_BUFFER_PRE_PADDING;
  std::memcpy(data_buffer, bytes, length);
  jni->ReleaseByteArrayElements(message, bytes, JNI_ABORT);
  CHECK_EXCEPTION(jni) << "error during ReleaseByteArrayElements";  
  container->SendBuffer(websocket, data_buffer, length, binary);
  lws_free(buffer);
}

}
}

