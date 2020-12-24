//
// Created by lumin on 2020/11/28.
//
#include <ext2_fs.h>
#include <sdcard.h>
#include <assert.h>
#include <stdio.h>
#include <kmalloc.h>
#include <vfs.h>
#include <inode.h>

#define BLK_SIZE    512
#define EXT2_OS_LINUX		0
#define EMPTY_PART_ENTRY(entry) ((((entry).reserv0) == 0) && (((entry).part_type) == 0) && (((entry).reserv1) == 0) && (((entry).reserv2) == 0) && (((entry).reserv3) == 0))



struct mbr
{
    char code_area[446];
    struct
    {
        uint32_t reserv0;
        uint8_t part_type;
        uint8_t reserv1;
        uint16_t reserv2;
        uint64_t reserv3;
    } part_entries[4];
    uint8_t magic0;
    uint8_t magic1;
} __attribute__((packed));

struct gpt_header
{
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc;
    uint32_t reserv;
    uint64_t this_LBA;
    uint64_t backup_LBA;
    uint64_t first_usable_LBA;
    uint64_t last_usable_LBA;
    uint64_t disk_guid_lo;
    uint64_t disk_guid_hi;
    uint64_t partentry_start_LBA;
    uint32_t nr_partentries;
    uint32_t partentry_size;
    uint32_t partentries_crc;
} __attribute__((packed));

typedef struct
{
    uint64_t lo;
    uint64_t hi;
} gpt_part_type_guid_t;

typedef struct
{
    uint64_t lo;
    uint64_t hi;
} gpt_part_guid_t;

struct gpt_entry
{
    // mixed endian fields
    gpt_part_type_guid_t type_guid;
    gpt_part_guid_t part_guid;

    // little endian fields
    uint64_t first_LBA;
    uint64_t last_LBA; // inclusive
    uint64_t flags;

    char part_name[72];
} __attribute__((packed));

enum {
    LINUX_LINUX_FS_DATA = 0,
    GPT_PART_TYPE_MAX,
} gpt_part_type;

gpt_part_type_guid_t support_gpt_part_types[]=
        {
            [LINUX_LINUX_FS_DATA] = { 0x8E793D69D8477DE4,0x0FC63DAF84834772},
        };

#define MAX_GPT_ENTRIES 128
struct gpt_entry gpt_entry[MAX_GPT_ENTRIES];



static int check_GPT_compatible_MBR(struct mbr* mbrp)
{
    int i, found = -1;
    for (i = 0; i < 4; i++) {
        // skip empty entries
        if (mbrp->part_entries[i].part_type == 0)
            continue;

        // type other than 0xEE means, this is not a GPT table
        // directly return false
        if (mbrp->part_entries[i].part_type != 0xEE)
            return 0;
        else{
            found = i;
            break;
        }
    }

    // if there is a 0xEE, the left is to
    // verify there is only one non-empty entry in MBR
    if (found >= 0){
        for (i = 0; i < 4; i++) {
            // skip the one with 0xEE
            if (i == found)
                continue;

            // other non-empty entry found, this is not a GPT table
            // directly return false
            if (!EMPTY_PART_ENTRY(mbrp->part_entries[i]))
                return 0;
        }
    }

    return 1;
}

static int check_gpt_header(struct gpt_header *header)
{
    assert(header->signature == 0x5452415020494645);
    assert(header->reserv == 0);
    assert(header->this_LBA == 1);
    assert(header->partentry_start_LBA >= 2);
}

void dump_sector(char data[])
{
    int row, col;
    for(row = 0; row < 64; row++){
        for (col = 0; col < 8; col++)
            cprintf("0x%x ", data[row * 8 + col]);
        cprintf("\n");
    }
}

