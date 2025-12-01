/*
 * test_operations.c - Unit tests for fs/operations.c
 * Tests high-level filesystem operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minunit.h"
#include "../fs/operations.h"
#include "../fs/state.h"

/* Helper to setup filesystem for each test */
static void setup() {
    init_fs();
}

/* Helper to teardown filesystem after each test */
static void teardown() {
    destroy_fs();
}

/* Test: init_fs creates root directory */
static char *test_init_fs() {
    setup();
    
    /* Root should exist and be a directory */
    int result = lookup("/");
    mu_assert_int_eq(FS_ROOT, result);
    
    type nType;
    inode_get(FS_ROOT, &nType, NULL);
    mu_assert_int_eq(T_DIRECTORY, nType);
    
    teardown();
    return NULL;
}

/* Test: create file in root */
static char *test_create_file_in_root() {
    setup();
    
    int result = create("/file1", T_FILE);
    mu_assert_int_eq(SUCCESS, result);
    
    /* Verify file exists */
    result = lookup("/file1");
    mu_assert("File should exist", result >= 0);
    
    type nType;
    inode_get(result, &nType, NULL);
    mu_assert_int_eq(T_FILE, nType);
    
    teardown();
    return NULL;
}

/* Test: create directory in root */
static char *test_create_directory_in_root() {
    setup();
    
    int result = create("/dir1", T_DIRECTORY);
    mu_assert_int_eq(SUCCESS, result);
    
    /* Verify directory exists */
    result = lookup("/dir1");
    mu_assert("Directory should exist", result >= 0);
    
    type nType;
    inode_get(result, &nType, NULL);
    mu_assert_int_eq(T_DIRECTORY, nType);
    
    teardown();
    return NULL;
}

/* Test: create nested file */
static char *test_create_nested_file() {
    setup();
    
    create("/dir1", T_DIRECTORY);
    int result = create("/dir1/file1", T_FILE);
    mu_assert_int_eq(SUCCESS, result);
    
    /* Verify nested file exists */
    result = lookup("/dir1/file1");
    mu_assert("Nested file should exist", result >= 0);
    
    teardown();
    return NULL;
}

/* Test: create fails with non-existent parent */
static char *test_create_invalid_parent() {
    setup();
    
    int result = create("/nonexistent/file1", T_FILE);
    mu_assert_int_eq(FAIL, result);
    
    teardown();
    return NULL;
}

/* Test: create fails when file already exists */
static char *test_create_duplicate() {
    setup();
    
    create("/file1", T_FILE);
    int result = create("/file1", T_FILE);
    mu_assert_int_eq(FAIL, result);
    
    teardown();
    return NULL;
}

/* Test: create fails when parent is not directory */
static char *test_create_parent_not_directory() {
    setup();
    
    create("/file1", T_FILE);
    int result = create("/file1/subfile", T_FILE);
    mu_assert_int_eq(FAIL, result);
    
    teardown();
    return NULL;
}

/* Test: delete file */
static char *test_delete_file() {
    setup();
    
    create("/file1", T_FILE);
    int result = delete("/file1");
    mu_assert_int_eq(SUCCESS, result);
    
    /* Verify file no longer exists */
    result = lookup("/file1");
    mu_assert_int_eq(FAIL, result);
    
    teardown();
    return NULL;
}

/* Test: delete empty directory */
static char *test_delete_empty_directory() {
    setup();
    
    create("/dir1", T_DIRECTORY);
    int result = delete("/dir1");
    mu_assert_int_eq(SUCCESS, result);
    
    /* Verify directory no longer exists */
    result = lookup("/dir1");
    mu_assert_int_eq(FAIL, result);
    
    teardown();
    return NULL;
}

/* Test: delete non-empty directory fails */
static char *test_delete_nonempty_directory() {
    setup();
    
    create("/dir1", T_DIRECTORY);
    create("/dir1/file1", T_FILE);
    
    int result = delete("/dir1");
    mu_assert_int_eq(FAIL, result);
    
    /* Directory should still exist */
    result = lookup("/dir1");
    mu_assert("Directory should still exist", result >= 0);
    
    teardown();
    return NULL;
}

/* Test: delete non-existent file fails */
static char *test_delete_nonexistent() {
    setup();
    
    int result = delete("/nonexistent");
    mu_assert_int_eq(FAIL, result);
    
    teardown();
    return NULL;
}

/* Test: lookup root */
static char *test_lookup_root() {
    setup();
    
    int result = lookup("/");
    mu_assert_int_eq(FS_ROOT, result);
    
    teardown();
    return NULL;
}

/* Test: lookup empty path returns root */
static char *test_lookup_empty_path() {
    setup();
    
    int result = lookup("");
    mu_assert_int_eq(FS_ROOT, result);
    
    teardown();
    return NULL;
}

/* Test: lookup non-existent path */
static char *test_lookup_nonexistent() {
    setup();
    
    int result = lookup("/nonexistent");
    mu_assert_int_eq(FAIL, result);
    
    teardown();
    return NULL;
}

/* Test: lookup nested path */
static char *test_lookup_nested() {
    setup();
    
    create("/dir1", T_DIRECTORY);
    create("/dir1/dir2", T_DIRECTORY);
    create("/dir1/dir2/file1", T_FILE);
    
    int result = lookup("/dir1/dir2/file1");
    mu_assert("Nested path should be found", result >= 0);
    
    teardown();
    return NULL;
}

