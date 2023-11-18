# httpd-ack next - makefile

VERSION = "20231117"
TARGET = httpd-ack.elf
OBJS = source/httpd-ack.o romdisk.o
KOS_ROMDISK_DIR = romdisk
KOS_CFLAGS += -DVERSION=$(VERSION)
ZIPPED_FILES = CHANGELOG LICENSE README README.KOS Makefile source/ romdisk/

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)
	-rm -f romdisk/httpd-ack-source-*.zip
	-rm -f httpd-ack.cdi

rm-elf:
	-rm -f $(TARGET) romdisk.*

prepsource:
	zip -9 -r romdisk/httpd-ack-source-$(VERSION).zip $(ZIPPED_FILES)

$(TARGET): prepsource $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lconio

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: $(TARGET)
	-rm -f $(OBJS) romdisk.img
	$(KOS_STRIP) $(TARGET)

disc: $(TARGET)
	mkdcdisc -a httpd-ack -e httpd-ack.elf -m -n httpd-ack -r $(VERSION) -o httpd-ack.cdi
