using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Graphics.Imaging;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.MediaProperties;
using SIPSorceryMedia.Abstractions;
using Windows.Devices.Enumeration;
using System.Diagnostics;
using Windows.Media;

namespace uwp_webrtc
{
    [ComImport]
    [Guid("5B0D3235-4DBA-4D44-865E-8F1D0E4FD04D")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    unsafe interface IMemoryBufferByteAccess
    {
        void GetBuffer(out byte* buffer, out uint capacity);
    }

    public struct VideoCaptureDeviceInfo
    {
        public string ID;
        public string Name;
    }

    class FrameRateMonitor
    {
        private static long lastFrameTicks = 0; // shared across threads
        private static readonly Stopwatch stopwatch = Stopwatch.StartNew();

        public static void OnFrameArrived()
        {
            long currentTicks = stopwatch.ElapsedTicks;
            long previousTicks = Interlocked.Exchange(ref lastFrameTicks, currentTicks);

            if (previousTicks != 0)
            {
                double deltaMs = (currentTicks - previousTicks) * 1000.0 / Stopwatch.Frequency;
                Debug.WriteLine($"Frame interval: {deltaMs:F2} ms");
            }
            else
            {
                Debug.WriteLine("First frame received.");
            }
        }
    }

    public class WindowsVideoEndpoint : IVideoSource, IDisposable
    {
        private MediaCapture _mediaCapture;
        private MediaFrameReader _frameReader;
        private DateTime _lastFrameAt = DateTime.MinValue;

        private SoftwareBitmap backBuffer;
        private bool isPaused;
        private bool isStarted = false;

        public event EncodedSampleDelegate OnVideoSourceEncodedSample;
        public event RawVideoSampleDelegate OnVideoSourceRawSample;
        public event RawVideoSampleFasterDelegate OnVideoSourceRawSampleFaster;
        public event SourceErrorDelegate OnVideoSourceError;

        private readonly MediaFoundationVideoEncoder _encoder;

        public WindowsVideoEndpoint()
        {
            _encoder = new MediaFoundationVideoEncoder();
        }

        public async Task StartVideo()
        {
            if (isStarted)
                return;

            _mediaCapture = await CreateMediaCapture();
            _frameReader = await GetMediaFrameReader(_mediaCapture);
            _frameReader.FrameArrived += OnFrameArrived;
            await _frameReader.StartAsync();
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

        public Task CloseVideo()
        {
            _frameReader.Dispose();
            _mediaCapture.Dispose();
            _encoder.Dispose();

            return Task.CompletedTask;
        }

        public List<VideoFormat> GetVideoSourceFormats()
        {
            return _encoder.SupportedFormats;
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


        private int _encoderInUse = 0;
        private async void OnFrameArrived(MediaFrameReader sender, MediaFrameArrivedEventArgs args)
        {
            //FrameRateMonitor.OnFrameArrived();

            if (OnVideoSourceEncodedSample == null && OnVideoSourceRawSample == null)
                return;

            if (OnVideoSourceEncodedSample != null && Interlocked.CompareExchange(ref _encoderInUse, 1, 0) != 0)
            {
                // Debug.WriteLine("Dropping frame because encoder is busy!");
                return;
            }

            Stopwatch stopwatch = Stopwatch.StartNew();
            using (MediaFrameReference frameReference = sender.TryAcquireLatestFrame())
            {
                VideoMediaFrame videoMediaFrame = frameReference?.VideoMediaFrame;
                SoftwareBitmap source = videoMediaFrame?.SoftwareBitmap;
                if (source == null && videoMediaFrame != null && videoMediaFrame.Direct3DSurface != null)
                {
                    source = await SoftwareBitmap.CreateCopyFromSurfaceAsync(videoMediaFrame.Direct3DSurface);
                }

                if (source == null)
                    return;

                SoftwareBitmap oldBitmap = Interlocked.Exchange(ref backBuffer, source);
                BitmapBuffer bitmapBuffer = backBuffer.LockBuffer(BitmapBufferAccessMode.Read);

                unsafe
                {
                    using (IMemoryBufferReference reference = bitmapBuffer.CreateReference())
                    {
                        byte* buffer;
                        uint capacity;

                        IntPtr unknown = Marshal.GetIUnknownForObject(reference);
                        ((IMemoryBufferByteAccess)Marshal.GetObjectForIUnknown(unknown)).GetBuffer(out buffer,
                            out capacity);
                        byte[] numArray = new byte[(int)capacity];
                        Marshal.Copy((IntPtr)buffer, numArray, 0, (int)capacity);

                        if (OnVideoSourceRawSample != null)
                        {
                            uint durationMilliseconds = 0;
                            if (_lastFrameAt != DateTime.MinValue)
                                durationMilliseconds =
                                    Convert.ToUInt32(DateTime.Now.Subtract(this._lastFrameAt).TotalMilliseconds);
                            OnVideoSourceRawSample(durationMilliseconds, source.PixelWidth, source.PixelHeight,
                                numArray, VideoPixelFormatsEnum.NV12);
                        }

                        if (OnVideoSourceEncodedSample != null)
                        {
                            lock (_encoder)
                            {
                                Stopwatch s = Stopwatch.StartNew();
                                byte[] encodedSample = _encoder.EncodeVideo(640, 480, numArray,
                                    VideoPixelFormatsEnum.NV12, VideoCodecsEnum.H264);
                                s.Stop();
                                Debug.WriteLine($"Time spent on encoding: {s.ElapsedMilliseconds} ms");

                                if (encodedSample != null && encodedSample.Length > 0)
                                {
                                    Debug.WriteLine($"Encoded sample length: {encodedSample.Length}");
                                    OnVideoSourceEncodedSample(90000 / 5, encodedSample);
                                }
                            }
                        }
                    }
                }

                bitmapBuffer.Dispose();
                backBuffer?.Dispose();
                oldBitmap?.Dispose();
            }
            
            stopwatch.Stop();
            Debug.WriteLine($"Time spent on Processing frame: {stopwatch.ElapsedMilliseconds} ms");
            if (OnVideoSourceEncodedSample != null)
            {
                Interlocked.Exchange(ref _encoderInUse, 0);
            }
        }

        public static async Task<List<VideoCaptureDeviceInfo>> GetVideoCatpureDevices()
        {
            DeviceInformationCollection allAsync = await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture);
            return !(allAsync != (DeviceInformationCollection)null)
                ? (List<VideoCaptureDeviceInfo>)null
                : allAsync.Select<DeviceInformation, VideoCaptureDeviceInfo>(
                    (Func<DeviceInformation, VideoCaptureDeviceInfo>)(x => new VideoCaptureDeviceInfo()
                    {
                        ID = x.Id,
                        Name = x.Name
                    })).ToList<VideoCaptureDeviceInfo>();
        }

        public static async Task ListDevicesAndFormats()
        {
            foreach (DeviceInformation vidCapDevice in await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture))
            {
                MediaCaptureInitializationSettings mediaCaptureInitializationSettings =
                    new MediaCaptureInitializationSettings()
                    {
                        StreamingCaptureMode = StreamingCaptureMode.Video,
                        SharingMode = MediaCaptureSharingMode.SharedReadOnly,
                        VideoDeviceId = vidCapDevice.Id
                    };

                Debug.WriteLine(vidCapDevice.Name + " ==============================");
                MediaCapture mediaCapture = new MediaCapture();
                await mediaCapture.InitializeAsync(mediaCaptureInitializationSettings);
                foreach (List<MediaFrameFormat> mediaFrameFormatList in mediaCapture.FrameSources.Values
                             .Select<MediaFrameSource, IReadOnlyList<MediaFrameFormat>>(
                                 (Func<MediaFrameSource, IReadOnlyList<MediaFrameFormat>>)(x => x.SupportedFormats))
                             .Select<IReadOnlyList<MediaFrameFormat>, List<MediaFrameFormat>>(
                                 (Func<IReadOnlyList<MediaFrameFormat>, List<MediaFrameFormat>>)(y =>
                                     y.ToList<MediaFrameFormat>())))
                {
                    foreach (MediaFrameFormat mediaFrameFormat in mediaFrameFormatList)
                    {
                        VideoMediaFrameFormat videoFormat = mediaFrameFormat.VideoFormat;
                        float num = videoFormat.MediaFrameFormat.FrameRate.Numerator /
                                    videoFormat.MediaFrameFormat.FrameRate.Denominator;
                        string str = videoFormat.MediaFrameFormat.Subtype == "{30323449-0000-0010-8000-00AA00389B71}"
                            ? "I420"
                            : videoFormat.MediaFrameFormat.Subtype;
                        Debug.WriteLine($"{videoFormat.Width}x{videoFormat.Height} - {num}fps - {str}");
                    }
                }
            }
        }

        private async Task<MediaCapture> CreateMediaCapture()
        {
            var settings = new MediaCaptureInitializationSettings
            {
                StreamingCaptureMode = StreamingCaptureMode.Video,
                SharingMode = MediaCaptureSharingMode.ExclusiveControl,
                MemoryPreference = MediaCaptureMemoryPreference.Auto,
            };
            MediaCapture mediaCapture = new MediaCapture();
            mediaCapture.Failed += (sender, args) => { Console.WriteLine("MediaCapture Failed: " + args.Message); };
            await mediaCapture.InitializeAsync(settings);

            return mediaCapture;
        }

        private async Task<MediaFrameReader> GetMediaFrameReader(MediaCapture mediaCapture)
        {
            MediaFrameSource colorSource = null;
            foreach (var (key, srcInfo) in mediaCapture.FrameSources)
            {
                if (srcInfo.Info.SourceKind == MediaFrameSourceKind.Color &&
                    (srcInfo.Info.MediaStreamType == MediaStreamType.VideoRecord ||
                     srcInfo.Info.MediaStreamType == MediaStreamType.VideoPreview))
                {
                    colorSource = mediaCapture.FrameSources[srcInfo.Info.Id];
                    break;
                }
            }

            if (colorSource == null)
                return null;

            MediaFrameFormat preferredFormat = null;
            foreach (var format in colorSource.SupportedFormats)
            {
                float framerate = format.FrameRate.Numerator / (float)format.FrameRate.Denominator;
                if (framerate == 5 && format.VideoFormat.Width == 640 && format.VideoFormat.Height == 480 &&
                    string.Compare(format.Subtype,
                        MediaEncodingSubtypes.Nv12, StringComparison.OrdinalIgnoreCase) == 0)
                {
                    preferredFormat = format;
                    break;
                }
            }

            if (preferredFormat == null)
                return null;

            await colorSource.SetFormatAsync(preferredFormat);
            var reader = await mediaCapture.CreateFrameReaderAsync(colorSource);
            reader.AcquisitionMode = MediaFrameReaderAcquisitionMode.Realtime;

            return reader;
        }

        public void Dispose()
        {
            _encoder.Dispose();
        }
    }
}