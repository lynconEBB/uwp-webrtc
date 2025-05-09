using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using SIPSorceryMedia.Abstractions;

namespace uwp_webrtc
{
    public class WindowsVideoEndpoint : IVideoSource, IDisposable
    {
        private bool isPaused;
        private bool isStarted = false;

        public event EncodedSampleDelegate OnVideoSourceEncodedSample;
        public event RawVideoSampleDelegate OnVideoSourceRawSample;
        public event RawVideoSampleFasterDelegate OnVideoSourceRawSampleFaster;
        public event SourceErrorDelegate OnVideoSourceError;

        public WindowsVideoEndpoint()
        {
            Task.Run(() =>
                {
                    WindowsUtils.Setup();
                    WindowsUtils.SetFrameEncodedCallback(FrameEncoded);
                    WindowsUtils.StartVideo();
                }
            );
        }

        private void FrameEncoded(uint rtpDuration, IntPtr data, int size)
        {
            if (size == 0)
                return;
            
            byte[] sample = new byte[size];
            Marshal.Copy(data, sample, 0, size);
            
            if (OnVideoSourceEncodedSample != null)
                OnVideoSourceEncodedSample(rtpDuration, sample);
        }

        public async Task StartVideo()
        {
            await Task.Run(() =>
                {
                    WindowsUtils.StartVideo();
                }
            );
        }

        public Task PauseVideo()
        {
            isPaused = true;
            return Task.CompletedTask;
        }

        public Task ResumeVideo()
        {
            isPaused = false;
            return Task.CompletedTask;
        }

        public async Task CloseVideo()
        {
            await Task.Run(() =>
                {
                    WindowsUtils.Shutdown();
                }
            );
        }

        public List<VideoFormat> GetVideoSourceFormats()
        {
            return new List<VideoFormat>();
        }

        public void SetVideoSourceFormat(VideoFormat videoFormat)
        {
            throw new NotImplementedException();
        }

        public void RestrictFormats(Func<VideoFormat, bool> filter)
        {
            throw new NotImplementedException();
        }

        public void ExternalVideoSourceRawSample(uint durationMilliseconds, int width, int height, byte[] sample,
            VideoPixelFormatsEnum pixelFormat)
        {
            throw new NotImplementedException();
        }

        public void ExternalVideoSourceRawSampleFaster(uint durationMilliseconds, RawImage rawImage)
        {
            throw new NotImplementedException();
        }

        public void ForceKeyFrame()
        {
            throw new NotImplementedException();
        }

        public bool HasEncodedVideoSubscribers() => OnVideoSourceEncodedSample != null;

        public bool IsVideoSourcePaused() => isPaused;

        public void Dispose()
        {
            CloseVideo().Wait();
        }
    }
}