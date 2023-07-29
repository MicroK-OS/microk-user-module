#include "ustar.h"

#include <mkmi_log.h>
#include <mkmi_memory.h>
#include <mkmi_string.h>
#include <mkmi_syscall.h>

static int oct2bin(unsigned char *str, int size) {
        int n = 0;
        unsigned char *c = str;
        while (size-- > 0) {
                n *= 8;
                n += *c - '0';
                c++;
        }
        return n;
}

void FindInArchive(uint8_t *archive, const char *name, uint8_t **file, size_t *size) {
	unsigned char *ptr = archive;

	while (!Memcmp(ptr + 257, "ustar", 5)) { // Until we have a valid header
		TarHeader *header = (TarHeader*)ptr;
		size_t fileSize = oct2bin(ptr + 0x7c, 11);

		if(Strcmp(header->Filename, name) == 0) {
			*file = ptr + 512;
			*size = fileSize;
			return;
		}

		ptr += (((fileSize + 511) / 512) + 1) * 512;
	}

	*file = NULL;
	*size = 0;

	return;
}

void LoadArchive(uint8_t *archive) {
	unsigned char *ptr = archive;

	while (!Memcmp(ptr + 257, "ustar", 5)) { // Until we have a valid header
		TarHeader *header = (TarHeader*)ptr;
		size_t fileSize = oct2bin(ptr + 0x7c, 11);
		
		MKMI_Printf(" - %s\r\n", header->Filename);

		ptr += (((fileSize + 511) / 512) + 1) * 512;
	}
}

void UnpackArchive(uint8_t *archive) {

}
