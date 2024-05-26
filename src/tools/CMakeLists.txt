# Extra tools, 3d model, XML files, font conversion...
if (DFTD_BUILD_TOOLS)
	add_executable (viewmodel      viewmodel.cpp)
	target_link_libraries (viewmodel dftdui)

    	# load exported model, dump hierarchy as XML
	add_executable (modelmeasure   modelmeasure.cpp)
	target_link_libraries (modelmeasure dftdmedia)

	add_executable (map_precompute map_precompute.cpp)
	target_link_libraries (map_precompute dftdall)
endif ()
