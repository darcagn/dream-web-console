// http code based on kos/examples/dreamcast/network/httpd/ by Megan Potter
// -ack

#include <stdarg.h>
#include <kos.h>
#include <kos/mutex.h>
#include <sys/socket.h>
#include <conio/conio.h>
#include <unistd.h>

extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);
KOS_INIT_FLAGS(INIT_DEFAULT | INIT_NET);

mutex_t cdrom_mutex = MUTEX_INITIALIZER;

#define TRACK_AUDIO 0
#define TRACK_DATA  4

#define SUB_NONE         0
#define SUB_SYSCALL      1
#define SUB_READ_SECTOR  2

typedef struct http_state {
  int                      socket;
  struct sockaddr_in       client;
  socklen_t                client_size;
  kthread_t                *thd;
} http_state_t;

typedef struct {
  char name[24];
  int start;
  int end;
} cdrom_info_t;

cdrom_info_t cdrom_info[] = {
  { "Title",             0x80, 0xFF },
  { "Media ID",          0x20, 0x24 },
  { "Media Config",      0x25, 0x2f },
  { "Regions",           0x30, 0x37 },
  { "Peripheral String", 0x38, 0x3F },
  { "Product Number",    0x40, 0x49 },
  { "Version",           0x4A, 0x4F },
  { "Release Date",      0x50, 0x5F },
  { "Manufacturer ID",   0x70, 0x7F },
};

// get the cdrom toc, its upto the caller to reinit the cdrom
#define IPBIN_TOC_OFFSET 0xFC
int get_toc(CDROM_TOC *toc, int session, int ipbintoc) {

  memset(toc, 0x0, sizeof(CDROM_TOC));
  char ipbin[2352];

  // use the TOC thats in the ip.bin instead of what the cdrom tells us
  if(session == 1 && ipbintoc) {
    if(cdrom_reinit_ex(-1,-1,-1) != ERR_OK) {
      conio_printf("get_toc(): failed to reinit cdrom\n");
      return ERR_SYS;
    }

    // read in ip.bin
    if(cdrom_read_sectors_ex(ipbin, 45150, 1, CDROM_READ_PIO) != ERR_OK) {
      conio_printf("get_toc(): failed to read ip.bin\n");
      return ERR_SYS;
    }

    // verify it looks like a dreamcast disc
    if(strncasecmp("SEGA SEGAKATANA", ipbin, strlen("SEGA SEGAKATANA")) != 0) {
      conio_printf("get_toc(): invalid ip.bin\n");
      return ERR_SYS;
    }

    conio_printf("get_toc(): using ip.bin for TOC\n");
    memcpy(toc,ipbin+IPBIN_TOC_OFFSET,sizeof(CDROM_TOC));
    return ERR_OK;
  }

  if(cdrom_read_toc(toc, session) != ERR_OK) {
    conio_printf("get_toc(): cdrom_read_toc failed for session: %d\n",
                 session);
    return ERR_SYS;
  }
  return ERR_OK;
}


// send http response, if len > 0 include length header
void send_ok(http_state_t *hs, const char *ct, int len) {
  char buf[512];

  if(len > 0) {
    sprintf(buf, "HTTP/1.0 200 OK\r\nContent-type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", ct, len);
  } else {
    sprintf(buf, "HTTP/1.0 200 OK\r\nContent-type: %s\r\nConnection: close\r\n\r\n", ct);
  }
  write(hs->socket, buf, strlen(buf));
}

void send_error(http_state_t *hs, int errcode, const char *str) {
  char buf[512];
  sprintf(buf, "HTTP/1.0 %d %s\r\nContent-type: text/html\r\n\r\n",
          errcode, str);
  write(hs->socket, buf, strlen(buf));
  sprintf(buf, "%d %s", errcode, str);
  write(hs->socket, buf, strlen(buf));
}

