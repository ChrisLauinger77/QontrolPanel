set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(nightlight_tests nightlight_tests.cpp)
target_include_directories(nightlight_tests PRIVATE ../include)
add_test(NAME nightlight_data COMMAND nightlight_tests)

find_package(Python3 3.11 COMPONENTS Interpreter REQUIRED)
add_test(NAME release_provenance COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_release_provenance.py")

find_package(Qt6 QUIET COMPONENTS Core Qml Network Test)
if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    add_executable(reliability_tests reliability_tests.cpp
        ../src/jsonstore.cpp ../src/logmanager.cpp ../src/workerthreads.cpp
        ../include/logmanager.h ../src/audiomodels.cpp ../include/audiomodels.h)
    target_include_directories(reliability_tests PRIVATE ../include)
    target_link_libraries(reliability_tests PRIVATE Qt6::Core Qt6::Qml Qt6::Network Qt6::Test)
    if(WIN32)
        find_package(Qt6 REQUIRED COMPONENTS Gui)
        target_sources(reliability_tests PRIVATE
            ../src/keyboardshortcutmanager.cpp ../include/keyboardshortcutmanager.h
            ../src/usersettings.cpp ../include/usersettings.h)
        target_compile_definitions(reliability_tests PRIVATE UNICODE NOMINMAX)
        target_link_libraries(reliability_tests PRIVATE Qt6::Gui user32)
        if(TARGET headsetcontrol_lib)
            # Exercise the actual audio policy APIs with device workers disabled in the test preferences.
            target_sources(reliability_tests PRIVATE
                ../src/audiobridge.cpp ../include/audiobridge.h
                ../src/audiomanager.cpp ../include/audiomanager.h
                ../src/windowfocusmanager.cpp ../include/windowfocusmanager.h
                ../src/nativeimage.cpp
                ../src/headsetcontrolbridge.cpp ../include/headsetcontrolbridge.h
                ../src/headsetcontrolmonitor.cpp ../include/headsetcontrolmonitor.h)
            target_sources(reliability_tests PRIVATE
                ../src/languagebridge.cpp ../include/languagebridge.h
                ../src/updater.cpp ../include/updater.h)
            target_include_directories(reliability_tests PRIVATE "${CMAKE_BINARY_DIR}")
            target_link_libraries(reliability_tests PRIVATE Qt6::Widgets)
            target_compile_definitions(reliability_tests PRIVATE QONTROLPANEL_AUDIO_POLICY_TESTS QONTROLPANEL_LANGUAGE_TESTS)
            # Keep test catalogs separate from the app's compiled translations in all generators.
            set_target_properties(reliability_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/runtime")
            add_custom_target(reliability_test_translations
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:reliability_tests>/i18n"
                COMMAND Qt6::lrelease "${CMAKE_CURRENT_SOURCE_DIR}/../i18n/QontrolPanel_de.ts"
                    -qm "$<TARGET_FILE_DIR:reliability_tests>/i18n/QontrolPanel_de.qm"
                VERBATIM)
            add_dependencies(reliability_tests reliability_test_translations)
            target_link_libraries(reliability_tests PRIVATE headsetcontrol_lib hidapi::hidapi Gdi32 Version Shell32)
        endif()
    endif()
    add_test(NAME qt_reliability COMMAND reliability_tests)
else()
    if(WIN32)
        message(FATAL_ERROR "Qt Core/Qml/Network/Test are required for release validation")
    endif()
    message(STATUS "Qt development packages absent; Qt reliability tests unavailable")
endif()
