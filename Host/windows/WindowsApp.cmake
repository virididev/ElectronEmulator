# On Windows, an emulator gets its icon in the program file, a manifest that has
# file names passed to it in UTF-8 (as SDL hands them over) and that makes it
# sharp on screens scaled up, and the DLLs it needs next to it, so that it can
# be started from Explorer.
#
#   add_windows_app(<target> "<name of the application>" <icon.ico>)
#
# The program stays a console program, so that --help, the monitor and headless
# runs can write to the command line.

set(WINDOWS_APP_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

function(add_windows_app TARGET APP_NAME ICON)
	if(NOT WIN32)
		return()
	endif()

	set(APP_ICON ${ICON})
	set(APP_MANIFEST ${WINDOWS_APP_DIRECTORY}/app.manifest)
	set(RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.rc)
	configure_file(${WINDOWS_APP_DIRECTORY}/app.rc.in ${RESOURCES} @ONLY)
	target_sources(${TARGET} PRIVATE ${RESOURCES})

	# Visual C++ makes a manifest of its own and merges this one into it; other
	# compilers take it from the resources
	if(MSVC)
		target_sources(${TARGET} PRIVATE ${APP_MANIFEST})
	else()
		set_source_files_properties(${RESOURCES} PROPERTIES COMPILE_DEFINITIONS APP_MANIFEST_IN_RESOURCES)
	endif()

	# MinGW's C++ runtime is in DLLs that Windows does not have, so it is built in
	if(MINGW)
		target_link_options(${TARGET} PRIVATE -static)
	endif()

	# SDL3.dll and zlib1.dll, if they are DLLs; nothing if they were built in
	set(DLLS $<TARGET_RUNTIME_DLLS:${TARGET}>)
	add_custom_command(TARGET ${TARGET} POST_BUILD
		COMMAND "$<$<BOOL:${DLLS}>:${CMAKE_COMMAND};-E;copy_if_different;${DLLS};$<TARGET_FILE_DIR:${TARGET}>>"
		COMMAND_EXPAND_LISTS)
endfunction()