// send a memory region
void send_memory(http_state_t *hs, uint32 start, uint32 end) {
  uint32 data_size, offset, rv;
  char tmp;

  conio_printf("sending memory 0x%lx - 0x%lx, socket %d\n", start, end, hs->socket);

  data_size = end - start + 1;
  if(data_size <= 0) {
    send_error(hs, 404, "end memory location before start");
    return;
  }

  send_ok(hs, "application/octet-stream", data_size);


  // write() doesnt like memory location to be zero
  // send that 1 byte, then the reset
  if(start == 0) {
    memcpy(&tmp, 0, 1);
    write(hs->socket, &tmp, 1);
    data_size--;
    start++;
  }

  offset = 0;
  while(data_size > 0) {
    rv = write(hs->socket, (char*)start+offset, data_size);
    if(rv <= 0) {
        return;
    }

    data_size -= rv;
    offset += rv;
  }
}

// send file from romdisk.img
// for some reason this file is causing a crash
// when (hs->socket); is called later on
// maybe something to do with fs_open/close?
#define FSFILE_BUFFER        32768
void send_fsfile(http_state_t *hs, char *file) {
  char *buf;
  file_t fd, count, offset, rv;


  conio_printf("sending fsfile %s, socket %d\n", file, hs->socket);

  fd = fs_open(file, O_RDONLY);
  if(fd < 0) {
    send_error(hs, 404, "File not found or unreadable");
    return;
  }

  buf = malloc(FSFILE_BUFFER);
  if(buf == NULL) {
    send_error(hs, 404, "ERROR: malloc failure in send_fsfile()");
    conio_printf("malloc failure in send_fsfile(), socket %d\n", hs->socket);
    return;
  }

  send_ok(hs, "application/octet-stream", -1);
  while((count = fs_read(fd, buf, FSFILE_BUFFER)) != 0) {
    offset = 0;
    while(count > 0) {
      rv = write(hs->socket, buf+offset, count);
      if(rv <= 0) {
        goto send_fsfile_out;
      }
      count -= rv;
      offset += rv;
    }
  }

send_fsfile_out:
 fs_close(fd);
 free(buf);

}