void dump_gpt_header(struct gpt_header* header)
{
    cprintf("{\n"
            "\tsignature: 0x%lx\n"
            "\trevision: 0x%x\n"
            "\theader size: %d\n"
            "\treserv: 0x%x\n"
            "\tthis LBA: 0x%lx\n"
            "\tbackup LBA: 0x%lx\n"
            "\tfirst usable LBA: 0x%lx\n"
            "\tlast usable LBA: 0x%lx\n"
            "\tpartition entries start LBA: 0x%lx\n"
            "\tnr partition entries: %d\n"
            "\tpartition entry size: %d\n"
            "}\n",
            header->signature,
            header->revision,
            header->header_size,
            header->reserv,
            header->this_LBA,
            header->backup_LBA,
            header->first_usable_LBA,
            header->last_usable_LBA,
            header->partentry_start_LBA,
            header->nr_partentries,
            header->partentry_size);
}

void dump_gpt_entry(struct gpt_entry* entry)
{
    cprintf("{\n"
            "\ttype guid: %016lx-%016lx\n"
            "\tpartition guid: %016lx-%016lx\n"
            "\tLBA: [0x%lx-0x%lx]\n"
            "\tflags: 0x%lx\n"
            "}\n",
            entry->type_guid.hi,
            entry->type_guid.lo,
            entry->part_guid.hi,
            entry->part_guid.lo,
            entry->first_LBA,
            entry->last_LBA,
            entry->flags);
}

void dump_ext2_disk_super(struct ext2_disk_super *super)
{
    cprintf("{\n"
            "\tnr inodes: 0x%x\n"
            "\tnr blocks: 0x%x\n"
            "\tnr blocks for su: 0x%x\n"
            "\tnr free blocks: 0x%x\n"
            "\tnr free inodes: 0x%x\n"
            "\tsuper block NO: 0x%x\n"
            "\tlog KB block size: %d\n"
            "\tlog KB fragment size: %d\n"
            "\tnr blocks per block group: 0x%x\n"
            "\tnr fragments per block group: 0x%x\n"
            "\tnr inodes per block group: 0x%x\n"
            "\tlast mount time: %d\n"
            "\tlast write time: %d\n"
            "\tnr mounts since last check: %d\n"
            "\tmax nr mounts since last check: %d\n"
            "\text signature: 0x%x\n"
            "\tfilesystem state: 0x%x\n"
            "\twhat to do when error: 0x%x\n"
            "\tminor version: 0x%x\n"
            "\tlast check time: %d\n"
            "\tcheck time intervel: %d\n"
            "\tcreator OS: 0x%x\n"
            "\tmajor version: 0x%x\n"
            "\tuid can use reserved blocks: 0x%x\n"
            "\tgid can use reserved blocks: 0x%x\n",
            super->s_inodes_count,
            super->nr_blks,
            super->nr_blks_su,
            super->nr_free_blks,
            super->nr_free_inodes,
            super->su_blkno,
            super->logkb_blksize,
            super->logkb_fragsize,
            super->nr_blks_per_grp,
            super->nr_frags_per_grp,
            super->nr_inodes_per_grp,
            super->last_mtime,
            super->last_wtime,
            super->nr_mnts_since_last_check,
            super->max_nr_mnts_since_last_check,
            super->signature,
            super->fs_state,
            super->error_deal,
            super->s_minor_rev_level,
            super->last_ctime,
            super->ctime_interval,
            super->creator_os,
            super->s_rev_level,
            super->uid_su_blks,
            super->gid_su_blks);

    if (super->s_rev_level >= 1)
        cprintf("\tfirst non-reserved inode in fs: 0x%x\n"
                "\tinode size: %d\n"
                "\tblock group this super in: %d\n"
                "\tcompatible feature bitmap: 0x%x\n"
                "\tincompatible feature bitmap: 0x%x\n"
                "\tread-only feature bitmap: 0x%x\n"
                "\tfs uuid: %lx-%lx\n"
                "\tvolume name: %s\n"
                "\tlast mount path: %s\n"
                "\tcompress algorithm used bitmap: 0x%x\n"
                "\tfile preallocation block number: %d\n"
                "\tdirectory preallocation block number: %d\n"
                "\tjournal uuid: %lx-%lx\n"
                "\tjournal inode: 0x%x\n"
                "\tjournal device: %d\n"
                "\thead of orphan inode list: 0x%x\n",
                super->s_first_ino,
                super->s_inode_size,
                super->s_block_group_nr,
                super->s_feature_compat,
                super->s_feature_incompat,
                super->s_feature_ro_compat,
                *(uint64_t *)&super->s_uuid[0],
                *(uint64_t *)&super->s_uuid[8],
                super->s_volume_name,
                super->s_last_mounted,
                super->s_algo_bitmap,
                super->s_prealloc_blocks,
                super->s_prealloc_dir_blocks,
                *(uint64_t *)&super->s_journal_uuid[0],
                *(uint64_t *)&super->s_journal_uuid[8],
                super->s_journal_inum,
                super->s_journal_dev,
                super->s_last_orphan);
    cprintf("}\n");
}

