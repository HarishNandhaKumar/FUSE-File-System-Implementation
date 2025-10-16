/*
 * file: homework.c
 * description: skeleton file for CS 5600 system
 *
 * CS 5600, Computer Systems, Northeastern
 */

#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "fs5600.h"

/* if you don't understand why you can't use these system calls here,
 * you need to read the assignment description another time
 */
#define stat(a,b) error do not use stat()
#define open(a,b) error do not use open()
#define read(a,b,c) error do not use read()
#define write(a,b,c) error do not use write()

#define FILENAME_MAXLENGTH 32
#define MAX_PATH_LEN 10
#define MAX_NAME_LEN 27
#define MAX_DIREN_NUM 128

int translate(char *path);
int parse(char *path, char **pathv);
int get_inum_from_path(char *pathv[], int pathc);
void set_attr(struct fs_inode inode, struct stat *sb);
int search_free_dirent_num(struct fs_inode *inode);
void generate_inode(struct fs_inode *inode, mode_t mode);
int search_free_inode_map_bit();
void update_inode(struct fs_inode *_in, int inum);
int search_free_block_number();
int check_in_directory(struct fs_dirent dirent[], char *name);
int truncate_path(const char *path, char **truncated_path);
int get_parent_inode(char *path);
int exists_in_same_dir(char *src_path, char *dst_path, char *src_pathv[], int *path_source, char *dst_pathv[], int *path_dst);
void write_block(int block_inum, int block_start, const char *curr_buf, int write_length, int *len_written);
int fs_truncate(const char *path, off_t len);
char *get_name(char *path);



/* disk access. All access is in terms of 4KB blocks; read and
 * write functions return 0 (success) or -EIO.
 */
extern int block_read(void *buf, int lba, int nblks);
extern int block_write(void *buf, int lba, int nblks);

/* bitmap functions
 */
void bit_set(unsigned char *map, int i)
{
    map[i/8] |= (1 << (i%8));
}
void bit_clear(unsigned char *map, int i)
{
    map[i/8] &= ~(1 << (i%8));
}
int bit_test(unsigned char *map, int i)
{
    return map[i/8] & (1 << (i%8));
}


struct fs_super superblock;
unsigned char bitmap[FS_BLOCK_SIZE];


/* init - this is called once by the FUSE framework at startup. Ignore
 * the 'conn' argument.
 * recommended actions:
 *   - read superblock
 *   - allocate memory, read bitmaps and inodes
 */
void* fs_init(struct fuse_conn_info *conn)
{
    /* your code here */
    block_read(&superblock, 0, 1);
    block_read(&bitmap, 1, 1);
    return NULL;
}

/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path doesn't exist.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */

/* note on splitting the 'path' variable:
 * the value passed in by the FUSE framework is declared as 'const',
 * which means you can't modify it. The standard mechanisms for
 * splitting strings in C (strtok, strsep) modify the string in place,
 * so you have to copy the string and then free the copy when you're
 * done. One way of doing this:
 *
 *    char *_path = strdup(path);
 *    int inum = translate(_path);
 *    free(_path);
 */



/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - for several fields in 'struct stat' there is no corresponding
 *  information in our file system:
 *    st_nlink - always set it to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 *
 * success - return 0
 * errors - path translation, ENOENT
 * hint - factor out inode-to-struct stat conversion - you'll use it
 *        again in readdir
 */
int fs_getattr(const char *path, struct stat *sb)
{
    /* your code here */
    fs_init(NULL);
    char *temp_path = strdup(path);
    int inum = translate(temp_path);
    free(temp_path);
    if (inum == -ENOENT || inum == -ENOTDIR || inum == -EOPNOTSUPP) {
        return inum;
    }

    struct fs_inode inode;
    block_read(&inode, inum, 1);
    set_attr(inode, sb);

    return 0;
}

int translate(char *path) {
    char *pathv[10];
    int pathc = parse(path, pathv);
    return get_inum_from_path(pathv, pathc);
}

int parse(char *path, char **pathv) {
    int i;
    for (i = 0; i < MAX_PATH_LEN; i++) {
        if ((pathv[i] = strtok((char *)path, "/")) == NULL) {
                break;
        }
        if (strlen(pathv[i]) > MAX_NAME_LEN) {
            pathv[i][MAX_NAME_LEN] = 0;  // truncate to 27 characters
        }
        path = NULL;
    }
    return i;
}