// send cdrom track
#define MAX_SECTOR_READ      128
#define SECTOR_BUFFER        (2352*(MAX_SECTOR_READ + 1))
#define MMAP_NOCACHE         0x20000000
void send_track(http_state_t *hs, int ipbintoc, int session, int track, int secpart, int cdxa,
                int sector_size, int gap, int dma, int sector_read,
                int sub, int abort, int retry) {
  char *buf, *nocache;
  int sector_start, sector_end, track_size;
  int sector;
  CDROM_TOC toc;
  int attempt, rv;
  int offset, data_size;
  buf = nocache = NULL;

  conio_printf("s:%d toc:%d t:%d p:%d cdxa:%d ss:%d dma:%d sr:%d sub:%d retry:%d ab:%d\n",
           ipbintoc, session, track, secpart, cdxa, sector_size, dma, sector_read,
           sub, retry, abort);

  if(sector_size <= 0) {
    conio_printf("WARN: invalid sector_size %d, forcing 2352\n", sector_size);
    sector_size = 2352;
  }

  if(dma != CDROM_READ_PIO) {
    dma = CDROM_READ_DMA;
  }

  if(sector_read > MAX_SECTOR_READ) {
    conio_printf("WARN: sector_read %d > MAX, forcing %d\n", sector_read,
             MAX_SECTOR_READ);
    sector_read = MAX_SECTOR_READ;
  }

  if(sector_read <= 0) {
    conio_printf("WARN: sector_read %d <= 0, forcing 1\n", sector_read);
    sector_read = 1;
  }

  if(sub < 0 || sub > 2) {
    conio_printf("WARN: invalid sub %d, forcing %d\n", sub, SUB_NONE);
    sub = SUB_NONE;
  }

  if(sub) {
    if(sector_read != 1) {
      conio_printf("WARN: sub enabled, sector_read forced to 1\n");
      sector_read = 1;
    }
    if(dma != 0) {
      conio_printf("WARN: sub enabled, dma read disabled\n");
      dma = 0;
    }
  }

  if(retry < 0) {
    conio_printf("WARN: retry %d < 0, forcing 0\n", retry);
    retry = 0;
  }

  // allow session/track to act as sector start/end
  if(session >= 100 && track >= 100 && track >= session) {
    sector_start = session;
    sector_end = track + 1;

  // determine the start/end sector from toc
  } else {
    if(get_toc(&toc, session - 1, ipbintoc) != ERR_OK) {
      send_error(hs, 404, "ERROR: toc read failed");
      goto send_track_out;
    }

    if(track < TOC_TRACK(toc.first) || track > TOC_TRACK(toc.last)) {
      send_error(hs, 404, "ERROR: invalid track for session");
      goto send_track_out;
    }

    sector_start = TOC_LBA(toc.entry[track-1]);
    if(track == TOC_TRACK(toc.last)) {
      sector_end = TOC_LBA(toc.leadout_sector) - gap;
    } else {
      sector_end = TOC_LBA(toc.entry[track]) - gap;
    }
  }

  if(cdrom_reinit_ex(secpart, cdxa, sector_size) != ERR_OK) {
    send_error(hs, 404, "ERROR: cdrom_reinit() failed");
    goto send_track_out;
  }

  track_size = sector_size * (sector_end - sector_start);
  if(sub) {
    track_size += 96 * (sector_end - sector_start);
  }

  buf = memalign(32, SECTOR_BUFFER);
  if(buf == NULL) {
    send_error(hs, 404, "ERROR: malloc failure in send_track()");
    conio_printf("malloc failure in send_track(), socket %d\n", hs->socket);
    goto send_track_out;
  }

  // malloc will return memory in the P1 memory map area (0x80000000-0x9FFFFFFF)
  // MMAP_NOCACHE will bump it into P2 memory map area (0xA0000000-0xBFFFFFFF)
  // which disables caching to avoid any issues with dma transfers
  nocache = (void*)((ptr_t) buf | MMAP_NOCACHE);

  sector = sector_start;
  send_ok(hs, "application/octet-stream", track_size);
  do {

    if(sector_end - sector < sector_read) {
      sector_read = sector_end - sector;
    }

    // poison the entire malloc
    memset(nocache, 'Q', SECTOR_BUFFER);
    attempt = 0;

    do {

      if(attempt != 0)
        conio_printf("READ ERROR: sector: %d, retrying\n", sector);

      rv = cdrom_read_sectors_ex(nocache, sector, sector_read, dma);
      attempt++;

    } while(rv != ERR_OK && attempt < retry);

    if(rv != ERR_OK) {
        conio_printf("READ ERROR: sector: %d, FAILED\n", sector);
        if(abort)
          goto send_track_out;
    }

    data_size = (sector_size * sector_read);

    // syscall method of getting sub channel data
    if(sub == SUB_SYSCALL) {

      rv = cdrom_get_subcode(nocache+sector_size, 100, CD_SUB_Q_CHANNEL);
      if(rv != ERR_OK) {
        conio_printf("SUB ERROR: sector: %d, FAILED\n", sector);
        if(abort)
          goto send_track_out;
      }
      // shift over 4 bytes to get rid of unneeded header
      memcpy(nocache+sector_size, nocache+sector_size+4, 96);
      data_size += 96;

    // cdrom_read_sectors_ex method of getting sub channel data
    } else if(sub == SUB_READ_SECTOR) {

      if(cdrom_change_datatype(16, cdxa, sector_size) != ERR_OK) {
        conio_printf("SUB ERROR: failed to set datatype (p=16)\n");
        if(abort)
          goto send_track_out;
      }

      rv = cdrom_read_sectors_ex(nocache+sector_size, sector, sector_read, dma);
      if(rv != ERR_OK) {
        conio_printf("SUB ERROR: sector: %d, FAILED\n", sector);
        if(abort)
          goto send_track_out;
      }

      // shift over 4 bytes to get rid of unneeded header
      memcpy(nocache+sector_size, nocache+sector_size+4, 96);
      data_size += 96;

      // reset the read datatype
      if(cdrom_change_datatype(secpart, cdxa, sector_size) != ERR_OK) {
        conio_printf("SUB ERROR: failed to reset datatype (p=%d)\n", secpart);
        if(abort)
          goto send_track_out;
      }

    }

    offset = 0;
    while(data_size > 0) {
      rv = write(hs->socket, nocache+offset, data_size);
      if(rv <= 0)
        goto send_track_out;

      data_size -= rv;
      offset += rv;
    }

    sector += sector_read;
  } while(sector < sector_end);

send_track_out:

  if(buf != NULL)
    free(buf);
}

