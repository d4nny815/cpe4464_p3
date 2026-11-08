#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define HASH_DEFAULT_SIZE 1024

typedef struct {
    bool is_valid;
    bool is_deleted;
    char* key;
    void* value;
} hashEntry_t;

typedef struct {
    hashEntry_t* entries;
    size_t num_entries;
    size_t size;
} hashTable_t;

// Create a new hashtable with the given size
// Returns a pointer to the new hashtable
hashTable_t* hashTable_create();

// Destroy the hashtable and free the memory
// table - pointer to the hashtable
void hashTable_destroy(hashTable_t* table);

// Insert a new element into the hashtable
// If the key already exists, the value is overwritten
// Hashtable takes ownership of the key and value element
// table - pointer to the hashtable
// key - string key to insert
// value - pointer to the value to insert
void hashTable_insert(hashTable_t* table, char* key, void* value);

// // Update an element in the hashtable
// // If the key does not exist, a new element is created
// // table - pointer to the hashtable
// // key - string key to update
// // value - pointer to the value to update
// // Returns a pointer to the old value if the key already exists, NULL otherwise
// void* hashTable_update(hashTable_t* table, char* key, void* value);

// Get the value for a given key
// table - pointer to the hashtable
// key - string key to lookup
// Returns a pointer to the value if the key exists, NULL otherwise
void* hashTable_get_value(hashTable_t* table, char* key);

// Get the key for a given value
// table - pointer to the hashtable
// value - pointer to the value to lookup
// Returns a pointer to the key if the value exists, NULL otherwise
bool hashTable_exists(hashTable_t* table, char* key);

// Remove an element from the hashtable
// table - pointer to the hashtable
// key - string key to remove
void hashTable_remove(hashTable_t* table, char* key);

// Get all the keys for a given value
// caller is responsible for freeing the array (not key strings)
// table - pointer to the hashtable
// value - pointer to the value to lookup
// Returns a NULL-terminated array of keys whose value matches
char** hashTable_get_keys(hashTable_t* table, void* value);

#endif /* HASHTABLE_H */