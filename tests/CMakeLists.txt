include(CTest)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

set(LMMS_TESTS
	src/core/ArrayVectorTest.cpp
	src/core/DelayLineTest.cpp
	src/core/AudioBufferTest.cpp
	src/core/AutomatableModelTest.cpp
	src/core/Base64Test.cpp
	src/core/ConfigUpgradeTest.cpp
	src/core/MathTest.cpp
	src/core/MixerSidechainTest.cpp
	src/core/HostedEffectTest.cpp
	src/core/HostedInstrumentTest.cpp
	src/core/HostedPluginModelsTest.cpp
	src/core/ParameterCurveTest.cpp
	src/core/PluginProcessSupervisorTest.cpp
	src/core/PluginScanCacheTest.cpp
	src/core/ProjectVersionTest.cpp
	src/core/RelativePathsTest.cpp
	src/core/SongAutomationTest.cpp
	src/core/TimelineTest.cpp
	src/core/ValidationTest.cpp
	src/core/InlineValueEditorTest.cpp
	src/gui/SideBarWorkspaceTest.cpp
	src/gui/ToolButtonTest.cpp
	src/gui/DropDownInteractionTest.cpp
	src/tracks/AutomationTrackTest.cpp
	src/tracks/InstrumentTrackTest.cpp
	src/tracks/MidiClipTest.cpp
)

