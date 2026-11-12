# On a Mac, an emulator is also put together as an application: a folder with the
# program, its ROMs and its icon in it, to be started from the Finder or put in
# Applications. The program by itself stays where it was, for the command line.
#
#   add_mac_app(<target> "<name of the application>" <icon.icns> <extension>...)
#
# The extensions are those of the files the application says it can open.

set(MAC_APP_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

function(add_mac_app TARGET APP_NAME ICON)
	if(NOT APPLE)
		return()
	endif()

	set(APP_EXECUTABLE ${TARGET})
	set(APP_EXTENSIONS "")
	foreach(EXTENSION ${ARGN})
		string(APPEND APP_EXTENSIONS "\t\t\t\t<string>${EXTENSION}</string>\n")
	endforeach()
	string(STRIP "${APP_EXTENSIONS}" APP_EXTENSIONS)
	set(APP_EXTENSIONS "\t\t\t\t${APP_EXTENSIONS}")

	set(PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)
	configure_file(${MAC_APP_DIRECTORY}/Info.plist.in ${PLIST} @ONLY)

	set(APP "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app")

	add_custom_command(TARGET ${TARGET} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${APP}/Contents/MacOS" "${APP}/Contents/Resources"
		COMMAND ${CMAKE_COMMAND} -E copy ${PLIST} "${APP}/Contents/Info.plist"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET}> "${APP}/Contents/MacOS/${TARGET}"
		COMMAND ${CMAKE_COMMAND} -E copy ${ICON} "${APP}/Contents/Resources/icon.icns"
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/roms "${APP}/Contents/Resources/roms"
		COMMAND codesign --force --sign - "${APP}"
		VERBATIM)
endfunction()
