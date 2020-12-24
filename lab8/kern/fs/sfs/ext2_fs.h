//
// Created by lumin on 2020/11/29.
//

#ifndef LAB8_EXT2_FS_H
#define LAB8_EXT2_FS_H

#include <defs.h>

/*
 * Revision levels
 */
#define EXT2_GOOD_OLD_REV	0	// The good old (original) format
#define EXT2_DYNAMIC_REV	1 	// V2 format w/ dynamic inode sizes

#define EXT2_CURRENT_REV	EXT2_GOOD_OLD_REV
#define EXT2_MAX_SUPP_REV	EXT2_DYNAMIC_REV

#define EXT2_GOOD_OLD_INODE_SIZE 128

struct ext2_disk_super
{
    /* followings are supported for all versions ext2 fs */
    uint32_t s_inodes_count;                     // Total number of inodes in file system
    uint32_t nr_blks;                       // Total number of blocks in file system
    uint32_t nr_blks_su;                    // Number of blocks reserved for superuser
    uint32_t nr_free_blks;                  // Total number of unallocated blocks
    uint32_t nr_free_inodes;                // Total number of unallocated inodes
    uint32_t su_blkno;                      // Block number of the block containing the superblock
    uint32_t logkb_blksize;                 // log2 (block size) - 10. (In other words, the number to shift 1,024 to the left by to obtain the block size)
    uint32_t logkb_fragsize;                // log2 (fragment size) - 10. (In other words, the number to shift 1,024 to the left by to obtain the fragment size)
    uint32_t nr_blks_per_grp;               // Number of blocks in each block group
    uint32_t nr_frags_per_grp;              // Number of fragments in each block group
    uint32_t nr_inodes_per_grp;             // Number of inodes in each block group
    uint32_t last_mtime;                    // Last mount time (in POSIX time)
    uint32_t last_wtime;                    // Last written time (in POSIX time)
    uint16_t nr_mnts_since_last_check;      // Number of times the volume has been mounted since its last consistency check (fsck)
    uint16_t max_nr_mnts_since_last_check;  // Number of mounts allowed before a consistency check (fsck) must be done
    uint16_t signature;                     // Ext2 signature (0xef53), used to help confirm the presence of Ext2 on a volume
    uint16_t fs_state;                      // File system state
    uint16_t error_deal;                    // What to do when an error is detected
    uint16_t s_minor_rev_level;             // Minor portion of version (combine with Major portion below to construct full version field)
    uint32_t last_ctime;                    // POSIX time of last consistency check (fsck)
    uint32_t ctime_interval;                // Interval (in POSIX time) between forced consistency checks (fsck)
    uint32_t creator_os;                    // Operating system ID from which the filesystem on this volume was created
    uint32_t s_rev_level;                   // Major portion of version (combine with Minor portion above to construct full version field)
    uint16_t uid_su_blks;                   // User ID that can use reserved blocks
    uint16_t gid_su_blks;                   // Group ID that can use reserved blocks

    /* followings are supported for only major version greater than or equal to 1 */

    /* EXT2_DYNAMIC_REV Specific */
    uint32_t s_first_ino;               // First non-reserved inode in file system. (In versions < 1.0, this is fixed as 11)
    uint16_t s_inode_size;              // Size of each inode structure in bytes. (In versions < 1.0, this is fixed as 128)
    uint16_t s_block_group_nr;          // Block group that this superblock is part of (if backup copy)
    uint32_t s_feature_compat;          // Optional features present (features that are not required to read or write, but usually result in a performance increase.)
    uint32_t s_feature_incompat;        // Required features present (features that are required to be supported to read or write.)
    uint32_t s_feature_ro_compat;       // Features that if not supported, the volume must be mounted read-only
    uint8_t s_uuid[16];                 // File system ID (what is output by blkid)
    char s_volume_name[16];             // Volume name (C-style string: characters terminated by a 0 byte)
    char s_last_mounted[64];            // Path volume was last mounted to (C-style string: characters terminated by a 0 byte)
    uint32_t s_algo_bitmap;             // Compression algorithms used (see Required features above)

    /* Performance Hints */
    uint8_t s_prealloc_blocks;          // Number of blocks to preallocate for files
    uint8_t s_prealloc_dir_blocks;      // Number of blocks to preallocate for directories0
    uint16_t unused;

    /* Journaling Support */
    uint8_t s_journal_uuid[16];         // Journal ID (same style as the File system ID above)
    uint32_t s_journal_inum;            // Journal inode
    uint32_t s_journal_dev;             // Journal device
    uint32_t s_last_orphan;             // Head of orphan inode list
} __attribute__((packed));

