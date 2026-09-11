cmake_minimum_required(VERSION 3.6)
project(harbour-snapszer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core LinguistTools Network Qml Quick)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SAILFISHAPP REQUIRED sailfishapp)

add_executable(harbour-snapszer
    main.cpp
    src/GameCore.cpp
    src/GameEngine.cpp
    src/LanSession.cpp
    src/MultiCore.cpp
    src/MultiEngine.cpp
)

target_include_directories(harbour-snapszer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SAILFISHAPP_INCLUDE_DIRS}
)

target_link_libraries(harbour-snapszer
    Qt5::Core Qt5::Network Qt5::Qml Qt5::Quick ${SAILFISHAPP_LIBRARIES}
)

set(SNAPSZER_TRANSLATIONS translations/harbour-snapszer-hu.ts)
qt5_add_translation(SNAPSZER_QM_FILES ${SNAPSZER_TRANSLATIONS})
add_custom_target(harbour-snapszer-translations ALL DEPENDS ${SNAPSZER_QM_FILES})
add_dependencies(harbour-snapszer harbour-snapszer-translations)

option(SNAPSZER_BUILD_TESTS "Build deterministic game and LAN tests" OFF)
if(SNAPSZER_BUILD_TESTS)
    enable_testing()
    add_executable(test_multicore src/GameCore.cpp src/MultiCore.cpp tests/test_multicore.cpp)
    target_include_directories(test_multicore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME multicore COMMAND test_multicore)

    # Headless LAN matches over localhost: two players, and a host with two
    # guests plus a computer seat.
    set(SNAPSZER_ENGINE_SOURCES src/GameCore.cpp src/GameEngine.cpp src/LanSession.cpp
        src/MultiCore.cpp src/MultiEngine.cpp)
    add_executable(lan_twoplayer tests/lan_twoplayer.cpp ${SNAPSZER_ENGINE_SOURCES})
    add_executable(lan_multiplayer tests/lan_multiplayer.cpp ${SNAPSZER_ENGINE_SOURCES})
    foreach(test lan_twoplayer lan_multiplayer)
        target_include_directories(${test} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(${test} Qt5::Core Qt5::Gui Qt5::Network)
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
endif()
