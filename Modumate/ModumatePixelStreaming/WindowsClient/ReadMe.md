Windows clients can be launched using the PixelStreaming launch template. The aws CLI command is:
aws ec2 run-instances --launch-template LaunchTemplateId=lt-0bc55462138a704bd

The launch template depends on the PixelStreamingClient AMI. Should this AMI need to be retired or replaced, the launch template can be updated with a new AMI.

Steps and considerations for creating a new AMI:

1) Enter the AWS console and navigate to "Launch an Instance"
2) For application and OS image, choose a dx12 compatible Windows option with Nvidia drivers. As of this writing, the NVidia Gaming PC on Windows Server 2019 works well.
3) For instance type, choose one of the g4dn.xlarge options
4) For Key/Login pair, choose MD-PixelStreaming. The corresponding PEM file is available in LastPass under AWS Pixel Streaming
5) Under Network settings: Allow HTTP and HTTPS traffic from the internet

6) While editing network settings, set the vpc to the appropriate vlaue (05fe0c63 as of this writing) and set the appropriate subnet (abe71ef1 as of this writing)

KNOWN ISSUE: the pixel streaming script depends on Amazon's 169.254.*.* services to acquire local IP. If an AMI is launched on a different subnet than it was created on, the re-routing script fails to attach these addresses to the new gateway. The result is the pixel streamer will fail to get its IP and therefore fail to send it. Until we can fix this rerouting, AMIs for pixel streaming clients (indeed, for any amazon-dependent machine) should always be launched in the same subnet they were created in.

7) Select PixelStreaming for the security group. 	Your open ports should include: 443 (https) 80 (http) 8888 (local stream) 9999 matchmaker and 3389 (rdp). NOTE: It's especially important to make sure that 3389 is open otherwise we will not be able to connect

8) The pixel streamer scripts are designed to run on volume D:. The NVidia AMI comes with a large secondary drive that can be initialized to volume D:. If your AMI does not have adequate space, you can add another volume later, or you can add a new volume now and remove it later if necessary

9) Launch instance and wait for it to initialize. It will take several minutes.

12) When the instance is ready, select it and navigate to Connect->RDP client. Download the remote desktop file.
13) If desired, update the launch template to use the new AMI

13) Decrypt the password using the MD-PixelStreaming.pem file available in LastPass (or your own PEM if you created a new key)

14) Use the RDP file and password to log in. 
15) Install Node.JS
16) Install Autologon (https://learn.microsoft.com/en-us/sysinternals/downloads/autologon). Set the machine to automatically log in to the adminstrator account
17) If there is no D: drive, go to Disk Management (right click on start menu to find it) and allocate and init secondary drive
18) Get the NodeScripts.zip bootstrapper and unzip it to D:/Modumate. You should see D:/Modumate/NodeScripts and D:/Modumate/PixelStreaming.bat
19) Run PixelStreaming.bat to get latest client. This should download and launch. The unreal client will ask to install prerequisites. Once everything is up and running, quit the client and the signalling server
20) Set the file explorer to show hidden directories (view->options) and copy PixelStreaming.bat to users->administrator->appdata->roaming->microsoft->windows->start menu->programs->startup
21) restart the instance and connect. Verify that it has become available to the matchmaker.