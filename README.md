# Dream Web Console
**Dream Web Console** is a web-based utility for Sega Dreamcast power users with a Broadband Adapter. Connect to your Dreamcast system through a web browser to view diagnostic information about the system and attached peripherals, dump GD-ROM discs, browse and download VMU files and images, browse and download files on attached SD cards or hard drives, and more!

## Usage
- Download the latest CDI image from the releases section, burn to CD-R using DiscJuggler or ImgBurn, and load on your Dreamcast.
- or, use the provided `dream-web-console.bin` file to load the software from an SD card using [DreamBoot BIOS](https://github.com/Cpasjuste/dreamboot).
- or, use the provided `dream-web-console.elf` file to load the software from a [Coder's Cable](https://dreamcast.wiki/Coder%27s_cable), [Broadband Adapter](https://dreamcast.wiki/Broadband_adapter), or [LAN Adapter](https://dreamcast.wiki/LAN_adapter) using **dc-tool** and **dcload-serial** or **dcload-ip**
- or, compile the code within a [KallistiOS](https://github.com/KallistiOS/KallistiOS) environment, and run the resulting `dream-web-console.elf` file using the above-mentioned **dc-tool**.
  - if using **dcload-ip**, make sure to use `-n` in **dc-tool** to disconnect from console to allow Dream Web Console to use the network adapter

## IP address configuration
**Dream Web Console** supports both DHCP and static IP configurations. It is recommended to use the XDP Browser or Broadband Passport to set up your internet connection settings.

## System diagnostics
WIP

## Track dump options
Users may dump GD-ROM discs using one of three presets:
- standard data (2048 bytes per data sector, useful for playing on DreamShell's ISO Loader or on emulators)
- raw data (2352 bytes per data sector, useful for disc preservation projects like TOSEC)
- raw + sub channel data (2448 bytes per sector -- slow, unnecessary overkill, but available as an option)

For standard data and raw data dumps, a GDI file is provided as well. 

#### IP.BIN TOC
You may use the standard drive TOC to browse discs or you may choose to ignore the drive TOC and have Dream Web Console read the GD-ROM layout directly from the disc's IP.BIN header. Bypassing the drive TOC in this way is useful for reading prototype GD-R discs using a swap trick.

#### Spin down GD-ROM drive
This convenience function stops the disc from spinning. This can be useful for GD-R swap tricks so one doesn't have to wait for the disc to stop spinning on its own.

## Browse VMUs
WIP

## Browse SD card
WIP

## Browse HDD
WIP

## Dump BIOS/Flash memory
- `http://IP/dc_bios.bin` - Download BIOS (memory `0x00000000` to `0x001FFFFF`)
- `http://IP/dc_flash.bin` - Download Flash ROM image, (memory `0x00200000` to `0x0021FFFF`)
- `http://IP/syscalls.bin` - Download syscalls data, (memory `0x8C000000` to `0x8C007FFF`)

## Dump memory region
`http://IP/memory_startX_endX` - Dumps memory from **startX** to **endX**. **X** is in hexadecimal representation.

## Advanced disc dumping options
If you hover of the link for a track, you will see the link structure with various parameters. For example:
- Standard data: `http://IP/track03.iso?ipbintoc0_session02_datasel1_trktype2_secsz2048_gap0_dma1_secrd16_sub0_abort1_retry5`
- Raw data: `http://IP/track03.bin?ipbintoc0_session02_datasel0_trktype0_secsz2352_gap0_dma1_secrd16_sub0_abort1_retry5`

Each parameter can be adjusted to change how the track is dumped. See below table for a description of each.

| **Parameter** | **Description**      |
|---------------|----------------------|
| track**X**    | Dump track number **X** |
| session**X**  | Dump track from session **X** |
| ipbintoc**X** | `0`: use standard TOC<br />`1`: use IP.BIN TOC |
| datasel**X**  | Data select<br />`0`: All 2352 bytes<br />`1`: Data<br />`2`: Subheader<br />`3`: Header |
| trktype**X**  | Expected data type<br />`0`: Any type<br />`1`: CDDA Audio (2352 bytes per sector)<br />`2`: Mode 1 Data (2048 bytes per sector)<br />`3`: Mode 2 XA Data (2336 bytes per sector)<br />`4`: Mode 2 XA Form 1 Data (2048 bytes per sector)<br />`5`: Mode 2 XA Form 2 Data (2324 bytes per sector)<br />`6`: Mode 2 Non-XA Data (2336 bytes per sector) |
| secsz**X**    | Sector size **X**: may not apply correctly if conflicts with **datasel** or **trktype** parameters, see **memory poisoning** section below.<br />If dumping sub channel data, exclude the sub channel sectors from this count (i.e. specify **2352** instead of **2448**). |
| gap**X**      | The end sector for a given track is not given in disc TOC, so it is calculated manually. Gaps only exist between tracks of two different types. For these tracks, calculated as<br />`(next track start sector - 1 ) - gap`, where gap **X** is **150** by default. |
| dma**X**      | `1`: use DMA to transfer from GD-ROM (fast)<br />`0`: use programmed I/O (slow) |
| secrd**X**    | Number of sectors **X** to read in each call to GD-ROM reading function |
| sub**X**      | `0`: Don't dump subchannel data (default)<br />`1`: Use syscall method to dump subchannel data<br />`2`: Use cdrom_read_sectors_ex() method to dump subchannel data<br />While dumping sub channel data, sector reads are forced to one sector at a time using programmed I/O transfer. It's slow. |
| abort**X**    | `1`: Abort on read error (default)<br />`0`: Continue on read error |
| retry**X**    | Number of times **X** to retry a sector read before aborting or continuing on. Default is **5**. |

### Dumping specific sector range
**trackX** and **sectorX** have a dual meaning: when both values are **>= 100**, they indicate a sector range to be dumped instead of a singular track. Use **sessionX** as the start sector and **trackX** as the ending sector of the desired range to be dumped. If the range of sectors to be dumped crosses across track gaps or spans different types of sectors, there may be read errors.

### Memory poisoning
The data buffer passed to the GD-ROM dumping function is filled with `QQQQ`. If the selected function does not fill the entire buffer size, `QQQQ` will show up in your downloaded track. This can happen, for example, if you specify a 2352 **sector size** but choose a **datasel** or **tracktype** option where the function only returns 2048 bytes of data for each sector.

## Source code
`http://IP/dwc-source-VERSION.zip` or `source.zip` - Download Dream Web Console source code embedded within the program

## Attribution
**Dream Web Console** started as a project by **darcagn** to update and modernize the httpd-ack utility created by **ackmed**.
**httpd-ack** itself was created from example code provided with **KallistiOS**, originally written by **Megan Potter**.
All source code in Dream Web Console can be considered under the same 3-clause BSD license as KallistiOS. 
