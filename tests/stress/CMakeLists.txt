# Local stress harness helpers.
#
# Every target here is EXCLUDE_FROM_ALL: the default build and the normal
# ctest run must not depend on long-running or GUI/TSAN-specific helpers.
# Build them explicitly with, for example:
#
#   cmake --build build --target GuiStress StressConcurrency SingleProcessSoak

add_executable(GuiStress EXCLUDE_FROM_ALL gui/gui_stress.cpp)
target_include_directories(GuiStress PRIVATE
	$<TARGET_PROPERTY:lmmsobjs,INCLUDE_DIRECTORIES>)
target_static_libraries(GuiStress PRIVATE lmmsobjs)
target_link_libraries(GuiStress PRIVATE
	${QT_LIBRARIES}
	${QT_QTTEST_LIBRARY})
target_compile_features(GuiStress PRIVATE cxx_std_20)
target_compile_definitions(GuiStress PRIVATE LMMS_TESTING)
# Third-party plugins resolve LMMS symbols from the loading executable.
set_target_properties(GuiStress PROPERTIES ENABLE_EXPORTS TRUE)

add_executable(StressConcurrency EXCLUDE_FROM_ALL
	concurrency/concurrency_stress.cpp)
target_include_directories(StressConcurrency PRIVATE
	$<TARGET_PROPERTY:lmmsobjs,INCLUDE_DIRECTORIES>)
target_static_libraries(StressConcurrency PRIVATE lmmsobjs)
target_link_libraries(StressConcurrency PRIVATE ${QT_LIBRARIES})
target_compile_features(StressConcurrency PRIVATE cxx_std_20)
target_compile_definitions(StressConcurrency PRIVATE LMMS_TESTING)
set_target_properties(StressConcurrency PROPERTIES ENABLE_EXPORTS TRUE)

add_executable(SingleProcessSoak EXCLUDE_FROM_ALL
	soak/single_process_soak.cpp)
target_include_directories(SingleProcessSoak PRIVATE
	$<TARGET_PROPERTY:lmmsobjs,INCLUDE_DIRECTORIES>)
target_static_libraries(SingleProcessSoak PRIVATE lmmsobjs)
target_link_libraries(SingleProcessSoak PRIVATE ${QT_LIBRARIES})
target_compile_features(SingleProcessSoak PRIVATE cxx_std_20)
target_compile_definitions(SingleProcessSoak PRIVATE LMMS_TESTING)
set_target_properties(SingleProcessSoak PROPERTIES ENABLE_EXPORTS TRUE)
