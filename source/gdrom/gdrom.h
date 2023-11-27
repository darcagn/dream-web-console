#ifndef DWC_GDROM_H
#define DWC_GDROM_H

#include "../common.h"

#define DEFAULT_DUMP_TYPE_DATA  0
#define DEFAULT_DUMP_TYPE_RAW   1
#define DEFAULT_DUMP_TYPE_SUB   2

#define TOC_TYPE_DEFAULT        false
#define TOC_TYPE_IPBIN          true

#define TRACK_AUDIO             0
#define TRACK_DATA              4

#define SUB_NONE                0
#define SUB_SYSCALL             1
#define SUB_READ_SECTOR         2

#define DATASEL_OTHER           0
#define DATASEL_DATA            1
#define DATASEL_SUBHEADER       2
#define DATASEL_HEADER          3

#define TRACK_TYPE_ANY          0
#define TRACK_TYPE_CDDA         1
#define TRACK_TYPE_MODE1        2
#define TRACK_TYPE_MODE2        3
#define TRACK_TYPE_MODE2_FORM1  4
#define TRACK_TYPE_MODE2_FORM2  5
#define TRACK_TYPE_MODE2_NONXA  6
#define TRACK_TYPE_UNKNOWN      7

void send_track(http_state_t *hs, bool ipbintoc, size_t session, size_t track, unsigned datasel,
                unsigned trktype, size_t secsz, size_t gap, unsigned dma, size_t secrd,
                unsigned sub, bool abort, size_t retry);
void send_gdi(http_state_t *hs, bool ipbintoc, unsigned defaulttype);
void send_disc_index(http_state_t *hs, bool ipbintoc, unsigned defaulttype);
unsigned get_toc(CDROM_TOC *toc, size_t session, bool ipbintoc);

#endif /* DWC_GDROM_H */
