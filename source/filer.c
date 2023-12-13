#include "common.h"

void send_dir(const char *path, http_state_t * hs, file_t fd) {
    bool prepend_slash = false;
    bool append_slash = false;
    if((path[0]) != '/')
        prepend_slash = true;
    if((path[strlen(path) - 1]) != '/')
        append_slash = true;

    WEBPAGE_START("Dream Web Console - File Browser");
    WEBPAGE_WRITE("<h1>Dream Web Console - Browsing ");
    if(prepend_slash)
        WEBPAGE_WRITE("/");
    WEBPAGE_WRITE("%s", path);
    if(append_slash)
        WEBPAGE_WRITE("/");
    WEBPAGE_WRITE("</h1>\n<hr>\n<table>\n");
    WEBPAGE_WRITE("<tr cellpadding=\"2\"><td class=\"top\">Filename</td>"
                  "<td colspan=\"3\" class=\"top\">Bytes</td></tr>");

    /* Write out all file and directory entries within this directory */
    dirent_t *d;
    while((d = fs_readdir(fd))) {
        WEBPAGE_WRITE("<tr><td><a href=\"?%s%s%s%s%s\">%s%s</a></td>",
                      /* Path -- if slashes must be
                       * added to path, do it here */
                      (prepend_slash ? "/" : ""), path, (append_slash ? "/" : ""),
                      /* Filename in URL -- if directory,
                       * append slash to end of name in URL */
                      d->name, (d->size < 0 ? "/" : ""),
                      /* Filename as displayed -- if directory,
                       * append slash to end of name as displayed */
                      d->name, (d->size < 0 ? "/" : ""));
        /* Write filesize in bytes if file */
        if(d->size < 0)
            WEBPAGE_WRITE("<td>dir</td>");
        else
            WEBPAGE_WRITE("<td>%d</td>", d->size);
        WEBPAGE_WRITE("</tr>\n");
    }

    fs_close(fd);

    WEBPAGE_WRITE("</table>\n");
    WEBPAGE_FINISH();
}

#define MAX_FILE_BUFFER   32768

void send_fs(http_state_t *hs, char *path) {
    /* If this is a directory, divert to send_dir file browser */
    file_t fd = fs_open(path, O_RDONLY | O_DIR);
    if(fd >= 0) {
        send_dir(path, hs, fd);
    } else {
        /* It's a file. Let's prepare to view or download the file */
        DWC_LOG("http: sending file %s, socket %d\n", strrchr(path, '/') + 1, hs->socket);
        fd = fs_open(path, O_RDONLY);
        if(fd < 0) {
            send_error(hs, 404, "File not found or unreadable");
            return;
        }

        /* Determine Content-type and Content-Disposition for file */
        const char *content = "application/octet-stream";
        bool attachment = true;
        char *ext = strrchr(path, '.');
        if(ext) {
            ext++;

            if(!strcasecmp(ext, "jpg")) {
                content = "image/jpeg";
                attachment = false;
            } else if(!strcasecmp(ext, "png")) {
                content = "image/png";
                attachment = false;
            } else if(!strcasecmp(ext, "gif")) {
                content = "image/gif";
                attachment = false;
            } else if(!strcasecmp(ext, "txt")) {
                content = "text/plain";
                attachment = false;
            } else if(!strcasecmp(ext, "mp3")) {
                content = "audio/mpeg";
                attachment = false;
            } else if(!strcasecmp(ext, "ogg")) {
                content = "application/ogg";
                attachment = false;
            } else if(!strcasecmp(ext, "html") ||
                      !strcasecmp(ext, "htm")) {
                content = "text/html";
                attachment = false;
            }
        }

        /* Get filesize for requested file */
        fs_seek(fd, 0L, SEEK_END);
        size_t output_size = fs_tell(fd);
        fs_seek(fd, 0L, SEEK_SET);

        /* Allocate buffer to store file data while sending out */
        unsigned buf_sz = (output_size > MAX_FILE_BUFFER ?
                           MAX_FILE_BUFFER : output_size);
        char *output = calloc(1, buf_sz);
        if(!output) {
            send_error(hs, 404, "ERROR: malloc failure in send_fs()");
            DWC_LOG("malloc failure in send_fs(), socket %d\n", hs->socket);
            goto send_out;
        }

        /* FIXME: Why is this sometimes super fast (> 1MB/s)
         * but sometimes super slow (< 70KB/s) ? */
        /* Write output to socket */
        send_ok(hs, content, output_size, strrchr(path, '/') + 1, attachment);
        int count;
        while((count = fs_read(fd, output, buf_sz))) {
            size_t offset = 0;
            while(count > 0) {
                int rv = write(hs->socket, output+offset, count);
                if(rv <= 0)
                    goto send_out;
                count -= rv;
                offset += rv;
            }
        }
        send_out:
        fs_close(fd);
        FREE(output);
    }
}
