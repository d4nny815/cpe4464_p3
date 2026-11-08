#include "hashtable.h"
#include "smartalloc.h"
#include <string.h>
#include <stdio.h>

// djb2 hash function from wikipedia
size_t hash(char* str) {
    size_t hash = 5381;
    char c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + (size_t)c;

    return hash;
}

size_t hashTable_get_index(hashTable_t* table, char* key) {
    return hash(key) % table->size;
}

hashEntry_t* hashTable_get_entry(hashTable_t* table, char* key) {
    // Hashes should never collide, so we can just return the entry
    size_t index = hashTable_get_index(table, key);
    return &table->entries[index];
}

void hashTable_resize(hashTable_t* table, size_t new_size) {
    hashEntry_t* new_entries = (hashEntry_t*)calloc(0, sizeof(hashEntry_t) * new_size);
    if (!new_entries) {
        perror("[HASHTABLE] Failed to allocate memory for new hashtable entries");
        return;
    }

    for (size_t i = 0; i < table->size; i++) {
        if (!table->entries[i].is_valid) {
            continue;
        }

        size_t index = hashTable_get_index(table, table->entries[i].key);
        new_entries[index] = table->entries[i];
    }

    free(table->entries);
    table->entries = new_entries;
    table->size = new_size;
    
    return;
}

hashTable_t* hashTable_create() {
    hashTable_t* table = (hashTable_t*)calloc(0, sizeof(hashTable_t));
    if (!table) {
        perror("[HASHTABLE] Failed to allocate memory for hashtable");
        return NULL;
    }
    
    table->entries = (hashEntry_t*)calloc(0, sizeof(hashEntry_t) * HASH_DEFAULT_SIZE);
    if (!table->entries) {
        perror("[HASHTABLE] Failed to allocate memory for hashtable entries");
        free(table);
        return NULL;
    }

    table->size = HASH_DEFAULT_SIZE;

    // ? if i calloc then are these already set to these values?
    for (size_t i = 0; i < table->size; i++) {
        table->entries[i].is_valid = false;
        table->entries[i].key = NULL;
        table->entries[i].value = NULL;
    }

    return table;
}

void hashTable_destroy(hashTable_t* table) {
    for (size_t i = 0; i < table->size; i++) {
        if (!table->entries[i].is_valid) {
            continue;
        }

        free(table->entries[i].key);
        free(table->entries[i].value);
    }

    free(table->entries);
    free(table);

    return;
}

void hashTable_insert(hashTable_t* table, char* key, void* value) {
    if (table->num_entries >= table->size * 0.75) {
        hashTable_resize(table, table->size * 2);
    }

    hashEntry_t* entry = hashTable_get_entry(table, key);
    entry->is_valid = true;
    entry->key = key;
    entry->value = value;    

    table->num_entries++;
    return;
}

void hashTable_remove(hashTable_t* table, char* key) {
    hashEntry_t* entry = hashTable_get_entry(table, key);
    if (!entry->is_valid) {
        return;
    }

    free(entry->key);
    free(entry->value);
    entry->is_valid = false;
    entry->key = NULL;
    entry->value = NULL;

    table->num_entries--;
    
    return;
}

void* hashTable_get_value(hashTable_t* table, char* key) {
    hashEntry_t* entry = hashTable_get_entry(table, key);
    if (!entry->is_valid) {
        return NULL;
    }

    return entry->value;
}

char** hashTable_get_keys(hashTable_t* table, void* value) {
    char** keys = (char**)calloc(0, sizeof(char*) * table->num_entries);
    if (!keys) {
        perror("[HASHTABLE] Failed to allocate memory for keys");
        return NULL;
    }

    for (size_t i = 0; i < table->size; i++) {
        if (!table->entries[i].is_valid) {
            continue;
        }

        if (table->entries[i].value == value) {
            keys[i] = table->entries[i].key;
        }
    }

    return keys;
}