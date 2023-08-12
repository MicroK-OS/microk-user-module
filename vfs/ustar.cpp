#include "ustar.h"
#include "fops.h"
#include "typedefs.h"
#include "vfs.h"

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

void UnpackArchive(VirtualFilesystem *vfs, uint8_t *archive, const char *directory) {
	unsigned char *ptr = archive;

	FileCreateRequest createRequest;
	createRequest.MagicNumber = FILE_OPERATION_REQUEST_MAGIC_NUMBER;
	createRequest.Request = FOPS_CREATE;

	while (!Memcmp(ptr + 257, "ustar", 5)) { // Until we have a valid header
		TarHeader *header = (TarHeader*)ptr;
		size_t fileSize = oct2bin(ptr + 0x7c, 11);
		char path[MAX_PATH_SIZE] = {0};
		char name[MAX_NAME_SIZE] = {0};

		size_t len = Strlen(header->Filename);

		Strcpy(path, directory);
		size_t baseDirectoryLength = Strlen(directory);

		char *lastSlashPointer = NULL;
		size_t lastSlashSize = 0;

		bool isDirectory = false;

		if(header->Filename[len - 1] == '/') {
			isDirectory= true;
			lastSlashPointer = &header->Filename[len - 2];
		} else {
			isDirectory = false;
			lastSlashPointer = &header->Filename[len - 1];
		}

		while(lastSlashPointer >= header->Filename && *lastSlashPointer != '/') {
			--lastSlashPointer;
			++lastSlashSize;
		}
			
		Memcpy(name, lastSlashPointer + 1, lastSlashSize);
		Memcpy(path + baseDirectoryLength, header->Filename, len - lastSlashSize - 1);
		if(isDirectory && baseDirectoryLength + len - lastSlashSize - 2 > 0)
			*(char*)(path + baseDirectoryLength + len - lastSlashSize - 2) = '\0';
		
		MKMI_Printf("Path: %s  %s\r\n", path, name);
		
		createRequest.Properties = 0;
		createRequest.Properties |= isDirectory ? NODE_PROPERTY_DIRECTORY : NODE_PROPERTY_FILE;

		Memset(createRequest.Path, 0, MAX_PATH_SIZE);
		Strcpy(createRequest.Path, path);
		Memset(createRequest.Name, 0, MAX_PATH_SIZE);
		Strcpy(createRequest.Name, name);

		vfs->DoFileOperation(&createRequest);

		MKMI_Printf("Result: %d\r\n", createRequest.Result);
	
		if(!isDirectory) {
			/*
			request->Request = FOPS_OPEN;

			Memset(request->Data.Open.Path, 0, MAX_PATH_SIZE);
			Strcpy(request->Data.Open.Path, directory);
			*(char*)&request->Data.Open.Path[baseDirectoryLength-1] = '/';
			Strcpy(request->Data.Open.Path + baseDirectoryLength, header->Filename);

			request->Data.Open.Capabilities = 0;
		
			vfs->DoFileOperation(request, &response, &responseSize);

			MKMI_Printf("Path: %s\r\n", request->Data.Open.Path);

			request->Request = FOPS_CHMOD;
			vfs->DoFileOperation(request, &response, &responseSize);
			request->Request = FOPS_WRITE;
			vfs->DoFileOperation(request, &response, &responseSize);
			request->Request = FOPS_CLOSE;
			vfs->DoFileOperation(request, &response, &responseSize);
*/
		}

		ptr += (((fileSize + 511) / 512) + 1) * 512;
	}
}
