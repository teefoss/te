@echo off
:: Turn off msft (c) message with /nologo
:: Turn off C++ runtime type info with /GR-
:: Turn off exception handling with /EHa-
:: Enable intrinsics with /Oi
:: Enable level 4 warnings with /W4
:: Output debug symbols with /Zi
:: Define the platform
:: Include our external libraries
cl ^
    /D_AMD64_ ^
    /nologo ^
    /GR- ^
    /EHa- ^
    /Oi ^
    /W4 ^
    /WX ^
    /wd 4201 ^
    /Zi ^
    /D_CRT_SECURE_NO_WARNINGS ^
    /I"external\include" ^
    source\*.c ^
    "SDL3.lib" "shell32.lib" "Ws2_32.lib" ^
    /link ^
    "/OUT:te.exe" ^
    "/LIBPATH:external\lib" ^
    "/SUBSYSTEM:CONSOLE"
