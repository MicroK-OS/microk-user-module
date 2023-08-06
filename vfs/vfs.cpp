#include "vfs.h"
#include "fops.h"
#include "typedefs.h"

#include <mkmi_memory.h>
#include <mkmi_string.h>
#include <mkmi_log.h>

VirtualFilesystem::VirtualFilesystem() {
	BaseNode = new RegisteredFilesystemNode;
	BaseNode->FS = NULL;
	BaseNode->Next = NULL;
}


VirtualFilesystem::~VirtualFilesystem() {
	delete BaseNode;
}
	

#define IF_IS_OURS( x ) \
	if ((x)->FS->OwnerVendorID == 0 && (x)->FS->OwnerProductID == 0)

result_t VirtualFilesystem::DoFileOperation(FileOperationRequest *request) {
	result_t result = 0;
	switch(request->Request) {
		case FOPS_CREATE: {
			FileCreateRequest *createRequest = (FileCreateRequest*)request;
			VNode baseDir;

			result = ResolvePath(createRequest->Path, &baseDir);

			if (result != 0) {
				createRequest->Result = result;

				break;
			}
		
			FSCreateNodeRequest fsCreateRequest;
			fsCreateRequest.MagicNumber = FS_OPERATION_REQUEST_MAGIC_NUMBER;
			fsCreateRequest.Request = NODE_CREATE;
			fsCreateRequest.Directory = baseDir.Inode;
			Strcpy(fsCreateRequest.Name,createRequest->Name);
			fsCreateRequest.Flags = createRequest->Properties;

			result = DoFilesystemOperation(baseDir.FSDescriptor, &fsCreateRequest);
			if(result != 0) {
				createRequest->Result = result;

				break;
			}

			result = 0;
			createRequest->Result = result;
			}
			break;
		default:
			result = -EBADREQUEST;
			break;
	}

	return result;
}
	
filesystem_t VirtualFilesystem::RegisterFilesystem(uint32_t vendorID, uint32_t productID, void *instance, FSOperations *ops) {
	Filesystem *fs = new Filesystem;

	fs->FSDescriptor = GetFSDescriptor();
	fs->OwnerVendorID = vendorID;
	fs->OwnerProductID = productID;
	fs->Instance = instance;
	fs->Operations = ops;

	RegisteredFilesystemNode *node = AddNode(fs);

	MKMI_Printf("Registered filesystem (ID: %x, VID: %x, PID: %x)\r\n",
			node->FS->FSDescriptor,
			node->FS->OwnerVendorID,
			node->FS->OwnerProductID);

	return fs->FSDescriptor;
}
	
result_t VirtualFilesystem::DoFilesystemOperation(filesystem_t fs, FSOperationRequest *request) {
	result_t result = 0;

	bool found = false;
	RegisteredFilesystemNode *previous; 
	RegisteredFilesystemNode *node = FindNode(fs, &previous, &found);

	if (node == NULL || !found) return -ENODRIVER;
	if (request == NULL) return -EBADREQUEST;

	switch(request->Request) {
		case NODE_CREATE:
			IF_IS_OURS(node) {
				FSCreateNodeRequest *createRequest = (FSCreateNodeRequest*)request;
				VNode *resultNode = node->FS->Operations->CreateNode(node->FS->Instance, createRequest->Directory, createRequest->Name, createRequest->Flags);
				if(resultNode == NULL) {
					result = -EFAULT;
				} else {
					result = 0;
					createRequest->ResultNode = *resultNode;
				}

				createRequest->Result = result;
			}
			break;
/*		case NODE_DELETE:
			IF_IS_OURS(node->FS->Operations->DeleteNode(node->FS->Instance, request->Data.DeleteNode.Node));
			break;*/
/*		case NODE_GETBYNODE:
			IF_IS_OURS(node->FS->Operations->GetByInode(node->FS->Instance, request->Data.GetByNode.Node));
			break;*/
		case NODE_GETBYNAME:
			IF_IS_OURS(node) {
				FSGetByNameRequest *getByNameRequest = (FSGetByNameRequest*)request;
				
				VNode *resultNode = node->FS->Operations->GetByName(node->FS->Instance, getByNameRequest->Directory, getByNameRequest->Name);
				if(resultNode == NULL) {
					result = -EFAULT;
				} else {
					result = 0;
					getByNameRequest->ResultNode = *resultNode;
				}

				getByNameRequest->Result = result;
			}
			break;
/*		case NODE_GETBYINDEX:
			IF_IS_OURS(node->FS->Operations->GetByIndex(node->FS->Instance, request->Data.GetByIndex.Directory, request->Data.GetByIndex.Index));
			break;*/
		case NODE_GETROOT:
			IF_IS_OURS(node) {
				FSGetRootRequest *getRootRequest = (FSGetRootRequest*)request;
				VNode *resultNode = node->FS->Operations->GetRootNode(node->FS->Instance);

				if(resultNode == NULL) {
					result = -EFAULT;
				} else {
					result = 0;
					getRootRequest->ResultNode = *resultNode;
				}

				getRootRequest->Result = result;
			}
			break;
		case NODE_READ:
			IF_IS_OURS(node) {
				FSReadNodeRequest *nodeReadRequest = (FSReadNodeRequest*)request;
				intmax_t readAmount = node->FS->Operations->ReadNode(node->FS->Instance, nodeReadRequest->Node, nodeReadRequest->Offset, nodeReadRequest->Size, (void*)&nodeReadRequest->Buffer);

				if(readAmount < 0) {
					result = -EFAULT;
				} else {
					result = readAmount;
				}

				nodeReadRequest->Result = result;
			}
			break;
		case NODE_WRITE:
			IF_IS_OURS(node) {
				FSWriteNodeRequest *nodeWriteRequest = (FSWriteNodeRequest*)request;
				intmax_t writeAmount = node->FS->Operations->WriteNode(node->FS->Instance, nodeWriteRequest->Node, nodeWriteRequest->Offset, nodeWriteRequest->Size, (void*)&nodeWriteRequest->Buffer);

				if(writeAmount < 0) {
					result = -EFAULT;
				} else {
					result = writeAmount;
				}

				nodeWriteRequest->Result = result;
			}
			break;
		default:
			return -EBADREQUEST;
	}

	return result;
}

