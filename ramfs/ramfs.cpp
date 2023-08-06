#include "ramfs.h"

#include <mkmi_memory.h>
#include <mkmi_string.h>
#include <mkmi_log.h>

RamFS::RamFS(inode_t maxInodes) {
	Descriptor = 0;
	MaxInodes = maxInodes;
	InodeTable = new InodeTableObject[MaxInodes];

	InodeTableObject *node = &InodeTable[0];
	node->Available = false;

	node->NodeData.FSDescriptor = 0;
	node->NodeData.Properties = NODE_PROPERTY_DIRECTORY;
	node->NodeData.Inode = 0;

	node->DirectoryTable = new DirectoryVNodeTable;
	node->DirectoryTable->NextTable = NULL;

	Memset(node->DirectoryTable->Elements, 0, NODES_IN_VNODE_TABLE * sizeof(uintptr_t));
}

RamFS::~RamFS() {
	delete InodeTable;
}

int RamFS::ListDirectory(const inode_t directory) {
	if (directory > MaxInodes) return 0;

	InodeTableObject *dir = &InodeTable[directory];

	if(dir->Available) return 0;
	if(!(dir->NodeData.Properties & NODE_PROPERTY_DIRECTORY)) return 0;
/*
	if(dir->NodeData.Properties & NODE_PROPERTY_MOUNTPOINT) {
		return -1;
	} 

	if(dir->NodeData.Properties & NODE_PROPERTY_SYMLINK) {
		return -1;
	}
*/
	if(dir->DirectoryTable == NULL) return 0;

	DirectoryVNodeTable *table = dir->DirectoryTable;

	MKMI_Printf("   Name   Inode\r\n");
	while(true) {
		for (size_t i = 0; i < NODES_IN_VNODE_TABLE; ++i) {
			if(table->Elements[i] == NULL || table->Elements[i] == -1) continue;

			MKMI_Printf(" -> %s   %d\r\n", table->Elements[i]->NodeData.Name, table->Elements[i]->NodeData.Inode);
		}

		if(table->NextTable == NULL) return 0;
		table = table->NextTable;
	}

	return 0;
}

VNode *RamFS::CreateNode(const inode_t directory, const char name[MAX_NAME_SIZE], property_t flags) {
	if (directory > MaxInodes) return 0;
	if (flags == 0) return 0;
				
	InodeTableObject *dir = &InodeTable[directory];

	if(dir->Available) return 0;
	if(!(dir->NodeData.Properties & NODE_PROPERTY_DIRECTORY)) return 0;
/*
	if(dir->NodeData.Properties & NODE_PROPERTY_MOUNTPOINT) {
		return 0;
	} 

	if(dir->NodeData.Properties & NODE_PROPERTY_SYMLINK) {
		return 0;
	}
*/
	if(dir->DirectoryTable == NULL) return 0;
			
	DirectoryVNodeTable *table = dir->DirectoryTable;

	for (size_t i = 1; i < MaxInodes; ++i) {
		if(InodeTable[i].Available) {
			InodeTable[i].Available = false;
			
			InodeTable[i].NodeData.FSDescriptor = Descriptor;

			Memcpy(InodeTable[i].NodeData.Name, name, MAX_NAME_SIZE);
			InodeTable[i].NodeData.Inode = i;

			if(flags & NODE_PROPERTY_DIRECTORY) {
				InodeTable[i].NodeData.Properties |= NODE_PROPERTY_DIRECTORY; 
				InodeTable[i].DirectoryTable = new DirectoryVNodeTable;
				InodeTable[i].DirectoryTable->NextTable = NULL;
				Memset(InodeTable[i].DirectoryTable->Elements, 0, NODES_IN_VNODE_TABLE * sizeof(uintptr_t));
			} else if (flags & NODE_PROPERTY_FILE) {
				InodeTable[i].NodeData.Properties |= NODE_PROPERTY_FILE;
				InodeTable[i].BlockTable = new BlockTable;
				InodeTable[i].BlockTable->NextTable = NULL;
				Memset(InodeTable[i].BlockTable->Blocks, 0, BLOCKS_IN_BLOCK_TABLE * sizeof(uintptr_t));
			}

			bool found = false;
			while(true) {
				for (size_t j = 0; j < NODES_IN_VNODE_TABLE; ++j) {
					if(table->Elements[j] != NULL && table->Elements[j] != -1) continue;

					table->Elements[j] = &InodeTable[i];
					found = true;
					break;
				}

				if(found) break;

				if(table->NextTable == NULL) {
					table->NextTable = new DirectoryVNodeTable;
					table->NextTable->NextTable = NULL;
					if (table->NextTable == NULL || table->NextTable == -1) return 0;
					Memset(table->NextTable->Elements, 0, NODES_IN_VNODE_TABLE * sizeof(uintptr_t));
				}
				table = table->NextTable;
			}

			return &InodeTable[i].NodeData;
		}
	}
	
	return 0;
}

