# This file is part of uZDL
# Copyright (C) 2026  luosmrow-lee
#
# uZDL is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.21)

project(uzdl
    VERSION 1.0.0
    DESCRIPTION "Launcher for ZDoom based Doom engine source ports"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network)
qt_standard_project_setup()

set(UZDL_HEADERS
    include/ZDLAboutDialog.h
    include/ZDLConfiguration.h
    include/ZDLConfigurationManager.h
    include/ZDLFileInfo.h
    include/ZDLFileList.h
    include/ZDLFileListable.h
    include/ZDLFilePane.h
    include/ZDLIWadList.h
    include/ZDLIdgamesTab.h
    include/ZDLImportDialog.h
    include/ZDLInterface.h
    include/ZDLLaunchFiles.h
    include/ZDLListEntry.hpp
    include/ZDLListWidget.h
    include/ZDLListable.h
    include/ZDLMainWindow.h
    include/ZDLMapFile.h
    include/ZDLPaths.h
    include/ZDLPresetDialog.h
    include/ZDLPresetList.h
    include/ZDLMultiPane.h
    include/ZDLNameInput.h
    include/ZDLNameListable.h
    include/ZDLNullDevice.h
    include/ZDLPwadList.h
    include/ZDLQSplitter.h
    include/ZDLSelfUpdate.h
    include/ZDLSettingsPane.h
    include/ZDLSettingsTab.h
    include/ZDLSourcePortList.h
    include/ZDLUpdateCheck.h
    include/ZDLWidget.h
    include/ZLibDir.h
    include/ZLibPK3.h
    include/libwad.h
    include/zdlcommon.h
    include/zdlconf.hpp
    include/zdlline.hpp
    include/zdlsection.hpp
    miniz/miniz.h)

set(UZDL_SOURCES
    src/ZDLAboutDialog.cpp
    src/ZDLConfiguration.cpp
    src/ZDLConfigurationManager.cpp
    src/ZDLFileInfo.cpp
    src/ZDLFileList.cpp
    src/ZDLFileListable.cpp
    src/ZDLFilePane.cpp
    src/ZDLIWadList.cpp
    src/ZDLIdgamesTab.cpp
    src/ZDLImportDialog.cpp
    src/ZDLInterface.cpp
    src/ZDLLaunchFiles.cpp
    src/ZDLListEntry.cpp
    src/ZDLListWidget.cpp
    src/ZDLListable.cpp
    src/ZDLMainWindow.cpp
    src/ZDLMapFile.cpp
    src/ZDLPaths.cpp
    src/ZDLPresetDialog.cpp
    src/ZDLPresetList.cpp
    src/ZDLMultiPane.cpp
    src/ZDLNameInput.cpp
    src/ZDLNameListable.cpp
    src/ZDLPwadList.cpp
    src/ZDLQSplitter.cpp
    src/ZDLSelfUpdate.cpp
    src/ZDLSettingsPane.cpp
    src/ZDLSettingsTab.cpp
    src/ZDLSourcePortList.cpp
    src/ZDLUpdateCheck.cpp
    src/ZDLWidget.cpp
    src/ZLibDir.cpp
    src/ZLibPK3.cpp
    src/libwad.cpp
    src/qzdl.cpp
    src/zdlconf.cpp
    src/zdlline.cpp
    src/zdlsection.cpp
    miniz/miniz.c)

# File associations are a Windows-only feature backed by SimpleWFA. On every
# other platform the feature compiles out via _ZDL_NO_WFA and neither the
# dialog nor the library is built.
if(WIN32)
    list(APPEND UZDL_SOURCES
        src/ZDLFileAssociations.cpp
        SimpleWFA/SimpleWFA.cpp
        res/Win32/qzdl.rc)
    list(APPEND UZDL_HEADERS
        include/ZDLFileAssociations.h
        SimpleWFA/SimpleWFA.h)
endif()

qt_add_executable(uzdl ${UZDL_SOURCES} ${UZDL_HEADERS})

# The window icon is a PNG rather than one of the XPMs the rest of the
# graphics use: XPM has no alpha channel, only a single transparent colour,
# so the icon's antialiased edges would come out stepped.
qt_add_resources(uzdl "appicon"
    PREFIX "/"
    BASE "res"
    FILES res/icon/uzdl.png)

# ZDL is a GUI application: without this it links as a console subsystem
# binary and Windows opens a console window behind the main window.
set_target_properties(uzdl PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE)

target_include_directories(uzdl PRIVATE
    include
    miniz
    res/XPM)

target_link_libraries(uzdl PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

# Silences the 32-bit-build notice in zdlcommon.h; it is informational only.
target_compile_definitions(uzdl PRIVATE _ZDL_NO_WARNINGS)

if(WIN32)
    target_include_directories(uzdl PRIVATE SimpleWFA res/Win32)
    # SimpleWFA calls the wide-character registry API directly, matching the
    # Unicode character set the original Visual Studio project used.
    target_compile_definitions(uzdl PRIVATE UNICODE _UNICODE NOMINMAX)
else()
    target_compile_definitions(uzdl PRIVATE _ZDL_NO_WFA)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # miniz punches through types; the WAD readers do the same with their
    # header structs.
    target_compile_options(uzdl PRIVATE
        -fno-strict-aliasing
        -Wno-missing-field-initializers)
endif()

install(TARGETS uzdl
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin)

# The release package: `cmake --build build --config Release --target dist`
# makes build/dist/uZDL-<version>-win64.zip holding the executable, the Qt
# and MSVC runtimes it needs and the docs. Deliberately no uzdl_portable.ini:
# unpacking a newer version over an old folder must never replace the
# configuration, so portable mode stays something the user opts into.
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)

    get_target_property(_uzdl_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_uzdl_qt_bin "${_uzdl_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_uzdl_qt_bin}")

    set(UZDL_DIST_NAME "uZDL-${PROJECT_VERSION}-win64")
    set(UZDL_DIST_ROOT "${CMAKE_BINARY_DIR}/dist")
    set(UZDL_DIST_DIR "${UZDL_DIST_ROOT}/${UZDL_DIST_NAME}")
    file(MAKE_DIRECTORY "${UZDL_DIST_ROOT}")

    # Without the MSVC runtime beside the executable, a machine that never
    # had Visual Studio or a redistributable installed fails to start it.
    set(_uzdl_runtime_copy)
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        set(_uzdl_runtime_copy COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} "${UZDL_DIST_DIR}/")
    else()
        message(WARNING "MSVC runtime DLLs not found; the dist package will need the VC++ redistributable installed")
    endif()

    add_custom_target(dist
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${UZDL_DIST_DIR}" "${UZDL_DIST_ROOT}/${UZDL_DIST_NAME}.zip"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${UZDL_DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:uzdl>" "${UZDL_DIST_DIR}/"
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-translations --no-compiler-runtime --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw --no-quick-import "${UZDL_DIST_DIR}/uzdl.exe"
        ${_uzdl_runtime_copy}
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/README.md" "${CMAKE_SOURCE_DIR}/CHANGELOG" "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/AUTHORS" "${UZDL_DIST_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E tar cf "${UZDL_DIST_NAME}.zip" --format=zip "${UZDL_DIST_NAME}"
        WORKING_DIRECTORY "${UZDL_DIST_ROOT}"
        COMMENT "Packaging ${UZDL_DIST_NAME}.zip"
        VERBATIM)
    add_dependencies(dist uzdl)
endif()
