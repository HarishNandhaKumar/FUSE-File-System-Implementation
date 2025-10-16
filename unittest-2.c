/*
 * file:        unittest-2.c
 * description: libcheck test skeleton, part 2
 */

#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26
#define FS_BLOCK_SIZE 4096

#include <check.h>
#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

extern struct fuse_operations fs_ops;
extern void block_init(char *file);

typedef struct {
    char *path;
    uint16_t uid;
    uint16_t gid;
    uint32_t mode;
    int32_t size;
    uint32_t ctime;
    uint32_t mtime;
    uint16_t blck_num;
} inode_attr;


typedef struct {
    char *childpath;
    int found;
} test_dir;


inode_attr inode_attrable[] = {
    {"/", 0, 0, 040777, 4096, 1565283152, 1565283167, 1},
    {"/file.1k", 500, 500, 0100666, 1000, 1565283152, 1565283152, 1},
    {"/file.10", 500, 500, 0100666, 10, 1565283152, 1565283167, 1},
    {"/dir-with-long-name", 0, 0, 040777, 4096, 1565283152, 1565283167, 1},
    {"/dir-with-long-name/file.12k+", 0, 500, 0100666, 12289, 1565283152,
     1565283167, 4},
    {"/dir2", 500, 500, 040777, 8192, 1565283152, 1565283167, 2},
    {"/dir2/twenty-seven-byte-file-name", 500, 500, 0100666, 1000, 1565283152,
     1565283167, 1},
    {"/dir2/file.4k+", 500, 500, 0100777, 4098, 1565283152, 1565283167, 2},
    {"/dir3", 0, 500, 040777, 4096, 1565283152, 1565283167, 1},
    {"/dir3/subdir", 0, 500, 040777, 4096, 1565283152, 1565283167, 1},
    {"/dir3/subdir/file.4k-", 500, 500, 0100666, 4095, 1565283152, 1565283167,
     1},
    {"/dir3/subdir/file.8k-", 500, 500, 0100666, 8190, 1565283152, 1565283167,
     2},
    {"/dir3/subdir/file.12k", 500, 500, 0100666, 12288, 1565283152, 1565283167,
     3},
    {"/dir3/file.12k-", 0, 500, 0100777, 12287, 1565283152, 1565283167, 3},
    {"/file.8k+", 500, 500, 0100666, 8195, 1565283152, 1565283167, 3},
    {NULL}};

test_dir mkdir_table[] = {{"mkdir1", 0}, {"mkdir2", 0}, {"mkdir3", 0}, {"mkdir4", 0}, {NULL}};

test_dir fscreate_table[] = {{"create1", 0}, {"create2", 0}, {"create3", 0}, {"create4", 0}, {NULL}};


/* mockup for fuse_get_context. you can change ctx.uid, ctx.gid in
 * tests if you want to test setting UIDs in mknod/mkdir
 */
struct fuse_context ctx = {.uid = 500, .gid = 500};
struct fuse_context *fuse_get_context(void) {
    return &ctx;
}



/* this is an example of a callback function for readdir
 */
int empty_filler(void *ptr, const char *name, const struct stat *stbuf,
                 off_t off) {
    /* FUSE passes you the entry name and a pointer to a 'struct stat'
     * with the inode_attributes. Ignore the 'ptr' and 'off' arguments
     *
     */
    return 0;
}

int test_filler(void *ptr, const char *name, const struct stat *st, off_t off) {
    test_dir *test = ptr;
    if (strcmp(test->childpath, name) == 0) {
        test->found = 1;
    }
    return 1;
}


/**
* @brief testing fs_mkdir single test
*/
START_TEST(fs_mkdir_single_test) {
    const char *parentdir = "/dir2";
    char *newdir = "/dir2/newdir";
    mode_t mode = 0777;
    int mkdir_status = fs_ops.mkdir(newdir, mode);
    ck_assert_int_eq(mkdir_status, 0);
    test_dir testdir = {"newdir", 0};
    int read_status = fs_ops.readdir(parentdir, &testdir, test_filler, 0, NULL);
    ck_assert_int_eq(read_status, 0);
    if (testdir.found == 0) {
        ck_abort();
    }
}
END_TEST




/**
* @brief testing fs_mkdir test
*/
START_TEST(fs_mkdir_test) {
    const char *parentdir = "/dir3/";
    mode_t mode = 0777;
    for (int i = 0; mkdir_table[i].childpath != NULL; i++) {
        char combined_path[100];
        sprintf(combined_path, "%s%s", parentdir, mkdir_table[i].childpath);
        int mkdir_status = fs_ops.mkdir(combined_path, mode);
        ck_assert_int_eq(mkdir_status, 0);
    }

    for (int j = 0; mkdir_table[j].childpath != NULL; j++) {
        test_dir testdir = {mkdir_table[j].childpath, 0};
        int read_status = fs_ops.readdir(parentdir, &testdir, test_filler, 0, NULL);
        ck_assert_int_eq(read_status, 0);
        if (testdir.found == 0) {
            ck_abort();
        }
    }
}
END_TEST


