node d:/Modumate/NodeScripts/checkversion.js
START /B D:/Modumate/pixelStreamingClient/PixelStreamingClient/Server/WebServers/SignallingWebServer/platform_scripts/cmd/runAWS.bat
timeout /t 5 /nobreak
START /B D:/Modumate/pixelStreamingClient/PixelStreamingClient/WindowsNoEditor/Modumate.exe -PixelStreamingIP=localhost -PixelStreamingPort=8888 -RenderOffScreen
