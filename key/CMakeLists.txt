# key — a chave digital (peripheral BLE + display CYD).
cmake_minimum_required(VERSION 3.22)

# Componentes compartilhados da bancada (cyd, pkes_contract) vivem na raiz do repo.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(key)
