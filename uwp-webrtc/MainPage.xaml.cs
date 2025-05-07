using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading;
using System.Threading.Tasks;
using Windows.Devices.Perception;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Graphics.Imaging;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.MediaProperties;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media.Imaging;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using Newtonsoft.Json;
using SIPSorcery.Net;
using SIPSorceryMedia.Abstractions;
using WebSocketSharp;
using System.Diagnostics;
using Windows.Media.Core;

namespace uwp_webrtc
{

    public class Message
    {
        public string type;
        public string data;
    }

    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        private SignalingSocket socket;
        private RTCPeerConnection pc;
        private WriteableBitmap bitmap;
        private WindowsVideoEndpoint endpoint;
        private List<RTCIceCandidate> queuedCandidates = new List<RTCIceCandidate>();

        public MainPage()
        {
            InitializeComponent();

            bitmap = new WriteableBitmap(640, 480);
            ImageFrame.Source = bitmap;

            socket = new SignalingSocket("ws://localhost:8081/signaling");
            socket.Connect();

            endpoint = new WindowsVideoEndpoint();
            endpoint.OnVideoSourceRawSample += DisplayFrame;

            StartWebRTC();
        }

        private async void DisplayFrame(uint durationMilliseconds, int width, int height, byte[] sample, VideoPixelFormatsEnum pixelFormat)
        {
            await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
            {
                using (Stream pixelStream = bitmap.PixelBuffer.AsStream())
                {
                    pixelStream.Seek(0, SeekOrigin.Begin);
                    pixelStream.Write(sample, 0, sample.Length);
                }
                bitmap.Invalidate();
            });
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
        }

        private void SendQueuedIceCandidates()
        {
            foreach (RTCIceCandidate candidate in queuedCandidates)
            {
                socket.SendIceCandidate(candidate);
            }
            queuedCandidates.Clear();
        }

        private async Task StartWebRTC()
        {
            RTCConfiguration configuration = new RTCConfiguration
            {
                iceServers = new List<RTCIceServer>
                {
                    new RTCIceServer
                    {
                        urls = "stun:stun.l.google.com:19302"
                    }
                }
            };
            pc = new RTCPeerConnection(configuration);
            pc.onicecandidate += (candidate) =>
            {
                if (pc.connectionState == RTCPeerConnectionState.connected || pc.connectionState == RTCPeerConnectionState.connecting) 
                {
                    socket.SendIceCandidate(candidate);
                } else
                {
                    queuedCandidates.Add(candidate);
                }
            };
            pc.onconnectionstatechange += async state => 
            {
                if (state == RTCPeerConnectionState.connected)
                {
                    await endpoint.StartVideo();
                }
                else if (state == RTCPeerConnectionState.failed)
                {
                    pc.Close("ice disconnection");
                }
                else if (state == RTCPeerConnectionState.closed)
                {
                    await endpoint.CloseVideo();
                }
            };
            
            MediaStreamTrack videoTrack = new MediaStreamTrack(endpoint.GetVideoSourceFormats(), MediaStreamStatusEnum.SendOnly);
            pc.addTrack(videoTrack);
            endpoint.OnVideoSourceEncodedSample += pc.SendVideo;

            socket.OnOffer += (offer) =>
            {
                pc.setRemoteDescription(offer);
                RTCSessionDescriptionInit answer = pc.createAnswer();
                pc.setLocalDescription(answer);

                socket.SendAnswer(answer);
                SendQueuedIceCandidates();
            };
            socket.OnIceCandidate += (candidate) =>
            {
                pc.addIceCandidate(candidate);
            };
        }
    }
}