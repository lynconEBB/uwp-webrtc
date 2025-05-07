#include "MediaFoundationEncoder.h"

#include <iostream>
#include <sstream>

#include "MediaFoundationEncoder.g.cpp"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <stdio.h>
#include <tchar.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <codecapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icodecapi.h>

namespace winrt::media_foundation_codecs::implementation
{
    using namespace Windows::Foundation;
    using namespace Windows::Storage;
    using namespace Windows::Storage::Streams;
    
    static com_ptr<IMFTransform> transform;
    
    static std::ofstream m_file;

    IAsyncAction SaveBinaryFileAsync()
    {
        try
        {
            StorageFolder localFolder = ApplicationData::Current().LocalFolder();
            StorageFile file = co_await localFolder.CreateFileAsync(L"output.h264",
                CreationCollisionOption::ReplaceExisting);

            IRandomAccessStream randomAccessStream = co_await file.OpenAsync(FileAccessMode::ReadWrite);
            IOutputStream outputStream = randomAccessStream.GetOutputStreamAt(0);
            DataWriter writer(outputStream);

            // Example: Write some binary data
            std::vector<uint8_t> binaryData = { /* your H.264 data here */ };
            writer.WriteBytes(binaryData);

            // Commit the DataWriter to the stream
            co_await writer.StoreAsync();
            co_await writer.FlushAsync();
            writer.Close();
        }
        catch (winrt::hresult_error const& ex)
        {
            // Handle errors
            winrt::hstring message = ex.message();
            // Log or display error
        }
    }

    IAsyncAction SaveWithStdOfstreamAsync()
    {
        StorageFolder localFolder = ApplicationData::Current().LocalFolder();

        StorageFile file = co_await localFolder.CreateFileAsync(L"output.h264",
            CreationCollisionOption::ReplaceExisting);

        std::wstring filePath = file.Path().c_str();

        m_file.open(filePath, std::ios::out | std::ios::binary);
        if (m_file.is_open())
        {
            OutputDebugString(filePath.c_str());
            OutputDebugString(L"File opened successfully\n");
        }
    }
    void MediaFoundationEncoder::Initialize()
    {
        try
        {
            SaveWithStdOfstreamAsync();
            
            check_hresult(MFStartup(MF_VERSION));

            check_hresult(CoCreateInstance(CLSID_MSH264EncoderMFT,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(transform.put()))
            );

            com_ptr<IMFMediaType> inputMediaType;
            check_hresult(MFCreateMediaType(inputMediaType.put()));
            check_hresult(inputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
            check_hresult(inputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12));
            check_hresult(inputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
            check_hresult(MFSetAttributeSize(inputMediaType.get(), MF_MT_FRAME_SIZE, 640, 480));   // Set resolution
            check_hresult(MFSetAttributeRatio(inputMediaType.get(), MF_MT_FRAME_RATE, 5, 1));       // Set FPS

            com_ptr<IMFMediaType> outputMediaType;
            check_hresult(MFCreateMediaType(outputMediaType.put()));
            check_hresult(outputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
            check_hresult(outputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));
            check_hresult(outputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 240000));
            check_hresult(outputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
            check_hresult(MFSetAttributeSize(outputMediaType.get(), MF_MT_FRAME_SIZE, 640, 480));
            check_hresult(MFSetAttributeRatio(outputMediaType.get(), MF_MT_FRAME_RATE, 5, 1));
            check_hresult(outputMediaType->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main));
            check_hresult(outputMediaType->SetUINT32(MF_MT_MPEG2_LEVEL, 40));

            check_hresult(transform->SetOutputType(0, outputMediaType.get(), 0));
            check_hresult(transform->SetInputType(0, inputMediaType.get(), 0));

            DWORD mftStatus;
            transform->GetInputStatus(0, &mftStatus);
            if (mftStatus != MFT_INPUT_STATUS_ACCEPT_DATA) {
                throw hresult_error(mftStatus);
            }
            
            check_hresult(transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL));
            check_hresult(transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL));

