
if (NOT DFTD_VERBOSE)
    set (FFTW3_FIND_QUIETLY TRUE)
    set (OpenGL_FIND_QUIETLY TRUE)
    set (OpenMP_FIND_QUIETLY TRUE)
    set (PkgConfig_FIND_QUIETLY TRUE)
    set (TinyXML_FIND_QUIETLY TRUE)
    set (ZLIB_FIND_QUIETLY TRUE)
    set (SDL_FIND_QUIETLY TRUE)
    set (SDL_IMAGE_FIND_QUIETLY TRUE)
    set (SDL_MIXER_FIND_QUIETLY TRUE)
    set (SDL_TTF_FIND_QUIETLY TRUE)
endif ()


# FFTW3, default is to use single, module provides double, long double,
# MPI and OpenMP variants.

if (FFTW_USE_FLOAT)
    set (_FFTW_PRECISION FLOAT_LIB)
else ()
    set (_FFTW_PRECISION DOUBLE_LIB)
endif ()

find_package (FFTW REQUIRED ${_FFTW_PRECISION})

if (FFTW_FOUND)
    message (STATUS "FFTW3 found.")

    include_directories (${FFTW3_INCLUDE_DIR})

    if (FFTW_USE_FLOAT)
        set (LIBS ${LIBS} ${FFTW_FLOAT_LIB})
    else ()
        set (LIBS ${LIBS} ${FFTW_DOUBLE_LIB})
    endif ()
    set (LIBS ${LIBS} ${FFTW_LIBRARIES})

else ()
    message (FATAL_ERROR "FFTW3 not found! This will fail.")
endif ()


# OpenGL, components: EGL | GLX | OpenGL
# OpenGL_GL_PREFERENCE: GLVND | LEGACY
#
set (OpenGL_GL_PREFERENCE LEGACY)

find_package (OpenGL REQUIRED COMPONENTS GLX OpenGL)
if (OPENGL_FOUND)
    message (STATUS 
    "OpenGL found: \"${OPENGL_gl_LIBRARY}\", \"${OPENGL_opengl_LIBRARY}\"")

    if (OPENGL_XMESA_FOUND)
        message (STATUS "OpenGL XMesa found.")
    else ()
        message (WARNING "OpenGL XMesa NOT found.")
    endif ()

    if (OPENGL_GLU_FOUND)
        message (STATUS "OpenGL GLU found: \"${OPENGL_glu_LIBRARY}\"")
    else ()
        message (WARNING "OpenGL GLU NOT found.")
    endif ()

    if (OPENGL_GLX_FOUND)
        message (STATUS "OpenGL GLX found: \"${OPENGL_glx_LIBRARY}\"")
    else ()
        message (WARNING "OpenGL GLX NOT found.")
    endif ()    
    
    if (OPENGL_EGL_FOUND)
        message (STATUS "OpenGL EGL found: \"${OPENGL_egl_LIBRARY}\"")
        include_directories (${OPENGL_EGL_INCLUDE_DIRS})
    else ()
        message (WARNING "OpenGL EGL NOT found.")
    endif ()       

    include_directories (${OPENGL_INCLUDE_DIR})
    set (LIBS ${LIBS} ${OPENGL_LIBRARIES})

    # add GL directory to find glu.h ?
    find_path (OPENGL_INCLUDE_SUBDIR
               NAMES gl.h
               HINTS ${OPENGL_INCLUDE_DIR}
               PATH_SUFFIXES GL OpenGL
               NO_DEFAULT_PATH
               )
    include_directories (${OPENGL_INCLUDE_SUBDIR})

else ()
    message (FATAL_ERROR "OpenGL NOT found, this will fail.")
endif ()


# GLEW, accepts GLEW_VERBOSE, GLEW_USE_STATIC_LIBS
#
find_package (GLEW REQUIRED)
if (GLEW_FOUND)
    if (GLEW_VERBOSE)
        message (STATUS "Found GLEW \"${GLEW_VERSION}\"")
    else ()
        message (STATUS "GLEW found.")
    endif ()

    include_directories (${GLEW_INCLUDE_DIRS})

    if (GLEW_USE_STATIC_LIBS)
        set (LIBS ${LIBS} ${GLEW_STATIC_LIBRARIES})
    else ()
        set (LIBS ${LIBS} ${GLEW_SHARED_LIBRARIES})
    endif ()

else ()
    message (FATAL_ERROR "FFTW3 not found! This will fail.")
endif ()  


# SDL2, requires Image, TTF, Mixer (net unused atm)
#
find_package (SDL2 REQUIRED)
if (SDL2_FOUND)
    message (STATUS "SDL2 found.")
    include_directories (${SDL2_INCLUDE_DIR})
    set (LIBS ${LIBS} ${SDL2_LIBRARY})
else ()
    message (FATAL_ERROR "SDL2 not found! This will fail.")
endif ()


# SDL_image
#
find_package (SDL2_image REQUIRED)
if (SDL2_IMAGE_FOUND)
    message (STATUS "SDL2_image found.")
    include_directories (${SDL2_IMAGE_INCLUDE_DIR})
    set (LIBS ${LIBS} ${SDL2_IMAGE_LIBRARY})
else ()
    message (FATAL_ERROR "SDL2_image not found! This will fail.")
endif ()


# SDL_mixer
#
find_package (SDL2_mixer REQUIRED)
if (SDL2_MIXER_FOUND)
    message (STATUS "SDL2_mixer found.")
    include_directories (${SDL2_MIXER_INCLUDE_DIR})
    set (LIBS ${LIBS} ${SDL2_MIXER_LIBRARY})
else ()
    message (FATAL_ERROR "SDL2_mixer not found! This will fail.")
endif ()   


# bzip2 package
#
find_package ( BZip2 REQUIRED )
if ( BZIP2_FOUND )
        message ( STATUS "BZip2 found." )
        include_directories ( ${BZIP2_INCLUDE_DIR} )
        set ( LIBS ${LIBS} ${BZIP2_LIBRARY} )
else ()
        message ( FATAL_ERROR "BZip2 not found! This will fail." )
endif ()  


# check electric fence option and system presence
#
if (USE_EFENCE)
    if (UNIX)
        find_library (EFENCE_LIBRARY NAMES efence
                                     DOC "The Electric Fence library"
                                     HINTS "/usr/lib"
                                           "/usr/lib64"
                                           "/usr/efence/lib"
                                           "/usr/share/efence/lib"
                                            )
        if (EFENCE_LIBRARY)
            message (STATUS "ElectricFence found, linking with ${PROJECT_NAME}")
            set (LIBS ${LIBS} ${EFENCE_LIBRARY})
        else ()
            message (WARNING "ElectricFence not found! User option ignored.")
        endif ()
    else ()
        message (WARNING "Not on UNIXes")
    endif ()
endif ()  


# FFMPEG for videoplaytest
#
if (DFTD_BUILD_TOOLS AND DFTD_BUILD_VIDEO_TEST)
    find_package (FFMPEG REQUIRED)
    if (FFMPEG_FOUND)
        message (STATUS "FFMPEG Found: " ${FFMPEG_INCLUDE_DIR})
        include_directories (${FFMPEG_INCLUDE_DIR})
        set (LIBS ${LIBS} ${FFMPEG_LIBRARY})
    else ()
        message (WARNING "FFMPEG not found! Skipping videoplaytest utility.")
    endif ()
endif ()

