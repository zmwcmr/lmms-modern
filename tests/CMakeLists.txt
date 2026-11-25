include(CTest)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

set(LMMS_TESTS
	src/core/ArrayVectorTest.cpp
	src/core/DelayLineTest.cpp
	src/core/AudioBufferTest.cpp
	src/core/AutomatableModelTest.cpp
	src/core/MathTest.cpp
	src/core/ProjectVersionTest.cpp
	src/core/RelativePathsTest.cpp
	src/core/SongAutomationTest.cpp
	src/core/TimelineTest.cpp
	src/core/ValidationTest.cpp
	src/tracks/AutomationTrackTest.cpp
	src/tracks/InstrumentTrackTest.cpp
	src/tracks/MidiClipTest.cpp
)

if(LMMS_HAVE_LV2)
	list(APPEND LMMS_TESTS src/core/Lv2PresetTest.cpp)
endif()

if(LMMS_BUILD_LINUX AND WANT_VST)
	add_library(FakeMidiVst SHARED plugins/fake-midi-vst.cpp)
	target_include_directories(FakeMidiVst PRIVATE ${CMAKE_SOURCE_DIR}/include)
	target_compile_features(FakeMidiVst PRIVATE cxx_std_17)
	set_target_properties(FakeMidiVst PROPERTIES
		PREFIX ""
		OUTPUT_NAME "fake-midi-vst"
	)
endif()

foreach(LMMS_TEST_SRC IN LISTS LMMS_TESTS)
	# TODO CMake 3.20: Use cmake_path
	get_filename_component(LMMS_TEST_NAME ${LMMS_TEST_SRC} NAME_WE)

	add_executable(${LMMS_TEST_NAME} ${LMMS_TEST_SRC})
	add_test(NAME ${LMMS_TEST_NAME} COMMAND ${LMMS_TEST_NAME})

	# TODO CMake 3.12: Propagate usage requirements by linking to lmmsobjs
	target_include_directories(${LMMS_TEST_NAME} PRIVATE $<TARGET_PROPERTY:lmmsobjs,INCLUDE_DIRECTORIES>)

	target_static_libraries("${LMMS_TEST_NAME}" PRIVATE lmmsobjs)
	target_link_libraries(${LMMS_TEST_NAME} PRIVATE
		${QT_LIBRARIES}
		${QT_QTTEST_LIBRARY}
	)

	target_compile_features(${LMMS_TEST_NAME} PRIVATE cxx_std_20)
	target_compile_definitions(${LMMS_TEST_NAME} PRIVATE LMMS_TESTING)
endforeach()
