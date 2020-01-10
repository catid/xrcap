/** \file
    \brief Xrcap C# SDK
    \copyright Copyright (c) 2017 Christopher A. Taylor.  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of Xrcap nor the names of its contributors may be
      used to endorse or promote products derived from this software without
      specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace UnityPlugin
{

[System.Security.SuppressUnmanagedCodeSecurityAttribute()]
public static class Xrcap
{
    public const int XRCAP_VERSION = 9;
    public const int XRCAP_DIRECT_PORT = 28772;
    public const int XRCAP_RENDEZVOUS_PORT = 28773;
    public const int XRCAP_MAX_CAMERAS = 8;
    public const int XRCAP_PERSPECTIVE_COUNT = 3;

    public enum XrCapState
    {
        XrcapStreamState_Idle = 0,
        XrcapStreamState_Reconnecting = 1,
        XrcapStreamState_ServerOffline = 2,
        XrcapStreamState_ServerBusy = 3,
        XrcapStreamState_Relaying = 4,
        XrcapStreamState_Authenticating = 5,
        XrcapStreamState_WrongServerName = 6,
        XrcapStreamState_IncorrectPassword = 7,
        XrcapStreamState_Live = 8
    }

    public enum XrCapMode
    {
        XrcapStreamMode_Disabled = 0,
        XrcapStreamMode_Calibration = 1,
        XrcapStreamMode_CaptureLowQuality = 2,
        XrcapStreamMode_CaptureHighQuality = 3
    }

    public enum XrcapCaptureStatus
    {
        XrcapCaptureStatus_Idle = 0,
        XrcapCaptureStatus_Initializing = 1,
        XrcapCaptureStatus_Capturing = 2,
        XrcapCaptureStatus_NoCameras = 3,
        XrcapCaptureStatus_BadUsbConnection = 4,
        XrcapCaptureStatus_FirmwareVersionMismatch = 5,
        XrcapCaptureStatus_SyncCableMisconfigured = 6
    }

    public enum XrcapCameraCodes
    {
        XrcapCameraCodes_Idle = 0,
        XrcapCameraCodes_Initializing = 1,
        XrcapCameraCodes_StartFailed = 2,
        XrcapCameraCodes_Capturing = 3,
        XrcapCameraCodes_ReadFailed = 4,
        XrcapCameraCodes_SlowWarning = 5
    }

    // Note that C# marshaller assumes char* returns are allocated with
    // CoTaskMemAlloc, which is not the case here.
#if UNITY_IPHONE && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
    [DllImport("xrcap", CallingConvention = CallingConvention.Cdecl)]
#endif
    public static extern IntPtr xrcap_stream_state_str(Int32 result);
    public static string StateToString(Int32 result)
    {
        return Marshal.PtrToStringAnsi(xrcap_stream_state_str(result));
    }

#if UNITY_IPHONE && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("xrcap", CallingConvention = CallingConvention.Cdecl)]
#endif
    public static extern IntPtr xrcap_stream_mode_str(Int32 result);
    public static string ModeToString(Int32 result)
    {
        return Marshal.PtrToStringAnsi(xrcap_stream_mode_str(result));
    }

#if UNITY_IPHONE && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("xrcap", CallingConvention = CallingConvention.Cdecl)]
#endif
    public static extern IntPtr xrcap_capture_status_str(Int32 result);
    public static string CaptureStatusToString(Int32 result)
    {
        return Marshal.PtrToStringAnsi(xrcap_capture_status_str(result));
    }

#if UNITY_IPHONE && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("xrcap", CallingConvention = CallingConvention.Cdecl)]
#endif
    public static extern IntPtr xrcap_camera_code_str(Int32 result);
    public static string CameraCodeToString(Int32 result)
    {
        return Marshal.PtrToStringAnsi(xrcap_camera_code_str(result));
    }



    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct XrcapStatus
    {
        // XrcapStreamState: Library status.
        public Int32 State;

        // XrcapStreamMode: App mode from xrcap_set_server_capture_mode().
        public Int32 Mode;

        // XrcapCaptureStatus: Status of the capture server.
        public Int32 CaptureStatus;

        // Number of cameras attached to the server.
        public Int32 CameraCount;

        // XrcapCameraCodes: Status code for each camera on the server.
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = XRCAP_MAX_CAMERAS)]
        public Int32[] CameraCodes;

        // Bits per second received from server
        public Int32 BitsPerSecond;

        // Loss rate 0..1
        public float PacketlossRate;

        // One Way Delay (OWD) from server to client in microseconds
        public Int32 TripUsec;
    }



    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct XrcapPerspective
    {
        // Check this first.  If Valid = 0, then do not render.
        public Int32 Valid;

        // Number for this camera for this perspective
        public Int32 CameraNumber;

        // Size of image and Y channel
        public Int32 Width, Height;
        public IntPtr Y;

        // Size of U, V channels
        // Note: YUV channels follow eachother contiguously in memory.
        public Int32 ChromaWidth, ChromaHeight;
        public IntPtr U;
        public IntPtr V;

        // Number of indices (multiple of 3) for triangles to render
        public UInt32 IndicesCount;
        public IntPtr Indices;

        // Vertices for mesh represented as repeated: x,y,z,u,v
        public UInt32 FloatsCount;
        public IntPtr XyzuvVertices;
    }



    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct XrcapFrame
    {
        // Check this first.  If Valid = 0, then do not render.
        public Int32 Valid;

        // Number for this frame.
        // Increments once for each frame to display.
        public Int32 FrameNumber;

        // Exposure time in microseconds since the UNIX epoch.
        public UInt64 ExposureEpochUsec;

        // Perspectives to render.
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = XRCAP_PERSPECTIVE_COUNT)]
        public XrcapPerspective[] Perspectives;
    }


    //------------------------------------------------------------------------------
    // API

    /*
        All functions are safe to call repeatedly each frame and will not slow down
        rendering of the application.

        Call xrcap_frame() to receive the current frame to render, which may be the
        same frame as the previous call.  Check `FrameNumber` to see if it changed.

        To start or stop capture remotely use xrcap_set_server_capture_mode().
    */

    // Connect to rendezvous or capture server (direct).
#if UNITY_IPHONE && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("xrcap", CallingConvention = CallingConvention.Cdecl)]
#endif
    public static extern void xrcap_connect(
        [MarshalAs(UnmanagedType.LPStr)] string server_address,
        Int32 server_port,
        [MarshalAs(UnmanagedType.LPStr)] string session_name,
        [MarshalAs(UnmanagedType.LPStr)] string password);


    // This should be called every frame.
    // Fills the frame with the current state.
    // The structure indicates if a mesh can be rendered this frame or if there is
    // an error status.
    // Calling this invalidates data from the previous frame.
#if UNITY_IPHONE && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
    [DllImport("xrcap", CallingConvention = CallingConvention.Cdecl)]
#endif
    public static extern void xrcap_get(
        ref XrcapFrame frame,
        ref XrcapStatus status);


    // Set capture server mode.
#if UNITY_IPHONE && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
    [DllImport("xrcap", CallingConvention = CallingConvention.Cdecl)]
#endif
    public static extern void xrcap_set_server_capture_mode(
        Int32 mode);

    // Blocks until shutdown is complete.
#if UNITY_IPHONE && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
    [DllImport("xrcap", CallingConvention = CallingConvention.Cdecl)]
#endif
    public static extern void xrcap_shutdown();


} // class Xrcap

} // UnityPlugins