int get_inum_from_path(char *pathv[], int pathc) {
    int inum = 2;
    for (int i = 0; i < pathc; i++) {
        struct fs_inode inode;
        block_read(&inode, inum, 1);
        if (!S_ISDIR(inode.mode)) {
            return -ENOTDIR;
        }
        int blocknum = inode.ptrs[0];
        struct fs_dirent dirent[MAX_DIREN_NUM];
        block_read(dirent, blocknum, 1);
        int entry_found = 0;
        for (int j = 0; j < MAX_DIREN_NUM; j++) {
            if (dirent[j].valid && strcmp(dirent[j].name, pathv[i]) == 0) {
                inum = dirent[j].inode;
                entry_found = 1;
                break;
            }
        }

        if (!entry_found) {
                return -ENOENT;
        }
    }
    return inum;
}



/* readdir - get directory contents.
 *
 * call the 'filler' function once for each valid entry in the
 * directory, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a pointer to a struct stat
 * success - return 0
 * errors - path resolution, ENOTDIR, ENOENT
 *
 * hint - check the testing instructions if you don't understand how
 *        to call the filler function
 */
int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
    char *temp_path = strdup(path);
    int inum = translate(temp_path);
    free(temp_path);
    if (inum == -ENOTDIR || inum == -ENOENT) {
        return inum;
    }

    struct fs_inode dir_inode;
    block_read(&dir_inode, inum, 1);
    if (!S_ISDIR(dir_inode.mode)) {
        return -ENOTDIR;
    }

    int blocknum = dir_inode.ptrs[0];

    struct fs_dirent dirents[MAX_DIREN_NUM];

    block_read(dirents, blocknum, 1);

    for (int i = 0; i < MAX_DIREN_NUM; i++) {
        if (dirents[i].valid) {
            struct stat sb;
            struct fs_inode dir_entry_inode;
            block_read(&dir_entry_inode, dirents[i].inode, 1);
            set_attr(dir_entry_inode, &sb);
            filler(ptr, dirents[i].name, &sb, 0);
        }
    }
    return 0;
}


void set_attr(struct fs_inode inode, struct stat *sb) {
    memset(sb, 0, sizeof(struct stat));
    sb->st_mode = inode.mode;
    sb->st_uid = inode.uid;
    sb->st_gid = inode.gid;
    sb->st_size = inode.size;
    sb->st_blksize = FS_BLOCK_SIZE;
    sb->st_blocks = (inode.size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    sb->st_nlink = 1;
    sb->st_atime = inode.mtime;
    sb->st_ctime = inode.ctime;
    sb->st_mtime = inode.mtime;
}


/* create - create a new file with specified permissions
 *
 * success - return 0
 * errors - path resolution, EEXIST
 *          in particular, for create("/a/b/c") to succeed,
 *          "/a/b" must exist, and "/a/b/c" must not.
 *
 * Note that 'mode' will already have the S_IFREG bit set, so you can
 * just use it directly. Ignore the third parameter.
 *
 * If a file or directory of this name already exists, return -EEXIST.
 * If there are already 128 entries in the directory (i.e. it's filled an
 * entire block), you are free to return -ENOSPC instead of expanding it.
 */
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    char *temp_path = strdup(path);
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(temp_path, pathv);

    int inum_dir = get_inum_from_path(pathv, pathc - 1);
    if (inum_dir == -ENOENT || inum_dir == -ENOTDIR) {
        free(temp_path);
        return inum_dir;
    }

    if (get_inum_from_path(pathv, pathc) > 0) {
        free(temp_path);
        return -EEXIST;
    }

    struct fs_inode parent_inode;
    if (block_read(&parent_inode, inum_dir, 1) < 0) {
        free(temp_path);
        return -EIO;
    }

    if (!S_ISDIR(parent_inode.mode)) {
        free(temp_path);
        return -ENOTDIR;
    }

    int free_dirent_num = search_free_dirent_num(&parent_inode);
    if (free_dirent_num < 0) {
        free(temp_path);
        return -ENOSPC;
    }

    int free_inum = search_free_inode_map_bit();
    if (free_inum < 0) {
        free(temp_path);
        return -ENOSPC;
    }

    struct fs_inode new_inode;
    generate_inode(&new_inode, mode);

    bit_set(bitmap, free_inum);
    block_write(&bitmap, 1, 1);
    update_inode(&new_inode, free_inum);

    char *tmp_name = pathv[pathc - 1];
    struct fs_dirent new_dirent;
    memset(&new_dirent, 0, sizeof(new_dirent));
    new_dirent.valid = 1;
    new_dirent.inode = free_inum;
    strncpy(new_dirent.name, tmp_name, sizeof(new_dirent.name) - 1);
    new_dirent.name[sizeof(new_dirent.name) - 1] = '\0';

    int blocknum = parent_inode.ptrs[0];
    struct fs_dirent dir[MAX_DIREN_NUM];
    if (block_read(dir, blocknum, 1) < 0) {
        free(temp_path);
        return -EIO;
    }

    memcpy(&dir[free_dirent_num], &new_dirent, sizeof(struct fs_dirent));
    block_write(dir, blocknum, 1);

    free(temp_path);
    return 0;
}

