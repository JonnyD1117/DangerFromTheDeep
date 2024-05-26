# * Try to find FFMPEG Once done this will define FFMPEG_FOUND - System has
#   FFMPEG FFMPEG_INCLUDE_DIRS - The FFMPEG INCLUDE directories FFMPEG_LIBRARIES
#   - The libraries needed to use FFMPEG FFMPEG_LIBRARY_DIRS - The directory to
#   find FFMPEG libraries
#
# written by Roy Shilkrot 2013 http://www.morethantechnical.com/
#

find_package (PkgConfig)

macro (FFMPEG_FIND varname shortname headername)

    if (NOT WIN32)
        pkg_check_modules (PC_${varname} ${shortname})

        find_path (
            ${varname}_INCLUDE_DIR "${shortname}/${headername}"
            HINTS ${PC_${varname}_INCLUDEDIR} ${PC_${varname}_INCLUDE_DIRS}
            NO_DEFAULT_PATH
            )
    else ()
        find_path (${varname}_INCLUDE_DIR "${shortname}/${headername}")
    endif ()

    if (${varname}_INCLUDE_DIR STREQUAL "${varname}_INCLUDE_DIR-NOTFOUND")
        message (STATUS "look for newer strcture")
        if (NOT WIN32)
            pkg_check_modules (PC_${varname} "lib${shortname}")

            find_path (
                ${varname}_INCLUDE_DIR "lib${shortname}/${headername}"
                HINTS ${PC_${varname}_INCLUDEDIR} ${PC_${varname}_INCLUDE_DIRS}
                NO_DEFAULT_PATH
                )
        else ()
            find_path (${varname}_INCLUDE_DIR "lib${shortname}/${headername}")
            if (${${varname}_INCLUDE_DIR} STREQUAL
                "${varname}_INCLUDE_DIR-NOTFOUND"
                )
                # Desperate times call for desperate measures
                message (STATUS "globbing...")
                file (GLOB_RECURSE ${varname}_INCLUDE_DIR
                      "/ffmpeg*/${headername}"
                      )
                message (STATUS "found: ${${varname}_INCLUDE_DIR}")
                if (${varname}_INCLUDE_DIR)
                    get_filename_component (
                        ${varname}_INCLUDE_DIR "${${varname}_INCLUDE_DIR}" PATH
                        )
                    get_filename_component (
                        ${varname}_INCLUDE_DIR "${${varname}_INCLUDE_DIR}" PATH
                        )
                else ()
                    set (${varname}_INCLUDE_DIR
                         "${varname}_INCLUDE_DIR-NOTFOUND"
                         )
                endif ()
            endif ()
        endif ()
    endif ()

    if (${${varname}_INCLUDE_DIR} STREQUAL "${varname}_INCLUDE_DIR-NOTFOUND")
        message (STATUS "Can't find includes for ${shortname}...")
    else ()
        message (
            STATUS "Found ${shortname} INCLUDE dirs: ${${varname}_INCLUDE_DIR}"
            )

        # get_directory_property(FFMPEG_PARENT DIRECTORY
        # ${${varname}_INCLUDE_DIR} PARENT_DIRECTORY)
        get_filename_component (FFMPEG_PARENT ${${varname}_INCLUDE_DIR} PATH)
        message (STATUS "Using FFMpeg dir parent as hint: ${FFMPEG_PARENT}")

        if (NOT WIN32)
            find_library (
                ${varname}_LIBRARIES
                NAMES ${shortname}
                HINTS ${PC_${varname}_LIBDIR} ${PC_${varname}_LIBRARY_DIR}
                      ${FFMPEG_PARENT}
                )
        else ()
            # find_path(${varname}_LIBRARIES "${shortname}.dll.a" HINTS
            # ${FFMPEG_PARENT})
            file (GLOB_RECURSE ${varname}_LIBRARIES
                  "${FFMPEG_PARENT}/*${shortname}.lib"
                  )
            # GLOBing is very bad... but windows sux, this is the only thing
            # that works
        endif ()

        if (${varname}_LIBRARIES STREQUAL "${varname}_LIBRARIES-NOTFOUND")
            message (STATUS "look for newer structure for library")
            find_library (
                ${varname}_LIBRARIES
                NAMES lib${shortname}
                HINTS ${PC_${varname}_LIBDIR} ${PC_${varname}_LIBRARY_DIR}
                      ${FFMPEG_PARENT}
                )
        endif ()

        if (${varname}_LIBRARIES STREQUAL "${varname}_LIBRARIES-NOTFOUND")
            message (STATUS "Can't find lib for ${shortname}...")
        else ()
            message (STATUS "Found ${shortname} libs: ${${varname}_LIBRARIES}")
        endif ()

        if (NOT ${varname}_INCLUDE_DIR STREQUAL
            "${varname}_INCLUDE_DIR-NOTFOUND"
            AND NOT ${varname}_LIBRARIES STREQUAL ${varname}_LIBRARIES-NOTFOUND
            )

            message (
                STATUS
                    "found ${shortname}: INCLUDE ${${varname}_INCLUDE_DIR} lib ${${varname}_LIBRARIES}"
                )
            set (FFMPEG_${varname}_FOUND 1)
            set (FFMPEG_${varname}_INCLUDE_DIRS ${${varname}_INCLUDE_DIR})
            set (FFMPEG_${varname}_LIBS ${${varname}_LIBRARIES})
        else ()
            message (STATUS "Can't find ${shortname}")
        endif ()

    endif ()

endmacro (FFMPEG_FIND)

ffmpeg_find (LIBAVFORMAT avformat avformat.h)
ffmpeg_find (LIBAVDEVICE avdevice avdevice.h)
ffmpeg_find (LIBAVCODEC avcodec avcodec.h)
ffmpeg_find (LIBAVUTIL avutil avutil.h)
ffmpeg_find (LIBSWSCALE swscale swscale.h)

set (FFMPEG_FOUND "NO")
if (FFMPEG_LIBAVFORMAT_FOUND
    AND FFMPEG_LIBAVDEVICE_FOUND
    AND FFMPEG_LIBAVCODEC_FOUND
    AND FFMPEG_LIBAVUTIL_FOUND
    AND FFMPEG_LIBSWSCALE_FOUND
    )

    set (FFMPEG_FOUND "YES")

    set (FFMPEG_INCLUDE_DIRS ${FFMPEG_LIBAVFORMAT_INCLUDE_DIRS})

    set (FFMPEG_LIBRARY_DIRS ${FFMPEG_LIBAVFORMAT_LIBRARY_DIRS})

    set (
        FFMPEG_LIBRARIES
        ${FFMPEG_LIBAVFORMAT_LIBS} ${FFMPEG_LIBAVDEVICE_LIBS}
        ${FFMPEG_LIBAVCODEC_LIBS} ${FFMPEG_LIBAVUTIL_LIBS}
        ${FFMPEG_LIBSWSCALE_LIBS}
        )

else ()

    message (STATUS "Could not find FFMPEG")

endif ()

message (STATUS ${FFMPEG_LIBRARIES} ${FFMPEG_LIBAVFORMAT_LIBRARIES})

include (FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set FFMPEG_FOUND to TRUE if all
# listed variables are TRUE
find_package_handle_standard_args (
    FFMPEG DEFAULT_MSG FFMPEG_LIBRARIES FFMPEG_INCLUDE_DIRS
    )

mark_as_advanced (FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARY_DIRS FFMPEG_LIBRARIES)
