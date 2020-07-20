@rem prerequisites: KML_HOME environment variable set
@rem simply launch a command prompt with the proper environment variables 
@rem necessary to build the project
set kmlApplicationName=KmlDInputGamepadMapper
set kmlApplicationVersion=r0
set kmlGameDllFileName=DInputGamepadMapper
@rem To prevent simulation from becoming unstable at low frame rates due to lag 
@rem	etc, `kmlMinimumFrameRate` will ensure that the platform layer never 
@rem	sends game code a deltaSeconds value less than 1/%kmlMinimumFrameRate%!
set kmlMinimumFrameRate=30
call %windir%\system32\cmd.exe /k %KML_HOME%\misc\env.bat %~dp0