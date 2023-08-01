#pragma once
#include <stdint.h>
#include <stddef.h>

#define MAX_NAME_SIZE            0x0100
#define MAX_PATH_SIZE            0x1000

#define NODES_IN_VNODE_TABLE     0x0040

#define NODE_CREATE              0x0001
#define NODE_DELETE              0x0002
#define NODE_GETBYNODE           0x0003
#define NODE_GETBYNAME           0x0004
#define NODE_GETBYINDEX          0x0005
#define NODE_GETROOT             0x0006

#define FOPS_CREATE              0x0001
#define FOPS_DELETE              0x0002
#define FOPS_RENAME              0x0003
#define FOPS_CHMOD               0x0004
#define FOPS_OPEN                0x0005
#define FOPS_CLOSE               0x0006
#define FOPS_READ                0x0007
#define FOPS_WRITE               0x0008
#define FOPS_OPENDIR             0x0009
#define FOPS_CLOSEDIR            0x000A
#define FOPS_READDIR             0x000B


#define NODE_PROPERTY_FILE       0x0001
#define NODE_PROPERTY_DIRECTORY  0x0002
#define NODE_PROPERTY_CHARDEV    0x0004
#define NODE_PROPERTY_BLOCKDEV   0x0008
#define NODE_PROPERTY_PIPE       0x0010
#define NODE_PROPERTY_SYMLINK    0x0020
#define NODE_PROPERTY_MOUNTPOINT 0x0040

#define FILE_OPERATION_REQUEST_MAGIC_NUMBER  0x4690738
#define FILE_OPERATION_RESPONSE_MAGIC_NUMBER 0x7502513

typedef uintmax_t filesystem_t;
typedef uintmax_t inode_t;
typedef uint32_t property_t;
typedef uint32_t mode_t;
typedef intmax_t fd_t;
typedef intmax_t dir_t;
typedef intmax_t result_t;