/**
* @brief testing fs_rmdir test
*/
START_TEST(fs_rmdir_test) {

    // Here, Empty directory is created.
    const char *parentdir = "/dir2";
    char *newdir = "/dir2/newdir";
    char *nameonly = "newdir";
    mode_t mode = 0777;
    int mkdir_status = fs_ops.mkdir(newdir, mode);
    ck_assert_int_eq(mkdir_status, 0);

    test_dir mkdir_read = {nameonly, 0};
    int read_status = fs_ops.readdir(parentdir, &mkdir_read, test_filler, 0, NULL);
    ck_assert_int_eq(read_status, 0);
    if (mkdir_read.found == 0) {
        ck_abort();
    }

    // check if empty directory is removed.
    test_dir rmdir_read = {newdir, 0};
    read_status = fs_ops.readdir(parentdir, &rmdir_read, test_filler, 0, NULL);
    ck_assert_int_eq(read_status, 0);
    if (rmdir_read.found == 1) {
        ck_abort();
    }
}
END_TEST


void fscreate_test() {
    const char *parentdir = "/dir3/";
    mode_t mode = 0777;
    for (int i = 0; fscreate_table[i].childpath != NULL; i++) {
        char combined_path[100];
        sprintf(combined_path, "%s%s", parentdir, fscreate_table[i].childpath);
        int mkdir_status = fs_ops.create(combined_path, mode, NULL);
        ck_assert_int_eq(mkdir_status, 0);
    }

    for (int j = 0; fscreate_table[j].childpath != NULL; j++) {
        int read_status = fs_ops.readdir(parentdir, &fscreate_table[j], test_filler, 0, NULL);
        ck_assert_int_eq(read_status, 0);
        if (fscreate_table[j].found == 0) {
            ck_abort();
        }
        fscreate_table[j].found = 0;
    }
}


/**
* @brief testing fs_create test
*/
START_TEST(fs_create_test) {
    fscreate_test();
}
END_TEST



/**
* @brief testing fs_unlink test
*/
START_TEST(fs_unlink_test) {

    fscreate_test();
    const char *parentdir = "/dir3/";
    for (int i = 0; fscreate_table[i].childpath != NULL; i++) {
        char combined_path[100];
        sprintf(combined_path, "%s%s", parentdir, fscreate_table[i].childpath);
        int unlink_status = fs_ops.unlink(combined_path);
        ck_assert_int_eq(unlink_status, 0);
    }

    for (int j = 0; fscreate_table[j].childpath != NULL; j++) {
        int read_status = fs_ops.readdir(parentdir, &fscreate_table[j], test_filler, 0, NULL);
        ck_assert_int_eq(read_status, 0);
        if (fscreate_table[j].found != 0) {
            ck_abort();
        }
    }
}
END_TEST



/**
* @brief testing make no directory
*/
START_TEST(nodir_create_error_test) {

    // path style used: /x/y/z

    // y doesn't exist
    mode_t mode = 0777;
    int expected = -ENOENT;
    int actual = fs_ops.create("/dir3/invalid/file.4k-", mode, NULL);
    ck_assert_int_eq(expected, actual);

    // y isn't directory
    expected = -ENOTDIR;
    actual = fs_ops.create("/dir3/file.12k-/file.4k-", mode, NULL);
    ck_assert_int_eq(expected, actual);

    // z exists
    expected = -EEXIST;
    actual = fs_ops.create("/dir3/subdir/file.4k-", mode, NULL);
    ck_assert_int_eq(expected, actual);

    // z exists
    expected = -EEXIST;
    actual = fs_ops.create("/dir3/subdir", mode, NULL);
    ck_assert_int_eq(expected, actual);

    expected = 0;
    actual = fs_ops.create("/dir3/fdfdfgtbgchftdgsdwsdxgtftfhggxdfdxvfchtfhgvncbgfhfjgvnghfgbcnbcvbcb.txt", mode, NULL);
    ck_assert_int_eq(expected, actual);
}
END_TEST