if(LMMS_HAVE_LV2)
	list(APPEND LMMS_TESTS src/core/Lv2PresetTest.cpp)
	list(APPEND LMMS_TESTS src/core/Lv2PortTest.cpp)
	list(APPEND LMMS_TESTS src/core/Lv2HostClientTest.cpp)
	list(APPEND LMMS_TESTS src/core/Lv2PluginHostTest.cpp)

	add_library(FakeLv2Gain MODULE plugins/fake-lv2-gain/gain.c)
	target_include_directories(FakeLv2Gain PRIVATE ${LV2_INCLUDE_DIRS})
	set_target_properties(FakeLv2Gain PROPERTIES
		PREFIX ""
		# The bundle manifests reference "gain.so"; keep that name on every
		# platform so lilv can open the module (Windows accepts any suffix).
		SUFFIX ".so"
		OUTPUT_NAME "gain"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-gain.lv2"
	)
	configure_file(plugins/fake-lv2-gain/manifest.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-gain.lv2/manifest.ttl" COPYONLY)
	configure_file(plugins/fake-lv2-gain/gain.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-gain.lv2/gain.ttl" COPYONLY)

	add_library(FakeLv2Midi MODULE plugins/fake-lv2-midi/midi.c)
	target_include_directories(FakeLv2Midi PRIVATE ${LV2_INCLUDE_DIRS})
	set_target_properties(FakeLv2Midi PROPERTIES
		PREFIX ""
		SUFFIX ".so"
		OUTPUT_NAME "midi"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-midi.lv2"
	)
	configure_file(plugins/fake-lv2-midi/manifest.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-midi.lv2/manifest.ttl" COPYONLY)
	configure_file(plugins/fake-lv2-midi/midi.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-midi.lv2/midi.ttl" COPYONLY)

	add_library(FakeLv2CrashDsp MODULE plugins/fake-lv2-crash-dsp/crash.c)
	target_include_directories(FakeLv2CrashDsp PRIVATE ${LV2_INCLUDE_DIRS})
	set_target_properties(FakeLv2CrashDsp PROPERTIES
		PREFIX ""
		OUTPUT_NAME "crash"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-crash-dsp.lv2"
	)
	configure_file(plugins/fake-lv2-crash-dsp/manifest.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-crash-dsp.lv2/manifest.ttl" COPYONLY)
	configure_file(plugins/fake-lv2-crash-dsp/crash.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-crash-dsp.lv2/crash.ttl" COPYONLY)

	add_library(FakeLv2CrashUi MODULE plugins/fake-lv2-crash-ui/crashui.c)
	target_include_directories(FakeLv2CrashUi PRIVATE ${LV2_INCLUDE_DIRS})
	set_target_properties(FakeLv2CrashUi PROPERTIES
		PREFIX ""
		OUTPUT_NAME "crashui"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-crash-ui.lv2"
	)
	configure_file(plugins/fake-lv2-crash-ui/manifest.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-crash-ui.lv2/manifest.ttl" COPYONLY)
	configure_file(plugins/fake-lv2-crash-ui/crashui.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-crash-ui.lv2/crashui.ttl" COPYONLY)

	add_library(FakeLv2Sidechain MODULE plugins/fake-lv2-sidechain/sidechain.c)
	target_include_directories(FakeLv2Sidechain PRIVATE ${LV2_INCLUDE_DIRS})
	set_target_properties(FakeLv2Sidechain PROPERTIES
		PREFIX ""
		SUFFIX ".so"
		OUTPUT_NAME "sidechain"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-sidechain.lv2"
	)
	configure_file(plugins/fake-lv2-sidechain/manifest.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-sidechain.lv2/manifest.ttl" COPYONLY)
	configure_file(plugins/fake-lv2-sidechain/sidechain.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-sidechain.lv2/sidechain.ttl" COPYONLY)

	add_library(FakeLv2MultiOut MODULE plugins/fake-lv2-multiout/multiout.c)
	target_include_directories(FakeLv2MultiOut PRIVATE ${LV2_INCLUDE_DIRS})
	set_target_properties(FakeLv2MultiOut PROPERTIES
		PREFIX ""
		SUFFIX ".so"
		OUTPUT_NAME "multiout"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-multiout.lv2"
	)
	configure_file(plugins/fake-lv2-multiout/manifest.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-multiout.lv2/manifest.ttl" COPYONLY)
	configure_file(plugins/fake-lv2-multiout/multiout.ttl
		"${CMAKE_CURRENT_BINARY_DIR}/lv2/fake-lv2-multiout.lv2/multiout.ttl" COPYONLY)
endif()

if(LMMS_BUILD_LINUX AND WANT_VST)
	add_library(FakeMidiVst SHARED plugins/fake-midi-vst.cpp)
	target_include_directories(FakeMidiVst PRIVATE ${CMAKE_SOURCE_DIR}/include)
	target_compile_features(FakeMidiVst PRIVATE cxx_std_17)
	set_target_properties(FakeMidiVst PROPERTIES
		PREFIX ""
		OUTPUT_NAME "fake-midi-vst"
	)

	add_library(FakeCrashVst SHARED plugins/fake-crash-vst.cpp)
	target_include_directories(FakeCrashVst PRIVATE ${CMAKE_SOURCE_DIR}/include)
	target_compile_features(FakeCrashVst PRIVATE cxx_std_17)
	set_target_properties(FakeCrashVst PROPERTIES
		PREFIX ""
		OUTPUT_NAME "fake-crash-vst"
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

if(TARGET vstbase)
	add_executable(Vst2PluginHostTest src/core/Vst2PluginHostTest.cpp)
	target_include_directories(Vst2PluginHostTest PRIVATE
		$<TARGET_PROPERTY:lmmsobjs,INCLUDE_DIRECTORIES>
		"${CMAKE_SOURCE_DIR}/plugins/VstBase")
	target_static_libraries(Vst2PluginHostTest PRIVATE lmmsobjs)
	target_link_libraries(Vst2PluginHostTest PRIVATE
		${QT_LIBRARIES}
		${QT_QTTEST_LIBRARY}
		vstbase
	)
	target_compile_features(Vst2PluginHostTest PRIVATE cxx_std_20)
	target_compile_definitions(Vst2PluginHostTest PRIVATE LMMS_TESTING)
	# The VST2 host loads bitmaps of LMMS symbols from the executable.
	set_target_properties(Vst2PluginHostTest PROPERTIES ENABLE_EXPORTS TRUE)
	add_test(NAME Vst2PluginHostTest COMMAND Vst2PluginHostTest)
	set_tests_properties(Vst2PluginHostTest PROPERTIES
		ENVIRONMENT "LMMS_PLUGIN_DIR=${CMAKE_BINARY_DIR}/plugins")
endif()

if(TARGET vstbase AND TARGET FakeCrashVst)
	add_executable(Vst2CrashTest src/core/Vst2CrashTest.cpp)
	target_include_directories(Vst2CrashTest PRIVATE
		$<TARGET_PROPERTY:lmmsobjs,INCLUDE_DIRECTORIES>
		"${CMAKE_SOURCE_DIR}/plugins/VstBase")
	target_static_libraries(Vst2CrashTest PRIVATE lmmsobjs)
	target_link_libraries(Vst2CrashTest PRIVATE
		${QT_LIBRARIES}
		${QT_QTTEST_LIBRARY}
		vstbase
	)
	target_compile_features(Vst2CrashTest PRIVATE cxx_std_20)
	target_compile_definitions(Vst2CrashTest PRIVATE
		LMMS_TESTING
		LMMS_FAKE_CRASH_VST="$<TARGET_FILE:FakeCrashVst>"
	)
	set_target_properties(Vst2CrashTest PROPERTIES ENABLE_EXPORTS TRUE)
	add_dependencies(Vst2CrashTest FakeCrashVst NativeLinuxRemoteVstPlugin64)
	add_test(NAME Vst2CrashTest COMMAND Vst2CrashTest)
	set_tests_properties(Vst2CrashTest PROPERTIES
		ENVIRONMENT "LMMS_PLUGIN_DIR=${CMAKE_BINARY_DIR}/plugins")
endif()

# Load/save/load helper used by the local stress harness. Not registered as
# a ctest because it needs a project file and two output paths.
add_executable(LmmsRoundTrip stress/roundtrip.cpp)
target_include_directories(LmmsRoundTrip PRIVATE
	$<TARGET_PROPERTY:lmmsobjs,INCLUDE_DIRECTORIES>)
target_static_libraries(LmmsRoundTrip PRIVATE lmmsobjs)
target_link_libraries(LmmsRoundTrip PRIVATE ${QT_LIBRARIES})
target_compile_features(LmmsRoundTrip PRIVATE cxx_std_20)

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

	add_executable(Vst3PluginHostTest
		src/core/Vst3PluginHostTest.cpp
		"${CMAKE_SOURCE_DIR}/plugins/Vst3Base/Vst3PluginHost.cpp"
		"${CMAKE_SOURCE_DIR}/plugins/Vst3Base/Vst3Host.cpp"
		"${CMAKE_SOURCE_DIR}/plugins/Vst3Base/Vst3Module.cpp"
	)
	target_include_directories(Vst3PluginHostTest PRIVATE
		$<TARGET_PROPERTY:lmmsobjs,INCLUDE_DIRECTORIES>
		"${CMAKE_SOURCE_DIR}/plugins/Vst3Base"
		"${CMAKE_SOURCE_DIR}/plugins/Vst3Base/travesty"
		"${CMAKE_BINARY_DIR}/plugins/Vst3Base/vst3base"
	)
	target_static_libraries(Vst3PluginHostTest PRIVATE lmmsobjs)
	target_link_libraries(Vst3PluginHostTest PRIVATE
		${QT_LIBRARIES}
		${QT_QTTEST_LIBRARY}
	)
	target_compile_features(Vst3PluginHostTest PRIVATE cxx_std_20)
	target_compile_definitions(Vst3PluginHostTest PRIVATE
		LMMS_TESTING
		VST3BASE_STATIC_DEFINE
		VST3_FAKE_BUNDLE="${FAKE_VST3_BUNDLE}"
		VST3_FAKE_CLASS="${FAKE_VST3_INSTRUMENT_ID}"
		VST3_FAKE_EFFECT_CLASS="${FAKE_VST3_EFFECT_ID}"
	)
	add_dependencies(Vst3PluginHostTest fake-vst3)
	add_test(NAME Vst3PluginHostTest COMMAND Vst3PluginHostTest)

	if(TARGET lmms-vst3-host)
		add_executable(Vst3RemoteBusTest
			src/core/Vst3RemoteBusTest.cpp
		)
		target_include_directories(Vst3RemoteBusTest PRIVATE
			$<TARGET_PROPERTY:lmmsobjs,INCLUDE_DIRECTORIES>
			"${CMAKE_SOURCE_DIR}/plugins/Vst3Base"
		)
		target_static_libraries(Vst3RemoteBusTest PRIVATE lmmsobjs)
		target_link_libraries(Vst3RemoteBusTest PRIVATE
			${QT_LIBRARIES}
			${QT_QTTEST_LIBRARY}
			vst3base
		)
		target_compile_features(Vst3RemoteBusTest PRIVATE cxx_std_20)
		target_compile_definitions(Vst3RemoteBusTest PRIVATE
			LMMS_TESTING
			VST3_FAKE_BUNDLE="${FAKE_VST3_BUNDLE}"
			VST3_FAKE_EFFECT_CLASS="${FAKE_VST3_EFFECT_ID}"
			LMMS_VST3_HOST_EXECUTABLE="$<TARGET_FILE:lmms-vst3-host>"
		)
		set_target_properties(Vst3RemoteBusTest PROPERTIES ENABLE_EXPORTS TRUE)
		add_dependencies(Vst3RemoteBusTest fake-vst3 lmms-vst3-host)
		add_test(NAME Vst3RemoteBusTest COMMAND Vst3RemoteBusTest)
		set_tests_properties(Vst3RemoteBusTest PROPERTIES
			ENVIRONMENT "LMMS_PLUGIN_DIR=${CMAKE_BINARY_DIR}/plugins")
	endif()
endif()

if(LMMS_BUILD_LINUX AND WANT_VST3 AND TARGET fake-vst3 AND TARGET vst3instrument)
	add_executable(Vst3ProjectTest
		src/core/Vst3ProjectTest.cpp
	)
	target_include_directories(Vst3ProjectTest PRIVATE
		$<TARGET_PROPERTY:lmmsobjs,INCLUDE_DIRECTORIES>
	)
	target_static_libraries(Vst3ProjectTest PRIVATE lmmsobjs)
	target_link_libraries(Vst3ProjectTest PRIVATE
		${QT_LIBRARIES}
		${QT_QTTEST_LIBRARY}
	)
	target_compile_features(Vst3ProjectTest PRIVATE cxx_std_20)
	target_compile_definitions(Vst3ProjectTest PRIVATE
		LMMS_TESTING
		VST3_FAKE_BUNDLE="${FAKE_VST3_BUNDLE}"
		VST3_FAKE_CLASS="${FAKE_VST3_INSTRUMENT_ID}"
		VST3_PLUGIN_DIR="${CMAKE_BINARY_DIR}/plugins"
	)
	# Plugins resolve LMMS core symbols from the loading executable, as they
	# do when loaded by the lmms binary.
	set_target_properties(Vst3ProjectTest PROPERTIES ENABLE_EXPORTS TRUE)
	add_dependencies(Vst3ProjectTest fake-vst3 vst3instrument)
	add_test(NAME Vst3ProjectTest COMMAND Vst3ProjectTest)
endif()

if(TARGET Lv2HostClientTest AND TARGET lmms-lv2-host AND TARGET FakeLv2Gain)
	add_dependencies(Lv2HostClientTest lmms-lv2-host FakeLv2Gain)
	if(TARGET FakeLv2Midi)
		add_dependencies(Lv2HostClientTest FakeLv2Midi)
	endif()
	if(TARGET FakeLv2CrashDsp AND TARGET FakeLv2CrashUi)
		add_dependencies(Lv2HostClientTest FakeLv2CrashDsp FakeLv2CrashUi)
	endif()
	if(TARGET FakeLv2Sidechain)
		add_dependencies(Lv2HostClientTest FakeLv2Sidechain)
	endif()
	if(TARGET FakeLv2MultiOut)
		add_dependencies(Lv2HostClientTest FakeLv2MultiOut)
	endif()
	target_compile_definitions(Lv2HostClientTest PRIVATE
		LMMS_LV2_HOST_EXECUTABLE="$<TARGET_FILE:lmms-lv2-host>"
		LMMS_FAKE_LV2_GAIN_BUNDLE="$<TARGET_FILE_DIR:FakeLv2Gain>"
		LMMS_FAKE_LV2_MIDI_BUNDLE="$<TARGET_FILE_DIR:FakeLv2Midi>"
		LMMS_FAKE_LV2_CRASH_DSP_BUNDLE="$<TARGET_FILE_DIR:FakeLv2CrashDsp>"
		LMMS_FAKE_LV2_CRASH_UI_BUNDLE="$<TARGET_FILE_DIR:FakeLv2CrashUi>"
		LMMS_FAKE_LV2_SIDECHAIN_BUNDLE="$<TARGET_FILE_DIR:FakeLv2Sidechain>"
		LMMS_FAKE_LV2_MULTIOUT_BUNDLE="$<TARGET_FILE_DIR:FakeLv2MultiOut>")
	target_include_directories(Lv2HostClientTest PRIVATE
		${Lilv_INCLUDE_DIRS}
		${LV2_INCLUDE_DIRS})
	target_link_libraries(Lv2HostClientTest PRIVATE Lilv::lilv)
endif()

if(TARGET Lv2PluginHostTest AND TARGET FakeLv2Gain)
	add_dependencies(Lv2PluginHostTest FakeLv2Gain)
	if(TARGET FakeLv2Sidechain)
		add_dependencies(Lv2PluginHostTest FakeLv2Sidechain)
		target_compile_definitions(Lv2PluginHostTest PRIVATE
			LMMS_FAKE_LV2_SIDECHAIN_BUNDLE="$<TARGET_FILE_DIR:FakeLv2Sidechain>"
		LMMS_FAKE_LV2_MULTIOUT_BUNDLE="$<TARGET_FILE_DIR:FakeLv2MultiOut>")
	endif()
	if(TARGET lmms-lv2-host)
		add_dependencies(Lv2PluginHostTest lmms-lv2-host)
		add_test(NAME Lv2PluginHostTestRemote COMMAND Lv2PluginHostTest)
		set_tests_properties(Lv2PluginHostTestRemote PROPERTIES
			ENVIRONMENT "LMMS_LV2_TEST_REMOTE=1;LMMS_LV2_REMOTE=1;LMMS_PLUGIN_DIR=${CMAKE_BINARY_DIR}/plugins")
	endif()
	target_compile_definitions(Lv2PluginHostTest PRIVATE
		LMMS_FAKE_LV2_GAIN_BUNDLE="$<TARGET_FILE_DIR:FakeLv2Gain>")
	target_include_directories(Lv2PluginHostTest PRIVATE
		${Lilv_INCLUDE_DIRS}
		${LV2_INCLUDE_DIRS})
	target_link_libraries(Lv2PluginHostTest PRIVATE Lilv::lilv)
endif()
