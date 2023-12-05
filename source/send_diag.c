#include "common.h"

void send_diag(http_state_t *hs) {
    WEBPAGE_START("Dream Web Console - System Diagnostics");
    WEBPAGE_WRITE("<h1>Dream Web Console - System Diagnostics</h1>\n");
    WEBPAGE_WRITE("<p>The diagnostics page is currently unimplemented. Come back later for more.</p>\n");
    WEBPAGE_FINISH();
}