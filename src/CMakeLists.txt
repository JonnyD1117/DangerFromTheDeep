# Main dangerdeep

add_compile_definitions (TIXML_USE_STL)


# Also build library for tinyxml
add_subdirectory (tinyxml)


# and for oglExt
add_subdirectory (oglext)

# add libraries
add_subdirectory(base)
add_subdirectory(core)
add_subdirectory(media)
add_subdirectory(tests)
add_subdirectory(tools)
add_subdirectory(ui)

# DFTD all
# TODO: distribute correctly,cfg/keys need global_data and that needs lot more
add_library (dftdall STATIC
	global_data.cpp
	global_data.hpp
	dftdtester/tests.cpp
)
target_link_libraries(dftdall dftdmedia)
target_include_directories(dftdall PRIVATE ./media/)

# fixme need to link dftdall for add_loading_screen in global_data etc.

set (dangerdeep_source
	subsim.cpp
	credits.cpp
	credits.hpp
)
add_executable (dangerdeep ${dangerdeep_source})
target_link_libraries (dangerdeep ${LIBS} ${CMAKE_DL_LIBS} dftdui dftdall)

# fixme have rather subdirectory for tests, tools and the remaining src
# files!
