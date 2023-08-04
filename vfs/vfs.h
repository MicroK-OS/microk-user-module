#pragma once
#include <stdint.h>
#include <stddef.h>

#include "typedefs.h"
#include "vnode.h"
#include "fs.h"
#include "fops.h"

struct RegisteredFilesystemNode {
	Filesystem *FS;

	RegisteredFilesystemNode *Next;
};

class VirtualFilesystem {
public:
	VirtualFilesystem();
	~VirtualFilesystem();
	
	result_t DoFileOperation(FileOperationRequest *request);

	filesystem_t RegisterFilesystem(uint32_t vendorID, uint32_t productID, void *instance, FSOperations *ops);
	result_t DoFilesystemOperation(filesystem_t fs, FSOperationRequest *request);
	void UnregisterFilesystem(filesystem_t fs);

	void SetRootFS(filesystem_t fs);
	result_t ResolvePath(const char *path, VNode *node);
private:
	RegisteredFilesystemNode *AddNode(Filesystem *fs);
	void RemoveNode(filesystem_t fs);
	RegisteredFilesystemNode *FindNode(filesystem_t fs, RegisteredFilesystemNode **previous, bool *found);

	filesystem_t RootFilesystem;

	result_t ProgressPath(VNode *current, VNode *next, const char *nextName);

	RegisteredFilesystemNode *BaseNode;

	filesystem_t MaxFSDescriptor = 0;
	filesystem_t GetFSDescriptor() { return ++MaxFSDescriptor; }
};