// trim begin/end spaces and copy into output buffer
void trim_cdrom_info(char *input, char *output, int size) {
  char *p;
  char *o;

  p = input;
  o = output;

  // skip any spaces at the beginning
  while(*p == ' ' && input + size > p)
    p++;

  // copy rest to output buffer
  while(input + size > p) {
    *o = *p;
    p++;
    o++;
  }

  // make sure output buf is null terminated
  *o = '\0';
  o--;

  // remove trailing spaces
  while(*o == ' ' && o > output) {
    *o='\0';
    o--;
  }
}

// send cdrom toc
#define TOC_BUFFER 32768
void send_toc(http_state_t *hs, int ipbintoc) {
  char *output, *cursor;
  char sector[2352], info[129];
  int output_size, i, rv;
  int sendgdi = 0;
  CDROM_TOC toc;
  int session, track, track_size, track_type, track_start, track_end;
  int sector_size, gap;

  conio_printf("sending TOC, socket %d\n", hs->socket);

  output = malloc(TOC_BUFFER);
  if(output == NULL) {
    send_error(hs, 404, "ERROR: malloc failure in send_toc()");
    conio_printf("malloc failure in send_toc(), socket %d\n", hs->socket);
    return;
  }

  cursor = output;
  sprintf(cursor, "<html><head><title>Listing of %s</title></head></html>\n<body bgcolor=\"white\">\n", "cdrom");
  cursor += strlen(cursor);
  sprintf(cursor, "<h4>Listing of %s</h4>\n<hr>\n<table width=480>\n", "cdrom");
  cursor += strlen(cursor);

  if(cdrom_reinit_ex(-1,-1,-1) != ERR_OK) {
    sprintf(cursor, "ERROR: cdrom_reinit_ex() failed");
    cursor += strlen(cursor);
    goto send_toc_out;
  }

  sprintf(cursor, "<tr cellpadding=2><td bgcolor=#CCCCCC>CD-ROM</td><td colspan=3 bgcolor=#CCCCCC>INFO</td></tr>\n");
  cursor += strlen(cursor);

  if(cdrom_read_sectors_ex(sector, 45150, 1, CDROM_READ_PIO) == ERR_OK) {
    // verify it looks like a dreamcast disc
    if(strncasecmp("SEGA SEGAKATANA", sector, strlen("SEGA SEGAKATANA")) == 0) {

      sendgdi = 1;
      for(i = 0; i < sizeof(cdrom_info) / sizeof(cdrom_info_t);i++) {
        trim_cdrom_info(sector + cdrom_info[i].start, info,
                        cdrom_info[i].end - cdrom_info[i].start + 1);
        sprintf(cursor, "<tr><td>%s</td><td colspan=3>%s</td></tr>\n",cdrom_info[i].name, info);
        cursor += strlen(cursor);
      }
      sprintf(cursor, "<tr><td>TOC</td><td colspan=3>%s</td></tr>\n", (ipbintoc ? "IP.BIN" : "DISC"));
      cursor += strlen(cursor);

   } else {
      sprintf(cursor, "<tr><td colspan=4>WARN: Not Dreamcast disc, bad sig</td></td>\n");
      cursor += strlen(cursor);
    }
  } else {
    sprintf(cursor, "<tr><td colspan=4>WARN: Not Dreamcast disc, read failed</td></td>\n");
    cursor += strlen(cursor);
  }

  sprintf(cursor, "<tr cellpadding=2><td bgcolor=#CCCCCC>Track</td><td align=right bgcolor=#CCCCCC>Start Sector</td><td align=right bgcolor=#CCCCCC>End Sector</td><td align=right bgcolor=#CCCCCC>Track Size</td></tr>");
  cursor += strlen(cursor);

  for(session = 0; session < 2; session++) {
    if(get_toc(&toc, session, (session == 1 ? ipbintoc : 0)) != ERR_OK) {
      conio_printf("WARN: Failed to read toc, session %d\n", session + 1);
      continue;
    }

    for(track = TOC_TRACK(toc.first);track <= TOC_TRACK(toc.last); track++) {
      track_type = TOC_CTRL(toc.entry[track-1]);
      track_start = TOC_LBA(toc.entry[track-1]);

      gap = 0;
      if(track == TOC_TRACK(toc.last)) {
        track_end = TOC_LBA(toc.leadout_sector) - 1;
      } else {

        // set 150 gap between tracks of different types
        if(track_type != TOC_CTRL(toc.entry[track])) {
          track_end = TOC_LBA(toc.entry[track]) - 1;
          gap = 150;
          track_end -= gap;
        } else {
          track_end = TOC_LBA(toc.entry[track]) - 1;
        }
      }

      sector_size = 2352;
      track_size = sector_size * (track_end - track_start + 1);

      // href part
      sprintf(cursor, "<tr><td><a href=\"track%02d.%s?ipbintoc%d_session%02d_p%d_cdxa%d_sector_size%d_gap%d_dma%d_sector_read%d_sub%d_abort%d_retry%d\">",
              track,
              (track_type == TRACK_DATA ? "bin" : "raw"),
              (session == 1 ? ipbintoc : 0),
              session + 1, 4096, 0, 2352, gap, 1, 16, 0, 1, 5);
      cursor += strlen(cursor);

      // rendered part
      sprintf(cursor, "track%02d.%s</a></td><td align=right>%d</td><td align=right>%d</td><td align=right>%d</td></tr>\n",
              track, (track_type == TRACK_DATA ? "bin" : "raw"),
              track_start, track_end, track_size);
      cursor += strlen(cursor);
    }
  }

  sprintf(cursor, "<tr bgcolor=#CCCCCC><td>Misc</td><td colspan=3>Info</td></tr>");
  cursor += strlen(cursor);

  sprintf(cursor, "<tr><td><a href=\"dc_bios.bin\">dc_bios.bin</a></td><td colspan=3>Dreamcast BIOS (0x00000000 - 0x001FFFFF)</td></tr>");
  cursor += strlen(cursor);

  sprintf(cursor, "<tr><td><a href=\"dc_flash.bin\">dc_flash.bin</a></td><td colspan=3>Dreamcast Flash (0x00200000 - 0x0021FFFF)</td></tr>");
  cursor += strlen(cursor);

  sprintf(cursor, "<tr><td><a href=\"syscalls.bin\">syscalls.bin</a></td><td colspan=3>Dreamcast Syscalls (0x8C000000 - 0x8C007FFF)</td></tr>");
  cursor += strlen(cursor);

  if(sendgdi) {
    sprintf(cursor, "<tr><td><a href=\"disc.gdi%s\">disc.gdi</a></td><td colspan=3>nullDC gdi file</td></tr>",
            (ipbintoc ? "?ipbintoc" : ""));
    cursor += strlen(cursor);
  }

send_toc_out:

  sprintf(cursor, "</table><hr>httpd-ack <a href=\"httpd-ack-source-%s.zip\">v%s</a> server</body></html>",
          VERSION, VERSION);
  cursor += strlen(cursor);

  output_size = strlen(output);

  send_ok(hs, "text/html", -1);
  cursor = output;
  while(output_size > 0) {
    rv = write(hs->socket, cursor, output_size);
    if (rv <= 0) {
      free(output);
      return;
    }
    output_size -= rv;
    cursor += rv;
  }
  free(output);
}

