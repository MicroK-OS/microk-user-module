#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include <cdefs.h>
#include <mkmi.h>
#include <mkmi_log.h>
#include <mkmi_exit.h>
#include <mkmi_memory.h>
#include <mkmi_message.h>
#include <mkmi_string.h>
#include <mkmi_syscall.h>

#include "vfs/fops.h"
#include "vfs/typedefs.h"
#include "vfs/vfs.h"
#include "vfs/ustar.h"
#include "ramfs/ramfs.h"
#include "fb/fb.h"

extern "C" uint32_t VendorID = 0xCAFEBABE;
extern "C" uint32_t ProductID = 0xDEADBEEF;

void VFSInit();
void InitrdInit();
void FBInit();
	
VirtualFilesystem *vfs;
RamFS *rootRamfs;
filesystem_t ramfsDesc;

int MessageHandler(MKMI_Message *msg, uint64_t *data) {
	uint32_t *magicNumber = (uint32_t*)data;
	MKMI_Printf("Magic number: %d\r\n", *magicNumber);

	if(*magicNumber == FILE_OPERATION_REQUEST_MAGIC_NUMBER) {
		vfs->DoFileOperation((FileOperationRequest*)data);
	
		SendDirectMessage(msg->SenderVendorID, msg->SenderProductID, (uint8_t*)msg, msg->MessageSize);
		return 0;
	}

	return -1;
}

extern "C" size_t OnInit() {
	SetMessageHandlerCallback(MessageHandler);
	Syscall(SYSCALL_MODULE_MESSAGE_HANDLER, MKMI_MessageHandler, 0, 0, 0, 0, 0);

	VFSInit();
	InitrdInit();
	FBInit();	

	return 0;
}

extern "C" size_t OnExit() {
	delete vfs;

	return 0;
}

void VFSInit() {
	Syscall(SYSCALL_MODULE_SECTION_REGISTER, "VFS", VendorID, ProductID, 0, 0 ,0);

	vfs = new VirtualFilesystem();
	rootRamfs = new RamFS(2048);

	FSOperations *ramfsOps = new FSOperations;
	
	ramfsOps->CreateNode = rootRamfs->CreateNodeWrapper;
	ramfsOps->DeleteNode = rootRamfs->DeleteNodeWrapper;
	ramfsOps->GetByInode = rootRamfs->GetByInodeWrapper;
	ramfsOps->GetByName = rootRamfs->GetByNameWrapper;
	ramfsOps->GetByIndex = rootRamfs->GetByIndexWrapper;
	ramfsOps->GetRootNode = rootRamfs->GetRootNodeWrapper;
	ramfsOps->ReadNode = rootRamfs->ReadNodeWrapper;
	ramfsOps->WriteNode = rootRamfs->WriteNodeWrapper;

	ramfsDesc = vfs->RegisterFilesystem(0, 0, rootRamfs, ramfsOps);

	rootRamfs->SetDescriptor(ramfsDesc);

	vfs->SetRootFS(ramfsDesc);

	intmax_t result = 0;

	FSGetRootRequest rootRequest;
	rootRequest.Request = NODE_GETROOT;
	result = vfs->DoFilesystemOperation(ramfsDesc, &rootRequest);
	if(rootRequest.Result != 0) {
		MKMI_Printf("Getting root failed.\r\n");
	}
	
	FSCreateNodeRequest createRequest;
	createRequest.Request = NODE_CREATE;
	createRequest.Directory = rootRequest.ResultNode.Inode;
	Strcpy(createRequest.Name, "README");
	createRequest.Flags = NODE_PROPERTY_FILE;
	result = vfs->DoFilesystemOperation(ramfsDesc, &createRequest);
	if(createRequest.Result != 0) {
		MKMI_Printf("Node creation failed.\r\n");
	}

	const char *writeData = "Hello, world!";
	size_t writeSize = Strlen(writeData);
	size_t requestSize = sizeof(FSWriteNodeRequest) + writeSize + 1;

	FSWriteNodeRequest *writeRequest = (FSWriteNodeRequest*)Malloc(requestSize);
	Memset((void*)writeRequest, 0, requestSize);

	writeRequest->Request = NODE_WRITE;
	writeRequest->Node = createRequest.ResultNode.Inode;
	writeRequest->Offset = 0;
	writeRequest->Size = writeSize;
	Memcpy(&writeRequest->Buffer, writeData, writeSize);

	result = vfs->DoFilesystemOperation(ramfsDesc, writeRequest);
	MKMI_Printf("Node write: %d\r\n", writeRequest->Result);

	FSReadNodeRequest *readRequest = (FSReadNodeRequest*)Malloc(requestSize);
	Memset((void*)readRequest, 0, requestSize);

	readRequest->Request = NODE_READ;
	readRequest->Node = createRequest.ResultNode.Inode;
	readRequest->Offset = 0;
	readRequest->Size = writeSize;

	result = vfs->DoFilesystemOperation(ramfsDesc, readRequest);
	MKMI_Printf("Node read: %d\r\n", readRequest->Result);
	MKMI_Printf("Read result: %s\r\n", &readRequest->Buffer);

	Free(writeRequest);
	Free(readRequest);

}

