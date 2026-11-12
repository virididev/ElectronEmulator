# SDL3 and zlib, as installed on the system if they are there; if not, they are
# downloaded and built with the emulator, so that a build needs no more than
# CMake and a compiler. That is mostly for Windows, which has no package manager
# that a build can count on, and for Linux distributions without SDL3 yet.
#
# -DEMULATOR_DOWNLOAD_DEPENDENCIES=ON downloads them also if they are installed.
#
# Afterwards there are the targets SDL3::SDL3 and ZLIB::ZLIB.

include(FetchContent)

option(EMULATOR_DOWNLOAD_DEPENDENCIES "Always download and build SDL3 and zlib" OFF)

set(EMULATOR_SDL3_VERSION 3.4.18)
set(EMULATOR_ZLIB_VERSION 1.3.1)

if(NOT EMULATOR_DOWNLOAD_DEPENDENCIES)
	find_package(SDL3 CONFIG QUIET)
	find_package(ZLIB QUIET)
endif()

if(NOT TARGET SDL3::SDL3)
	message(STATUS "SDL3 not found: downloading SDL ${EMULATOR_SDL3_VERSION}")
	enable_language(C)

	# as a static library, so that the program needs no DLL beside it
	set(SDL_SHARED OFF CACHE BOOL "" FORCE)
	set(SDL_STATIC ON CACHE BOOL "" FORCE)
	set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
	set(SDL_TESTS OFF CACHE BOOL "" FORCE)
	set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

	FetchContent_Declare(SDL3
		URL https://github.com/libsdl-org/SDL/releases/download/release-${EMULATOR_SDL3_VERSION}/SDL3-${EMULATOR_SDL3_VERSION}.tar.gz
		DOWNLOAD_EXTRACT_TIMESTAMP ON)
	FetchContent_MakeAvailable(SDL3)
endif()

if(NOT TARGET ZLIB::ZLIB)
	message(STATUS "zlib not found: downloading zlib ${EMULATOR_ZLIB_VERSION}")
	enable_language(C)

	# zlib's own CMakeLists makes more than is wanted here, and differs from
	# version to version, so its sources are built directly; SOURCE_SUBDIR names
	# a folder that is not there, which keeps FetchContent from using it
	FetchContent_Declare(zlib
		URL https://github.com/madler/zlib/releases/download/v${EMULATOR_ZLIB_VERSION}/zlib-${EMULATOR_ZLIB_VERSION}.tar.gz
		DOWNLOAD_EXTRACT_TIMESTAMP ON
		SOURCE_SUBDIR none)
	FetchContent_MakeAvailable(zlib)

	set(ZLIB_SOURCES adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c
		gzwrite.c infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c)
	list(TRANSFORM ZLIB_SOURCES PREPEND ${zlib_SOURCE_DIR}/)

	add_library(emuzlib STATIC ${ZLIB_SOURCES})
	target_include_directories(emuzlib SYSTEM PUBLIC ${zlib_SOURCE_DIR})
	if(MSVC)
		target_compile_definitions(emuzlib PRIVATE _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE)
	else()
		# for the declarations of open, read and close that the gz functions use
		target_compile_definitions(emuzlib PRIVATE HAVE_UNISTD_H)
	endif()
	add_library(ZLIB::ZLIB ALIAS emuzlib)
endif()