// send gdi file used by nullDC
#define GDI_BUFFER	4096
void send_gdi(http_state_t *hs, int ipbintoc) {
  CDROM_TOC toc1, toc2;
  char *output, *cursor;
  int output_size, track, rv;

  conio_printf("sending gdi file, socket %d\n", hs->socket);

  if(get_toc(&toc1, 0, 0) != ERR_OK) {
    conio_printf("FATAL: Failed to read toc, session %d\n", 1);
    send_error(hs, 404, "No disc in drive? (error reading session 1 TOC)");
    return;
  }

  if(get_toc(&toc2, 1, ipbintoc) != ERR_OK) {
    conio_printf("FATAL: Failed to read toc, session %d\n", 2);
    send_error(hs, 404, "Not a dreamcast disc (2nd session)");
    return;
  }

  output = malloc(GDI_BUFFER);
  if(output == NULL) {
    send_error(hs, 404, "ERROR: malloc failure in send_gdi()\n");
    conio_printf("malloc failure in send_gdi(), socket %d", hs->socket);
    return;
  }

  cursor = output;

  // total track on first line
  sprintf(cursor, "%ld\r\n", TOC_TRACK(toc2.last));
  cursor += strlen(cursor);

  // remaining lines are
  // track_num track_start track_type sector_size filename offset

  // session1
  for(track = TOC_TRACK(toc1.first); track <= TOC_TRACK(toc1.last); track++) {
    sprintf(cursor, "%d %ld %ld %d track%02d.%s %d\r\n", track,
            TOC_LBA(toc1.entry[track-1]) - 150,
            TOC_CTRL(toc1.entry[track-1]),
            2352, track,
            (TOC_CTRL(toc1.entry[track-1]) == TRACK_DATA ? "bin" : "raw"),
            0);
    cursor += strlen(cursor);
  }

  // session2
  for(track = TOC_TRACK(toc2.first); track <= TOC_TRACK(toc2.last); track++) {
    sprintf(cursor, "%d %ld %ld %d track%02d.%s %d\r\n", track,
            TOC_LBA(toc2.entry[track-1]) - 150,
            TOC_CTRL(toc2.entry[track-1]),
            2352, track,
            (TOC_CTRL(toc2.entry[track-1]) == TRACK_DATA ? "bin" : "raw"),
            0);
    cursor += strlen(cursor);
  }


  output_size = strlen(output);
  send_ok(hs, "text/plain", output_size);

  cursor = output;
  while(output_size > 0) {
    rv = write(hs->socket, cursor, output_size);
    if (rv <= 0) {
      free(output);
      return;
    }
    output_size -= rv;
    cursor += rv;
  }

  free(output);
}

