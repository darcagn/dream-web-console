# Dream Web Console v202311??
- Officially start new fork as Dream Web Console
- Major code reorganization and modernization
- Revised Makefile with DiscJuggler CDI generation
- Documentation overhauled
- Obtaining IP via DHCP now supported
- Created new main index page
- Moved raw dumping from being the only page to a subpage
- Separate pages for dumping presets
  - 2048 bytes per sector (standard ISO data)
  - 2352 bytes per sector (TOSEC-style raw data, formerly the only index page)
  - 2448 bytes per sector (raw data plus sub channels)
- Preset page varieties for using IP.BIN TOC provided
- Dynamically size send buffers to avoid overflows
- source.zip download now works again
- Dynamically size disc read buffer to improve dumping speed/performance
- Add gdrom_spin_down alias for cdrom_spin_down
- Adjusted look and feel with a new dark style
- Client connections now display IP address correctly in log
- 'cdxa' parameter changed to 'trktype' and properly documented, acceptable values changed
- 'p' parameter changed to 'datasel' and properly documented, acceptable values changed
- 'sector_size' parameter renamed to 'secsz'
- 'sector_read' parameter renamed to 'secrd'
- adjust CD_SUB_Q_CHANNEL to CD_SUB_Q_ALL due to upstream changes in KOS
- Fixed potential memory corruption issue when dumping sub channel data

# httpd-ack next v20231117
- updated to latest KallistiOS
- adjust network buffer size for peformance gains
- 20+% dumping speed increase compared to v20080711

# httpd-ack next v20230505
- unofficial update by darcagn
- code cleaned up and all compilation warnings eliminated
- ported functionality to KallistiOS 2.x
- eliminated need for patches against KOS

# httpd-ack v20080711
- make the source code link on index.html page
- auto spin down cdrom once loaded

# httpd-ack v20071123
- update disc.gdi format for newer nulldc (s/-8/0/)
- add ability to use TOC from ip.bin for session 2
- return 404 for unknown GET instead of cdrom contents

# httpd-ack v20070720
- add some what nasty work around for kos/lwip killing off a socket before all data had been sent out.  this would also cause the tcp stack to crash
- switch to using conio for screen output
- add http://IP/cdrom_spin_down
- fixed possible crash when reading ip.bin sector for meta data.
- gave session/track dual meaning on url.  If both are >= 150, session will act as the start sector, and track the end sector when dumping.
- fixed overflow when sending gdi file on discs with 60+ tracks, bug report by Nologic
- add abort option url, default on
- merged in sub channel dump code, default off

# httpd-ack v20070511
- fix some spelling errors on web interface pointed out by maddog
- adjust default dump settings per tosec/dumpcast agreement
- add syscalls dump /syscalls.bin, suggested by darcagn
- use built in memalign() instead of #define
- make sector buffer in P2 memory map region to disable caching, suggested by drkIIRaziel

# httpd-ack v20070430
- remove the 2048/2352 sector size restriction
- update Makefile to create boot discs

# httpd-ack v20070421
- fix an issue with reading the ip.bin data

# httpd-ack v20070415
- make the gdi more windows friendly by adding \r at eol
- use unsigned long on memory dump params

# httpd-ack v20070412
- add locking around send_toc/send_track
- forgot the -150 on session2 tracks in gdi file
- adjust url so track%.[iso|raw] will be the download file

# httpd-ack v20070411
- fix bug in memory dump that starts at 0x0
- add bios dump /dc_bios.bin
- add flash dump /dc_flash.bin
- add gdi file for current disc /disc.gdi
- adjust track urls so they download as session%d_track%d.(iso|raw)  

# httpd-ack v20070410
- only apply 150 gap to track if the next track is a different track type
- add memory dump /memory_start%d_end%d.bin

# httpd-ack v20070409
- add cdrom info output

# httpd-ack v20070408a
- fix retry bug
- add gap% option to url, default to 150 for all tracks except ones that end a session.  

# httpd-ack v20070408
- full rewrite
- clean up the web interface a bit
- add source code download
- fix overflow in tcp/ip stack
- work around realpath() crashes

# httpd-ack v20070407
- properly align memory for dma xfers
- fix tcp/ip stack corruption that was causing crashes
- misc fixes

# httpd-ack v20070405
- add ability to change dump options based on url
- patch kos to support initializing cdrom with different params
- patch kos to allow PIO or DMA cdrom sector reads
- logging to screen
- misc fixes

# httpd-ack v20070404
- initial concept testing 