            com_ptr<ICodecAPI> codec = transform.as<ICodecAPI>();
        } catch (hresult_error const& e)
        {
            std::wcout << e.message().c_str() << std::endl;    
        }
        
    }

    void MediaFoundationEncoder::Shutdown()
    {
        transform = nullptr;
        MFShutdown();
        m_file.close();
    }

    HRESULT CreateSample(com_ptr<IMFSample>& sample, DWORD maxLenght)
    {
        try
        {
            com_ptr<IMFMediaBuffer> buffer;
            check_hresult(MFCreateMemoryBuffer(maxLenght, buffer.put()));
            check_hresult(MFCreateSample(sample.put()));
            sample->AddBuffer(buffer.get());
            return S_OK;
        } catch (hresult_error const& e)
        {
            return e.code();
        } 
    }
    
    HRESULT ProcessOutput(com_ptr<IMFSample>& decodeOutput)
    {
        MFT_OUTPUT_STREAM_INFO streamInfo;
        MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
        DWORD processOutputStatus;
        HRESULT result = S_OK;
        
        check_hresult(transform->GetOutputStreamInfo(0, &streamInfo));
        
        outputDataBuffer.dwStreamID = 0;
        outputDataBuffer.dwStatus = 0;
        outputDataBuffer.pEvents = nullptr;
        if ((streamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0)
        {
            // OutputDebugString(L"4 - MFT dont provide sample, creating...\n");
            check_hresult(CreateSample(decodeOutput, streamInfo.cbSize));
            outputDataBuffer.pSample = decodeOutput.get();
        }
        
        HRESULT transformResult = transform->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);
		std::wstringstream ss;
        ss << L"5 - Status: " << processOutputStatus << L" - result: " << transformResult << "\n";
		//OutputDebugString(ss.str().c_str());

        if (transformResult == S_OK)
        {
            decodeOutput.copy_to(&outputDataBuffer.pSample);
        }
        else if (transformResult == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            //OutputDebugString(L" 6 - Needs more input\n");
            decodeOutput = nullptr;
            result = MF_E_TRANSFORM_NEED_MORE_INPUT;
        } else if (transformResult == MF_E_TRANSFORM_STREAM_CHANGE)
        {
            OutputDebugString(L"Stream changed!!");
            decodeOutput = nullptr;
            result = transformResult; 
        }
        else {
            //OutputDebugString(L"6 - Dont know what the fuck happened!\n");
            decodeOutput = nullptr;
            result = transformResult; 
        }

        return result;
    }

    com_array<uint8_t> MediaFoundationEncoder::ProcessFrame(array_view<uint8_t const> data, int64_t timestamp)
    {
        try {
            std::wstringstream ss;
            ss << L"1 - Received Frame: " << timestamp << " with " << data.size() << " bytes \n";
            //OutputDebugString(ss.str().c_str());

			com_ptr<IMFSample> sample;
			check_hresult(MFCreateSample(sample.put()));
			com_ptr<IMFMediaBuffer> buffer;
			check_hresult(MFCreateMemoryBuffer(data.size(), buffer.put()));
			
			uint8_t* bufferData = nullptr;
			DWORD maxLength = 0;
			buffer->Lock(&bufferData, &maxLength, nullptr);
			memcpy(bufferData, data.data(), data.size());
			buffer->Unlock();
			buffer->SetCurrentLength(data.size());
			
			check_hresult(sample->AddBuffer(buffer.get()));
			check_hresult(sample->SetSampleTime(timestamp));
			check_hresult(sample->SetSampleDuration(333333));

            //OutputDebugString(L"2 - Processing Input\n");
            transform->ProcessInput(0, sample.get(), 0);
			
			HRESULT encoderResult = S_OK;
			std::vector<uint8_t> outputData;

			while (encoderResult == S_OK)
			{
				com_ptr<IMFSample> decodeOutput;
                //OutputDebugString(L"3 - Processing Output\n");
				encoderResult = ProcessOutput(decodeOutput);

				if (encoderResult == S_OK)
				{
                    ss.clear();
                    ss.str(L"");
					uint8_t* buffData = nullptr;
					DWORD lenght = 0;
					com_ptr<IMFMediaBuffer> decodeBuffer;

					check_hresult(decodeOutput->ConvertToContiguousBuffer(decodeBuffer.put()));
					check_hresult(decodeBuffer->Lock(&buffData, nullptr, &lenght));
					outputData.insert(outputData.end(), buffData, buffData + lenght);
					ss.clear();
					ss.str(L"");
					ss << "Got " << lenght << " bytes From Process\n";
				    OutputDebugString(ss.str().c_str());
				    
					check_hresult(decodeBuffer->Unlock());
				}
			}
            
			return com_array<uint8_t>(outputData.begin(), outputData.end());
        } catch (hresult_error const& e)
        {
            std::wstringstream ss;
            ss << L"ERRRORRRRRR ==========: " << e.message().c_str() << "\n";
            OutputDebugString(ss.str().c_str());
            return com_array<uint8_t>();
        }

    }
}