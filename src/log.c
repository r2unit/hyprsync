#include "log.h"

hs_log_level hs_current_log_level = HS_LOG_INFO;

void hs_log_set_level(hs_log_level level) {
    hs_current_log_level = level;
}
