#pragma once

class MediaFoundationEncoder
{
public:
    void Initialize();
    void Shutdown();
    std::vector<uint8_t> ProcessFrame(uint8_t* data, int size, int64_t timestamp);
};