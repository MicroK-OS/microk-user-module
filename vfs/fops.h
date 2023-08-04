#pragma once
#include "typedefs.h"
#include "vnode.h"

struct FSOperations {
	VNode *(*CreateNode)(void *instance, const inode_t directory, const char name[MAX_NAME_SIZE], property_t flags);
	uintmax_t (*DeleteNode)(void *instance, const inode_t node);

	VNode *(*GetByInode)(void *instance, const inode_t node);
	VNode *(*GetByName)(void *instance, const inode_t directory, const char name[MAX_NAME_SIZE]);
	VNode *(*GetByIndex)(void *instance, const inode_t directory, const size_t index);
	VNode *(*GetRootNode)(void *instance);
	
//	size_t (*WriteNode)(void *instance, const inode_t node, );
//	size_t (*ReadNode)(void *instance, const inode_t node, );
};

struct FSOperationRequest {
	uint32_t MagicNumber;
	uint16_t Request;

	result_t Result : 64;
}__attribute__((packed));

struct FSCreateNodeRequest : public FSOperationRequest {
	VNode ResultNode;

	inode_t Directory;
	char Name[MAX_NAME_SIZE];
	property_t Flags;
}__attribute__((packed));

struct FSDeleteNodeRequest : public FSOperationRequest {
	inode_t Node;
}__attribute__((packed));

struct FSGetByNodeRequest : public FSOperationRequest {
	VNode ResultNode;

	inode_t Node;
}__attribute__((packed));

struct FSGetByNameRequest : public FSOperationRequest {
	VNode ResultNode;

	inode_t Directory;
	char Name[MAX_NAME_SIZE];
}__attribute__((packed));

struct FSGetByIndexRequest : public FSOperationRequest {
	VNode ResultNode;

	inode_t Directory;
	size_t Index;
}__attribute__((packed));

struct FSGetRootRequest : public FSOperationRequest {
	VNode ResultNode;
}__attribute__((packed));


struct FileOperations {
	/* Universal */
	result_t (*Create)(const char *path, const char *name, property_t properties);
	result_t (*Delete)(const char *path);
	result_t (*Rename)(const char *initialPath, const char *newPath);
	result_t (*Chmod)(const char *path, property_t properties);

	/* Just for files */
	fd_t (*Open)(const char *path, mode_t capabilites);
	result_t (*Close)(fd_t file, mode_t capabilities);
	result_t (*Read)(fd_t file, size_t offset, void *buffer, size_t count);
	result_t (*Write)(fd_t file, size_t offset, const void *buffer, size_t count);

	/* Just for directories */
	dir_t (*OpenDir)(const char *path);
	result_t (*CloseDir)(dir_t directory);
	result_t (*ReadDir)(dir_t directory, size_t offset, DirNode *dirNode);
};

struct FileOperationRequest {
	uint32_t MagicNumber;
	uint16_t Request;
			
	result_t Result : 64;
}__attribute__((packed));

struct FileCreateRequest : public FileOperationRequest {
	const char Path[MAX_PATH_SIZE];
	const char Name[MAX_NAME_SIZE];
	property_t Properties;
}__attribute__((packed));

struct FileDeleteRequest : public FileOperationRequest {
	const char Path[MAX_PATH_SIZE];
}__attribute__((packed));

struct FileRenameRequest : public FileOperationRequest {
	const char InitialPath[MAX_PATH_SIZE];
	const char NewPath[MAX_PATH_SIZE];
}__attribute__((packed));

struct FileChmodRequest : public FileOperationRequest {
	const char Path[MAX_PATH_SIZE];
	property_t Properties;
}__attribute__((packed));

struct FileOpenRequest : public FileOperationRequest {
	const char Path[MAX_PATH_SIZE];
	mode_t Capabilities;
}__attribute__((packed));

struct FileCloseRequest : public FileOperationRequest {
	fd_t FileHandle;
	mode_t Capabilities;
}__attribute__((packed));

struct FileReadRequest : public FileOperationRequest {
	fd_t FileHandle;
	size_t Offset;
	size_t Size;

	/* The buffer extends for an amount defined by Size */
	uint8_t Buffer;
}__attribute__((packed));

struct FileWriteRequest : public FileOperationRequest {
	fd_t FileHandle;
	mode_t Capabilities;
	size_t Offset;
	size_t Size;

	/* The buffer extends for an amount defined by Size */
	uint8_t Buffer;
}__attribute__((packed));

struct FileOpenDirRequest : public FileOperationRequest {
	dir_t DirectoryHandle;

	const char Path[MAX_PATH_SIZE];
}__attribute__((packed));

struct FileCloseDirRequest : public FileOperationRequest {
	dir_t DirectoryHandle;
	mode_t Capabilities;
}__attribute__((packed));

struct FileReadDirRequest : public FileOperationRequest {
	dir_t Directory;
	size_t Offset;

	DirNode NodeData; /* The resulting data is put here */
}__attribute__((packed));