// This is undoubtedly very slow
int readline(int sock, char *buf, int bufsize) {
  int r, rt;
  char c;

  rt = 0;
  do {
    r = read(sock, &c, 1);
    if(r == 0)
      return -1;
    if(rt < bufsize)
      buf[rt++] = c;
  } while (c != '\n');

  buf[rt-1] = 0;
  if(buf[rt-2] == '\r')
    buf[rt-2] = 0;

  return 0;
}

int read_headers(http_state_t *hs, char *buffer, int bufsize) {
  char fn[256];
  int i, j;

  for(i=0; ; i++) {
    if(readline(hs->socket, buffer, bufsize) < 0) {
      if(i > 0)
        return 0;
      else
        return -1;
    }
    if(strlen(buffer) == 0)
       break;

    if(i == 0) {
      if(!strncmp(buffer, "GET", 3)) {
        for(j=4; buffer[j] && buffer[j] != 32 && j<256; j++) {
          fn[j-4] = buffer[j];
        }
        fn[j-4] = 0;
      }
    }
  }

  strcpy(buffer, fn);
  return 0;
}

// client thread
#define HEADER_BUFSIZE 1024
void *client_thread(void *p) {
  http_state_t * hs = (http_state_t *)p;
  char *buf = NULL;
  int ipbintoc, session, track, secpart, cdxa, sector_size, gap, dma;
  int sector_read, sub, abort, retry;
  unsigned long memory_start, memory_end;

  conio_printf("httpd: client thread started, socket %d\n", hs->socket);

  buf = malloc(HEADER_BUFSIZE);
  if(buf == NULL) {
    conio_printf("httpd: malloc failure in client_thread(), socket %d\n",
             hs->socket);
    goto client_thread_out;
  }

  if(read_headers(hs, buf, HEADER_BUFSIZE) < 0) {
    goto client_thread_out;
  }

  conio_printf("httpd: client request '%s', socket %d\n", buf, hs->socket);

  // deal with requests
  if((sscanf(buf, "/track%d.bin?ipbintoc%d_session%d_p%d_cdxa%d_sector_size%d_gap%d_dma%d_sector_read%d_sub%d_abort%d_retry%d",
           &track, &ipbintoc, &session, &secpart, &cdxa, &sector_size, &gap,
           &dma, &sector_read, &sub, &abort, &retry) == 12) ||
     (sscanf(buf, "/track%d.raw?ipbintoc%d_session%d_p%d_cdxa%d_sector_size%d_gap%d_dma%d_sector_read%d_sub%d_abort%d_retry%d",
           &track, &ipbintoc, &session, &secpart, &cdxa, &sector_size, &gap,
           &dma, &sector_read, &sub, &abort, &retry) == 12)) {
    if(mutex_trylock(&cdrom_mutex) == 0) {
      send_track(hs, ipbintoc, session, track, secpart, cdxa, sector_size,
                 gap, dma, sector_read, sub, abort, retry);
      mutex_unlock(&cdrom_mutex);
    } else {
      send_error(hs, 404, "ERROR: track dump refused, cdrom is locked by another thread");
    }

  } else if(strcmp(buf, "/httpd-ack-source-" VERSION ".zip") == 0) {
    send_fsfile(hs, "/rd/httpd-ack-source-" VERSION ".zip");

  } else if(sscanf(buf, "/memory_start%lu_end%lu.bin", &memory_start, &memory_end) == 2) {
    send_memory(hs, memory_start, memory_end);

  } else if(strcmp(buf, "/disc.gdi") == 0) {
    if(mutex_trylock(&cdrom_mutex) == 0) {
      send_gdi(hs, 0);
      mutex_unlock(&cdrom_mutex);
    } else {
      send_error(hs, 404, "ERROR: disc.gdi refused, cdrom is locked by another thread");
    }

  } else if(strcmp(buf, "/disc.gdi?ipbintoc") == 0) {
    if(mutex_trylock(&cdrom_mutex) == 0) {
      send_gdi(hs, 1);
      mutex_unlock(&cdrom_mutex);
    } else {
      send_error(hs, 404, "ERROR: disc.gdi refused, cdrom is locked by another thread");
    }

  } else if(strcmp(buf, "/dc_bios.bin") == 0) {
    send_memory(hs, 0xA0000000, 0xA01FFFFF);

  } else if(strcmp(buf, "/dc_flash.bin") == 0) {
    send_memory(hs, 0xA0200000, 0xA021FFFF);

  } else if(strcmp(buf, "/syscalls.bin") == 0) {
    send_memory(hs, 0xAC000000, 0xAC007FFF);

  } else if(strcmp(buf, "/cdrom_spin_down") == 0) {
    if(mutex_trylock(&cdrom_mutex) == 0) {
      cdrom_spin_down();
      mutex_unlock(&cdrom_mutex);
    } else {
      send_error(hs, 404, "ERROR: cdrom spin down refusted, cdrom is locked by another thread");
    }
  } else if(strcmp(buf, "/") == 0 || strcmp(buf, "/index.html") == 0) {
    if(mutex_trylock(&cdrom_mutex) == 0) {
      send_toc(hs, 0);
      mutex_unlock(&cdrom_mutex);
    } else {
      send_error(hs, 404, "TOC list refused, cdrom locked by another thread");
    }

  } else if(strcmp(buf, "/?ipbintoc") == 0 ||
            strcmp(buf, "/index.html?ipbintoc") == 0) {
    if(mutex_trylock(&cdrom_mutex) == 0) {
      send_toc(hs, 1);
      mutex_unlock(&cdrom_mutex);
    } else {
      send_error(hs, 404, "TOC list refused, cdrom locked by another thread");
    }
  } else {
    send_error(hs, 404, "File not found.");
  }

client_thread_out:
  conio_printf("httpd: closed connection, socket %d\n", hs->socket);
  if(buf != NULL)
    free(buf);

  close(hs->socket);
  free(hs);
  return NULL;
}