void dump_group_descriptor(group_descriptor_t *gdp)
{
    cprintf("{\n"
            "\tblock bitmap block no: 0x%x\n"
            "\tblock bitmap block no: 0x%x\n"
            "\tinode table block no: 0x%x\n"
            "\tfree blocks: 0x%x\n"
            "\tfree inodes: 0x%x\n"
            "\tnumber of dirs: 0x%x\n"
            "}\n",
            gdp->block_bitmap_blkno,
            gdp->inode_bitmap_blkno,
            gdp->inode_table_blkno,
            gdp->nr_free_blocks,
            gdp->nr_free_inodes,
            gdp->nr_dirs);
}

void dump_disk_inode(struct ext2_disk_inode *inode)
{
    uint64_t fsize = inode->i_dir_acl;
    fsize <<= 32;
    fsize += inode->i_size;

    cprintf("{\n"
            "\tinode mode: 0x%x\n"
            "\tuid: 0x%x\n"
            "\tfile size: 0x%lx\n"
            "\tgid: 0x%x\n"
            "\thard link counts: %d\n"
            "\tsectors occupied: 0x%x\n"
            "\tflags: 0x%x\n"
            "\tblocks:\n",
            inode->i_mode,
            inode->i_uid,
            fsize,
            inode->i_gid,
            inode->i_links_count,
            inode->i_blocks,
            inode->i_flags);
    for (int i = 0; i < 15; i++)
        cprintf("\t\t[0x%x]\n", inode->i_block[i]);
    cprintf("}\n");
}

void mix2little(uint64_t *lop, uint64_t *hip)
{
    uint8_t lo[8], hi[8];
    *((uint32_t *)&hi[4]) = *lop & 0xFFFFFFFF;
    *((uint16_t *)&hi[2]) = (*lop >> 32) & 0xFFFF;
    *((uint16_t *)&hi[0]) = (*lop >> 48) & 0xFFFF;
    lo[7] = *hip & 0xFF;
    lo[6] = (*hip >> 8) & 0xFF;
    lo[5] = (*hip >> 16) & 0xFF;
    lo[4] = (*hip >> 24) & 0xFF;
    lo[3] = (*hip >> 32) & 0xFF;
    lo[2] = (*hip >> 40) & 0xFF;
    lo[1] = (*hip >> 48) & 0xFF;
    lo[0] = (*hip >> 56) & 0xFF;

    *hip = *(uint64_t *)hi;
    *lop = *(uint64_t *)lo;
}

#define ext2_blk_read(super, blkno)     (ext2_blks_read(super, blkno, 1))
static void *ext2_blks_read(struct ext2_mm_super *super,uint32_t blkno, uint32_t nr_blks)
{
    uint32_t sector = super->lba_base + blkno * super->s_sector_per_block;
    char *buff = (char *)kmalloc(super->s_block_size);

    if (!buff)
        goto failed;

    if (sd_read_sector(buff, sector, super->s_sector_per_block * nr_blks) != 0)
        goto undone;

    return buff;

undone:
    kfree(buff);
failed:
    return NULL;
}

struct ext2_mm_super *read_in_super(struct gpt_entry* entry)
{
    uint64_t lba_base, lba_bpb, offset_bpb;
    lba_base = entry->first_LBA;
    lba_bpb = lba_base + (1024 / BLK_SIZE);
    offset_bpb = (1024 % BLK_SIZE);

    uint16_t magic;
    char* bpb;
    struct ext2_mm_super *mm_super;
    struct ext2_disk_super *disk_super;

    if (!(bpb = (char *)kmalloc(BLK_SIZE)))
        return NULL;

