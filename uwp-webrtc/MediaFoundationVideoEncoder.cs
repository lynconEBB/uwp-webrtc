﻿using System;
using System.Collections.Generic;
using System.Diagnostics;
using SIPSorceryMedia.Abstractions;

namespace uwp_webrtc
{
    public class MediaFoundationVideoEncoder: IVideoEncoder, IDisposable
    {
        private static readonly List<VideoFormat> _supportedFormats = new List<VideoFormat>()
        {
            new VideoFormat(VideoCodecsEnum.H264, 96 /*0x60*/)
        };
        public List<VideoFormat> SupportedFormats => _supportedFormats;
        
        private bool _forceKeyFrame;
        
        public void ForceKeyFrame() => _forceKeyFrame = true;

        public MediaFoundationVideoEncoder()
        {
        }
        
        public byte[] EncodeVideo(int width, int height, byte[] sample, VideoPixelFormatsEnum pixelFormat, VideoCodecsEnum codec)
        {
            return null;
        }

        public IEnumerable<VideoSample> DecodeVideo(byte[] encodedSample, VideoPixelFormatsEnum pixelFormat, VideoCodecsEnum codec)
        {
            throw new System.NotImplementedException();
        }
        
        public byte[] EncodeVideoFaster(RawImage rawImage, VideoCodecsEnum codec)
        {
            throw new System.NotImplementedException();
        }

        public IEnumerable<RawImage> DecodeVideoFaster(byte[] encodedSample, VideoPixelFormatsEnum pixelFormat, VideoCodecsEnum codec)
        {
            throw new System.NotImplementedException();
        }
        
        public void Dispose()
        {
        }
    }
}