/**
* @brief testing fs_mkdir error test
*/
START_TEST(fsmkdir_error_test) {

    // path style used: /x/y/z

    // y doesn't exist
    mode_t mode = 0777;
    int expected = -ENOENT;
    int actual = fs_ops.mkdir("/dir3/invalid/newdir", mode);
    ck_assert_int_eq(expected, actual);

    // y isn't directory
    expected = -ENOTDIR;
    actual = fs_ops.mkdir("/dir3/file.12k-/newdir", mode);
    ck_assert_int_eq(expected, actual);

    // z exists
    expected = -EEXIST;
    actual = fs_ops.mkdir("/dir3/subdir/file.4k-", mode);

    // z exists
    expected = -EEXIST;
    actual = fs_ops.mkdir("/dir3/subdir", mode);
    ck_assert_int_eq(expected, actual);

    expected = 0;
    actual = fs_ops.mkdir("/dir3/abcdefesfsfsfgfdfdsffrghcgbfgvdfadscvnghgfssvfbfbgfhgfbgcbgfngngfbgg",mode);
    ck_assert_int_eq(expected, actual);
}
END_TEST



/**
* @brief testing fs_unlink error test
*/
START_TEST(fs_unlink_error_test) {

    // path style used: /x/y/z

    // y doesn't exist
    int expected = -ENOENT;
    int actual = fs_ops.unlink("/dir3/invalid/file.4k-");
    ck_assert_int_eq(expected, actual);

    // y isn't directory
    expected = -ENOTDIR;
    actual = fs_ops.unlink("/dir3/file.12k-/file.4k-");
    ck_assert_int_eq(expected, actual);

    // z not exists
    expected = -ENOENT;
    actual = fs_ops.unlink("/dir3/subdir/invalid.txt");
    ck_assert_int_eq(expected, actual);

    // z exists
    expected = -EISDIR;
    actual = fs_ops.unlink("/dir3/subdir");
    ck_assert_int_eq(expected, actual);
}
END_TEST



/**
* @brief testing fs_rmdir error test
*/
START_TEST(fs_rmdir_error_test) {

    // path style used: /x/y/z

    // y doesn't exist
    int expected = -ENOENT;
    int actual = fs_ops.rmdir("/invalid/subdir");
    ck_assert_int_eq(expected, actual);

    // y isn't directory
    expected = -ENOTDIR;
    actual = fs_ops.rmdir("/dir3/file.12k-/invalid");
    ck_assert_int_eq(expected, actual);

    // z doesn't exist
    expected = -ENOENT;
    actual = fs_ops.rmdir("/dir3/subdir/invalid");
    ck_assert_int_eq(expected, actual);

    // z is file
    expected = -ENOTDIR;
    char *enotdirpath = "/dir3/file.12k-";
    actual = fs_ops.rmdir(enotdirpath);
    ck_assert_int_eq(expected, actual);

    // directory not empty
    expected = -ENOTEMPTY;
    char *ENOTEMPTY_path = "/dir3/subdir";
    actual = fs_ops.rmdir(ENOTEMPTY_path);
    ck_assert_int_eq(expected, actual);
}
END_TEST


void new_buf(char *buf, int len) {
    char *ptr = buf;
    int i;
    for (i = 0, ptr = buf; ptr < buf + len; i++) {
        ptr += sprintf(ptr, "%d ", i);
    }
}

void verify_write(char *path, int len, int offset, unsigned expect_cksum) {
    char *read_buf = malloc(sizeof(char) * len);
    int byte_read = fs_ops.read(path, read_buf, len, offset, NULL);
    ck_assert_int_eq(len, byte_read);

    unsigned read_cksum = crc32(0, (unsigned char *)read_buf, len);
    ck_assert_int_eq(expect_cksum, read_cksum);
    free(read_buf);
}



/**
* @brief testing writing small file
*/
START_TEST(write_smallfile_test) {
    char path[] = "/file.10";
    int len = 4000;
    char *write_buf = malloc(len);
    new_buf(write_buf, len);
    unsigned write_cksum = crc32(0, (unsigned char *)write_buf, len);
    int byte_written = fs_ops.write(path, write_buf, len, 0, NULL);
    ck_assert_int_eq(byte_written, len);
    verify_write(path, len, 0, write_cksum);
    free(write_buf);
}
END_TEST



/**
* @brief testing fs_write
*/
START_TEST(fswrite_test) {
    char *path[] = {"/file.1k", NULL};
    int lens[] = {4000};
    for (int i = 0; path[i] != NULL; i++) {
        int len = lens[i];
        char *write_buf = malloc(len);
        new_buf(write_buf, len);
        unsigned write_cksum = crc32(0, (unsigned char *)write_buf, len);
        int byte_written = fs_ops.write(path[i], write_buf, len, 0, NULL);
        ck_assert_int_eq(byte_written, len);
        verify_write(path[i], len, 0, write_cksum);
        free(write_buf);
    }
}
END_TEST



