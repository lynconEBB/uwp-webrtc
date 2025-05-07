#pragma once

#include "MediaFoundationEncoder.g.h"
#include <fstream>

namespace winrt::media_foundation_codecs::implementation
{
    class MediaFoundationEncoder : public MediaFoundationEncoderT<MediaFoundationEncoder>
    {
    public:
        MediaFoundationEncoder() = default;
        void Initialize();
        void Shutdown();
        com_array<uint8_t> ProcessFrame(array_view<uint8_t const> data, int64_t timestamp);
    };
}

namespace winrt::media_foundation_codecs::factory_implementation
{
    class MediaFoundationEncoder : public MediaFoundationEncoderT<MediaFoundationEncoder, implementation::MediaFoundationEncoder>
    {
        
    };
}
