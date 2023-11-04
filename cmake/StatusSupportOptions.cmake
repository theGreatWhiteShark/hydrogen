#
# Copyright (c) 2009, Jérémy Zurcher, <jeremy@asynk.ch>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

macro(MANDATORY_PKG pkg_name)
    set(WANT_${pkg_name} TRUE)
    set(${pkg_name}_FIND_REQUIRED TRUE)
endmacro()

string( ASCII 27 _escape)

function(COLOR_MESSAGE TEXT)
    if(CMAKE_COLOR_MAKEFILE)
        message(${TEXT})
    else()
        string(REGEX REPLACE "${_escape}.[0123456789;]*m" "" __TEXT ${TEXT})
        message(${__TEXT})
    endif()
endfunction()

macro(COMPUTE_PKGS_FLAGS prefix)
    if(NOT ${prefix}_FOUND)
        set(${prefix}_LIBRARIES "" )
        set(${prefix}_INCLUDE_DIRS "" )
    endif()
	
	set(VERSION_STRING "")
	if(NOT "${${prefix}_VERSION}" STREQUAL "")
		# version discovered using project-specific .cmake file and
		# (cmake command find_package).
		set(VERSION_STRING "[ ${${prefix}_VERSION} ] ")
	endif()
	if(NOT "${PC_${prefix}_VERSION}" STREQUAL "")
		# version discovered using project-specific .pc file and
		# `pkg-config`. This is the default way for determining
		# versions of most packages we use.
		set(VERSION_STRING "[ ${PC_${prefix}_VERSION} ] ")
	endif()
	
    if(${prefix}_NOT_NEEDED)
        if(${prefix}_FOUND)
            set(${prefix}_STATUS "${_escape}[0;33m| available\${_escape}[0m but not needed ${VERSION_STRING} ( ${${prefix}_LIBRARIES} )")
        else()
            set(${prefix}_STATUS "${_escape}[1;33m| not found\${_escape}[0m but not needed" )
        endif()
    elseif(${prefix}_FOUND)
        if(NOT WANT_${prefix})
            set(${prefix}_STATUS "${_escape}[0;32m? available${_escape}[0m but not desired ${VERSION_STRING} ( ${${prefix}_LIBRARIES} )")
        else()
            set(${prefix}_STATUS "${_escape}[1;32m+ used${_escape}[0m ${VERSION_STRING}( ${${prefix}_LIBRARIES} )")
            set(H2CORE_HAVE_${prefix} TRUE)
        endif()
    else()
        if(NOT WANT_${prefix})
            set(${prefix}_STATUS "${_escape}[0;31m- not found${_escape}[0m and not desired" )
        else()
            set(${prefix}_STATUS "${_escape}[1;31m-- not found${_escape}[0m but desired ..." )
        endif()
    endif()
endmacro()

macro(COMPUTE_PROGRAM_FLAGS PREFIX REQUIRED)
    if(${${PREFIX}} MATCHES ${PREFIX}-NOTFOUND)
        if(${REQUIRED})
            set(${PREFIX}_STATUS "${_escape}[1;31m-- not found${_escape}[0m but desired ...")
        else()
            set(${PREFIX}_STATUS "${_escape}[0;31m- not found${_escape}[0m and not desired")
        endif()
    else()
        if(${REQUIRED})
            set(${PREFIX}_STATUS "${_escape}[1;32m+ used${_escape}[0m ( ${${PREFIX}} )")
        else()
            set(${PREFIX}_STATUS "${_escape}[0;32m? available${_escape}[0m but not desired ( ${${PREFIX}} )")
        endif()
    endif()
endmacro()
