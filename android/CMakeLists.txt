cmake_minimum_required(VERSION 3.21)
project(snapszer VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.9)

# The game logic, AI and LAN code are shared with the Sailfish OS build.
set(SNAPSZER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

qt_add_executable(snapszer
    main.cpp
    ScreenHelper.h
    ${SNAPSZER_ROOT}/src/AppSettings.h
    ${SNAPSZER_ROOT}/src/BtLink.cpp
    ${SNAPSZER_ROOT}/src/BtLink.h
    ${SNAPSZER_ROOT}/src/GameCore.cpp
    ${SNAPSZER_ROOT}/src/GameCore.h
    ${SNAPSZER_ROOT}/src/GameEngine.cpp
    ${SNAPSZER_ROOT}/src/GameEngine.h
    ${SNAPSZER_ROOT}/src/LanSession.cpp
    ${SNAPSZER_ROOT}/src/LanSession.h
    ${SNAPSZER_ROOT}/src/MultiCore.cpp
    ${SNAPSZER_ROOT}/src/MultiCore.h
    ${SNAPSZER_ROOT}/src/MultiEngine.cpp
    ${SNAPSZER_ROOT}/src/MultiEngine.h
)
target_include_directories(snapszer PRIVATE ${SNAPSZER_ROOT}/src)
target_link_libraries(snapszer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2)

set_source_files_properties(qml/Theme.qml qml/Style.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# Table and rules of the three- and four-player games are shared with Sailfish OS.
set(SNAPSZER_SHARED_QML
    ${SNAPSZER_ROOT}/qml-common/MultiTable.qml
    ${SNAPSZER_ROOT}/qml-common/MultiRules.qml
)
foreach(file ${SNAPSZER_SHARED_QML})
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()
qt_add_qml_module(snapszer
    URI Snapszer
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Style.qml
        qml/TableButton.qml
        qml/MultiPage.qml
        ${SNAPSZER_SHARED_QML}
        qml/Table.qml
        qml/Card.qml
        qml/FlyingCard.qml
        qml/SubPage.qml
        qml/SectionLabel.qml
        qml/TextBlock.qml
        qml/Settings.qml
        qml/LanPage.qml
        qml/GameRules.qml
        qml/AboutPage.qml
)

file(GLOB SNAPSZER_CARDS "${SNAPSZER_ROOT}/images/cards/*.png")
qt_add_resources(snapszer snapszer_images
    PREFIX "/"
    BASE "${SNAPSZER_ROOT}"
    FILES ${SNAPSZER_CARDS} "${SNAPSZER_ROOT}/sailfish/icons/icon-256.png"
)

# The Android Qt package has no Linguist tools, so use lrelease of the host Qt.
find_program(SNAPSZER_LRELEASE NAMES lrelease-qt6 lrelease
             HINTS "${QT_HOST_PATH}/lib/qt6/bin" "${QT_HOST_PATH}/bin" REQUIRED)
set(SNAPSZER_TS "${SNAPSZER_ROOT}/translations/harbour-snapszer-hu.ts")
set(SNAPSZER_QM "${CMAKE_CURRENT_BINARY_DIR}/harbour-snapszer-hu.qm")
add_custom_command(OUTPUT "${SNAPSZER_QM}"
    COMMAND "${SNAPSZER_LRELEASE}" -silent "${SNAPSZER_TS}" -qm "${SNAPSZER_QM}"
    DEPENDS "${SNAPSZER_TS}"
    VERBATIM)
set_source_files_properties("${SNAPSZER_QM}" PROPERTIES
    GENERATED TRUE
    QT_RESOURCE_ALIAS harbour-snapszer-hu.qm)
qt_add_resources(snapszer snapszer_translations
    PREFIX "/i18n"
    FILES "${SNAPSZER_QM}"
)

if(ANDROID)
    set_target_properties(snapszer PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/package"
        QT_ANDROID_PACKAGE_NAME "org.edp17.snapszer"
        QT_ANDROID_APP_NAME "Snapszer"
        QT_ANDROID_APP_ICON "@drawable/icon"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36
        QT_ANDROID_VERSION_CODE 4
        QT_ANDROID_VERSION_NAME "1.1.0"
    )
    # Broadcast discovery needs a Wi-Fi multicast lock on many devices.
    qt_add_android_permission(snapszer NAME android.permission.ACCESS_WIFI_STATE)
    qt_add_android_permission(snapszer NAME android.permission.CHANGE_WIFI_MULTICAST_STATE)
    # Foreground service and Wi-Fi lock while a LAN game is hosted or played.
    qt_add_android_permission(snapszer NAME android.permission.FOREGROUND_SERVICE)
    qt_add_android_permission(snapszer NAME android.permission.FOREGROUND_SERVICE_SPECIAL_USE)
    qt_add_android_permission(snapszer NAME android.permission.POST_NOTIFICATIONS)
    qt_add_android_permission(snapszer NAME android.permission.WAKE_LOCK)
endif()

if(EMSCRIPTEN)
    # The browser edition. Qt for WebAssembly gives the page no raw sockets, so
    # the LAN pages are hidden at run time (see main.cpp); the code still
    # compiles. 64 MB up front spares the card images a heap growth during the
    # first deal. The deployed page is wasm/index.html, not the generated
    # snapszer.html, which is kept as the reference loader.
    set_target_properties(snapszer PROPERTIES QT_WASM_INITIAL_MEMORY "67108864")
endif()
