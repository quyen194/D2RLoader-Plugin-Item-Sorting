cmake_minimum_required(VERSION 3.29)
project(RemoteStashSorting LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- D2RLoader Plugin SDK -----------------------------------------------
# Sibling checkout under D2RLoader-Plugins-Dev/PluginSDK. Switch to
# find_package(D2RLPlugin CONFIG REQUIRED) once an installed SDK package is
# used instead of a source checkout.
if(NOT TARGET D2RLPlugin::D2RLPlugin)
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../PluginSDK" "${CMAKE_CURRENT_BINARY_DIR}/PluginSDK" EXCLUDE_FROM_ALL)
endif()

# --- toml++ (header-only) ------------------------------------------------
# Same TOML library the rest of the D2RLoader plugin ecosystem uses
# (RuffnecKk-D2RLoader-Suite plugins all include <toml++/toml.hpp>), kept
# consistent on purpose rather than picking a different parser.
include(FetchContent)
FetchContent_Declare(
	tomlplusplus
	GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
	GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# --- Sort engine (Phase 1) -------------------------------------------------
# Pure algorithm, no D2RLPlugin dependency: the auto-sort layout/classification
# port from D2R-Reimagined-Save-Editor. Kept as its own object library so it
# can eventually get its own unit tests without linking the plugin DLL.
#
# item_classification_data.hpp is GENERATED (see tools/gen_classification.py)
# from D2R-Reimagined-Save-Editor's base/{itemtypes,weapons,armor,misc,
# setitems}.txt; it is committed as a normal source file rather than
# regenerated at build time, so building this plugin does not require a
# checkout of the save-editor repo.
add_library(RemoteStashSortEngine STATIC
	src/sort_engine.cpp
	src/sort_engine.hpp
	src/item_classification.cpp
	src/item_classification.hpp
	src/item_classification_data.hpp
)
target_include_directories(RemoteStashSortEngine PUBLIC src)

# --- Plugin DLL -------------------------------------------------------------
add_library(RemoteStashSorting SHARED
	src/plugin.cpp
	src/plugin.rc
	src/ui_layouts.hpp
)
target_link_libraries(RemoteStashSorting PRIVATE
	D2RLPlugin::D2RLPlugin
	RemoteStashSortEngine
	tomlplusplus::tomlplusplus
)

d2rlplugin_embed_config(
	TARGET RemoteStashSorting
	FILE "${CMAKE_CURRENT_SOURCE_DIR}/remote-stash-sorting.toml"
)

# --- Local "ready to copy" deploy folder ------------------------------------
# Every build (Debug or Release, from VS Code or the command line) mirrors
# the freshly built DLL into deploy/d2rloader/plugins. The whole deploy/
# d2rloader folder can then be dragged straight into <game>/d2rloader. This
# is also the exact folder the release workflow zips up for GitHub Releases
# (see .github/workflows/release.yml) -- one definition of "what a deployed
# plugin looks like", used by both local builds and CI.
set(RSS_DEPLOY_PLUGINS_DIR "${CMAKE_SOURCE_DIR}/deploy/d2rloader/plugins")
add_custom_command(TARGET RemoteStashSorting POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${RSS_DEPLOY_PLUGINS_DIR}"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:RemoteStashSorting>" "${RSS_DEPLOY_PLUGINS_DIR}/"
	COMMENT "Copying RemoteStashSorting.dll into deploy/d2rloader/plugins"
	VERBATIM
)

# Optional convenience: point this at your D2R install to also copy the
# freshly built DLL straight into the real game's plugins folder on every
# build, e.g. a CMakeUserPresets.json cacheVariables entry or:
#   cmake --preset debug -DD2RLOADER_GAME_DIR=D:/Games/DiabloII-Resurrected
if(DEFINED D2RLOADER_GAME_DIR)
	add_custom_command(TARGET RemoteStashSorting POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${D2RLOADER_GAME_DIR}/d2rloader/plugins"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:RemoteStashSorting>" "${D2RLOADER_GAME_DIR}/d2rloader/plugins/"
		COMMENT "Copying RemoteStashSorting.dll into ${D2RLOADER_GAME_DIR}/d2rloader/plugins"
		VERBATIM
	)
endif()
