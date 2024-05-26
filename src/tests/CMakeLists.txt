# Extra tools, 3d model, XML files, font conversion...
if (DFTD_BUILD_TESTS)
	add_executable (oceantest      oceantest.cpp)
	target_link_libraries (oceantest dftdmedia)

	add_executable (bsplinetest    bspline_test.cpp)
	target_link_libraries (bsplinetest dftdbasic)

	# disabled, irrelevant for DftD and doesnt compile
	#add_executable (treegentest    treegentest.cpp)
	#target_link_libraries (treegentest dftdall)

	add_executable (bvtreetest     bvtreeintersecttest.cpp)
	target_link_libraries (bvtreetest dftdmedia)

	add_executable (portal         portal.cpp)
	target_link_libraries (portal dftdmedia)

	add_executable (test_display test_display.cpp)
	target_link_libraries (test_display dftdui)

	if (FFMPEG_FOUND AND DFTD_BUILD_VIDEO_TEST)
	    add_executable (videoplay  videoplaytest.cpp)
	    target_link_libraries (videoplay dftdmedia)
	endif ()
endif ()
