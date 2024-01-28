//
// Created by Javad, Vincent.
//

#include "../main/access/access.h"

void test_add_key() 
{
    int curr_amount_keys = get_keys(NULL, NULL);
    // Test adding a valid key
    TEST_ASSERT_EQUAL(ESP_OK, add_key("1234567812345678", 3));
    TEST_ASSERT_EQUAL(++curr_amount_keys, get_keys(NULL, NULL));

    // Test adding a key with an invalid key format
    TEST_ASSERT_EQUAL(ESP_FAIL, add_key("123", 3)); //too short
    TEST_ASSERT_EQUAL(curr_amount_keys, get_keys(NULL, NULL));

    TEST_ASSERT_EQUAL(ESP_FAIL, add_key("123456789", 3)); //too long
    TEST_ASSERT_EQUAL(curr_amount_keys, get_keys(NULL, NULL));

    TEST_ASSERT_EQUAL(ESP_FAIL, add_key("abcdef", 3));
    TEST_ASSERT_EQUAL(curr_amount_keys, get_keys(NULL, NULL));

    // Test adding a key with an invalid access level
    TEST_ASSERT_EQUAL(ESP_FAIL, add_key("1234567812345678", -1)); // too low
    TEST_ASSERT_EQUAL(curr_amount_keys, get_keys(NULL, NULL));

    // Test adding a key that already exists
    TEST_ASSERT_EQUAL(ESP_FAIL, add_key("1234567812345678", 2));
    TEST_ASSERT_EQUAL(curr_amount_keys, get_keys(NULL, NULL));

    delete_key("1234567812345678");
}

void test_delete_key() 
{
    add_key("1234567812345678", 3);

    // Test deleting non-existing key
    TEST_ASSERT_EQUAL(ESP_FAIL, delete_key("8018010612345678"));

    // Test deleting key with invalid name
    TEST_ASSERT_EQUAL(ESP_FAIL, delete_key("123"));

    // Test deleting existing key
    TEST_ASSERT_EQUAL(ESP_OK, delete_key("1234567812345678"));
}

void test_change_key_access_level(void)
{
    // Initialize keys and devices
    add_key("1111111111111111", 0);
    add_key("2222222222222222", 1);
    add_key("3333333333333333", 2);
    add_key("4444444444444444", 3);

    add_device(1, 2);

    // Change access level of an existing key
    TEST_ASSERT_EQUAL(ESP_OK, change_key_access_level("2222222222222222", 3));

    // Attempt to change access level of a non-existing key
    TEST_ASSERT_EQUAL(ESP_FAIL, change_key_access_level("8018010880180108", 1));

    // Attempt to change access level of a key with an invalid access level
    TEST_ASSERT_EQUAL(ESP_FAIL, change_key_access_level("3333333333333333", -1));

    // Attempt to change access level of an invalid key
    TEST_ASSERT_EQUAL(ESP_FAIL, change_key_access_level("123", 2));

    // Attempt to change access level of a key and test if it has an access to a device
    TEST_ASSERT_EQUAL(ESP_OK, has_access(1, "4444444444444444"));

    TEST_ASSERT_EQUAL(ESP_OK, change_key_access_level("4444444444444444", 1));

    TEST_ASSERT_EQUAL(ESP_FAIL, has_access(1, "4444444444444444"));

    delete_key("1111111111111111");
    delete_key("2222222222222222");
    delete_key("3333333333333333");
    delete_key("4444444444444444");
    delete_device(1);
}

void test_change_device_access_level(void)
{
    // Initialize devices and keys
    add_device(1, 0);
    add_device(2, 1);
    add_device(3, 2);
    add_device(4, 3);

    add_key("1234567812345678", 1);

    // Change access level of an existing device
    TEST_ASSERT_EQUAL(ESP_OK, change_device_access_level(2, 3));

    // Attempt to change access level of a non-existing device
    TEST_ASSERT_EQUAL(ESP_FAIL, change_device_access_level(5, 1));

    // Attempt to change access level of a device with an invalid access level
    TEST_ASSERT_EQUAL(ESP_FAIL, change_device_access_level(3, -1));

    // Attempt to change access level of a device with an invalid device ID
    TEST_ASSERT_EQUAL(ESP_FAIL, change_device_access_level(0, 2));

    // Attempt to change access level of a device and test if the key has an access
    TEST_ASSERT_EQUAL(ESP_OK, has_access(1, "1234567812345678"));

    TEST_ASSERT_EQUAL(ESP_OK, change_device_access_level(1, 5));

    TEST_ASSERT_EQUAL(ESP_FAIL, has_access(1, "1234567812345678"));

    delete_device(1);
    delete_device(2);
    delete_device(3);
    delete_device(4);
    delete_key("1234567812345678");
}

void test_has_access(void)
{
    add_device(1, 2);
    add_device(2, 3);

    add_key("1234567812345678", 2);

    //Valid key and device
    TEST_ASSERT_EQUAL(ESP_OK, has_access(1, "1234567812345678"));

    //Lower access level
    TEST_ASSERT_EQUAL(ESP_FAIL, has_access(2, "1234567812345678"));

    //Invalid device
    TEST_ASSERT_EQUAL(ESP_FAIL, has_access(0, "1234567812345678"));

    //Invalid key
    TEST_ASSERT_EQUAL(ESP_FAIL, has_access(1, "2345678912345678"));

    delete_device(1);
    delete_device(2);
    delete_key("1234567812345678");
}

void test_delete_all_keys(void)
{
    add_key("1111111111111111", 0);
    add_key("2222222222222222", 1);
    add_key("3333333333333333", 2);
    add_key("4444444444444444", 3);

    delete_all_keys();

    TEST_ASSERT_EQUAL(0, get_keys(NULL, NULL));
}

void test_delete_all_devices(void)
{
    add_device(1,0);
    add_device(2,1);
    add_device(3,2);
    add_device(4,3);

    delete_all_devices();

    TEST_ASSERT_EQUAL(0, get_devices(NULL, NULL));
}