int search_free_dirent_num(struct fs_inode *inode) {
    struct fs_dirent dir[MAX_DIREN_NUM];
    int blocknum = inode->ptrs[0];
    block_read(dir, blocknum, 1);

    int no_free_dirent = -1;
    for (int i = 0; i < MAX_DIREN_NUM; i++) {
        if (!dir[i].valid) {
            no_free_dirent = i;
            break;
        }
    }
    return no_free_dirent;
}

void generate_inode(struct fs_inode *inode, mode_t mode) {
    struct fuse_context *ctx = fuse_get_context();
    uint16_t uid = ctx->uid;
    uint16_t gid = ctx->gid;
    time_t time_raw_format;
    time(&time_raw_format);
    inode->uid = uid;
    inode->gid = gid;
    inode->ctime = time_raw_format;
    inode->mtime = time_raw_format;
    inode->mode = mode;
    inode->size = 0;
}

int search_free_inode_map_bit() {
    int inode_capacity = FS_BLOCK_SIZE * 8;
    for (int i = 2; i < inode_capacity; i++) {
        if (!bit_test(bitmap, i)) {
            return i;
        }
    }
    return -ENOSPC;
}

void update_inode(struct fs_inode *_in, int inum) {
    block_write(_in, inum, 1);
}



/* mkdir - create a directory with the given mode.
 *
 * WARNING: unlike fs_create, @mode only has the permission bits. You
 * have to OR it with S_IFDIR before setting the inode 'mode' field.
 *
 * success - return 0
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create.
 */
int fs_mkdir(const char *path, mode_t mode)
{
    mode |= S_IFDIR;
    if (!S_ISDIR(mode))
        return -EINVAL;

    char *temp_path = strdup(path);
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(temp_path, pathv);

    int inum_dir = get_inum_from_path(pathv, pathc - 1);
    if (inum_dir == -ENOENT || inum_dir == -ENOTDIR) {
        free(temp_path);
        return inum_dir;
    }

    // Check if the target already exists
    if (get_inum_from_path(pathv, pathc) > 0) {
        free(temp_path);
        return -EEXIST;
    }

    struct fs_inode parent_inode;
    if (block_read(&parent_inode, inum_dir, 1) < 0) {
        free(temp_path);
        return -EIO;
    }

    if (!S_ISDIR(parent_inode.mode)) {
        free(temp_path);
        return -ENOTDIR;
    }

    int free_dirent_number = search_free_dirent_num(&parent_inode);
    if (free_dirent_number < 0) {
        free(temp_path);
        return -ENOSPC;
    }

    int free_inode_num = search_free_block_number();
    int free_diren_num = search_free_block_number();

    if (free_inode_num < 0 || free_diren_num < 0) {
        free(temp_path);
        return -ENOSPC;
    }

    struct fs_inode new_inode;
    generate_inode(&new_inode, mode);
    new_inode.ptrs[0] = free_diren_num;

    bit_set(bitmap, free_inode_num);
    bit_set(bitmap, free_diren_num);
    block_write(&bitmap, 1, 1);

    update_inode(&new_inode, free_inode_num);

    int *free_block = (int *)calloc(FS_BLOCK_SIZE, sizeof(int));
    block_write(free_block, free_diren_num, 1);
    free(free_block);

    struct fs_dirent new_dirent;
    memset(&new_dirent, 0, sizeof(new_dirent));
    new_dirent.valid = 1;
    new_dirent.inode = free_inode_num;

    char *tmp_name = pathv[pathc - 1];
    strncpy(new_dirent.name, tmp_name, sizeof(new_dirent.name) - 1);
    new_dirent.name[sizeof(new_dirent.name) - 1] = '\0';

    struct fs_dirent *dir = (struct fs_dirent *)malloc(FS_BLOCK_SIZE);
    int blocknum = parent_inode.ptrs[0];
    block_read(dir, blocknum, 1);

    memcpy(&dir[free_dirent_number], &new_dirent, sizeof(struct fs_dirent));
    block_write(dir, blocknum, 1);

    free(dir);
    free(temp_path);
    return 0;
}