uintmax_t RamFS::DeleteNode(const inode_t inode) {
	if (inode > MaxInodes) return 0;

	InodeTableObject *node = &InodeTable[inode];

	if(node->Available) return 0;

	/* Todo */

	return 0;
}

VNode *RamFS::GetByInode(const inode_t inode) {
	if (inode > MaxInodes) return 0;

	InodeTableObject *node = &InodeTable[inode];

	if(node->Available) {
		return 0;
	}

	VNode *vnode = &node->NodeData;

	return vnode;

}

VNode *RamFS::GetByName(const inode_t directory, const char name[MAX_NAME_SIZE]) {
	if (directory > MaxInodes) return 0;

	InodeTableObject *dir = &InodeTable[directory];
	
	if(dir->Available) return 0;
	if((dir->NodeData.Properties & NODE_PROPERTY_DIRECTORY) == 0) return 0;

	if(dir->DirectoryTable == NULL) return 0;

	DirectoryVNodeTable *table = dir->DirectoryTable;

	while(true) {
		for (size_t i = 0; i < NODES_IN_VNODE_TABLE; ++i) {
			if(table->Elements[i] == NULL || table->Elements[i] == -1) continue;

			if(Strcmp(name, table->Elements[i]->NodeData.Name) == 0) {
				return &table->Elements[i]->NodeData;
			}
		}

		if(table->NextTable == NULL) {
			return 0;
		}
		table = table->NextTable;
	}

	return 0;
}
	
VNode *RamFS::GetByIndex(const inode_t directory, const size_t index) {
	if (directory > MaxInodes) return 0;

	InodeTableObject *dir = &InodeTable[directory];

	if(dir->Available) return 0;
	if(!(dir->NodeData.Properties & NODE_PROPERTY_DIRECTORY)) return 0;

	/* Handle mountpoints first */
	if(dir->NodeData.Properties & NODE_PROPERTY_MOUNTPOINT) {
		return 0;
	} 

	/* Handle symlinks then */
	if(dir->NodeData.Properties & NODE_PROPERTY_SYMLINK) {
		return 0;
	}

	if(dir->DirectoryTable == NULL) return 0;

	DirectoryVNodeTable *table = dir->DirectoryTable;

	size_t tableIndex = index % NODES_IN_VNODE_TABLE;
	size_t tablesToCross = (index - tableIndex) / NODES_IN_VNODE_TABLE;

	for (size_t i = 0; i < tablesToCross; ++i) {
		if(table->NextTable == NULL) return 0;
		table = table->NextTable;
	}

	if(table->Elements[tableIndex] == NULL || table->Elements[tableIndex] == -1) return 0;
	if(table->Elements[tableIndex]->Available) return 0;

	return &table->Elements[tableIndex]->NodeData;
}
	
VNode *RamFS::GetRootNode() {
	VNode *node = &InodeTable[0].NodeData;
	return node;
}

