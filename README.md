# xrcap
Experimental XR Capture tool for Azure Kinect on Intel Windows PCs

Allows multiple cameras on several Capture Servers on a 5 GHz wireless LAN to stream to a single Viewer application that can record the volumetric stream to disk and replay it.

It supports password-based security, and the video data sent from capture server to viewer is encrypted.  The recorded data is not currently encrypted.

Here's an example playback of a 3-camera recording using this toolset: https://youtu.be/bPHLcOLTEV8

## Known Issues

Lots of bugs everywhere.  Mainly releasing this so people have access to the code, but I don't think it's ready for a binary release yet.

Currently the software only uses the Intel video encoder built into Intel CPUs.
On computers with a GPU some additional setup needs to be done: https://twitter.com/MrCatid/status/1181772132373520385


## How to build

The software requires an Intel Windows PC to build.

Visual Studio 2019 (community edition might work) is required: https://visualstudio.microsoft.com/vs/

The Windows 10 SDK is required: https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk

CMake is required: https://github.com/Kitware/CMake/releases/download/v3.16.2/cmake-3.16.2-win64-x64.msi

Check out a copy of the code and then run CMake.  Specify the source folder at the top (e.g. `C:/git/xrcap`) and an "out of source" build by setting Where to build the binaries: `C:/git/xrcap/build`.

Click `Configure` and specify Visual Studio 2019.  Then wait for a while as the configure script downloads and builds dependencies, which may take about 30 minutes.

Then click `Generate` and `Open Project`.  From VS2019 select `Release` mode at the top and hit build.

I've mainly been doing development from VSCode using the CMake Tools extension, which is a bit better for code editing.

## How to use

Upgrading firmware on all the cameras is a good idea.  Capture will not be allowed unless all the firmware versions match.  See the end of this document for a guide.

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


## Hardware recommendations

Hardware shared by all cameras:
+ 1x Central sync hub for all the cameras: https://www.amazon.com/gp/product/B01L7HX4XY/
+ 1x Power strip with 4 USB3 power ports: https://www.amazon.com/gp/product/B075L2T347/

For each camera to add to the rig:

+ 2x USB3 extension cables (power + data): https://www.amazon.com/gp/product/B00AA0U08M/
+ 1x USB-C to USB3 adapter if needed: https://www.amazon.com/gp/product/B01GGKYS6E/
+ 1x Tripod with adjustable height: https://www.amazon.com/gp/product/B0772WLSHZ/
+ 1x Multi-camera sync in/out cable: https://www.amazon.com/gp/product/B004JWIPKM/
+ 1x Table-top tripod with a ballhead mount that allows for side-ways cameras: https://www.amazon.com/gp/product/B07L3RN7ZK/


## Multi-camera setup guidelines:

Turning the cameras 90 degrees is a good idea because it allows the cameras to get closer to the subjects (more pixels on target), so getting a ballhead allowing that is recommended.  The extrinsics calibration currently requires the cameras to all be turned the same way, either all tilted right or all tilted left (FIXME).

Each camera needs a power USB3 and a data USB3 and one sync (in/out) cable.  I've found it's helpful to weave the sync cable between the other two cables to make the bundle more tidy, and then zip-tie the cables together at about 10 or so points.  All the sync cables plug into the sync hub linked above.

One of the cameras needs a sync out (master) and the others need sync in cable connected.

Still working on these guidelines: Distance from subject of about 4-5 meters seems like a good rule of thumb for a single subject.  Three cameras seems suitable for one subject, and four seems required for two subjects sitting across from eachother.  You'll probably want one of the cameras positioned farther above the subject to catch more of their hair because that's often hard for the depth cameras to capture.


## Credits

Inspired by discussion with Moeti : Author of Sensor Stream Pipe ( https://github.com/moetsi/Sensor-Stream-Pipe )

Software by Christopher A. Taylor mrcatid@gmail.com

Please reach out if you need support or would like to collaborate on a project.
