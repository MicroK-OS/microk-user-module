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
		void *response;
		size_t responseSize;
		vfs->DoFileOperation((FileOperationRequest*)data, &response, &responseSize);
	
		rootRamfs->ListDirectory(1);
//		SendDirectMessage(msg->SenderVendorID, msg->SenderProductID, response, responseSize);
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

	/* First, initialize VFS data structures */
	/* Instantiate the rootfs (it will be overlayed soon) */
	/* Create a ramfs, overlaying it in the rootfs */
	/* Extract the initrd in the /init directory in the ramfs */
	return 0;
}

extern "C" size_t OnExit() {
	/* Close and destroy all files */
	/* Unmount all filesystems and overlays */
	/* Destroy the rootfs */
	/* Delete VFS data structures */
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

	ramfsDesc = vfs->RegisterFilesystem(0, 0, rootRamfs, ramfsOps);

	rootRamfs->SetDescriptor(ramfsDesc);

	vfs->SetRootFS(ramfsDesc);

	FSOperationRequest request;
	request.Request = NODE_GETROOT;
	VNode *rootNode = vfs->DoFilesystemOperation(ramfsDesc, &request);

	request.Request = NODE_CREATE;
	request.Data.CreateNode.Directory = rootNode->Inode;
	request.Data.CreateNode.Flags = NODE_PROPERTY_DIRECTORY;
	Strcpy(request.Data.CreateNode.Name, "dev");

	VNode *devNode = vfs->DoFilesystemOperation(ramfsDesc, &request);

	request.Request = NODE_CREATE;
	request.Data.CreateNode.Directory = devNode->Inode;
	request.Data.CreateNode.Flags = NODE_PROPERTY_DIRECTORY;
	Strcpy(request.Data.CreateNode.Name, "tty");

	VNode *ttyNode = vfs->DoFilesystemOperation(ramfsDesc, &request);

	request.Request = NODE_CREATE;
	request.Data.CreateNode.Directory = ttyNode->Inode;
	request.Data.CreateNode.Flags = NODE_PROPERTY_FILE;
	Strcpy(request.Data.CreateNode.Name, "tty1");

	VNode *ttyFile = vfs->DoFilesystemOperation(ramfsDesc, &request);

	request.Request = NODE_CREATE;
	request.Data.CreateNode.Directory = ttyNode->Inode;
	request.Data.CreateNode.Flags = NODE_PROPERTY_FILE;
	Strcpy(request.Data.CreateNode.Name, "tty2");

	vfs->DoFilesystemOperation(ramfsDesc, &request);

	request.Request = NODE_CREATE;
	request.Data.CreateNode.Directory = devNode->Inode;
	request.Data.CreateNode.Flags = NODE_PROPERTY_FILE;
	Strcpy(request.Data.CreateNode.Name, "kmsg");

	vfs->DoFilesystemOperation(ramfsDesc, &request);

	const char *path = "/dev/kmsg";
	MKMI_Printf("Resolving path: %s\r\n", path);
	VNode *node = vfs->ResolvePath(path);
	if (node == NULL) MKMI_Printf("%s not found.\r\n", path);
	else MKMI_Printf("%s\'s inode: %d\r\n", path, node->Inode);

	rootRamfs->ListDirectory(0);
	rootRamfs->ListDirectory(devNode->Inode);
	rootRamfs->ListDirectory(ttyNode->Inode);

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

		LoadArchive(address);

		uint8_t *file;
		size_t size;

		FindInArchive(address, "modules/cafebabe-b830c0de-pci-module.elf", &file, &size);
		if(file == NULL || size == 0) _exit(69);
		
		Syscall(SYSCALL_PROC_EXEC, file, size, 0, 0, 0, 0);

		FindInArchive(address, "modules/cafebabe-a3c1c0de-acpi-module.elf", &file, &size);
		if(file == NULL || size == 0) _exit(128);

		Syscall(SYSCALL_PROC_EXEC, file, size, 0, 0, 0, 0);
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
