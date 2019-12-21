# xrcap
Experimental XR Capture tool for Azure Kinect on Intel Windows PCs

Allows multiple cameras on several Capture Servers on a 5 GHz wireless LAN to stream to a single Viewer application that can record the volumetric stream to disk and replay it.

It supports password-based security, and the video data sent from capture server to viewer is encrypted.  The recorded data is not currently encrypted.

## Known Issues

Lots of bugs everywhere.  Mainly releasing this so people have access to the code, but I don't think it's ready for a binary release yet.

Currently the software only uses the Intel video encoder built into Intel CPUs.
On computers with a GPU some additional setup needs to be done: https://twitter.com/MrCatid/status/1181772132373520385

## How to build

You'll need Visual Studio Code: https://code.visualstudio.com/

You'll also need CMake and a copy of Visual Studio (community edition might work) installed.

I'm using the "C/C++" and "CMake Tools" extensions.

Then I just hit the build button and it downloads and makes all the software with one click.

The Azure Kinect SDK is required: https://github.com/microsoft/Azure-Kinect-Sensor-SDK
Upgrading firmware on all the cameras is a good idea.


## How to use

Run the `rendezvous_server` application and note the IP address it shows.

Connect Kinect to capture PC and run the `capture_server` application.  Enter the IP address of the `rendezvous_server` in the Host field, optionally enter name/password and click connect.

You can press `m` to view the mesh and rotate with mouse.  Multiple camera views get tiled or overlaid.

Run the `viewer` application on the same PC or another one on the network.  Enter the IP address of the `rendezvous_server` in the Host field same as before.


## How to do multi-camera calibration

In the viewer application you can put the cameras in to Calibration mode from the System Status panel.  Once in that mode you can run calibration.  You'll need to print the April Tag called `tag41_12_00000.pdf` on paper and position it in view of all the cameras.

Click the `April Tag calibration` button and it will report success/failure pretty quick.  The software needs to be built in Release mode or this will be extremely slow.

You can click the `Refine` button which uses the geometry of the room to try to refine the calibration further.

Next switch the mode to `CaptureHighQ` and you'll be able to press the `m` key and use the mouse to view the mesh.

To calibrate lighting between the cameras, use the Configuration panel.  First set a clip region of interest, which is centered around where the April tag was located.  Then use the Lock Lighting feature and Calibrate Lighting to normalize the brightness and saturation for all the cameras in this clip region.

Now you're ready to capture!  You can use the Recording panel to record to a file and the Playback panel to load the file back in to view it.

## Install Pointcloud Library

If this is required for some reason:

Download the 64-bit installer from:
https://github.com/PointCloudLibrary/pcl/releases/tag/pcl-1.9.1


## Install OpenCV under this directory

If this is required for some reason:

Download latest Windows version:
https://sourceforge.net/projects/opencvlibrary/files/opencv-win/

Copy the opencv folder it extracts to this folder, so there is
this folder structure: `xrcap/opencv/build/` and `xrcap/opencv/sources/`


## How to upgrade Azure Kinect DK firmware

This process is documented on the Microsoft website:
https://docs.microsoft.com/en-us/azure/kinect-dk/update-device-firmware

1. Download SDK.

2. Install the SDK (Under program files).

3. Open a command prompt in the (SDK install location)\tools\ folder.

4. Run the firmware tool to upgrade:

`AzureKinectFirmwareTool.exe -u firmware\AzureKinectDK_Fw_1.5.926614.bin`

5. Run the firmware tool to verify:

`AzureKinectFirmwareTool.exe -q`
