#pragma once
#include "typedefs.h"

struct VNode {
	char Name[MAX_NAME_SIZE] = { 0 };

	filesystem_t FSDescriptor;
	inode_t Inode;

	property_t Properties;
	inode_t Directory;
}__attribute__((packed));

struct DirNode {
	char Name[MAX_NAME_SIZE] = { 0 };

	property_t Properties;
}__attribute__((packed));
