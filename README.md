# xrcap
Experimental XR Capture tool for Azure Kinect on Intel Windows PCs

Allows multiple cameras on several Capture Servers on a 5 GHz wireless LAN to stream to a single Viewer application that can record the volumetric stream to disk and replay it.

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
