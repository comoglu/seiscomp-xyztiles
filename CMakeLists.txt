cmake_minimum_required(VERSION 3.14)
project(maptiles_community CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# SeisComP installation root
# ---------------------------------------------------------------------------
if(NOT SEISCOMP_ROOT)
    if(DEFINED ENV{SEISCOMP_ROOT})
        set(SEISCOMP_ROOT "$ENV{SEISCOMP_ROOT}")
    else()
        set(SEISCOMP_ROOT "/opt/seiscomp")
    endif()
endif()

message(STATUS "SeisComP root: ${SEISCOMP_ROOT}")

find_path(SC_INCLUDE_DIR seiscomp/core/baseobject.h
    PATHS "${SEISCOMP_ROOT}/include"
    NO_DEFAULT_PATH
    REQUIRED)

# The Qt-integrated GUI library is called libseiscomp_qt in installed builds
find_library(SC_QT_LIB seiscomp_qt
    PATHS "${SEISCOMP_ROOT}/lib"
    NO_DEFAULT_PATH
    REQUIRED)

find_library(SC_CLIENT_LIB seiscomp_client
    PATHS "${SEISCOMP_ROOT}/lib"
    NO_DEFAULT_PATH
    REQUIRED)

find_library(SC_CORE_LIB seiscomp_core
    PATHS "${SEISCOMP_ROOT}/lib"
    NO_DEFAULT_PATH
    REQUIRED)

# ---------------------------------------------------------------------------
# Qt5
# ---------------------------------------------------------------------------
find_package(Qt5 COMPONENTS Core Network Gui REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# ---------------------------------------------------------------------------
# Plugin target (MODULE = not linked into other targets, loaded at runtime)
# ---------------------------------------------------------------------------
add_library(maptiles_community MODULE
    plugin.cpp
    xyztilestore.cpp
)

target_include_directories(maptiles_community PRIVATE
    ${SC_INCLUDE_DIR}
)

target_link_libraries(maptiles_community PRIVATE
    ${SC_QT_LIB}
    ${SC_CLIENT_LIB}
    ${SC_CORE_LIB}
    Qt5::Core
    Qt5::Network
    Qt5::Gui
)

# Plugins must not have a "lib" prefix
set_target_properties(maptiles_community PROPERTIES PREFIX "")

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS maptiles_community
    LIBRARY DESTINATION "${SEISCOMP_ROOT}/share/plugins"
)

install(FILES
    seiscomp/etc/descriptions/global_maptiles_community.xml
    DESTINATION "${SEISCOMP_ROOT}/etc/descriptions"
)

message(STATUS "Install prefix: ${SEISCOMP_ROOT}")
message(STATUS "  plugin  → ${SEISCOMP_ROOT}/share/plugins/maptiles_community.so")
message(STATUS "  config  → ${SEISCOMP_ROOT}/etc/descriptions/global_maptiles_community.xml")