int search_free_block_number() {
    for (int i = 0; i < FS_BLOCK_SIZE * 8; i++) {
        if (!bit_test(bitmap, i)) {
            int *free_block = calloc(1, FS_BLOCK_SIZE);
            block_write(free_block, i, 1);
            free(free_block);
            return i;
        }
    }
    return -ENOSPC;
}


/* unlink - delete a file
 *  success - return 0
 *  errors - path resolution, ENOENT, EISDIR
 */
int fs_unlink(const char *path)
{
    char *temp_path = strdup(path);
    int inum = translate(temp_path);
    free(temp_path);

    if (inum == -ENOENT || inum == -ENOTDIR) {
        return inum;
    }

    struct fs_inode inode;
    if (block_read(&inode, inum, 1) < 0) {
        return -EIO;
    }

    if (S_ISDIR(inode.mode)) {
        return -EISDIR;
    }

    int truncate_result = fs_truncate(path, 0);
    if (truncate_result != 0) {
        return truncate_result;
    }

    bit_clear(bitmap, inum);
    block_write(&bitmap, 1, 1); // update bitmap

    char *parent_path = NULL;
    truncate_path(path, &parent_path);
    int parent_inum = translate(parent_path);
    free(parent_path);

    struct fs_inode parent_inode;
    if (block_read(&parent_inode, parent_inum, 1) < 0) {
        return -EIO;
    }

    if (!S_ISDIR(parent_inode.mode)) {
        return -ENOTDIR;
    }

    int blocknum = parent_inode.ptrs[0];
    struct fs_dirent dir[MAX_DIREN_NUM];

    if (block_read(dir, blocknum, 1) < 0) {
        return -EIO;
    }

    char *filepath = strdup(path);
    char *filename = get_name(filepath);
    int found = check_in_directory(dir, filename);
    free(filepath);

    if (found < 0) {
        return -ENOENT;
    }

    memset(&dir[found], 0, sizeof(struct fs_dirent));
    block_write(dir, blocknum, 1);

    return 0;
}