/* Test: move file to different directory */
static char *test_move_file() {
    setup();
    
    create("/dir1", T_DIRECTORY);
    create("/dir2", T_DIRECTORY);
    create("/dir1/file1", T_FILE);
    
    int result = move("/dir1/file1", "/dir2/file1");
    mu_assert_int_eq(SUCCESS, result);
    
    /* Verify file moved */
    result = lookup("/dir1/file1");
    mu_assert_int_eq(FAIL, result);
    
    result = lookup("/dir2/file1");
    mu_assert("File should exist in new location", result >= 0);
    
    teardown();
    return NULL;
}

/* Test: move file with rename */
static char *test_move_file_rename() {
    setup();
    
    create("/dir1", T_DIRECTORY);
    create("/dir1/oldname", T_FILE);
    
    int result = move("/dir1/oldname", "/dir1/newname");
    mu_assert_int_eq(SUCCESS, result);
    
    /* Verify rename */
    result = lookup("/dir1/oldname");
    mu_assert_int_eq(FAIL, result);
    
    result = lookup("/dir1/newname");
    mu_assert("File should have new name", result >= 0);
    
    teardown();
    return NULL;
}

/* Test: move directory */
static char *test_move_directory() {
    setup();
    
    create("/dir1", T_DIRECTORY);
    create("/dir2", T_DIRECTORY);
    create("/dir1/subdir", T_DIRECTORY);
    create("/dir1/subdir/file1", T_FILE);
    
    int result = move("/dir1/subdir", "/dir2/subdir");
    mu_assert_int_eq(SUCCESS, result);
    
    /* Verify directory moved with contents */
    result = lookup("/dir1/subdir");
    mu_assert_int_eq(FAIL, result);
    
    result = lookup("/dir2/subdir");
    mu_assert("Directory should exist in new location", result >= 0);
    
    result = lookup("/dir2/subdir/file1");
    mu_assert("Contents should be preserved", result >= 0);
    
    teardown();
    return NULL;
}

/* Test: move fails when source doesn't exist */
static char *test_move_nonexistent_source() {
    setup();
    
    create("/dir1", T_DIRECTORY);
    
    int result = move("/nonexistent", "/dir1/file");
    mu_assert_int_eq(FAIL, result);
    
    teardown();
    return NULL;
}

/* Test: move fails when destination already exists */
static char *test_move_destination_exists() {
    setup();
    
    create("/file1", T_FILE);
    create("/file2", T_FILE);
    
    int result = move("/file1", "/file2");
    mu_assert_int_eq(FAIL, result);
    
    /* Both files should still exist */
    mu_assert("Source should still exist", lookup("/file1") >= 0);
    mu_assert("Destination should still exist", lookup("/file2") >= 0);
    
    teardown();
    return NULL;
}

/* Test: move fails when destination parent doesn't exist */
static char *test_move_invalid_destination_parent() {
    setup();
    
    create("/file1", T_FILE);
    
    int result = move("/file1", "/nonexistent/file1");
    mu_assert_int_eq(FAIL, result);
    
    /* Source should still exist */
    mu_assert("Source should still exist", lookup("/file1") >= 0);
    
    teardown();
    return NULL;
}

/* Test: is_dir_empty returns SUCCESS for empty directory */
static char *test_is_dir_empty_true() {
    setup();
    
    create("/dir1", T_DIRECTORY);
    
    int inumber = lookup("/dir1");
    type nType;
    union Data data;
    inode_get(inumber, &nType, &data);
    
    int result = is_dir_empty(data.dirEntries);
    mu_assert_int_eq(SUCCESS, result);
    
    teardown();
    return NULL;
}

/* Test: is_dir_empty returns FAIL for non-empty directory */
static char *test_is_dir_empty_false() {
    setup();
    
    create("/dir1", T_DIRECTORY);
    create("/dir1/file1", T_FILE);
    
    int inumber = lookup("/dir1");
    type nType;
    union Data data;
    inode_get(inumber, &nType, &data);
    
    int result = is_dir_empty(data.dirEntries);
    mu_assert_int_eq(FAIL, result);
    
    teardown();
    return NULL;
}

/* Run all tests */
static char *all_tests() {
    printf("\n=== Operations Module Tests ===\n\n");
    
    /* init_fs tests */
    mu_run_test(test_init_fs);
    
    /* create tests */
    mu_run_test(test_create_file_in_root);
    mu_run_test(test_create_directory_in_root);
    mu_run_test(test_create_nested_file);
    mu_run_test(test_create_invalid_parent);
    mu_run_test(test_create_duplicate);
    mu_run_test(test_create_parent_not_directory);
    
    /* delete tests */
    mu_run_test(test_delete_file);
    mu_run_test(test_delete_empty_directory);
    mu_run_test(test_delete_nonempty_directory);
    mu_run_test(test_delete_nonexistent);
    
    /* lookup tests */
    mu_run_test(test_lookup_root);
    mu_run_test(test_lookup_empty_path);
    mu_run_test(test_lookup_nonexistent);
    mu_run_test(test_lookup_nested);
    
    /* move tests */
    mu_run_test(test_move_file);
    mu_run_test(test_move_file_rename);
    mu_run_test(test_move_directory);
    mu_run_test(test_move_nonexistent_source);
    mu_run_test(test_move_destination_exists);
    mu_run_test(test_move_invalid_destination_parent);
    
    /* helper function tests */
    mu_run_test(test_is_dir_empty_true);
    mu_run_test(test_is_dir_empty_false);
    
    return NULL;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    all_tests();
    mu_print_summary();
    
    return mu_exit_code();
}
