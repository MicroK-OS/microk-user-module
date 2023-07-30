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
};

struct FSOperationRequest {
	uint32_t MagicNumber;
	uint16_t Request;

	union {
		struct {
			inode_t Directory;
			char Name[MAX_NAME_SIZE];
			property_t Flags;
		} CreateNode;

		struct {
			inode_t Node;
		} DeleteNode;

		struct {
			inode_t Node;
		} GetByNode;

		struct {
			inode_t Directory;
			char Name[MAX_NAME_SIZE];
		} GetByName;

		struct {
			inode_t Directory;
			size_t Index;
		} GetByIndex;
	} Data;
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

	union {
		struct {
			const char Path[MAX_PATH_SIZE];
			const char Name[MAX_NAME_SIZE];
			property_t Properties;
		} Create;

		struct {
			const char Path[MAX_PATH_SIZE];
		} Delete;

		struct {
			const char InitialPath[MAX_PATH_SIZE];
			const char NewPath[MAX_PATH_SIZE];
		} Rename;

		struct {
			const char Path[MAX_PATH_SIZE];
			property_t Properties;

		} Chmod;

		struct {
			const char Path[MAX_PATH_SIZE];
			mode_t Capabilities;
		} Open;

		struct {
			fd_t FileHandle;
			mode_t Capabilities;
		} Close;

		struct {
			fd_t FileHandle;
			size_t Offset;
			size_t Count;

			/* The buffer extends for an amount defined by Count */
			uint8_t Buffer;
		} Read;

		struct {
			fd_t FileHandle;
			mode_t Capabilities;
			size_t Offset;
			size_t Count;

			/* The buffer extends for an amount defined by Count */
			uint8_t Buffer;
		} Write;

		struct {
			dir_t DirectoryHandle;

			const char Path[MAX_PATH_SIZE];
		} OpenDir;

		struct {
			fd_t FileHandle;
			mode_t Capabilities;
		} CloseDir;

		struct {
			dir_t Directory;
			size_t Offset;

			DirNode NodeData;
		} ReadDir;
	} Data;
}__attribute__((packed));
