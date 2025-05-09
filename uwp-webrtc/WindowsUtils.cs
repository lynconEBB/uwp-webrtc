using System;
using System.Runtime.InteropServices;

namespace uwp_webrtc
{
    internal class WindowsUtils
    {
        public delegate void FrameEncodedCallback(uint rtpDuration, IntPtr data, int size);
        
        [DllImport("webrtc-utils.dll", EntryPoint = "Setup", ExactSpelling = true)]
        internal static extern bool Setup();
        
        [DllImport("webrtc-utils.dll", EntryPoint = "StartVideo", ExactSpelling = true)]
        internal static extern bool StartVideo();
        
        [DllImport("webrtc-utils.dll", EntryPoint = "Shutdown", ExactSpelling = true)]
        internal static extern bool Shutdown();
        
        [DllImport("webrtc-utils.dll", EntryPoint = "SetFrameEncodedCallback", ExactSpelling = true)]
        internal static extern bool SetFrameEncodedCallback(FrameEncodedCallback callback);
    }
    
}