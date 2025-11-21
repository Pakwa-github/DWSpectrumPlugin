:: Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

@echo off
setlocal %= Reset errorlevel and prohibit changing env vars of the parent shell. =%

if [%1] == [/?] goto :show_usage
if [%1] == [-h] goto :show_usage
if [%1] == [--help] goto :show_usage
goto :skip_show_usage
:show_usage
    echo Usage: %~n0%~x0 [--no-tests] [--debug] [^<cmake-generation-args^>...]
    echo  --debug Compile using Debug configuration (without optimizations) instead of Release.
    echo  --no-tests Skip unit tests (Although no tests for AIBox plugin now)
    goto :exit
:skip_show_usage

set BASE_DIR_WITH_BACKSLASH=%~dp0
set BASE_DIR=%BASE_DIR_WITH_BACKSLASH:~0,-1%
set BUILD_DIR=%BASE_DIR%-build
set PLUGIN_NAME=AIBox_plugin
set SOURCE_DIR=%BASE_DIR%\src\%PLUGIN_NAME%

echo on
call "%BASE_DIR%/call_vcvars64.bat" || goto :exit
@echo off

if [%1] == [--no-tests] (
    shift
    set NO_TESTS=1
) else (
    set NO_TESTS=1
)

if [%1] == [--debug] (
    shift
    set BUILD_TYPE=Debug
) else (
    set BUILD_TYPE=Release
)

set GENERATOR_OPTIONS=-GNinja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe

echo on
    rmdir /S /Q "%BUILD_DIR%" 2>NUL
@echo off

call :build_AIBox %SOURCE_DIR% %1 %2 %3 %4 %5 %6 %7 %8 %9 || goto :exit

if [%NO_TESTS%] == [1] echo NOTE: Unit tests were not run (AIBox plugin has no tests). & goto :skip_tests
echo on
    cd "%BUILD_DIR%/unit_tests" || @goto :exit
    ctest --output-on-failure -C %BUILD_TYPE% || @goto :exit
@echo off
:skip_tests

echo:
echo AIBox plugin built successfully!
echo See the binaries in %BUILD_DIR%\%PLUGIN_NAME%
goto :exit

:build_AIBox
    set SOURCE_DIR=%1
    set PLUGIN=%~n1
    shift
    if /i "%PLUGIN:~0,4%" == "rpi_" exit /b

    set PLUGIN_BUILD_DIR=%BUILD_DIR%\%PLUGIN%
    @echo on
        mkdir "%PLUGIN_BUILD_DIR%" || @exit /b
        cd "%PLUGIN_BUILD_DIR%" || @exit /b

        cmake "%SOURCE_DIR%" %GENERATOR_OPTIONS% %1 %2 %3 %4 %5 %6 %7 %8 %9 || @exit /b
        cmake --build . || @exit /b
    @echo off

    if [%PLUGIN%] == [unit_tests] (
        set ARTIFACT=%PLUGIN_BUILD_DIR%\analytics_plugin_ut.exe
    ) else (
        set ARTIFACT=%PLUGIN_BUILD_DIR%\%PLUGIN%.dll
    )

    if not exist "%ARTIFACT%" (
        echo ERROR: Failed to build %PLUGIN%.
        exit /b 70
    )
    echo:
    echo Built: %ARTIFACT%
exit /b

:exit
    exit /b %ERRORLEVEL% %= Needed for a proper cmd.exe exit status. =%