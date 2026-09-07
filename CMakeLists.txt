cmake_minimum_required(VERSION 3.6)
project(harbour-snapszer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core LinguistTools Qml Quick)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SAILFISHAPP REQUIRED sailfishapp)

add_executable(harbour-snapszer
    main.cpp
    src/GameCore.cpp
    src/GameEngine.cpp
)

target_include_directories(harbour-snapszer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SAILFISHAPP_INCLUDE_DIRS}
)

target_link_libraries(harbour-snapszer
    Qt5::Core Qt5::Qml Qt5::Quick ${SAILFISHAPP_LIBRARIES}
)

set(SNAPSZER_TRANSLATIONS translations/harbour-snapszer-hu.ts)
qt5_add_translation(SNAPSZER_QM_FILES ${SNAPSZER_TRANSLATIONS})
add_custom_target(harbour-snapszer-translations ALL DEPENDS ${SNAPSZER_QM_FILES})
add_dependencies(harbour-snapszer harbour-snapszer-translations)

option(SNAPSZER_BUILD_TESTS "Build deterministic GameCore tests" OFF)
if(SNAPSZER_BUILD_TESTS)
    enable_testing()
    add_executable(test_gamecore src/GameCore.cpp tests/test_gamecore.cpp)
    target_include_directories(test_gamecore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME gamecore COMMAND test_gamecore)
endif()
