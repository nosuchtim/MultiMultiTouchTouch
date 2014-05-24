if "x%ProgramFiles(x86)%" == "x" set ProgramFiles(x86)=%ProgramFiles%
"%ProgramFiles(x86)%\Nosuch Media\MultiMultiTouchTouch\KinectRuntime-v1.8-Setup.exe"
"%ProgramFiles(x86)%\Nosuch Media\MultiMultiTouchTouch\KinectSDK-v1.8-Setup.exe"
"%ProgramFiles(x86)%\Nosuch Media\MultiMultiTouchTouch\intel_pc_sdk_runtime_ia32_9817.msi"
"%ProgramFiles(x86)%\Nosuch Media\MultiMultiTouchTouch\DepthSenseSDK-1.2.0.814-win32.exe"
cd "%ProgramFiles(x86)%\Nosuch Media\MultiMultiTouchTouch"
.\7z.exe x -y "-o%USERPROFILE%\Documents" Processing.zip
msg %USERNAME% "You are all done installing the prerequisites for MultiMultiTouchTouch!!"
