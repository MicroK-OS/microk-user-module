#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include <cdefs.h>
#include <mkmi.h>

#include "vfs/fops.h"
#include "vfs/typedefs.h"
#include "vfs/vfs.h"
#include "vfs/ustar.h"
#include "ramfs/ramfs.h"

extern "C" uint32_t VendorID = 0xCAFEBABE;
extern "C" uint32_t ProductID = 0xDEADBEEF;

void VFSInit();
void InitrdInit();
	
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

	//Syscall(SYSCALL_PROC_RETURN, 0, 0, 0, 0, 0 ,0);

	VFSInit();
	InitrdInit();

	Syscall(SYSCALL_MODULE_SECTION_REGISTER, "VFS", VendorID, ProductID, 0, 0 ,0);
	
	
	return 0;
}

extern "C" size_t OnExit() {
	delete vfs;

	return 0;
}

void VFSInit() {
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

	const char *writeData = "Hello, world! You seem to be able to store easily a lot of data.";
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
	UserTCB *tcb = GetUserTCB();
	TableListElement *systemTableList = GetSystemTableList(tcb);
	BFST *bfst = (BFST*)GetTableWithSignature(systemTableList, tcb->SystemTables, "BFST");
	BootFile *initrd = GetFileFromBFST(bfst, "/initrd.tar");


	/* Here we check whether it exists 
	 * If it isn't there, just skip this step
	 */
	if (initrd != NULL) {
		MKMI_Printf("Loading file initrd.tar from 0x%x with size %dkb.\r\n", initrd->Address, initrd->Size / 1024);

		UnpackArchive(vfs, initrd->Address, "/");
		rootRamfs->ListDirectory(0);

		uint8_t *configFile = NULL;
		size_t configFileSize = 0;

		uint8_t *execFile = NULL;
		size_t execFileSize = 0;
	
		MKMI_Printf("Finding preload.conf...\r\n");
		FindInArchive(initrd->Address, "etc/modules.d/preload.conf", &configFile, &configFileSize);
		if(configFile == NULL || configFileSize == 0) return;

		size_t fileLength = Strlen(configFile);
		char fileData[fileLength + 1] = { '\0' };
		Memcpy(fileData, configFile, fileLength);

		char *id = Strtok(fileData, "=");
		if(id == NULL) return;
		char *val = Strtok(NULL, "\r\n");
		if (val == NULL) return;
				
		MKMI_Printf("OK\r\n");

		while(true) {
			if(Strcmp(id, "always") == 0) {
				char fileName[256] = {0};
				Strcpy(fileName, "modules/");
				Strcpy(fileName + 8, val);

				MKMI_Printf("Starting %s\r\n", fileName);

				FindInArchive(initrd->Address, fileName, &execFile, &execFileSize);
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
