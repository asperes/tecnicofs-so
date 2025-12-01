/*
 * test_state.c - Unit tests for fs/state.c
 * Tests inode table operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minunit.h"
#include "../fs/state.h"

/* Test: inode_table_init initializes all inodes to T_NONE */
static char *test_inode_table_init() {
    inode_table_init();
    
    /* Verify by creating an inode - should get index 0 */
    int inumber = inode_create(T_FILE);
    mu_assert_int_eq(0, inumber);
    
    inode_table_destroy();
    return NULL;
}

/* Test: inode_create returns sequential inumbers */
static char *test_inode_create_sequential() {
    inode_table_init();
    
    int i0 = inode_create(T_FILE);
    int i1 = inode_create(T_FILE);
    int i2 = inode_create(T_DIRECTORY);
    
    mu_assert_int_eq(0, i0);
    mu_assert_int_eq(1, i1);
    mu_assert_int_eq(2, i2);
    
    inode_table_destroy();
    return NULL;
}

/* Test: inode_create with T_DIRECTORY allocates dirEntries */
static char *test_inode_create_directory() {
    inode_table_init();
    
    int inumber = inode_create(T_DIRECTORY);
    mu_assert_int_eq(0, inumber);
    
    type nType;
    union Data data;
    int result = inode_get(inumber, &nType, &data);
    
    mu_assert_int_eq(SUCCESS, result);
    mu_assert_int_eq(T_DIRECTORY, nType);
    mu_assert_not_null(data.dirEntries);
    
    /* All entries should be FREE_INODE initially */
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        mu_assert_int_eq(FREE_INODE, data.dirEntries[i].inumber);
    }
    
    inode_table_destroy();
    return NULL;
}

/* Test: inode_create with T_FILE sets fileContents to NULL */
static char *test_inode_create_file() {
    inode_table_init();
    
    int inumber = inode_create(T_FILE);
    mu_assert_int_eq(0, inumber);
    
    type nType;
    union Data data;
    int result = inode_get(inumber, &nType, &data);
    
    mu_assert_int_eq(SUCCESS, result);
    mu_assert_int_eq(T_FILE, nType);
    mu_assert_null(data.fileContents);
    
    inode_table_destroy();
    return NULL;
}

/* Test: inode_create fails when table is full */
static char *test_inode_create_full_table() {
    inode_table_init();
    
    /* Fill the table */
    for (int i = 0; i < INODE_TABLE_SIZE; i++) {
        int inumber = inode_create(T_FILE);
        mu_assert_int_eq(i, inumber);
    }
    
    /* Next create should fail */
    int inumber = inode_create(T_FILE);
    mu_assert_int_eq(FAIL, inumber);
    
    inode_table_destroy();
    return NULL;
}

/* Test: inode_delete removes an inode */
static char *test_inode_delete() {
    inode_table_init();
    
    int inumber = inode_create(T_FILE);
    mu_assert_int_eq(0, inumber);
    
    int result = inode_delete(inumber);
    mu_assert_int_eq(SUCCESS, result);
    
    /* inode_get should fail now */
    type nType;
    result = inode_get(inumber, &nType, NULL);
    mu_assert_int_eq(FAIL, result);
    
    inode_table_destroy();
    return NULL;
}

/* Test: inode_delete with invalid inumber fails */
static char *test_inode_delete_invalid() {
    inode_table_init();
    
    /* Negative inumber */
    int result = inode_delete(-1);
    mu_assert_int_eq(FAIL, result);
    
    /* Out of bounds inumber */
    result = inode_delete(INODE_TABLE_SIZE);
    mu_assert_int_eq(FAIL, result);
    
    /* Non-existent inode */
    result = inode_delete(5);
    mu_assert_int_eq(FAIL, result);
    
    inode_table_destroy();
    return NULL;
}