void InitrdInit() {
	MKMI_Printf("Requesting initrd.\r\n");

	/* We load the initrd using the kernel file method */
	uintptr_t address;
	size_t size;
	Syscall(SYSCALL_FILE_OPEN, "FILE:/initrd.tar", &address, &size, 0, 0, 0);

	/* Here we check whether it exists 
	 * If it isn't there, just skip this step
	 */
	if (address != 0 && size != 0) {
		/* Make it accessible in memory */
		VMMap(address, address + HIGHER_HALF, size, 0);

		address += HIGHER_HALF;
		MKMI_Printf("Loading file initrd.tar from 0x%x with size %dkb.\r\n", address, size / 1024);

		UnpackArchive(vfs, address, "/");
		rootRamfs->ListDirectory(0);

		uint8_t *configFile;
		size_t configFileSize;

		uint8_t *execFile;
		size_t execFileSize;
		
		FindInArchive(address, "etc/modules.d/preload.conf", &configFile, &configFileSize);
		if(configFile == NULL || configFileSize == 0) return;

		const char *id = Strtok(configFile, "=");
		if(id == NULL) return;
		const char *val = Strtok(NULL, "\r\n");
		if (val == NULL) return;

		while(true) {
			if(Strcmp(id, "always") == 0) {
				char fileName[256] = {0};
				Strcpy(fileName, "modules/");
				Strcpy(fileName + 8, val);

				MKMI_Printf("Starting %s\r\n", fileName);

				FindInArchive(address, fileName, &execFile, &execFileSize);
				if(execFile != NULL && execFileSize != 0) {
					Syscall(SYSCALL_PROC_EXEC, execFile, execFileSize, 0, 0, 0, 0);
				}
			}
			
			id = Strtok(NULL, "=");
			if(id == NULL) break;
			val = Strtok(NULL, "\r\n");
			if (val == NULL) break;
		}


	} else {
		MKMI_Printf("No initrd found");
	}
}

void FBInit() {
	MKMI_Printf("Probing for framebuffer.\r\n");

	uintptr_t fbAddr;
	size_t fbSize;
	Syscall(SYSCALL_FILE_OPEN, "FB:0", &fbAddr, &fbSize, 0, 0, 0);

	/* Here we check whether it exists 
	 * If it isn't there, just skip this step
	 */
	if (fbSize != 0 && fbAddr != 0) {
		/* Make it accessible in memory */
		MKMI_Printf("Mapping...\r\n");
		VMMap(fbAddr, fbAddr, fbSize, 0);
	
		Framebuffer fbData;
		Memcpy(&fbData, fbAddr, sizeof(fbData));

		MKMI_Printf("fb0 starting from 0x%x.\r\n", fbData.Address);
		InitFB(&fbData);

		PrintScreen(" __  __  _                _  __\n"
			    "|  \\/  |(_) __  _ _  ___ | |/ /\n"
			    "| |\\/| || |/ _|| '_|/ _ \\|   < \n"
			    "|_|  |_||_|\\__||_|  \\___/|_|\\_\\\n\n");

		PrintScreen("The user module is starting...\n");
	} else {
		MKMI_Printf("No framebuffer found.\r\n");
	}
}
