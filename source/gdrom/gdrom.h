#ifndef DWC_GDROM_H
#define DWC_GDROM_H

#include "../common.h"

typedef enum dump_type {
    DEFAULT_DUMP_TYPE_DATA    = 0,
    DEFAULT_DUMP_TYPE_RAW     = 1,
    DEFAULT_DUMP_TYPE_SUB     = 2,
} dump_type_t;

typedef enum track_content {
    TRACK_AUDIO               = 0,
    TRACK_DATA                = 4,
} track_content_t;

typedef enum sub_parameter {
    SUB_NONE                  = 0,
    SUB_SYSCALL               = 1,
    SUB_READ_SECTOR           = 2,
} sub_parameter_t;

typedef enum datasel_parameter {
    DATASEL_OTHER             = 0,
    DATASEL_DATA              = 1,
    DATASEL_SUBHEADER         = 2,
    DATASEL_HEADER            = 3,
} datasel_parameter_t;

typedef enum track_type {
    TRACK_TYPE_ANY            = 0,
    TRACK_TYPE_CDDA           = 1,
    TRACK_TYPE_MODE1          = 2,
    TRACK_TYPE_MODE2          = 3,
    TRACK_TYPE_MODE2_FORM1    = 4,
    TRACK_TYPE_MODE2_FORM2    = 5,
    TRACK_TYPE_MODE2_NONXA    = 6,
    TRACK_TYPE_UNKNOWN        = 7,
} track_type_t;

typedef enum track_file_ext {
    TRACK_FILE_EXT_BIN        = 0,
    TRACK_FILE_EXT_ISO        = 1,
    TRACK_FILE_EXT_RAW        = 2,
} track_file_ext_t;

#define TOC_TYPE_DEFAULT        false
#define TOC_TYPE_IPBIN          true

extern mutex_t gdrom_mutex;

void gdrom_spin_down(http_state_t *hs);
void send_track(http_state_t *hs, bool ipbintoc, size_t session, size_t track, unsigned datasel,
                unsigned trktype, size_t secsz, size_t gap, unsigned dma, size_t secrd,
                unsigned sub, bool abort, size_t retry, unsigned track_file_ext);
void send_gdi(http_state_t *hs, bool ipbintoc, unsigned defaulttype, bool view);
void send_disc_index(http_state_t *hs, bool ipbintoc, unsigned defaulttype);
unsigned get_toc(CDROM_TOC *toc, size_t session, bool ipbintoc);

#endif /* DWC_GDROM_H */
