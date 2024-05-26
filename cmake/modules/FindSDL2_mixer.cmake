find_path (
    SDL2_MIXER_INCLUDE_DIR SDL_mixer.h
    HINTS ${SDL2} $ENV{SDL2} $ENV{SDL2_MIXER}
    PATH_SUFFIXES include/SDL2 include SDL2 i686-w64-mingw32/include/SDL2
                  x86_64-w64-mingw32/include/SDL2
    PATHS ~/Library/Frameworks
          /Library/Frameworks
          /usr/local/include/SDL2
          /usr/include/SDL2
          /sw # Fink
          /opt/local # DarwinPorts
          /opt/csw # Blastwave
          /opt
          "${PROJECT_SOURCE_DIR}/build/win64/SDL2_mixer" # win32
    )

# Lookup the 64 bit libs on x64
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_library (
        SDL2_MIXER_LIBRARY_TEMP
        NAMES SDL2_mixer
        HINTS ${SDL2} $ENV{SDL2} $ENV{SDL2_MIXER}
        PATH_SUFFIXES lib64 lib lib/x64 x86_64-w64-mingw32/lib
        PATHS /sw /opt/local /opt/csw /opt
              "${PROJECT_SOURCE_DIR}/build/win64/SDL2_mixer" # win32
        )
    # On 32bit build find the 32bit libs
else (CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_library (
        SDL2_MIXER_LIBRARY_TEMP
        NAMES SDL2_mixer
        HINTS ${SDL2} $ENV{SDL2} $ENV{SDL2_MIXER}
        PATH_SUFFIXES lib lib/x86 i686-w64-mingw32/lib
        PATHS /sw /opt/local /opt/csw /opt
              "${PROJECT_SOURCE_DIR}/build/win64/SDL2_mixer" # win32
        )
endif (CMAKE_SIZEOF_VOID_P EQUAL 8)

set (SDL2_MIXER_FOUND OFF BOOL)
if (SDL2_MIXER_LIBRARY_TEMP)
    # Set the final string here so the GUI reflects the final state.
    set (
        SDL2_MIXER_LIBRARY
        ${SDL2_MIXER_LIBRARY_TEMP}
        CACHE STRING "Where the SDL2_mixer Library can be found"
        )
    # Set the temp variable to INTERNAL so it is not seen in the CMake GUI
    set (
        SDL2_MIXER_LIBRARY_TEMP
        "${SDL2_MIXER_LIBRARY_TEMP}"
        CACHE INTERNAL ""
        )
    set (SDL2_MIXER_FOUND ON BOOL)
endif (SDL2_MIXER_LIBRARY_TEMP)

include (FindPackageHandleStandardArgs)

find_package_handle_standard_args (
    SDL2_MIXER REQUIRED_VARS SDL2_MIXER_LIBRARY SDL2_MIXER_INCLUDE_DIR
    )
