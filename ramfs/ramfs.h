#pragma once
#include "../vfs/typedefs.h"
#include "../vfs/vnode.h"

#define NODES_IN_VNODE_TABLE     0x0100
#define BLOCKS_IN_BLOCK_TABLE    0x0100
#define BLOCK_SIZE    0x1000

struct InodeTableObject;

struct DirectoryVNodeTable {
	InodeTableObject *Elements[NODES_IN_VNODE_TABLE];

	DirectoryVNodeTable *NextTable;
};

struct BlockTable {
	uint8_t *Blocks[BLOCKS_IN_BLOCK_TABLE];

	BlockTable *NextTable;
};

struct InodeTableObject {
	bool Available = true;

	VNode NodeData;

	union {
		BlockTable *BlockTable;
		DirectoryVNodeTable *DirectoryTable;
	};
};

class RamFS {
public:
	RamFS(inode_t maxInodes);
	~RamFS();

	void SetDescriptor(filesystem_t desc) {
		if (Descriptor != 0) return;
		Descriptor = desc;

		if (InodeTable == NULL) return;
		InodeTable[0].NodeData.FSDescriptor = Descriptor;
	}

	int ListDirectory(const inode_t directory);

	VNode *CreateNode(const inode_t directory, const char name[MAX_NAME_SIZE], property_t flags);
	static VNode *CreateNodeWrapper(void *instance, const inode_t directory, const char name[MAX_NAME_SIZE], property_t flags) {
		return static_cast<RamFS*>(instance)->CreateNode(directory, name, flags);
	}

	uintmax_t DeleteNode(const inode_t inode);
	static uintmax_t DeleteNodeWrapper(void *instance, const inode_t inode) {
		return static_cast<RamFS*>(instance)->DeleteNode(inode);
	}

	VNode *GetByInode(const inode_t inode);
	static VNode *GetByInodeWrapper(void *instance, const inode_t inode) {
		return static_cast<RamFS*>(instance)->GetByInode(inode);
	}

	VNode *GetByName(const inode_t directory, const char name[MAX_NAME_SIZE]);
	static VNode *GetByNameWrapper(void *instance, const inode_t directory, const char name[MAX_NAME_SIZE]) {
		return static_cast<RamFS*>(instance)->GetByName(directory, name);
	}
	
	VNode *GetByIndex(const inode_t directory, const size_t index);
	static VNode *GetByIndexWrapper(void *instance, const inode_t directory, const size_t index) {
		return static_cast<RamFS*>(instance)->GetByIndex(directory, index);
	}

	VNode *GetRootNode();
	static VNode *GetRootNodeWrapper(void *instance) {
		return static_cast<RamFS*>(instance)->GetRootNode();
	}
	
	intmax_t ReadNode(const inode_t node, const size_t offset, const size_t size, void *buffer);
	static intmax_t ReadNodeWrapper(void *instance, const inode_t node, const size_t offset, const size_t size, void *buffer) {
		return static_cast<RamFS*>(instance)->ReadNode(node, offset, size, buffer);
	}

	intmax_t WriteNode(const inode_t node, const size_t offset, const size_t size, void *buffer);
	static intmax_t WriteNodeWrapper(void *instance, const inode_t node, const size_t offset, const size_t size, void *buffer) {
		return static_cast<RamFS*>(instance)->WriteNode(node, offset, size, buffer);
	}
private:
	filesystem_t Descriptor;

	inode_t MaxInodes;
	InodeTableObject *InodeTable;
};
