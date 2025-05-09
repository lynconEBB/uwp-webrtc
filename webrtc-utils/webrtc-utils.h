#pragma once

#ifdef WEBRTCUTILS_EXPORTS
#define WEBRTCUTILS_API __declspec(dllexport)
#else
#define WEBRTCUTILS_API __declspec(dllimport)
#endif

using FrameEncodedCallback = void (*)(int rtpDuration, uint8_t* data, uint32_t size);

extern "C" {
	WEBRTCUTILS_API void SetFrameEncodedCallback(FrameEncodedCallback callback);
	
	WEBRTCUTILS_API bool Setup();
	
	WEBRTCUTILS_API bool StartVideo();
	
	WEBRTCUTILS_API bool Shutdown();
}
