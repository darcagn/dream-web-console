# Dream Web Console - Makefile

VERSION = "202311XX"
TARGET = dream-web-console.elf

# Poison GD-ROM read buffer with 'Q'?
POISON = 1

# Core modules
OBJS = source/dwc.o source/client_thread.o source/style.o romdisk.o

# Send modules
OBJS += source/send_diag.o source/send_fs_file.o source/send_hdd.o source/send_main.o
OBJS += source/send_mem.o source/send_ok_error.o source/send_sd.o source/send_vmu.o

# GD-ROM modules
OBJS += source/gdrom/get_toc.o source/gdrom/send_disc_index.o source/gdrom/send_gdi.o
OBJS += source/gdrom/send_track.o

KOS_ROMDISK_DIR = romdisk
KOS_CFLAGS += -O3 -flto=auto -DVERSION=$(VERSION) -DPOISON=$(POISON)
ZIPPED_FILES = CHANGELOG.md LICENSE README.md README.KOS Makefile source/ romdisk/

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)
	-rm -f romdisk/dwc-source-*.zip
	-rm -f dream-web-console.cdi

rm-elf:
	-rm -f $(TARGET) romdisk.*

prepsource:
	zip -9 -r romdisk/dwc-source-$(VERSION).zip $(ZIPPED_FILES)

$(TARGET): prepsource $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lconio

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: $(TARGET)
	-rm -f $(OBJS) romdisk.img
	$(KOS_STRIP) $(TARGET)

disc: $(TARGET)
	mkdcdisc -a darcagn -e dream-web-console.elf -m -n dream-web-console -r $(VERSION) -o dream-web-console.cdi
