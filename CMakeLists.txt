cmake_minimum_required(VERSION 3.21)
project(AcornElectron LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Visual C++: sources in UTF-8, and no warnings that fopen and the like are unsafe
if(MSVC)
	add_compile_options(/utf-8)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# SDL3 and zlib, from the system or else downloaded
include(Host/Dependencies.cmake)

# The 6502, which knows nothing of SDL, and the layer between the emulated
# machine and the host, which is all SDL

add_library(emucpu STATIC
	CPU/MOS6502.cpp
)
target_include_directories(emucpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(emucpu PUBLIC cxx_std_17)

add_library(emuhost STATIC
	Host/AudioOutput.cpp
	Host/AutoType.cpp
	Host/Display.cpp
	Host/FrameLimiter.cpp
	Host/Monitor.cpp
	Host/Profiler.cpp
	Host/Settings.cpp
	Host/Shell.cpp
)
target_include_directories(emuhost PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(emuhost PUBLIC cxx_std_17)
target_link_libraries(emuhost PUBLIC emucpu SDL3::SDL3)

# runs Klaus Dormann's functional test, see Tests/FunctionalTest.cpp
add_executable(functionaltest EXCLUDE_FROM_ALL Tests/FunctionalTest.cpp)
target_link_libraries(functionaltest PRIVATE emucpu)

# The Electron itself

# UEF tape images are usually compressed, hence zlib
add_executable(acornelectron
	src/BASIC.cpp
	src/Keyboard.cpp
	src/Machine.cpp
	src/Main.cpp
	src/Sound.cpp
	src/Tape.cpp
	src/TapeRecorder.cpp
	src/ULA.cpp
	src/Video.cpp
)

target_include_directories(acornelectron PRIVATE src)
target_link_libraries(acornelectron PRIVATE emuhost ZLIB::ZLIB)

# the emulator looks for its ROMs in a 'roms' folder next to the executable
add_custom_command(TARGET acornelectron POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_CURRENT_SOURCE_DIR}/roms $<TARGET_FILE_DIR:acornelectron>/roms)

# on a Mac, an application as well
include(Host/macos/MacApp.cmake)
add_mac_app(acornelectron "Acorn Electron" ${CMAKE_CURRENT_SOURCE_DIR}/icon/icon.icns uef csw state bas)

# on Windows, a program with an icon that can be started from Explorer
include(Host/windows/WindowsApp.cmake)
add_windows_app(acornelectron "Acorn Electron" ${CMAKE_CURRENT_SOURCE_DIR}/icon/icon.ico)
