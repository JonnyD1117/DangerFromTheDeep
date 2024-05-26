
cmake_minimum_required (VERSION 3.22)


# standard project definitions and versions
set (DFTD_VERSION_MAJOR 0)
set (DFTD_VERSION_MINOR 4)
set (DFTD_VERSION_PATCH 0)
set (DFTD_VERSION_RELEASE_TYPE "dev")  

project (DFTD
         VERSION 
         ${DFTD_VERSION_MAJOR}.${DFTD_VERSION_MINOR}.${DFTD_VERSION_PATCH}
         LANGUAGES CXX)

configure_file (${PROJECT_SOURCE_DIR}/src/version.h.in
                ${PROJECT_SOURCE_DIR}/src/version.hpp)

# FFMPEG | FFTW3 | SDL2 | SDL2_image | SDL2_mixer | GL | GLU | GLEW | OpenGL

# language options
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF) 
set (CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)
              

# Add extra cmake modules
include (FindPkgConfig)

set (CMAKE_MODULE_PATH
     "${PROJECT_SOURCE_DIR}/cmake/modules"
     "${PROJECT_SOURCE_DIR}/cmake")   


# use ccache IF found
find_program (CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property (GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif ()   


# organize targets into folders
option (CMAKE_USE_FOLDERS
        "Use the FOLDER target property to organize targets into folders." ON)

mark_as_advanced (CMAKE_USE_FOLDERS)

if (CMAKE_USE_FOLDERS)
    set_property (GLOBAL PROPERTY USE_FOLDERS ON)
endif ()


# option details, output control
set (DFTD_VERBOSE ON CACHE BOOL "Extra compiler messages when building.")
set (DFTD_BUILD_LIBCPLUSPLUS OFF CACHE BOOL "Compile with clang libc++.")
set (DFTD_BUILD_TOOLS OFF CACHE BOOL "Build dftd extra developer tools.")
set (DFTD_BUILD_TESTS OFF CACHE BOOL "Build dftd code tests.")

set (DFTD_HIDE_SYMBOLS OFF CACHE BOOL
     "Hide internal symbols that aren't oficially external")

set (DFTD_USE_IPO OFF CACHE BOOL 
     "Use Interprocedural and Link-Time Optimization")

set (DFTD_STOP_ON_WARNING OFF CACHE BOOL 
     "Stop building IF there are any compiler warnings")

set (DFTD_COLOR_MAKEFILE ON CACHE BOOL
     "Enable colored compiler warnings, GCC, Clang only")

set (DFTD_BUILD_DOCS OFF CACHE BOOL "Build developer documentation")


# FFTW options
set (FFTW_USE_FLOAT ON CACHE BOOL 
     "Use single or double precision FFTW3, pending availability")

set (FFTW_USE_STATIC_LIBS OFF CACHE BOOL
     "Use FFTW static libraries instead of shared.")  

if (FFTW_USE_FLOAT)
    add_compile_definitions (WITH_FLOAT_FFTW)
endif ()


# Other build options, clang tools, formatting, developer tools

set (USE_LIB64_SUFFIX ON CACHE BOOL 
     "Use lib64 instead of lib on x86_64 systems")  

set (USE_EFENCE OFF CACHE BOOL "Use Electric fence, UNIX only.")

set (USE_CLANG_FORMAT OFF CACHE BOOL 
     "Use clang-format, ensure strict code formatting.")

set (USE_CPPCHECK OFF CACHE BOOL "Use CPPcheck static analysis tool")
set (USE_CPPCHECK_HTML_REPORT OFF CACHE BOOL "Output cppcheck to HTML file")


# CPP check static analysis
if (USE_CPPCHECK)
    find_program (CMAKE_CXX_CPPCHECK NAMES cppcheck
                  HINTS /usr
                        /usr/local
                        /opt
                  PATH_SUFFIXES bin)

    if (CMAKE_CXX_CPPCHECK)

        set (CPPCHECK_TEMPLATE "cppcheck:{file}:{line}: [{severity}][{id}] {message}\\n{code}\\n{callstack}")

        list (APPEND CMAKE_CXX_CPPCHECK
            "--enable=all"
            "--inline-suppr"
            #"--force"
            "--language=c++"
            "--library=opengl,posix,zlib,std"
            "--template=${CPPCHECK_TEMPLATE}"
            "--suppressions-list=${CMAKE_SOURCE_DIR}/.cppcheck_suppressions.txt"
            "--std=c++17"
            )

        if (DFTD_VERBOSE)
            list (APPEND CMAKE_CXX_CPPCHECK
                #"--verbose"
                "--max-ctu-depth=1")
        else ()
            list (APPEND CMAKE_CXX_CPPCHECK "--quiet")
        endif()

        list (APPEND CMAKE_CXX_CPPCHECK
            "--xml"
            "--xml-version=2"
            "--output-file=${CMAKE_BINARY_DIR}/cppcheck_report.xml")
        
    else ()
        message (WARNING "CPPcheck NOT found.") 
    endif ()

endif ()


# set property for 64bits unices, debian does not follow the convention and
# puts 32bits libraries in lib32/ whilst symlinking lib64/ to lib/
if (UNIX)
    if (USE_LIB64_SUFFIX)
        set (FIND_LIBRARY_USE_LIB64_PATHS ON)
    else ()
        set (FIND_LIBRARY_USE_LIB64_PATHS OFF)
    endif ()
endif ()


# status, directories, disable in-source builds
if (DFTD_VERBOSE)
    message (STATUS "Project source dir = ${PROJECT_SOURCE_DIR}")
endif ()
message (STATUS "Project build dir = ${CMAKE_BINARY_DIR}")

if ("${PROJECT_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message (FATAL_ERROR "Not allowed to run in-source builds!")
endif ()


# IPO/LTO control, instead of passing -flto, allow in DEBUG mode ?
if (DFTD_USE_IPO)
    include (CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES CXX) 

    if (ipo_supported)
        message (STATUS "IPO/LTO supported and enabled")
        set (CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else ()
        message (WARNING "IPO/LTO unsupported, error = \"${ipo_error}\"")
        set (CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
    endif ()
endif ()


# set cmake build verbosity and compiler coloring options
set (CMAKE_VERBOSE_MAKEFILE ${DFTD_VERBOSE})

if (DFTD_COLOR_MAKEFILE)
    set (CMAKE_COLOR_MAKEFILE ON)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
        CMAKE_CXX_COMPILER_VERSION GREATER 4.9)
        add_compile_options (-fdiagnostics-color=always)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options (-fcolor-diagnostics)
    endif ()
else ()
    set (CMAKE_COLOR_MAKEFILE OFF)
endif ()


# Add build types and drop-down menu with the build types
set (CMAKE_CONFIGURATION_TYPES 
     "Pedantic;Debug;Release;RelWithDebInfo;MinSizeRel;Profile" 
     CACHE STRING "" FORCE)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release
         CACHE STRING "Choose type type of build, the options are:
         ${CMAKE_CONFIGURATION_TYPES}" FORCE
         )
    set_property (CACHE CMAKE_BUILD_TYPE 
                  PROPERTY STRINGS ${CMAKE_CONFIGURATION_TYPES}
                  )
endif ()


# we can add definitions for other compilers platforms
if (CMAKE_BUILD_TYPE STREQUAL "Pedantic" OR
    CMAKE_BUILD_TYPE STREQUAL "Debug" OR
    CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set (DEBUGMODE ON)
    add_compile_definitions (DEBUG)
else ()
    add_compile_definitions (NDEBUG)
endif ()


# this would require set CMAKE_CXX_<BUILDTYPE>_FLAGS but this
# might be better done with cmake build toolchains and specify them here,
# if we can override with CMAKE_BUILD_TOOLCHAIN_FILE
#
if (CMAKE_BUILD_TYPE STREQUAL "Pedantic")
    if (UNIX AND CMAKE_COMPILER_IS_GNUCXX)
        add_compile_options (-O0 -g3 -ggdb3 -Wall -Wextra -pedantic)
    endif ()
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (UNIX AND CMAKE_COMPILER_IS_GNUCXX)
        add_compile_options (-O0 -g2 -ggdb)
    endif ()
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (UNIX AND CMAKE_COMPILER_IS_GNUCXX)
        add_compile_options (-O3)
    endif ()
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if (UNIX AND CMAKE_COMPILER_IS_GNUCXX)
        add_compile_options (-O3 -g1)
    endif ()
elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    if (UNIX AND CMAKE_COMPILER_IS_GNUCXX)
        add_compile_options (-Os)
    endif ()
elseif (CMAKE_BUILD_TYPE STREQUAL "Profile")
    if (UNIX AND CMAKE_COMPILER_IS_GNUCXX)
        add_compile_options (-O3 -g1 -pg)
    endif ()
else ()
    message (FATAL_ERROR "Unsupported build type!")
endif ()


if (NOT MSVC)
    if (DFTD_VERBOSE)
        add_compile_options (-Wall)
    endif ()
    if (DFTD_STOP_ON_WARNING)
        add_compile_options (-Werror)
    endif ()
endif ()


# figure out which compiler we're using
# https://cmake.org/cmake/help/v3.12/variable/CMAKE_LANG_COMPILER_ID.html
if (DFTD_VERBOSE)
    message (WARNING "Using \"${CMAKE_CXX_COMPILER_ID}\"
    version \"${CMAKE_CXX_COMPILER_VERSION}\""
    )
endif ()

# Common options to GCC and Clang
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"
    )
    # Cmake doesn't know what to do with include_directories(SYSTEM) in
    # these cases
    set (CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ")

    # Ensure this macro is SET for stdint.h
    add_compile_definitions (__STDC_LIMIT_MACROS)

    # this allows native instructions to be used for sqrtf instead of a 
    # function call
    add_compile_options (-fno-math-errno)

    if (DFTD_HIDE_SYMBOLS AND NOT DEBUGMODE)
        set (CMAKE_CXX_VISIBILITY_PRESET hidden)
        set (CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)
    endif ()
    
endif ()


# Clang-specific options
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR 
    CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"
    )
    # disable some warnings for clang
    add_compile_options (-Wno-unused-function)
    add_compile_options (-Wno-overloaded-virtual)
    add_compile_options (-Wno-unneeded-internal-declaration)
    add_compile_options (-Wno-tautological-compare)
    add_compile_options (-Wno-unused-private-field)

    # disable warning about unused command line arguments
    add_compile_options (-Qunused-arguments)
    
    # don't warn IF we ask it not to warn about warnings it doesn't know
    add_compile_options (-Wunknown-warning-option)
    add_compile_options (-Wno-unknown-pragmas)

    if (DEBUGMODE)
        add_compile_options (-Wno-unused-function -Wno-unused-variable)
    endif ()

    if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
         CMAKE_CXX_COMPILER_VERSION GREATER 3.5) OR
        (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND
         CMAKE_CXX_COMPILER_VERSION GREATER 6.1))
        add_compile_options (-Wno-unused-local-typedefs)
    endif ()

endif () 

if (USE_LIBCPLUSPLUS AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message (WARNING "Using libc++")
    add_compiler_options (-stdlib=libc++)
endif ()


# try to detect IF this is an osx distribution new enough that the system
# library is libc++
if (EXISTS "/usr/lib/libc++.dylib")
     set (DFTD_SYSTEM_HAS_LIBCPP ON)
endif ()   


# GCC specific options
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND DFTD_VERBOSE)
    add_compile_options (-Wno-error=strict-overflow -Wno-unused-result)
endif ()


# windows 32/64 specific stuff
if (WIN32)

	# We don't use FindGLEW on windows, as it doesn't work.
	set (GLEW_LIBRARY 
        "${PROJECT_SOURCE_DIR}/build/win64/glew/lib/x64/glew32.lib")

	set (GLEW_INCLUDE_DIR 
        "${PROJECT_SOURCE_DIR}/build/win64/glew/include")
    
	include_directories (${GLEW_INCLUDE_DIR})
    set (LIBS ${LIBS} ${GLEW_LIBRARY})

	add_compile_definitions ("-DNOMINMAX")
	message (INFO " : CMake uses windows platform: " ${CMAKE_VS_PLATFORM_NAME})

	set (CMAKE_EXE_LINKER_FLAGS "/machine:x64")	# fixme try

	if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		message (INFO "Building Win64 (x64) version.")
	else ()
		message (INFO "Building Win32 (x86) version.")
	endif ()

endif ()


# Test Endianess of System
include (TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)

if (IS_BIG_ENDIAN)
	add_compile_definitions (BIG_ENDIAN)
else ()
	add_compile_definitions (LITTLE_ENDIAN)
endif ()


# extra arguments
set (EXTRA_CPP_ARGS "" CACHE STRING "Extra C++ command line options.")

if (EXTRA_CPP_ARGS)
    message (STATUS "Extra C++ args: ${EXTRA_CPP_ARGS}")
    add_compile_options (${EXTRA_CPP_ARGS})
endif ()
           

include (platform)
include (externalpackages)


# paths for installation customization
# some checks might be needed for other platforms
#
set (DEFAULT_INSTALL_PREFIX     "/usr/local")
set (DEFAULT_BIN_INSTALL_DIR    "bin")
set (DEFAULT_DATA_INSTALL_DIR   "share/dangerdeep")

# build version from branch and hash
# this might require some work for osx, and specially windows
#
execute_process (COMMAND git rev-parse --abbrev-ref HEAD
                         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                         OUTPUT_VARIABLE GIT_BRANCH
                         OUTPUT_STRIP_TRAILING_WHITESPACE
                         )

execute_process (COMMAND git log -1 --format=%h
                         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                         OUTPUT_VARIABLE GIT_COMMIT_HASH
                         OUTPUT_STRIP_TRAILING_WHITESPACE
                         )

# add data location and version
string (CONCAT DFTD_VERSION ${GIT_BRANCH} "-" ${GIT_COMMIT_HASH})

string (CONCAT DFTD_DATADIR ${DEFAULT_INSTALL_PREFIX} "/"
                             ${DEFAULT_DATA_INSTALL_DIR} "/")

#if (WIN32)
	string (CONCAT DFTD_DATADIR ${PROJECT_SOURCE_DIR} "/data/")
#endif ()


# we define it twice because old build system uses the definitions without the DFTD_ prefix
add_definitions ("-DDFTD_VERSION=\"${DFTD_VERSION}\"" "-DVERSION=\"${DFTD_VERSION}\"")
add_definitions ("-DDFTD_DATADIR=\"${DFTD_DATADIR}\"" "-DDATADIR=\"${DFTD_DATADIR}\"")


# Documentation
if (DFTD_BUILD_DOCS)

    # Developer documentation comprised of doxygen and TeX docs.

    find_package (Doxygen REQUIRED dot)

    if (DOXYGEN_FOUND AND DOXYGEN_VERSION GREATER 1.7)
        set (DOXYGEN_BUILTIN_STL_SUPPORT TRUE)
        set (DOXYGEN_QUIET TRUE)
        set (DOXYGEN_WARN_IF_UNDOCUMENTED TRUE)
        set (DOXYGEN_RECURSIVE TRUE)
        set (DOXYGEN_SOURCE_BROWSER TRUE)
        set (DOXYGEN_REFERENCES_RELATION TRUE)
        set (DOXYGEN_ALPHABETICAL_INDEX TRUE)
        set (DOXYGEN_GENERATE_TREEVIEW TRUE)
        set (DOXYGEN_CLASS_GRAPH TRUE)
        set (DOXYGEN_CALLER_GRAPH TRUE)
        set (DOXYGEN_CALL_GRAPH TRUE)
        set (DOXYGEN_EXTRACT_ALL TRUE)
        set (DOXYGEN_EXTRACT_PRIVATE TRUE)
        set (DOXYGEN_EXTRACT_STATIC TRUE)
        set (DOXYGEN_SUBGROUPINT TRUE)
        set (DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/doc/doxygen)
        set (DOXYGEN_SOURCE_BROWSER TRUE)
        set (DOXYGEN_DOT_GRAPH_MAX_NODES 150)
        set (DOXYGEN_DOT_INTERACTIVE_SVG FALSAE)

        find_package (Clang)
        if (CLANG_FOUND AND CMAKE_EXPORT_COMPILE_COMMANDS)
            set (DOXYGEN_CLANG_ASSISTED_PARSING TRUE)
            set (DOXYGEN_CLANG_DATABASE_PATH ${CMAKE_BINARY_DIR})
        else ()
            set (WARNING "Doxygen couldn't use Clang assisted parsing or compilation database was disabled.")
        endif ()

        doxygen_add_docs (doxygendocs
            ${CMAKE_SOURCE_DIR}/src
            ALL
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
            COMMENT "Building doxygen documentation")
    else ()
        message (WARNING "Doxygen NOT found, skipping doxygen docs.")
    endif ()

    # Finally build the PDF from tex files
    find_package (LATEX COMPONENTS PDFLATEX)

    if (LATEX_FOUND AND PDFLATEX_COMPILER)

        set (LATEX_OUTPUT_PATH ${CMAKE_BINARY_DIR}/doc)

        add_custom_target (texdocs_prebuild
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/doc
            COMMAND ${PDFLATEX_COMPILER} -output-directory
                ${LATEX_OUTPUT_PATH} -draft-mode -interaction=nonstopmode
                --no-file-line-error
                ${CMAKE_SOURCE_DIR}/doc/tex/codeguide.tex
            COMMAND ${PDFLATEX_COMPILER} -output-directory
                ${LATEX_OUTPUT_PATH} -draft-mode -interaction=nonstopmode
                --no-file-line-error
                ${CMAKE_SOURCE_DIR}/doc/tex/sonar.tex
            COMMAND ${PDFLATEX_COMPILER} -output-directory
                ${LATEX_OUTPUT_PATH} -draft-mode -interaction=nonstopmode
                --no-file-line-error
                ${CMAKE_SOURCE_DIR}/doc/tex/translation.tex    
            DEPENDS ${CMAKE_SOURCE_DIR}/doc/tex/codeguide.tex
                    ${CMAKE_SOURCE_DIR}/doc/tex/sonar.tex
                    ${CMAKE_SOURCE_DIR}/doc/tex/translation.tex
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc/tex
            COMMENT "Prebuilding tex to PDF documents.")

        add_custom_target (texdocs_build
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/doc
            COMMAND ${PDFLATEX_COMPILER} -output-directory ${LATEX_OUTPUT_PATH}
                ${CMAKE_SOURCE_DIR}/doc/tex/codeguide.tex
            COMMAND ${PDFLATEX_COMPILER} -output-directory ${LATEX_OUTPUT_PATH}
                ${CMAKE_SOURCE_DIR}/doc/tex/sonar.tex
            COMMAND ${PDFLATEX_COMPILER} -output-directory ${LATEX_OUTPUT_PATH}
                ${CMAKE_SOURCE_DIR}/doc/tex/translation.tex    
            DEPENDS ${CMAKE_SOURCE_DIR}/doc/tex/codeguide.tex
                    ${CMAKE_SOURCE_DIR}/doc/tex/sonar.tex
                    ${CMAKE_SOURCE_DIR}/doc/tex/translation.tex  
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc/tex
            COMMENT "Building tex to PDF documents.")   

        add_custom_target (texdocs ALL)
        add_dependencies (texdocs texdocs_build)    

    else ()
        message (WARNING "Latex and/or pdflatex compiler NOT found. Skipping.")
    endif ()

endif ()


# install tree
#
if (UNIX AND NOT SELF_CONTAINED_INSTALL_TREE)
    # assume standard filesystem directory hierarchy
    set (DEFAULT_DOC_INSTALL_DIR "share/doc/dangerdeep")
    set (DEFAULT_MAN_INSTALL_DIR "share/man/dangerdeep")
endif ()

if (EXEC_INSTALL_PREFIX)
    set (DEFAULT_BIN_INSTALL_DIR
         "${EXEC_INSTALL_PREFIX}/${DEFAULT_BIN_INSTALL_DIR}")
endif ()


# Tell cmake to process the sub-directories
add_subdirectory (src)

#if ( DFTD_BUILD_TOOLS )
#    add_subdirectory (src/tools)
#endif ()


# Custom post-build stages
#
# 1) cppcheck if enabled, has the option to produce a XML file for
#    cppcheck-html to generate a HTML report
#
if (USE_CPPCHECK AND CMAKE_CXX_CPPCHECK AND USE_CPPCHECK_HTML_REPORT)

    find_program (CPPCHECK_HTMLREPORT_BIN cppcheck-htmlreport
                  HINTS /usr
                        /usr/local
                        /opt
                  PATH_SUFFIXES bin)

    if (CPPCHECK_HTMLREPORT_BIN)

        add_custom_command (OUTPUT cppcheck_report.html
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/cppc
            COMMAND cppcheck-htmlreport 
                --title=${PROJECT_NAME}
                --file=${CMAKE_BINARY_DIR}/cppcheck_report.xml
                --source-dir=${CMAKE_SOURCE_DIR}
                --report-dir=${CMAKE_BINARY_DIR}/cppc
            DEPENDS build
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Custom post-built for cppcheck HTML report")

        add_custom_target (cppcheck_html_report ALL
            DEPENDS ${CMAKE_BINARY_DIR}/cppcheck_report.xml
            cppcheck_report.html)

        add_dependencies (cppcheck_html_report dangerdeep)

    else ()
        message (WARNING "cppcheck-htmlreport NOT found.")
    endif ()

endif ()


# Packaging
set (CPACK_PACKAGE_VERSION_MAJOR ${DFTD_VERSION_MAJOR})
set (CPACK_PACKAGE_VERSION_MINOR ${DFTD_VERSION_MINOR})
set (CPACK_PACKAGE_VERSION_PATCH ${DFTD_VERSION_PATCH})
set (CPACK_PACKAGE_VERSION_RELEASE_TYPE ${DFTD_VERSION_RELEASE_TYPE})
set (CPACK_PACKAGE_VENDOR "Atlantic Cods Inc")

# License
file (COPY "${PROJECT_SOURCE_DIR}/LICENSE.md" DESTINATION "${CMAKE_BINARY_DIR}")
set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/LICENSE.md")

#file (COPY "${PROJECT_SOURCE_DIR}/README" DESTINATION "${CMAKE_BINARY_DIR}")
#file (RENAME "${CMAKE_BINARY_DIR}/README" "${CMAKE_BINARY_DIR}/Readme.txt")
#set (CPACK_RESOURCE_FILE_README "${CMAKE_BINARY_DIR}/Readme.txt")

# common package options
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set (CPACK_GENERATOR "TGZ;STGZ;RPM;DEB")
    set (CPACK_SOURCE_GENERATOR "TGZ")
endif ()

if (APPLE)
    set (CPACK_GENERATOR "TGZ;STGZ;PackageMaker")
    set (CPACK_SOURCE_GENERATOR "TGZ")
endif ()

# for packaing use major.minor.patch versioning
#
set (CPACK_SOURCE_PACKAGE_FILE_NAME 
     DFTD-${DFTD_VERSION_MAJOR}.${DFTD_VERSION_MINOR}.${DFTD_VERSION_PATCH}-source)

set (CPACK_SOURCE_IGNORE_FILES ".*~*")
include (CPack)

# clang-format as target
include(cmake/clang-dev-tools.cmake)
