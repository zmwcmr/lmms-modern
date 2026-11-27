include(CTest)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

set(LMMS_TESTS
	src/core/ArrayVectorTest.cpp
	src/core/DelayLineTest.cpp
	src/core/AudioBufferTest.cpp
	src/core/AutomatableModelTest.cpp
	src/core/MathTest.cpp
	src/core/ParameterCurveTest.cpp
	src/core/ProjectVersionTest.cpp
	src/core/RelativePathsTest.cpp
	src/core/SongAutomationTest.cpp
	src/core/TimelineTest.cpp
	src/core/ValidationTest.cpp
	src/core/InlineValueEditorTest.cpp
	src/gui/SideBarWorkspaceTest.cpp
	src/gui/ToolButtonTest.cpp
	src/gui/UiFeedbackTest.cpp
	src/gui/EditorShortcutsTest.cpp
	src/gui/FileBrowserTest.cpp
	src/gui/SubWindowTest.cpp
	src/gui/LcdFloatSpinBoxTest.cpp
	src/gui/DropDownInteractionTest.cpp
	src/tracks/AutomationTrackTest.cpp
	src/tracks/InstrumentTrackTest.cpp
	src/tracks/MidiClipTest.cpp
)

if(LMMS_HAVE_LV2)
	list(APPEND LMMS_TESTS src/core/Lv2PresetTest.cpp)
	list(APPEND LMMS_TESTS src/core/Lv2PortTest.cpp)
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

if(LMMS_BUILD_LINUX AND WANT_VST3 AND TARGET fake-vst3)
	set(FAKE_VST3_BUNDLE "${CMAKE_BINARY_DIR}/plugins/fake-vst3.vst3")
	set(FAKE_VST3_INSTRUMENT_ID "112233445566778899aabbccddee0001")
	set(FAKE_VST3_EFFECT_ID "112233445566778899aabbccddee0002")

	add_test(NAME vst3-scanner-fake
		COMMAND $<TARGET_FILE:lmms-vst3-scanner> --json "${FAKE_VST3_BUNDLE}"
			--output "${CMAKE_CURRENT_BINARY_DIR}/fake-scan.json")
	add_test(NAME vst3-host-fake-instrument
		COMMAND $<TARGET_FILE:vst3-host-test> "${FAKE_VST3_BUNDLE}"
			"${FAKE_VST3_INSTRUMENT_ID}")
	add_test(NAME vst3-host-fake-effect
		COMMAND $<TARGET_FILE:vst3-host-test> "${FAKE_VST3_BUNDLE}"
			"${FAKE_VST3_EFFECT_ID}" 0.5 --effect)
	add_test(NAME vst3-remote-host-fake-instrument
		COMMAND $<TARGET_FILE:vst3-remote-host-test> "${FAKE_VST3_BUNDLE}"
			"${FAKE_VST3_INSTRUMENT_ID}")
endif()

if(LMMS_BUILD_LINUX AND WANT_VST3 AND TARGET fake-vst3)
	add_executable(Vst3AutomationTest
		src/core/Vst3AutomationTest.cpp
		"${CMAKE_SOURCE_DIR}/plugins/Vst3Base/Vst3ParameterModels.cpp"
		"${CMAKE_SOURCE_DIR}/plugins/Vst3Base/Vst3Host.cpp"
		"${CMAKE_SOURCE_DIR}/plugins/Vst3Base/Vst3Module.cpp"
	)
	target_include_directories(Vst3AutomationTest PRIVATE
		$<TARGET_PROPERTY:lmmsobjs,INCLUDE_DIRECTORIES>
		"${CMAKE_SOURCE_DIR}/plugins/Vst3Base"
		"${CMAKE_SOURCE_DIR}/plugins/Vst3Base/travesty"
		"${CMAKE_BINARY_DIR}/plugins/Vst3Base/vst3base"
	)
	target_static_libraries(Vst3AutomationTest PRIVATE lmmsobjs)
	target_link_libraries(Vst3AutomationTest PRIVATE
		${QT_LIBRARIES}
		${QT_QTTEST_LIBRARY}
	)
	target_compile_features(Vst3AutomationTest PRIVATE cxx_std_20)
	target_compile_definitions(Vst3AutomationTest PRIVATE
		LMMS_TESTING
		VST3_FAKE_BUNDLE="${FAKE_VST3_BUNDLE}"
		VST3_FAKE_CLASS="${FAKE_VST3_INSTRUMENT_ID}"
	)
	add_dependencies(Vst3AutomationTest fake-vst3)
	add_test(NAME Vst3AutomationTest COMMAND Vst3AutomationTest)
endif()
