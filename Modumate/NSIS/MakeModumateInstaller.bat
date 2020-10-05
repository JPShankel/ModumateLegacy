REM TODO: receive product version, path_out_root and path_in from command line in Jenkins  
"C:\Program Files (x86)\NSIS\makensis.exe" /DPRODUCT_VERSION="0.8.0.0" /DPATH_OUT_ROOT="c:\ReleaseBuilds" /DPATH_IN="c:\Modumate\Builds\Release\WindowsNoEditor" .\ModumateSetup.nsi