// main httpd thread
void *httpd(void *arg) {
  int listenfd;
  struct sockaddr_in saddr;
  fd_set readset, writeset;
  int maxfdp1;
  http_state_t *hs;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if(listenfd < 0) {
    conio_printf("httpd: socket create failed\n");
    return NULL;
  }

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_port = htons(80);

  if(bind(listenfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    conio_printf("httpd: bind failed\n");
    close(listenfd);
    return NULL;
  }

  if(listen(listenfd, 10) < 0) {
    conio_printf("httpd: listen failed\n");
    close(listenfd);
    return NULL;
  }

  conio_printf("httpd: listening for connections on socket %d\n", listenfd);

  for ( ; ; ) {
    maxfdp1 = listenfd + 1;

    FD_ZERO(&readset);
    FD_ZERO(&writeset);
    FD_SET(listenfd, &readset);

    if(select(maxfdp1, &readset, &writeset, 0, 0) == 0)
      continue;

    // Check for new incoming connections
    if(FD_ISSET(listenfd, &readset)) {

      hs = calloc(1, sizeof(http_state_t));
      hs->client_size = sizeof(hs->client);
      hs->socket = accept(listenfd, (struct sockaddr *)&hs->client,
                          &hs->client_size);
      conio_printf("httpd: connect from %08lx, port %d, socket %d\n",
                hs->client.sin_addr.s_addr, hs->client.sin_port, hs->socket);
      if(hs->socket < 0) {
        free(hs);
      } else {
        // create thread for new client
        hs->thd = thd_create(1, client_thread, hs);
      }
    }
  }
}

int main(int argc, char **argv) {
  flashrom_ispcfg_t ispcfg;

  cdrom_init();

  // auto spin down cdrom once we are loaded
  cdrom_spin_down();

  pvr_init_defaults();

  conio_init(CONIO_TTY_PVR, CONIO_INPUT_LINE);
  conio_set_theme(CONIO_THEME_MATRIX);

  conio_printf("httpd-ack v%s\n", VERSION);
  conio_printf("Press Start to Quit\n");

  if(flashrom_get_ispcfg(&ispcfg) == 0) {

    // complain if ip is 0.0.0.0
    if(*(uint32*)ispcfg.ip == 0) {
      conio_printf("ERROR: isp setting IP is 0.0.0.0\n");
    } else {
      conio_printf("Connect to http://%d.%d.%d.%d/\n", ispcfg.ip[0],
                   ispcfg.ip[1], ispcfg.ip[2], ispcfg.ip[3]);
      thd_create(0, httpd, NULL);
    }
  } else {
    conio_printf("ERROR: Unable to load isp settings from flash\n");
  }

  for ( ; ; ) {
    MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, st)
    if (st->buttons & CONT_START) {
      return 0;
    }
    MAPLE_FOREACH_END()
  }
  return 0;
}