/* Test: inode_get with invalid inumber fails */
static char *test_inode_get_invalid() {
    inode_table_init();
    
    type nType;
    union Data data;
    
    /* Negative inumber */
    int result = inode_get(-1, &nType, &data);
    mu_assert_int_eq(FAIL, result);
    
    /* Out of bounds - this tests the >= fix */
    result = inode_get(INODE_TABLE_SIZE, &nType, &data);
    mu_assert_int_eq(FAIL, result);
    
    /* Non-existent inode */
    result = inode_get(5, &nType, &data);
    mu_assert_int_eq(FAIL, result);
    
    inode_table_destroy();
    return NULL;
}

/* Test: dir_add_entry adds entry to directory */
static char *test_dir_add_entry() {
    inode_table_init();
    
    int dir_inumber = inode_create(T_DIRECTORY);
    int file_inumber = inode_create(T_FILE);
    
    int result = dir_add_entry(dir_inumber, file_inumber, "test.txt");
    mu_assert_int_eq(SUCCESS, result);
    
    /* Verify entry was added */
    type nType;
    union Data data;
    inode_get(dir_inumber, &nType, &data);
    
    mu_assert_int_eq(file_inumber, data.dirEntries[0].inumber);
    mu_assert_str_eq("test.txt", data.dirEntries[0].name);
    
    inode_table_destroy();
    return NULL;
}

/* Test: dir_add_entry fails on non-directory */
static char *test_dir_add_entry_not_directory() {
    inode_table_init();
    
    int file1 = inode_create(T_FILE);
    int file2 = inode_create(T_FILE);
    
    int result = dir_add_entry(file1, file2, "test.txt");
    mu_assert_int_eq(FAIL, result);
    
    inode_table_destroy();
    return NULL;
}

/* Test: dir_add_entry fails with empty name */
static char *test_dir_add_entry_empty_name() {
    inode_table_init();
    
    int dir = inode_create(T_DIRECTORY);
    int file = inode_create(T_FILE);
    
    int result = dir_add_entry(dir, file, "");
    mu_assert_int_eq(FAIL, result);
    
    inode_table_destroy();
    return NULL;
}

/* Test: dir_reset_entry removes entry from directory */
static char *test_dir_reset_entry() {
    inode_table_init();
    
    int dir = inode_create(T_DIRECTORY);
    int file = inode_create(T_FILE);
    
    dir_add_entry(dir, file, "test.txt");
    
    int result = dir_reset_entry(dir, file);
    mu_assert_int_eq(SUCCESS, result);
    
    /* Verify entry was removed */
    type nType;
    union Data data;
    inode_get(dir, &nType, &data);
    
    mu_assert_int_eq(FREE_INODE, data.dirEntries[0].inumber);
    
    inode_table_destroy();
    return NULL;
}

/* Test: deleted inode slot can be reused */
static char *test_inode_reuse_after_delete() {
    inode_table_init();
    
    int i0 = inode_create(T_FILE);
    int i1 = inode_create(T_FILE);
    
    mu_assert_int_eq(0, i0);
    mu_assert_int_eq(1, i1);
    
    /* Delete first inode */
    inode_delete(i0);
    
    /* Create new inode - should reuse slot 0 */
    int i2 = inode_create(T_FILE);
    mu_assert_int_eq(0, i2);
    
    inode_table_destroy();
    return NULL;
}

/* Run all tests */
static char *all_tests() {
    printf("\n=== State Module Tests ===\n\n");
    
    mu_run_test(test_inode_table_init);
    mu_run_test(test_inode_create_sequential);
    mu_run_test(test_inode_create_directory);
    mu_run_test(test_inode_create_file);
    mu_run_test(test_inode_create_full_table);
    mu_run_test(test_inode_delete);
    mu_run_test(test_inode_delete_invalid);
    mu_run_test(test_inode_get_invalid);
    mu_run_test(test_dir_add_entry);
    mu_run_test(test_dir_add_entry_not_directory);
    mu_run_test(test_dir_add_entry_empty_name);
    mu_run_test(test_dir_reset_entry);
    mu_run_test(test_inode_reuse_after_delete);
    
    return NULL;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    all_tests();
    mu_print_summary();
    
    return mu_exit_code();
}
