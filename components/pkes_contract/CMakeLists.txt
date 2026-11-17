# GATT contract + HMAC + BLE/run-header helpers shared by key/, vehicle/ and relays/.
# REQUIRES bt: ble_contract.h uses the NimBLE UUID types (host/ble_uuid.h) and
#              ble_link.c calls the GAP/GATT API.
# mbedtls: pkes_hmac.c computes the HMAC (PSA on IDF v6, legacy MD on v5.4).
# log: esp_log in the helpers.
idf_component_register(SRCS "pkes_hmac.c" "ble_link.c" "run_meta.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt mbedtls log)

# Run header (M-13): git commit and state, read at configure time. "+mod" marks
# uncommitted changes in tracked files (untracked files do not count, otherwise scratch
# builds inside the tree would dirty the mark).
execute_process(COMMAND git rev-parse --short HEAD
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                OUTPUT_VARIABLE _run_commit
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE _run_rc)
if(NOT _run_rc EQUAL 0 OR _run_commit STREQUAL "")
    set(_run_commit "no-git")
endif()
execute_process(COMMAND git status --porcelain --untracked-files=no
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                OUTPUT_VARIABLE _run_dirty
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
if(NOT _run_dirty STREQUAL "")
    set(_run_commit "${_run_commit}+mod")
endif()
string(TIMESTAMP _run_build_time "%Y-%m-%d %H:%M")

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    "RUN_GIT_COMMIT=\"${_run_commit}\""
    "RUN_BUILD_TIME=\"${_run_build_time}\"")