    if (!(mm_super = (struct ext2_mm_super *)kmalloc(sizeof(struct ext2_mm_super))))
        goto undone_bpb;

    if (sd_read_sector(bpb, lba_bpb, 1) != 0)
        goto undone_super;

    magic = *(uint16_t *)&bpb[offset_bpb + 0x38];
    if (magic == 0xEF53){
        disk_super = (struct ext2_disk_super *)bpb;
        mm_super->disk_super = disk_super;

        mm_super->s_block_size_bits = disk_super->logkb_blksize + 10;
        mm_super->s_block_size = 1 << mm_super->s_block_size_bits;
        mm_super->s_sector_per_block = mm_super->s_block_size / BLK_SIZE;

        if (mm_super->s_block_size % BLK_SIZE != 0)
            goto undone_super;

        mm_super->lba_base = entry->first_LBA;
        mm_super->lba_size = entry->last_LBA - entry->first_LBA;

        return mm_super;
    }

undone_super:
    kfree(mm_super);
undone_bpb:
    kfree(bpb);
    return NULL;
}

static int read_in_gdt(struct ext2_mm_super *mm_super)
{
    if (!mm_super || !mm_super->disk_super)
        goto failed;

    struct ext2_disk_super *disk_super = mm_super->disk_super;
    uint32_t nr_grps = ROUNDUP_DIV(disk_super->nr_blks, disk_super->nr_blks_per_grp);

    if (nr_grps != ROUNDUP_DIV(disk_super->s_inodes_count, disk_super->nr_inodes_per_grp))
        cprintf("Number of block groups inconsistent!\n");
    else
        cprintf("Number of block groups: %d\n", nr_grps);

    uint32_t gdt_blkno = mm_super->disk_super->su_blkno + 1;
    uint64_t nr_blk = ROUNDUP_DIV(nr_grps * sizeof(group_descriptor_t), mm_super->s_block_size);
    group_descriptor_t *gdt = (group_descriptor_t*)ext2_blks_read(mm_super, gdt_blkno, nr_blk);

    if (!gdt)
        goto failed;

    mm_super->nr_grps = nr_grps;
    mm_super->gdt = gdt;

    return 0;

failed:
    return -1;
}

group_descriptor_t *ext2_get_group_desc(struct ext2_mm_super *super, uint32_t grpno)
{
    if (!super || !super->gdt || grpno >= super->nr_grps)
        return NULL;

    return &super->gdt[grpno];
}

struct inode_ops *ext2_get_iops(struct ext2_disk_inode *dinode)
{
    // todo
    return NULL;
};

#define EXT2_INODE_SIZE(super) ((super)->disk_super->s_rev_level == EXT2_GOOD_OLD_REV ? EXT2_GOOD_OLD_INODE_SIZE : (super)->disk_super->s_inode_size)

struct ext2_disk_inode *ext2_get_disk_inode(struct ext2_mm_super *super, uint32_t ino)
{
    if (ino == 0)
        return NULL;

    uint32_t grpno, offset, blkoff;
    group_descriptor_t *gdp;
    char *buff;
    struct ext2_disk_inode *inodep;

    if(!(inodep = (struct ext2_disk_inode *)kmalloc(sizeof(struct ext2_disk_inode))))
        goto failed;

    grpno = (ino - 1) / super->disk_super->nr_inodes_per_grp;
    offset = (ino - 1) % super->disk_super->nr_inodes_per_grp * EXT2_INODE_SIZE(super);

    if (!(gdp = ext2_get_group_desc(super, grpno)))
        goto undone;

    blkoff = gdp->inode_table_blkno + (offset >> super->s_block_size_bits);
    if (!(buff = ext2_blk_read(super, blkoff)))
        goto undone;

    *inodep = ((struct ext2_disk_inode *)buff)[offset % super->s_block_size / EXT2_INODE_SIZE(super)];
    kfree(buff);
    return inodep;

undone:
    kfree(inodep);
failed:
    return NULL;
}

struct inode *ext2_get_inode(struct ext2_mm_super *super, uint32_t ino)
{
    struct ext2_disk_inode *dinode;
    struct ext2_mm_inode *minode;
    struct inode *inodep;

