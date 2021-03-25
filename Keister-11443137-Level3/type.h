typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct ext2_super_block SUPER;
typedef struct ext2_group_desc GD;
typedef struct ext2_inode INODE;
typedef struct ext2_dir_entry_2 DIR;

#define FREE 0
#define READY 1

#define MTS 16
#define BLKSIZE 1024
#define NMINODE 128
#define NFD 16
#define NOFT 40
#define NPROC 2

#define SUPER_USER 0

typedef struct minode
{
    INODE INODE;
    int dev, ino;
    int refCount;
    int dirty;
    int mounted;
    struct mtable *mptr;
} MINODE;

typedef struct mtable
{
    int dev;
    int ninodes;
    int nblocks;
    int free_blocks;
    int free_inodes;
    int bmap;
    int imap;
    int iblock;
    MINODE *mntDirPtr;
    char devName[64];
    char mntName[64];
} MTABLE;

typedef struct oft
{
    int mode;
    int refCount;
    MINODE *mptr;
    int offset;
} OFT;

typedef struct proc
{
    struct proc *next;
    int pid;
    int status;
    int uid, gid;
    MINODE *cwd;
    OFT *fd[NFD];
} PROC;

