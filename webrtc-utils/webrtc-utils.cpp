#include "pch.h"
#include "webrtc-utils.h"
#include "MediaFoundationEncoder.h"

#include <sstream>
#include <mutex>

using namespace winrt;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Media::Capture::Frames;
using namespace winrt::Windows::Media::Effects;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Imaging;

static FrameEncodedCallback s_frameEncodedCallback = nullptr;
static MediaCapture s_mediaCapture = nullptr;
static MediaFrameReader s_mediaReader = nullptr;
static MediaFoundationEncoder s_encoder;
static std::mutex s_encoderMutex;

void OnMediaCaptureFailed(MediaCapture const& sender, MediaCaptureFailedEventArgs const& errorEventArgs)
{
	OutputDebugString(L"MediaCapture Failed");
	OutputDebugString(errorEventArgs.Message().c_str());
}

static hstring ToLower(const hstring& str)
{
	std::wstring lower_str;
	lower_str = str;
	std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return hstring(lower_str);
}

static bool CaseInsensitiveCompare(const hstring& str1, const hstring& str2)
{
	return ToLower(str1) == ToLower(str2);
}

static void OnFrameArrived(MediaFrameReader sender, const MediaFrameArrivedEventArgs& args) {
    try {
    	auto start = std::chrono::high_resolution_clock::now();
        MediaFrameReference reference = sender.TryAcquireLatestFrame();
        if (reference == nullptr) 
            return;
        VideoMediaFrame videoFrame = reference.VideoMediaFrame();
        if (videoFrame == nullptr)
            return;

    	SoftwareBitmap source = videoFrame.SoftwareBitmap();
		if (videoFrame.SoftwareBitmap() == nullptr) {
			source = SoftwareBitmap::CreateCopyFromSurfaceAsync(videoFrame.Direct3DSurface()).get();
		}

    	if (source == nullptr)
    	{
    		OutputDebugString(L"Could not get SoftwareBitmap");
    		return;
    	}

    	BitmapBuffer lockedBuffer = source.LockBuffer(BitmapBufferAccessMode::Read);
        Windows::Foundation::IMemoryBufferReference referenceBuff = lockedBuffer.CreateReference();
    	uint8_t* buffer;
    	uint32_t capacity;
    	referenceBuff.as<impl::IMemoryBufferByteAccess>()->GetBuffer(&buffer, &capacity);

    	if (s_frameEncodedCallback != nullptr)
    	{
    		{
    			std::lock_guard lock(s_encoderMutex);
    			
    			std::vector<uint8_t> data = s_encoder.ProcessFrame(buffer, capacity, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    			if (data.size() > 0)
    				s_frameEncodedCallback(3000, data.data(), data.size());
    		}
    	}
    	lockedBuffer.Close();
    	source.Close();
    	
    	auto end = std::chrono::high_resolution_clock::now();
    	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    	std::wstringstream ss;
    	ss << L"Frame processed in " << duration.count() << L" miliseconds\n";
		OutputDebugString(ss.str().c_str()); 
    }
    catch (hresult_error const& ex)
    {
		hstring message = ex.message();
		
		OutputDebugString(L"Deu errorrrr!!!\n");
		OutputDebugString(message.c_str());
    }
}

static void SetupMediaCapture() 
{
	try {
		MediaCaptureInitializationSettings settings;
		settings.SharingMode(MediaCaptureSharingMode::ExclusiveControl);
		settings.MemoryPreference(MediaCaptureMemoryPreference::Auto);
		settings.StreamingCaptureMode(StreamingCaptureMode::Video);

		s_mediaCapture = MediaCapture();
		s_mediaCapture.InitializeAsync(settings).get();
		s_mediaCapture.Failed(OnMediaCaptureFailed);

		//auto definition = winrt::make<MrcEffectDefinitions::MrcVideoEffectDefinition>();
		//g_MediaCapture.AddVideoEffectAsync(definition, MediaStreamType::VideoRecord);

		MediaFrameSource colorSource(nullptr);
		for (IKeyValuePair<hstring, MediaFrameSource> item : s_mediaCapture.FrameSources()) {
			if (item.Value().Info().MediaStreamType() == MediaStreamType::VideoRecord || item.Value().Info().MediaStreamType() == MediaStreamType::VideoPreview
				&& item.Value().Info().SourceKind() == MediaFrameSourceKind::Color) {
					colorSource = item.Value();
			}
		}

		for (MediaFrameFormat format : colorSource.SupportedFormats()) {
			float framerate = format.FrameRate().Numerator() / format.FrameRate().Denominator();

			if (framerate >= 30 && CaseInsensitiveCompare(format.Subtype(), MediaEncodingSubtypes::Nv12()) && format.VideoFormat().Width() == 640 && format.VideoFormat().Height() == 480) {
				colorSource.SetFormatAsync(format).get();
				break;
			}
		}

		s_mediaReader = s_mediaCapture.CreateFrameReaderAsync(colorSource).get();
		s_mediaReader.AcquisitionMode(MediaFrameReaderAcquisitionMode::Realtime);
		s_mediaReader.FrameArrived(OnFrameArrived);
	}
	catch (hresult_error const& ex)
	{
		hstring message = ex.message();
		
		OutputDebugString(L"Deu errorrrr!!!\n");
		OutputDebugString(message.c_str());
	}
}

extern "C" 
{
	WEBRTCUTILS_API bool Setup()
	{
		SetupMediaCapture();
		s_encoder.Initialize();
		return true;
	}

	WEBRTCUTILS_API bool StartVideo()
	{
		if (s_mediaCapture == nullptr || s_mediaReader == nullptr)
			return false;
			
		MediaFrameReaderStartStatus status = s_mediaReader.StartAsync().get();
		return status == MediaFrameReaderStartStatus::Success;
	}
	
	WEBRTCUTILS_API void SetFrameEncodedCallback(FrameEncodedCallback callback)
	{
		s_frameEncodedCallback = callback;
	}
	
	WEBRTCUTILS_API bool Shutdown()
	{
		if (s_mediaCapture == nullptr)
		{
			s_mediaCapture.Close();
			s_mediaCapture = nullptr;
		}
		if (s_mediaReader != nullptr)
		{
			s_mediaReader.StopAsync().get();
			s_mediaReader = nullptr;
		}
		s_encoder.Shutdown();
		
		return true;
	}
}