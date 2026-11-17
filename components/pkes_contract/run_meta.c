#include "run_meta.h"

#include "esp_log.h"

/* Defined by CMake (components/pkes_contract/CMakeLists.txt). */
#ifndef RUN_GIT_COMMIT
#define RUN_GIT_COMMIT "no-git"
#endif
#ifndef RUN_BUILD_TIME
#define RUN_BUILD_TIME "?"
#endif

void run_meta_print(const char *role)
{
    ESP_LOGI("RUN_META", "role=%s fw=%s build=%s", role, RUN_GIT_COMMIT, RUN_BUILD_TIME);
}

const char *run_meta_commit(void)
{
    return RUN_GIT_COMMIT;
}