int check_in_directory(struct fs_dirent dirent[], char *name) {
    for (int i = 0; i < MAX_DIREN_NUM; i++) {
        if (dirent[i].valid && strcmp(dirent[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}



/* rmdir - remove a directory
 *  success - return 0
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
int fs_rmdir(const char *path)
{
    char *temp_path = strdup(path);
    int inum = translate(temp_path);
    free(temp_path);

    if (inum == -ENOENT || inum == -ENOTDIR) {
        return inum;
    }

    struct fs_inode inode;
    if (block_read(&inode, inum, 1) < 0) {
        return -EIO;
    }

    if (!S_ISDIR(inode.mode)) {
        return -ENOTDIR;
    }

    struct fs_dirent entries[MAX_DIREN_NUM];
    if (block_read(entries, inode.ptrs[0], 1) < 0) {
        return -EIO;
    }

    for (int i = 0; i < MAX_DIREN_NUM; i++) {
        if (entries[i].valid) {
            return -ENOTEMPTY;
        }
    }

    char *parent_path = NULL;
    if (!truncate_path(path, &parent_path)) {
        return -EINVAL;
    }

    int diren_inum = inode.ptrs[0];
    bit_clear(bitmap, diren_inum);
    bit_clear(bitmap, inum);
    block_write(&bitmap, 1, 1);

    int parent_inum = translate(parent_path);
    free(parent_path);

    char *npath = strdup(path);
    if (npath[strlen(npath) - 1] == '/') {
        npath[strlen(npath) - 1] = '\0';
    }
    char *name = get_name(npath);

    struct fs_inode *parent_inode = malloc(sizeof(struct fs_inode));
    if (block_read(parent_inode, parent_inum, 1) < 0) {
        free(parent_inode);
        free(npath);
        return -EIO;
    }

    if (!S_ISDIR(parent_inode->mode)) {
        free(parent_inode);
        free(npath);
        return -ENOTDIR;
    }

    int blocknum = parent_inode->ptrs[0];
    struct fs_dirent *directory = malloc(FS_BLOCK_SIZE);
    if (block_read(directory, blocknum, 1) < 0) {
        free(directory);
        free(parent_inode);
        free(npath);
        return -EIO;
    }

    int found = check_in_directory(directory, name);
    if (found < 0) {
        free(directory);
        free(parent_inode);
        free(npath);
        return -ENOENT;
    }

    memset(&directory[found], 0, sizeof(struct fs_dirent));
    block_write(directory, blocknum, 1);

    free(directory);
    free(parent_inode);
    free(npath);
    return 0;
}


int truncate_path(const char *path, char **truncated_path) {
    int i = strlen(path) - 1;
    for (; i >= 0; i--) {
        if (path[i] != '/') {
            break;
        }
    }
    for (; i >= 0; i--) {
        if (path[i] == '/') {
            *truncated_path = (char *)malloc(sizeof(char) * (i + 2));
            memcpy(*truncated_path, path, i + 1);
            (*truncated_path)[i + 1] = '\0';
            return 1;
        }
    }
    return 0;
}


char *get_name(char *path) {
    int i = strlen(path) - 1;
    for (; i >= 0; i--) {
        if (path[i] == '/') {
            i++;
            break;
        }
    }
    char *result = &path[i];
    return result;
}



/* rename - rename a file or directory
 * success - return 0
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
int fs_rename(const char *src_path, const char *dst_path)
{
    char *temp_src = strdup(src_path);
    char *temp_dst = strdup(dst_path);

    char *src_pathv[MAX_PATH_LEN];
    char *dst_pathv[MAX_PATH_LEN];
    int path_source = 0, path_dst = 0;

    int same_dir = exists_in_same_dir(temp_src, temp_dst, src_pathv, &path_source,
                                      dst_pathv, &path_dst);
    if (same_dir < 0) {
        free(temp_src);
        free(temp_dst);
        return -EINVAL;
    }

    char *src_path_copy = strdup(src_path);
    int enc_inum = get_parent_inode(src_path_copy);
    free(src_path_copy);

    if (enc_inum < 0) {
        free(temp_src);
        free(temp_dst);
        return enc_inum;
    }

    struct fs_inode _in;
    if (block_read(&_in, enc_inum, 1) < 0) {
        free(temp_src);
        free(temp_dst);
        return -EIO;
    }

    int blocknum = _in.ptrs[0];
    struct fs_dirent direns[MAX_DIREN_NUM];
    if (block_read(direns, blocknum, 1) < 0) {
        free(temp_src);
        free(temp_dst);
        return -EIO;
    }

    char *src_name = src_pathv[path_source - 1];
    char *dst_name = dst_pathv[path_dst - 1];

    int src_entry_index = check_in_directory(direns, src_name);
    if (src_entry_index < 0) {
        free(temp_src);
        free(temp_dst);
        return -ENOENT;
    }

    if (check_in_directory(direns, dst_name) >= 0) {
        free(temp_src);
        free(temp_dst);
        return -EEXIST;
    }

    strncpy(direns[src_entry_index].name, dst_name, MAX_NAME_LEN - 1);
    direns[src_entry_index].name[MAX_NAME_LEN - 1] = '\0';

    if (block_write(direns, blocknum, 1) < 0) {
        free(temp_src);
        free(temp_dst);
        return -EIO;
    }

    free(temp_src);
    free(temp_dst);
    return 0;
}


int get_parent_inode(char *path) {
    char *pathv[10];
    int pathc = parse(path, pathv);

    if (pathc == 0) {
        return -ENOTSUP;
    }

    int inum = 2;
    int entry_found = 0;

    for (int i = 0; i < pathc - 1; i++) {
        struct fs_inode _in;
        if (block_read(&_in, inum, 1) < 0) {
            return -EIO;
        }

        if (!S_ISDIR(_in.mode)) {
            return -ENOTDIR;
        }

        int blocknum = _in.ptrs[0];
        struct fs_dirent dirent[MAX_DIREN_NUM];
        if (block_read(dirent, blocknum, 1) < 0) {
            return -EIO;
        }

        entry_found = 0;
        for (int j = 0; j < MAX_DIREN_NUM; j++) {
            if (dirent[j].valid && strcmp(dirent[j].name, pathv[i]) == 0) {
                inum = dirent[j].inode;
                entry_found = 1;
                break;
            }
        }

        if (!entry_found) {
            return -ENOENT;
        }
    }

    return inum;
}


int exists_in_same_dir(char *src_path, char *dst_path,char *src_pathv[],
        int *path_source, char *dst_pathv[], int *path_dst) {

            *path_source = parse(src_path, src_pathv);
            *path_dst = parse(dst_path, dst_pathv);
            if (*path_source != *path_dst) {
                    return -1;
            }
            for (int i = 0; i < *path_source - 1; i++) {
                if (strcmp(src_pathv[i], dst_pathv[i]) != 0) return -1;
            }
            return 1;
}



/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * success - return 0
 * Errors - path resolution, ENOENT.
 */
int fs_chmod(const char *path, mode_t mode)
{
    char *temp_path = strdup(path);
    int inum = translate(temp_path);
    free(temp_path);
    if (inum < 0) {
        return inum;
    }
    mode_t new_permission = mode & 0000777;

    struct fs_inode inode;
    block_read(&inode, inum, 1);
    mode_t file_type = inode.mode & S_IFMT;

    inode.mode = file_type | new_permission;
    block_write(&inode, inum, 1);

    return 0;
}


int fs_utime(const char *path, struct utimbuf *ut)
{
    char *temp_path = strdup(path);
    int inum = translate(temp_path);
    free(temp_path);

    if (inum < 0) {
        return inum;
    }

    struct fs_inode inode;
    block_read(&inode, inum, 1);
    inode.mtime = ut->modtime;
    block_write(&inode, inum, 1);

    return 0;
}



/* truncate - truncate file to exactly 'len' bytes
 * success - return 0
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
int fs_truncate(const char *path, off_t len)
{
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
    if (len != 0) {
        return -EINVAL;      /* invalid argument */
    }

    /* your code here */
    char *temp_path = strdup(path);
    int inum = translate(temp_path);
    free(temp_path);

    if (inum == -ENOENT || inum == -ENOTDIR) {
        return inum;
    }

    struct fs_inode inode;
    if (block_read(&inode, inum, 1) < 0) {
        return -EIO;
    }

    if (S_ISDIR(inode.mode)) {
        return -EISDIR;
    }

    int block_allocated = (inode.size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;

    if (block_allocated > 0) {
        char zeros[FS_BLOCK_SIZE];
        memset(zeros, 0, FS_BLOCK_SIZE);

        for (int i = 0; i < block_allocated; i++) {
            int block_num = inode.ptrs[i];
            block_write(zeros, block_num, 1);
            bit_clear(bitmap, block_num);
            inode.ptrs[i] = 0;
        }
    }

    inode.size = 0;

    block_write(&inode, inum, 1);
    block_write(&bitmap, 1, 1);

    return 0;

}



/* read - read data from an open file.
 * success: should return exactly the number of bytes requested, except:
 *   - if offset >= file len, return 0
 *   - if offset+len > file len, return #bytes from offset to end
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
int fs_read(const char *path, char *buf, size_t len, off_t offset, struct fuse_file_info *fi) {

    int byte_read = 0;
    char *temp_path = strdup(path);
    int inum = translate(temp_path);
    free(temp_path);

    if (inum == -ENOENT || inum == -ENOTDIR) {
        return inum;
    }

    struct fs_inode inode;
    if (block_read(&inode, inum, 1) < 0) {
        return -EIO;
    }

    if (!S_ISREG(inode.mode)) {
        return -EISDIR;
    }

    int file_len = inode.size;
    if (offset >= file_len) {
        return 0;
    }

    int end = offset + len;
    if (end > file_len) {
        end = file_len;
    }

    int curr_ptr = offset;
    int buf_ptr = 0;

    for (int i = 0; curr_ptr < end; i++) {
        if ((i + 1) * FS_BLOCK_SIZE <= offset) {
            continue;
        }

        int lba = inode.ptrs[i];
        char tmp[FS_BLOCK_SIZE];

        if (block_read(tmp, lba, 1) < 0) {
            return -EIO;
        }

        int blck_read_start = (curr_ptr > i * FS_BLOCK_SIZE) ? curr_ptr - i * FS_BLOCK_SIZE : 0;

        for (int blck_ptr = blck_read_start; curr_ptr < end && blck_ptr < FS_BLOCK_SIZE; blck_ptr++) {
            buf[buf_ptr++] = tmp[blck_ptr];
            curr_ptr++;
        }
    }

    byte_read = curr_ptr - offset;
    return byte_read;
}



/* write - write data to a file
 * success - return number of bytes written. (this will be the same as
 *           the number requested, or else it's an error)
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them,
 *   but we don't)
 */
int fs_write(const char *path, const char *buf, size_t len, off_t offset,
             struct fuse_file_info *fi) {

    int total_write_length = 0;
    char *temp_path = strdup(path);
    int inum = translate(temp_path);
    free(temp_path);

    if (inum == -ENOENT || inum == -ENOTDIR) {
        return inum;
    }

    struct fs_inode inode;
    if (block_read(&inode, inum, 1) < 0) {
        return -EIO;
    }

    if (!S_ISREG(inode.mode)) {
        return EISDIR;
    }

    int file_len = inode.size;
    if (offset > file_len) {
        return EINVAL;
    }

    const char *curr_buf = buf;
    off_t curr_offset = offset;
    int write_length = len;

    int block_allocated = (file_len + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;

    while (write_length > 0) {
        int block_index = curr_offset / FS_BLOCK_SIZE;
        int block_start = curr_offset % FS_BLOCK_SIZE;
        int block_inum = 0;
        int len_written = 0;

        if (block_index >= block_allocated) {
            block_inum = search_free_inode_map_bit();
            if (block_inum < 0) {
                return -ENOSPC;
            }

            bit_set(bitmap, block_inum);
            block_write(&bitmap, 1, 1);
            inode.ptrs[block_index] = block_inum;
        } else {
            block_inum = inode.ptrs[block_index];
        }

        write_block(block_inum, block_start, curr_buf, write_length, &len_written);

        total_write_length += len_written;
        curr_buf += len_written;
        curr_offset += len_written;
        write_length -= len_written;
    }

    if (file_len < offset + len) {
        inode.size = offset + len;
    }

    block_write(&inode, inum, 1);
    return total_write_length;
}


void write_block(int block_inum, int block_start, const char *curr_buf, int write_length,
                    int *len_written)
{
    char modified_block[FS_BLOCK_SIZE];
    int actual_len;

    if (block_start != 0 || write_length < FS_BLOCK_SIZE) {
        block_read(modified_block, block_inum, 1);
    }

    actual_len = (write_length + block_start > FS_BLOCK_SIZE) ? (FS_BLOCK_SIZE - block_start) : write_length;

    memcpy(modified_block + block_start, curr_buf, actual_len);

    *len_written = actual_len;
    block_write(modified_block, block_inum, 1);
}



/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
int fs_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - (superblock + block map)
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namemax = <whatever your max namelength is>
     *
     * it's OK to calculate this dynamically on the rare occasions
     * when this function is called.
     */
    /* your code here */

    st->f_bsize = FS_BLOCK_SIZE;
    st->f_blocks = superblock.disk_size - 2;
    int free_num = 0;

    for (int i = 0; i < superblock.disk_size; i++) {
        if (!bit_test(bitmap, i)) {
            free_num++;
        }
    }

    st->f_bfree = free_num;
    st->f_bavail = free_num;
    st->f_namemax = MAX_NAME_LEN;

    return 0;
}



/* operations vector. Please don't rename it, or else you'll break things
 */
struct fuse_operations fs_ops = {
    .init = fs_init,            /* read-mostly operations */
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .rename = fs_rename,
    .chmod = fs_chmod,
    .read = fs_read,
    .statfs = fs_statfs,

    .create = fs_create,        /* write operations */
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .utime = fs_utime,
    .truncate = fs_truncate,
    .write = fs_write,
};

