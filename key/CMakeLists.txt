# key — the digital key (BLE peripheral + CYD display).
cmake_minimum_required(VERSION 3.22)

# Shared bench components (cyd, pkes_contract) live at the root of the repo.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(key)
