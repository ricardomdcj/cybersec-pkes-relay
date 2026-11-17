#include "run_meta.h"

#include "esp_log.h"

/* Definidos pelo CMake (components/pkes_contract/CMakeLists.txt). */
#ifndef RUN_GIT_COMMIT
#define RUN_GIT_COMMIT "sem-git"
#endif
#ifndef RUN_BUILD_TIME
#define RUN_BUILD_TIME "?"
#endif

void run_meta_print(const char *papel)
{
    ESP_LOGI("RUN_META", "papel=%s fw=%s build=%s", papel, RUN_GIT_COMMIT, RUN_BUILD_TIME);
}
