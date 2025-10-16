/*
 * file:        testing.c
 * description: libcheck test skeleton for file system project
 */

#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>
#include <zlib.h>
#include <fuse.h>
#include <stdlib.h>
#include <errno.h>

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
    unsigned cksum;
    int len;
    char *path;
} cksum;

typedef struct {
    char *name;
    int seen;
} dir_entry;


inode_attr inode_attr_table[] = {
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

cksum cksum_table[] = {
    {1726121896, 1000, "/file.1k"},
    {3766980606, 10, "/file.10"},
    {2781093465, 12289, "/dir-with-long-name/file.12k+"},
    {2902524398, 1000, "/dir2/twenty-seven-byte-file-name"},
    {1626046637, 4098, "/dir2/file.4k+"},
    {2991486384, 4095, "/dir3/subdir/file.4k-"},
    {724101859, 8190, "/dir3/subdir/file.8k-"},
    {1483119748, 12288, "/dir3/subdir/file.12k"},
    {1203178000, 12287, "/dir3/file.12k-"},
    {1217760297, 8195, "/file.8k+"}};

dir_entry root_table[] = {{"dir2", 0},    {"dir3", 0},    {"dir-with-long-name", 0},
    {"file.10", 0}, {"file.1k", 0}, {"file.8k+"},{NULL}};

dir_entry dir2_table[] = {{"twenty-seven-byte-file-name", 0}, {"file.4k+", 0}, {NULL}};

dir_entry dir3_table[] = {{"subdir", 0}, {"file.12k-", 0}, {NULL}};

dir_entry dir3sub_table[] = {{"file.4k-", 0}, {"file.8k-", 0}, {"file.12k", 0}, {NULL}};

dir_entry dirlongname_table[] = {{"file.12k+", 0}, {NULL}};


/* this is an example of a callback function for readdir
 */
int empty_filler(void *ptr, const char *name, const struct stat *stbuf,
                 off_t off)
{
    /* FUSE passes you the entry name and a pointer to a 'struct stat'
     * with the inode_attributes. Ignore the 'ptr' and 'off' arguments
     *
     */
    return 0;
}


/**
 * @brief Testing get_inode_attr
 */
START_TEST(getattr_test) {
    int i = 0;
    for (i = 0; inode_attr_table[i].path != NULL; i++) {
        struct stat *sb = malloc(sizeof(*sb));
        fs_ops.getattr(inode_attr_table[i].path, sb);

        ck_assert_int_eq(inode_attr_table[i].uid, sb->st_uid);
        ck_assert_int_eq(inode_attr_table[i].gid, sb->st_gid);
        ck_assert_int_eq(inode_attr_table[i].mode, sb->st_mode);
        ck_assert_int_eq(inode_attr_table[i].size, sb->st_size);
        ck_assert_int_eq(inode_attr_table[i].blck_num, sb->st_blocks);
        ck_assert_int_eq(inode_attr_table[i].ctime, sb->st_ctim.tv_sec);
        ck_assert_int_eq(inode_attr_table[i].mtime, sb->st_mtim.tv_sec);
        free(sb);
    }
}


/**
 * @brief Testing get_inode_attr error test
 */
START_TEST(getattr_error_test) {
    int count = 4;
    char *paths[] = {"/invalidfile", "/file.1k/file.0", "/invaliddir/file.0", "/dir2/invalidfile"};
    int expected_error_msgs[] = {ENOENT, ENOTDIR, ENOENT, ENOENT};
    for (int i = 0; i < count; i++) {
        struct stat *sb = malloc(sizeof(*sb));
        int error = fs_ops.getattr(paths[i], sb);
        ck_assert_int_eq(expected_error_msgs[i], -error);
        free(sb);
    }
}
END_TEST


int test_flag_filler(void *ptr, const char *name, const struct stat *st, off_t off) {
    dir_entry *table = ptr;
    for (int i = 0; table[i].name != NULL; i++) {
        if (strcmp(table[i].name, name) == 0) {
            table[i].seen = 1;
            return 0;
        }
    }
    ck_abort();
    return 1;
}

void readdir_help_func(dir_entry *table, char *path) {
    int i = 0;
    int status = fs_ops.readdir(path, table, test_flag_filler, 0, NULL);
    ck_assert_int_eq(status, 0);
    for (i = 0; table[i].name != NULL; i++) {
        if (!table[i].seen) {
            ck_abort();
        }
        table[i].seen = 0;
    }
}


/**
 *@brief Testing fs_readdir
 */
START_TEST(readdir_test) {
    char *paths[] = {"/", "/dir2", "/dir3", "/dir3/subdir", "/dir-with-long-name"};
    dir_entry *tables[] = {root_table, dir2_table, dir3_table, dir3sub_table,
                            dirlongname_table};
    for (int i = 0; i < 5; i++) {
        readdir_help_func(tables[i], paths[i]);
    }
}
END_TEST



/**
 *@brief Testing fs_readdir error test
 */
START_TEST(readdir_error_test) {
    char *paths[] = {"/dir2/file.4k+", "/dir2/invalid"};
    int expected_error[] = {ENOTDIR, ENOENT};
    for (int i = 0; i < 2; i++) {
        int status = fs_ops.readdir(paths[i], dir2_table, test_flag_filler, 0, NULL);
        ck_assert_int_eq(-status, expected_error[i]);
    }
}
END_TEST



/**
 *@brief Testing fs_read single big read
 */
START_TEST(read_sbr_test) {
    system("python gen-disk.py -q disk1.in test.img");
    int i = 0;
    for (i = 0; cksum_table[i].path != NULL; i++) {
        char *buf = malloc(sizeof(char) * cksum_table[i].len);

        int byte_read = fs_ops.read(cksum_table[i].path, buf, cksum_table[i].len, 0, NULL);
        ck_assert_int_eq(cksum_table[i].len, byte_read);

        unsigned cksum = crc32(0, (unsigned char *)buf, cksum_table[i].len);
        ck_assert_int_eq(cksum_table[i].cksum, cksum);
        free(buf);
    }

}


/**
 *@brief Testing fs_read small read test
 */
START_TEST(read_srt_test) {
    int i = 0;
    int steps[] = {17, 100, 1000, 1024, 1970, 3000};
    for (i = 0; cksum_table[i].path != NULL; i++) {
        for (int j = 0; j < 6; j++) {
            int file_len = cksum_table[i].len;
            char *buf = malloc(sizeof(char) * cksum_table[i].len);
            for (int offset = 0; offset < file_len; offset += steps[j]) {
                fs_ops.read(cksum_table[i].path, buf + offset, steps[j], offset,
                            NULL);
            }
            unsigned cksum = crc32(0, (unsigned char *)buf, cksum_table[i].len);
            ck_assert_int_eq(cksum_table[i].cksum, cksum);
            free(buf);
        }
    }
}
END_TEST



/**
 *@brief Testing fs_statfs
 */
START_TEST(fsstatfs_test) {
    struct statvfs expected;
    expected.f_bsize = 4096;
    expected.f_blocks = 398;
    expected.f_bfree = 355;
    expected.f_bavail = 355;
    expected.f_namemax = 27;

    struct statvfs *actual = malloc(sizeof(*actual));
    fs_ops.statfs("/", actual);

    ck_assert_int_eq(expected.f_bsize, actual->f_bsize);
    ck_assert_int_eq(expected.f_blocks, actual->f_blocks);
    ck_assert_int_eq(expected.f_bfree, actual->f_bfree);
    ck_assert_int_eq(expected.f_bavail, actual->f_bavail);
    ck_assert_int_eq(expected.f_namemax, actual->f_namemax);
    free(actual);
}
END_TEST



/**
 *@brief Testing fs_chmod file test
 */
START_TEST(fschmod_file_test) {
    int i = 1;
    inode_attr file = inode_attr_table[i];
    mode_t new_perm = 0100222;
    fs_ops.chmod(file.path, new_perm);
    struct stat st;
    fs_ops.getattr(file.path, &st);
    ck_assert_int_eq(new_perm, st.st_mode);
    ck_assert_int_eq(S_ISREG(file.mode), S_ISREG(st.st_mode));

    // testing with bad permission
    new_perm = 0040333;
    mode_t expect_perm = 0100333;
    fs_ops.chmod(file.path, new_perm);
    fs_ops.getattr(file.path, &st);
    ck_assert_int_eq(expect_perm, st.st_mode);
    ck_assert_int_eq(S_ISREG(file.mode), S_ISREG(st.st_mode));
}
END_TEST



/**
 *@brief Testing fs_chmod directory test
 */
START_TEST(fschmod_dir_test) {
    int i = 3;
    inode_attr file = inode_attr_table[i];
    mode_t new_perm = 040222;
    fs_ops.chmod(file.path, new_perm);
    struct stat st;
    fs_ops.getattr(file.path, &st);
    ck_assert_int_eq(new_perm, st.st_mode);
    ck_assert_int_eq(S_ISREG(file.mode), S_ISREG(st.st_mode));

    // testing with bad permission
    new_perm = 0100333;
    mode_t expect_perm = 040333;
    fs_ops.chmod(file.path, new_perm);
    fs_ops.getattr(file.path, &st);
    ck_assert_int_eq(expect_perm, st.st_mode);
    ck_assert_int_eq(S_ISREG(file.mode), S_ISREG(st.st_mode));
}
END_TEST



/**
 *@brief Testing fs_rename file test
 */
START_TEST(fsrename_file_test) {
    int cksum_index = 7;
    cksum cksum_entry = cksum_table[cksum_index];
    const char *src_path = "/dir3/subdir/file.12k";
    const char *des_path = "/dir3/subdir/renamed.12k";
    int status = fs_ops.rename(src_path, des_path);
    ck_assert_int_eq(status, 0);

    char *buf = malloc(sizeof(*buf) * cksum_entry.len);
    fs_ops.read(des_path, buf, cksum_entry.len, 0, NULL);
    unsigned act_cksum = crc32(0, (unsigned char *)buf, cksum_entry.len);
    ck_assert_int_eq(cksum_entry.cksum, act_cksum);
}
END_TEST



/**
 *@brief Testing fs_rename file test
 */
START_TEST(fsrename_dir_test) {
    system("python gen-disk.py -q disk1.in test.img");
    int cksum_index = 6;
    cksum cksum_entry = cksum_table[cksum_index];
    const char *src_dir = "/dir3/subdir";
    const char *des_dir = "/dir3/renameddir";

    int status = fs_ops.rename(src_dir, des_dir);
    ck_assert_int_eq(status, 0);

    char *expected_path = "/dir3/renameddir/file.8k-";
    char *buf = malloc(sizeof(*buf) * cksum_entry.len);
    fs_ops.read(expected_path, buf, cksum_entry.len, 0, NULL);
    unsigned act_cksum = crc32(0, (unsigned char *)buf, cksum_entry.len);
    ck_assert_int_eq(cksum_entry.cksum, act_cksum);
}
END_TEST


/**
 *@brief Testing fs_rename error test
 */
START_TEST(fsrename_error_test) {

    system("python gen-disk.py -q disk1.in test.img");
    const char *src_dir = "/dir3/invalid";
    const char *des_dir = "/dir3/renameddir";
    int status;
    int expected[] = {-ENOENT, -EEXIST, -EINVAL};
    status = fs_ops.rename(src_dir, des_dir);
    ck_assert_int_eq(expected[0], status);

    src_dir = "/dir3/subdir/file.4k-";
    des_dir = "/dir3/subdir/file.8k-";
    status = fs_ops.rename(src_dir, des_dir);
    ck_assert_int_eq(expected[1], status);

    src_dir = "/dir3/subdir/file.12k";
    des_dir = "/dir3/renamed.12k";
    status = fs_ops.rename(src_dir, des_dir);
    ck_assert_int_eq(expected[2], status);
}
END_TEST


void test_setup(Suite *s, const char *str, const TTest *f) {
    TCase *tc = tcase_create(str);
    tcase_add_test(tc, f);
    suite_add_tcase(s, tc);
}


int main(int argc, char **argv)
{
    block_init("test.img");
    fs_ops.init(NULL);

    system("python gen-disk.py -q disk1.in test.img");

    Suite *s = suite_create("unittest1");

    SRunner *sr = srunner_create(s);

    test_setup(s, "test1 - get_inode_attr test", getattr_test);
    test_setup(s, "test2 -  get_inode_attr error test", getattr_error_test);
    test_setup(s, "test3 -  readdir test", readdir_test);
    test_setup(s, "test4 - readdir error test", readdir_error_test);
    test_setup(s, "test5 - read single big test", read_sbr_test);
    test_setup(s, "test6 - read small read test", read_srt_test);
    test_setup(s, "test7 - statvfs test", fsstatfs_test);
    test_setup(s, "test8 - chmod file test", fschmod_file_test);
    test_setup(s, "test9 - chmod directory test", fschmod_dir_test);
    test_setup(s, "test10 - rename file test", fsrename_file_test);
    test_setup(s, "test11 - rename directory test", fsrename_dir_test);
    test_setup(s, "test12 - rename error test", fsrename_error_test);

    srunner_set_fork_status(sr, CK_NOFORK);

    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    printf("%d tests failed\n", n_failed);

    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