/**
* @brief testing fs_write appending test
*/
START_TEST(fswrite_append_test) {

    int init_lens[] = {FS_BLOCK_SIZE, FS_BLOCK_SIZE * 2};
    int after_append_lens[] = {FS_BLOCK_SIZE * 2 - 1, FS_BLOCK_SIZE * 4};
    int steps[] = {17, 100, 1000, 1024, 1970, 3000};
    char *paths[] = {"/block1", "/block2", NULL};

    for (int i = 0; paths[i] != NULL; i++) {
        for (int j = 0; j < 6; j++) {
            char m_path[50];
            sprintf(m_path, "%s.%d", paths[i], steps[j]);
            int file_len = after_append_lens[i];
            mode_t mode = 0000777 | S_IFREG;
            int create_status = fs_ops.create(m_path, mode, NULL);
            ck_assert_int_eq(0, create_status);

            char *buf = malloc(file_len);
            new_buf(buf, init_lens[i]);

            fs_ops.write(m_path, buf, init_lens[i], 0, NULL);
            for (int offset = init_lens[i]; offset < file_len; offset += steps[j]) {
                    int len_to_write = steps[j];
                    if (steps[j] + offset > file_len) {
                            len_to_write = file_len - offset;
                    }
                    new_buf(buf + offset, len_to_write);
                    fs_ops.write(m_path, buf + offset, len_to_write, offset, NULL);
            }

            unsigned expect_cksum = crc32(0, (unsigned char *)buf, file_len);
            verify_write(m_path, file_len, 0, expect_cksum);
            free(buf);
        }
    }
}
END_TEST



/**
* @brief testing fs_truncate
*/
START_TEST(fs_truncate_test) {
    char *paths[] = {"/dir3/subdir/file.4k-","/dir3/subdir/file.8k-","/dir3/subdir/file.12k",};
    int expected_freed[] = {1, 2, 3};
    struct statvfs st;
    fs_ops.statfs("nothing", &st);
    int num_free = st.f_bfree;
    for (int i = 0; i < 3; i++) {
        int offset = 0;
        fs_ops.truncate(paths[i], offset);
        fs_ops.statfs("nothing", &st);
        int new_num_free = st.f_bfree;
        int bck_freed = new_num_free - num_free;
        ck_assert_int_eq(expected_freed[i], bck_freed);
        num_free = new_num_free;
    }
}
END_TEST


void reset_testdata() {
    for (int i = 0; mkdir_table[i].childpath != NULL; i++) {
        mkdir_table[i].found = 0;
    }
    for (int i = 0; fscreate_table[i].childpath != NULL; i++) {
        fscreate_table[i].found = 0;
    }
}

void initial_reset_disk() {
    system("python gen-disk.py -q disk1.in test.img");
    reset_testdata();
}

void end_reset_disk() {
    system("python gen-disk.py -q disk1.in test.img");
    reset_testdata();
}

void test_setup(Suite *s, const char *str, const TTest *f) {
    TCase *tc = tcase_create(str);
    tcase_add_test(tc, f);
    tcase_add_unchecked_fixture(tc, initial_reset_disk, end_reset_disk);
    suite_add_tcase(s, tc);
}



int main(int argc, char **argv) {
    block_init("test.img");
    fs_ops.init(NULL);

    Suite *s = suite_create("unittest2");

    SRunner *sr = srunner_create(s);

    test_setup(s, "test1 - fs_mkdir test", fs_mkdir_test);
    test_setup(s, "test2 - rmdir single test", fs_rmdir_test);
    test_setup(s, "test3 - create test", fs_create_test);
    test_setup(s, "test4 - fs_unlink test", fs_unlink_test);
    test_setup(s, "test5 - fs_mkdir single test", fs_mkdir_single_test);
    test_setup(s, "test6 - fs_rmdir error test", fs_rmdir_error_test);
    test_setup(s, "test7 - fs_unlink error test", fs_unlink_error_test);
    test_setup(s, "test8 - fsmkdir error test", fsmkdir_error_test);
    test_setup(s, "test9 - nodir create error test", nodir_create_error_test);
    test_setup(s, "test10 - fswrite append test", fswrite_append_test);
    test_setup(s, "test11 - fswrite test", fswrite_test);
    test_setup(s, "test12 - write smallfile test", write_smallfile_test);
    test_setup(s, "test13 - fs_truncate test", fs_truncate_test);
    srunner_set_fork_status(sr, CK_NOFORK);

    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    printf("%d tests failed\n", n_failed);

    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