    if (!(dinode = ext2_get_disk_inode(super, EXT2_ROOT_INO)))
        goto failed;

    if (!(inodep = alloc_inode(ext2_inode)))
        goto undone;

    vop_init(inodep, ext2_get_iops(dinode), info2fs(super, ext2));
    minode = vop_info(inodep, ext2_inode);
    minode->dinode = dinode;

    return inodep;

undone:
    kfree(inodep);
failed:
    return NULL;
}

void mnt_root(struct ext2_mm_super *super)
{
    struct ext2_disk_inode *iroot = ext2_get_disk_inode(super, EXT2_ROOT_INO);
    uint32_t pos = 0;
    uint32_t block_idx = pos >> super;
    uint32_t nr_blocks = ROUNDUP_DIV(iroot->i_size);

    if (iroot)
        dump_disk_inode(iroot);


}


struct inode *ext2_get_root(struct fs *fs)
{
    return ext2_get_inode(fsop_info(fs, ext2), EXT2_ROOT_INO);
}

void ext2_test()
{
    struct mbr *mbr;
    char block0[BLK_SIZE];
    assert(sd_read_sector(block0, 0, 1) == 0);
    mbr = (struct mbr *)block0;
    if (check_GPT_compatible_MBR(mbr) == 0)
        cprintf("Only GPT supported!\n");
    else
        cprintf("GPT detected!\n");

    struct gpt_header* header;
    char LBA1[BLK_SIZE];
    assert(sd_read_sector(LBA1, 1, 1) == 0);
    header = (struct gpt_header *)LBA1;
    check_gpt_header(header);

    char LBAX[BLK_SIZE];
    int i, nr_gpt_entry = 0, nr_entry_per_blk = BLK_SIZE / header->partentry_size;
    uint32_t lba_start = header->partentry_start_LBA;
    uint32_t lba_end = lba_start + ROUNDUP_DIV(header->nr_partentries, nr_entry_per_blk);
    struct gpt_entry *tmp;
    for (int lba = lba_start; lba < lba_end; lba++){
        assert(sd_read_sector(LBAX, lba, 1) == 0);
        tmp = (struct gpt_entry *)LBAX;
        for (i = 0; i < nr_entry_per_blk; i++) {
            if ((tmp[i].type_guid.lo == 0) && (tmp[i].type_guid.hi == 0))
                break;
            gpt_entry[nr_gpt_entry + i] = tmp[i];
        }
        nr_gpt_entry += i;
        if (i < nr_entry_per_blk)
            break;
    }
    for (i = 0; i < nr_gpt_entry; i++) {
        mix2little(&gpt_entry[i].type_guid.lo, &gpt_entry[i].type_guid.hi);
        mix2little(&gpt_entry[i].part_guid.lo, &gpt_entry[i].part_guid.hi);
        dump_gpt_entry(&gpt_entry[i]);
    }

    if (gpt_entry[0].type_guid.hi == support_gpt_part_types[LINUX_LINUX_FS_DATA].hi && gpt_entry[0].type_guid.lo == support_gpt_part_types[LINUX_LINUX_FS_DATA].lo){
        struct ext2_mm_super *mm_super;
        mm_super = read_in_super(&gpt_entry[0]);
        if (mm_super) {
            dump_ext2_disk_super(mm_super->disk_super);
            if (read_in_gdt(mm_super) == 0){
                cprintf("success");
                for (i = 0; i < mm_super->nr_grps; ++i) {
                    dump_group_descriptor(&mm_super->gdt[i]);
                }
                char *blk_bitmap = ext2_blk_read(mm_super, mm_super->gdt[0].block_bitmap_blkno);
                char *inode_bitmap = ext2_blk_read(mm_super, mm_super->gdt[0].inode_bitmap_blkno);
                cprintf("block bitmap:\n");
                dump_sector(blk_bitmap);
                cprintf("inode bitmap:\n");
                dump_sector(inode_bitmap);
                struct ext2_disk_inode *inodes = ext2_blk_read(mm_super, mm_super->gdt[0].inode_table_blkno);
                if (inodes)
                    mnt_root(mm_super);

            }
        }
    }
}




