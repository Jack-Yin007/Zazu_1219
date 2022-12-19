@echo off

set CMD=%1

REM Read version values
set string='findstr APP_ .\application\version.h'
for /f "tokens=2,3" %%a in (%string%) do set %%a=%%b

echo Building version %APP_MAJOR%.%APP_MINOR%.%APP_REVISION%.%APP_BUILD%

REM build loader first
cd bootloader\pca10040_uart\armgcc
IF "%CMD%" == "clean" (
    make -f Makefile clean
    IF %ERRORLEVEL% NEQ 0 ( 
        @echo Exit with error %ERRORLEVEL%
        cd ..
        exit /B %ERRORLEVEL%
    )
)
make -f Makefile
IF %ERRORLEVEL% NEQ 0 ( 
    @echo Exit with error %ERRORLEVEL%
    cd ..
    exit /B %ERRORLEVEL%
)
cd ..\..\..

REM now build application
cd application\pca10040\s132\armgcc
IF "%CMD%" == "clean" (
    make -f Makefile clean
    IF %ERRORLEVEL% NEQ 0 ( 
        @echo Exit with error %ERRORLEVEL%
        cd ..
        exit /B %ERRORLEVEL%
    )
)
make -f Makefile
IF %ERRORLEVEL% NEQ 0 ( 
    @echo Exit with error %ERRORLEVEL%
    cd ..\..\..\..
    exit /B %ERRORLEVEL%
)
cd ..\..\..\..

REM Now blend the 2 binaries into 1 executable