intmax_t RamFS::ReadNode(const inode_t node, const size_t offset, const size_t size, void *buffer) {
	if (node > MaxInodes) return -1;

	InodeTableObject *file = &InodeTable[node];

	if(file->Available) return -1;
/*	if((file->NodeData.Properties & NODE_PROPERTY_DIRECTORY)) return -1; */
	
	if(file->BlockTable == NULL) return -1;

	BlockTable *table = file->BlockTable;

	size_t startIndexInBlock = offset % BLOCK_SIZE;
	size_t startRoundedOffset = offset - offset % BLOCK_SIZE;
	size_t startBlockIndex = startRoundedOffset / BLOCK_SIZE % BLOCKS_IN_BLOCK_TABLE;
	size_t startTablesToCross = (startRoundedOffset % BLOCK_SIZE - startBlockIndex) / BLOCKS_IN_BLOCK_TABLE;

	size_t end = offset + size;
	size_t endIndexInBlock = end % BLOCK_SIZE;
	size_t endRoundedOffset = end - end % BLOCK_SIZE;
	size_t endBlockIndex = endRoundedOffset / BLOCK_SIZE % BLOCKS_IN_BLOCK_TABLE;
	size_t endTablesToCross = (endRoundedOffset % BLOCK_SIZE - endBlockIndex) / BLOCKS_IN_BLOCK_TABLE;

	for (size_t i = 0; i < startTablesToCross; ++i) {
		if(table->NextTable == NULL) return -1;
		table = table->NextTable;
	}

	size_t crossedTables = startTablesToCross;
	size_t readAmount = 0;
	size_t block = startBlockIndex;
	size_t index = startIndexInBlock;
	do {
		while(block < BLOCKS_IN_BLOCK_TABLE) {
			if(readAmount >= size) return readAmount;

			if(table->Blocks[block] == NULL || table->Blocks[block] == -1) return -1;

			if(crossedTables == endTablesToCross - 1 && block == endBlockIndex - 1) {
				Memcpy(buffer + readAmount, &table->Blocks[block][index], BLOCK_SIZE - endBlockIndex - index);
				readAmount += BLOCK_SIZE - endBlockIndex - index;
			} else if(size - readAmount + index < BLOCK_SIZE) {
				Memcpy((uintptr_t)buffer + readAmount, &table->Blocks[block][index], size);
				readAmount += size;
			} else {
				Memcpy(buffer + readAmount, &table->Blocks[block][index], BLOCK_SIZE - index);
				readAmount += BLOCK_SIZE - index;
			}

			index = 0;
			++block;
		}

		if (crossedTables < endTablesToCross) {
			if(table->NextTable == NULL) return -1;
			table = table->NextTable;
		} else {
			break;
		}
	} while(true);

	return readAmount;
}

intmax_t RamFS::WriteNode(const inode_t node, const size_t offset, const size_t size, void *buffer) {
	if (node > MaxInodes) return -1;

	InodeTableObject *file = &InodeTable[node];

	if(file->Available) return -1;
/*	if(file->NodeData.Properties & NODE_PROPERTY_DIRECTORY) return -1;*/
	
	if(file->BlockTable == NULL) return -1;

	BlockTable *table = file->BlockTable;

	size_t startIndexInBlock = offset % BLOCK_SIZE;
	size_t startRoundedOffset = offset - offset % BLOCK_SIZE;
	size_t startBlockIndex = startRoundedOffset / BLOCK_SIZE % BLOCKS_IN_BLOCK_TABLE;
	size_t startTablesToCross = (startRoundedOffset % BLOCK_SIZE - startBlockIndex) / BLOCKS_IN_BLOCK_TABLE;

	size_t end = offset + size;
	size_t endIndexInBlock = end % BLOCK_SIZE;
	size_t endRoundedOffset = end - end % BLOCK_SIZE;
	size_t endBlockIndex = endRoundedOffset / BLOCK_SIZE % BLOCKS_IN_BLOCK_TABLE;
	size_t endTablesToCross = (endRoundedOffset % BLOCK_SIZE - endBlockIndex) / BLOCKS_IN_BLOCK_TABLE;

	for (size_t i = 0; i < startTablesToCross; ++i) {
		if(table->NextTable == NULL) return -1;
		table = table->NextTable;
	}

	size_t crossedTables = startTablesToCross;
	size_t writtenAmount = 0;
	size_t block = startBlockIndex;
	size_t index = startIndexInBlock;

	do {
		while(block < BLOCKS_IN_BLOCK_TABLE) {
			if(writtenAmount >= size) return writtenAmount;

			if(table->Blocks[block] == NULL || table->Blocks[block] == -1) {
				table->Blocks[block] = Malloc(BLOCK_SIZE);
				Memset(table->Blocks[block], 0, BLOCK_SIZE);
			}

			if(crossedTables == endTablesToCross - 1 && block == endBlockIndex - 1) {
				Memcpy(&table->Blocks[block][index], buffer + writtenAmount, BLOCK_SIZE - endBlockIndex - index);
				writtenAmount += BLOCK_SIZE - endBlockIndex - index;
			} else if(size - writtenAmount + index < BLOCK_SIZE) {
				Memcpy(&table->Blocks[block][index], buffer + writtenAmount, size);
				writtenAmount += size;
			} else {
				Memcpy(&table->Blocks[block][index], buffer + writtenAmount, BLOCK_SIZE - index);
				writtenAmount += BLOCK_SIZE - index;
			}

			index = 0;
			++block;
		}

		if (crossedTables < endTablesToCross) {
			if(table->NextTable == NULL) return -1;
			table = table->NextTable;
		} else {
			break;
		}
	} while(true);

	return writtenAmount;
}
