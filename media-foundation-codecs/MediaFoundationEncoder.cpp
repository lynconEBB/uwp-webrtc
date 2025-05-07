#include "MediaFoundationEncoder.h"

#include <iostream>
#include <sstream>

#include "MediaFoundationEncoder.g.cpp"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Imaging.h>
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
    static com_ptr<IMFTransform> transform;
    
    void MediaFoundationEncoder::Initialize()
    {
        try
        {
            check_hresult(MFStartup(MF_VERSION));

            IMFActivate** active = nullptr;

            MFT_REGISTER_TYPE_INFO inputType = {MFVideoFormat_NV12 };
            MFT_REGISTER_TYPE_INFO outputType = { MFMediaType_Video, MFVideoFormat_H264 };
            uint32_t count;

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
            check_hresult(MFSetAttributeRatio(inputMediaType.get(), MF_MT_FRAME_RATE, 30, 1));       // Set FPS

            com_ptr<IMFMediaType> outputMediaType;
            check_hresult(MFCreateMediaType(outputMediaType.put()));
            check_hresult(outputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
            check_hresult(outputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));
            check_hresult(outputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 240000));
            check_hresult(outputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
            check_hresult(MFSetAttributeSize(outputMediaType.get(), MF_MT_FRAME_SIZE, 640, 480));
            check_hresult(MFSetAttributeRatio(outputMediaType.get(), MF_MT_FRAME_RATE, 30, 1));
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
            if (codec) {
                VARIANT var;
                HRESULT hr = codec->GetValue(&CODECAPI_AVEncMPVGOPSize, &var);
                if (SUCCEEDED(hr)) {
                    std::wstringstream ss3;
                    ss3 << "Valor: " << var.intVal << "\n";
                    OutputDebugString(ss3.str().c_str());
                }
                var.intVal = 10;
                hr = codec->SetValue(&CODECAPI_AVEncMPVGOPSize, &var);
                hr = codec->GetValue(&CODECAPI_AVEncMPVGOPSize, &var);
                if (SUCCEEDED(hr)) {
                    std::wstringstream ss3;
                    ss3 << "Valor: " << var.intVal << "\n";
                    OutputDebugString(ss3.str().c_str());
                }
            }
            
        } catch (hresult_error const& e)
        {
            std::wcout << e.message().c_str() << std::endl;    
        }
        
    }

    void MediaFoundationEncoder::Shutdown()
    {
        transform = nullptr;
        MFShutdown();
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
            //OutputDebugString(L"6 - Found data on output\n");
            ss.clear();
            ss.str(L"");
            DWORD bufferCount;
            outputDataBuffer.pSample->GetBufferCount(&bufferCount);
            ss << "6 - Count of buffers = " << bufferCount << "\n";
            //OutputDebugString(ss.str().c_str());
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

            auto start = std::chrono::high_resolution_clock::now();

            //OutputDebugString(L"2 - Processing Input\n");
            transform->ProcessInput(0, sample.get(), 0);
			
			HRESULT encoderResult = S_OK;
			std::vector<uint8_t> outputData;

            int count = 0;
			//while (encoderResult == S_OK)
			//{
                count++;
				com_ptr<IMFSample> decodeOutput;
                //OutputDebugString(L"3 - Processing Output\n");
				encoderResult = ProcessOutput(decodeOutput);

				if (encoderResult == S_OK)
				{
                    ss.clear();
                    ss.str(L"");
					DWORD bufferCount;
                    decodeOutput->GetBufferCount(&bufferCount);
                    ss << "7 - Count do buffer: " << bufferCount << "\n";
                    //OutputDebugString(ss.str().c_str());
                    if (bufferCount > 1) {
                        //OutputDebugString(L"Tem mais de 1 count \n");
                    }
                    for (int i = 0; i < bufferCount; i++) {
						uint8_t* buffData = nullptr;
						DWORD lenght = 0;
                        com_ptr<IMFMediaBuffer> decodeBuffer;
                        check_hresult(decodeOutput->GetBufferByIndex(i, decodeBuffer.put()));
						check_hresult(decodeBuffer->Lock(&buffData, &lenght, nullptr));
						outputData.insert(outputData.end(), buffData, buffData + lenght);
						check_hresult(decodeBuffer->Unlock());
                    }
				}
			//}

			if (count > 1) {
				//OutputDebugString(L"Tem mais de 1 count \n");
			}

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            ss.clear();
            ss.str(L"");
            ss << "Encoding frame took: " << duration.count() << " ms" << std::endl;
            OutputDebugString(ss.str().c_str());

			return com_array<uint8_t>(outputData.begin(), outputData.end());
        } catch (hresult_error const& e)
        {
            std::wstringstream ss;
            ss << L"ERRRORRRRRR ==========: " << e.message().c_str() << "\n";
            OutputDebugString(e.message().c_str());
            return com_array<uint8_t>();
        }

    }
}