void VirtualFilesystem::SetRootFS(filesystem_t fs) {
	RootFilesystem = fs;
}

result_t VirtualFilesystem::ProgressPath(VNode *current, VNode *next, const char *nextName) {
	result_t result = 0;
	FSGetByNameRequest request;

	request.Request = NODE_GETBYNAME;
	request.Directory = current->Inode;
	Strcpy(request.Name, nextName);

	result = DoFilesystemOperation(current->FSDescriptor, &request);

	if(result != 0) {
		return result;
	}

	*next = request.ResultNode;

	return result;
}

result_t VirtualFilesystem::ResolvePath(const char *path, VNode *node) {
	result_t result = 0;

	FSGetRootRequest request;
	request.Request = NODE_GETROOT;
	result = DoFilesystemOperation(RootFilesystem, &request);
	if(request.Result != 0) {
		return result;
	}
	
	VNode current = request.ResultNode;
	VNode next;

	const char *id = Strtok(path, "/");
	if(id == NULL) {
		*node = current;
		return result;
	}

	while(true) {
		result = ProgressPath(&current, &next, id);
		if(result != 0) {
			return result;
		}

		current = next;

		id = Strtok(NULL, "/");
		if(id == NULL) {
			*node = current;
			return result;
		}
	}

	return -ENOTPRESENT;
}

RegisteredFilesystemNode *VirtualFilesystem::AddNode(Filesystem *fs) {
	bool found = false;
	RegisteredFilesystemNode *node, *prev;
	node = FindNode(fs->FSDescriptor, &prev, &found);

	/* Already present, return this one */
	if(found) return node;

	/* If not, prev is now our last node. */
	RegisteredFilesystemNode *newNode = new RegisteredFilesystemNode;
	newNode->FS = fs;
	newNode->Next = NULL;

	prev->Next = newNode;
	
	return prev->Next;
}

void VirtualFilesystem::RemoveNode(filesystem_t fs) {
	bool found = false;
	RegisteredFilesystemNode *previous; 
	RegisteredFilesystemNode *node = FindNode(fs, &previous, &found);

	/* This issue shall be reported */
	if(!found) return;

	previous->Next = node->Next;

	delete node->FS;
	delete node;
}

RegisteredFilesystemNode *VirtualFilesystem::FindNode(filesystem_t fs, RegisteredFilesystemNode **previous, bool *found) {
	RegisteredFilesystemNode *node = BaseNode->Next, *prev = BaseNode;

	if (node == NULL) {
		*previous = prev;
		*found = false;
		return NULL;
	}


	while(true) {
		if (node->FS->FSDescriptor == fs) {
			*found = true;
			*previous = prev;
			return node;
		}

		prev = node;
		if (node->Next == NULL) break;
		node = node->Next;
	}

	*previous = prev;
	*found = false;
	return NULL;
}