typedef struct
{
    uint32_t block_bitmap_blkno;            // Block address of block usage bitmap
    uint32_t inode_bitmap_blkno;            // Block address of inode usage bitmap
    uint32_t inode_table_blkno;             // Starting block address of inode table
    uint16_t nr_free_blocks;                // Number of unallocated blocks in group
    uint16_t nr_free_inodes;                // Number of unallocated inodes in group
    uint16_t nr_dirs;                       // Number of directories in group
    uint8_t unused[14];
} __attribute__((packed)) group_descriptor_t;

struct ext2_mm_super
{
    uint32_t s_block_size_bits;
    uint32_t s_block_size;
    uint32_t s_sector_per_block;

    uint32_t lba_base;
    uint32_t lba_size;

    uint32_t nr_grps;
    group_descriptor_t *gdt;

    struct ext2_disk_super *disk_super;
};

/* file format */
#define EXT2_S_IFSOCK   0xC000  // socket
#define EXT2_S_IFLNK    0xA000  // symbolic link
#define EXT2_S_IFREG    0x8000  // regular file
#define EXT2_S_IFBLK    0x6000  // block device
#define EXT2_S_IFDIR    0x4000  // directory
#define EXT2_S_IFCHR    0x2000  // character device
#define EXT2_S_IFIFO    0x1000  // fifo

/* process execution user/group override */
#define EXT2_S_ISUID    0x0800  // Set process User ID
#define EXT2_S_ISGID    0x0400  // Set process Group ID
#define EXT2_S_ISVTX    0x0200  // sticky bit

/* access rights */
#define EXT2_S_IRUSR    0x0100  // user read
#define EXT2_S_IWUSR    0x0080  // user write
#define EXT2_S_IXUSR    0x0040  // user execute
#define EXT2_S_IRGRP    0x0020  // group read
#define EXT2_S_IWGRP    0x0010  // group write
#define EXT2_S_IXGRP    0x0008  // group execute
#define EXT2_S_IROTH    0x0004  // others read
#define EXT2_S_IWOTH    0x0002  // others write
#define EXT2_S_IXOTH    0x0001  // others execute

struct ext2_disk_inode
{
    uint16_t i_mode;            // 16bit value used to indicate the format of the described file and the access rights.
    uint16_t i_uid;             // User ID
    uint32_t i_size;            // In revision 0, (signed) 32bit value indicating the size of the file in bytes. In revision 1 and later revisions,
    // and only for regular files, this represents the lower 32-bit of the file size; the upper 32-bit is located in
    // the i_dir_acl.

    uint32_t i_atime;           // Last Access Time (in POSIX time)
    uint32_t i_ctime;           // Creation Time (in POSIX time)
    uint32_t i_mtime;           // Last Modification time (in POSIX time)
    uint32_t i_dtime;           // Deletion time (in POSIX time)
    uint16_t i_gid;             // Group ID
    uint16_t i_links_count;     // Count of hard links (directory entries) to this inode. When this reaches 0, the data blocks are marked as unallocated.
    uint32_t i_blocks;          // Count of disk sectors (not Ext2 blocks) in use by this inode, not counting the actual inode structure nor directory entries linking to the inode.
    uint32_t i_flags;           // 32bit value indicating how the ext2 implementation should behave when accessing the data for this inode.
    uint32_t i_osd1;            // 32bit OS dependant value.
    uint32_t i_block[15];       // 15 x 32bit block numbers pointing to the blocks containing the data for this inode.
    // The first 12 blocks are direct blocks.
    // The 13th entry in this array is the block number of the first indirect block.
    // The 14th entry in this array is the block number of the first doubly-indirect block.
    // The 15th entry in this array is the block number of the triply-indirect block.

    uint32_t i_generation;      // Generation number (Primarily used for NFS)
    uint32_t i_file_acl;        // In Ext2 version 0, this field is reserved. In version >= 1, Extended attribute block (File ACL).
    uint32_t i_dir_acl;         // In Ext2 version 0, this field is reserved. In version >= 1, Upper 32 bits of file size (if feature bit set) if it's a file, Directory ACL if it's a directory
    uint32_t i_faddr;           // Block address of fragment
    uint8_t i_osd2[12];         // 96bit OS dependant structure.
} __attribute__((packed));

struct ext2_mm_inode
{
    struct ext2_disk_inode *dinode;
};

/*
 * Structure of a directory entry
 */
#define EXT2_NAME_LEN 255

struct ext2_dentry
{
    uint32_t inode;             // inode number
    uint16_t rec_len;           // dentry length
    union {
        uint16_t name_len;      // name length for revision before 1
        struct {                // name length for revision after 1, low 8 bit used as name_len, high 8 bit used as file_type
            uint8_t name_len_2;
            uint8_t file_type;
        };
    };
    char name[EXT2_NAME_LEN];   // file name
};

void ext2_test();
void dump_sector(char data[]);

#endif //LAB8_EXT2_FS_H
