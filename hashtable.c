#include "hashtable.h"
#include "smartalloc.h"
#include <string.h>
#include <stdio.h>

// djb2 hash function from wikipedia
static size_t hash(char* str) {
    size_t hash = 5381;
    unsigned char c;

    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + (size_t)c;

    return hash;
}

static size_t hashTable_get_index(size_t size, char* key) {
    return hash(key) % size;
}

static hashEntry_t* hashTable_find_entry(hashTable_t* table, char* key) {
    size_t index = hashTable_get_index(table->size, key);

    // linear probing
    for (size_t i = 0; i < table->size; i++) {
        hashEntry_t* entry = &table->entries[(index + i) % table->size];
        if (!entry->is_valid && !entry->is_deleted) {
            return NULL;
        }

        if (entry->is_valid && strcmp(entry->key, key) == 0) {
            return entry;
        }
    }

    return NULL;
}

static hashEntry_t* hashTable_find_free(hashEntry_t* entries, size_t size, char* key) {
    size_t index = hashTable_get_index(size, key);

    for (size_t i = 0; i < size; i++) {
        hashEntry_t* entry = &entries[(index + i) % size];
        if (!entry->is_valid) {
            return entry;
        }
    }

    return NULL;
}

static void hashTable_free_entry(hashEntry_t* entry) {
    free(entry->key);
    if (entry->value) {
        free(entry->value);
    }
}

static void hashTable_resize(hashTable_t* table, size_t new_size) {
    hashEntry_t* new_entries = (hashEntry_t*)calloc(new_size, sizeof(hashEntry_t));
    if (!new_entries) {
        perror("[HASHTABLE] Failed to allocate memory for new hashtable entries");
        return;
    }

    for (size_t i = 0; i < table->size; i++) {
        if (!table->entries[i].is_valid) {
            continue;
        }

        hashEntry_t* new_entry = hashTable_find_free(new_entries, new_size, table->entries[i].key);
        if (new_entry) {
            *new_entry = table->entries[i];
        }
    }

    free(table->entries);
    table->entries = new_entries;
    table->size = new_size;

    return;
}

hashTable_t* hashTable_create() {
    hashTable_t* table = (hashTable_t*)calloc(1, sizeof(hashTable_t));
    if (!table) {
        perror("[HASHTABLE] Failed to allocate memory for hashtable");
        return NULL;
    }

    // calloc zeroes memory, so every entry starts invalid
    table->entries = (hashEntry_t*)calloc(HASH_DEFAULT_SIZE, sizeof(hashEntry_t));
    if (!table->entries) {
        perror("[HASHTABLE] Failed to allocate memory for hashtable entries");
        free(table);
        return NULL;
    }

    table->size = HASH_DEFAULT_SIZE;
    table->num_entries = 0;

    return table;
}

void hashTable_destroy(hashTable_t* table) {
    for (size_t i = 0; i < table->size; i++) {
        if (!table->entries[i].is_valid) {
            continue;
        }

        hashTable_free_entry(&table->entries[i]);
    }

    free(table->entries);
    free(table);

    return;
}

void hashTable_insert(hashTable_t* table, char* key, void* value) {
    hashEntry_t* entry = hashTable_find_entry(table, key);
    if (entry) {
        // overwrite
        // table owns both the old and new key/value
        if (entry->value && entry->value != value) {
            free(entry->value);
        }
        if (entry->key != key) {
            free(key);
        }
        entry->value = value;
        return;
    }

    // resize if table is 3/4 full
    if (table->num_entries + 1 > table->size * 3 / 4) {
        hashTable_resize(table, table->size * 2);
    }

    entry = hashTable_find_free(table->entries, table->size, key);
    entry->is_valid = true;
    entry->is_deleted = false;
    entry->key = key;
    entry->value = value;

    table->num_entries++;
    return;
}

void hashTable_remove(hashTable_t* table, char* key) {
    hashEntry_t* entry = hashTable_find_entry(table, key);
    if (!entry) {
        return;
    }

    hashTable_free_entry(entry);
    entry->is_valid = false;
    entry->is_deleted = true;
    entry->key = NULL;
    entry->value = NULL;

    table->num_entries--;

    return;
}

void* hashTable_get_value(hashTable_t* table, char* key) {
    hashEntry_t* entry = hashTable_find_entry(table, key);
    if (!entry) {
        return NULL;
    }

    return entry->value;
}

bool hashTable_exists(hashTable_t* table, char* key) {
    return hashTable_find_entry(table, key) != NULL;
}

char** hashTable_get_keys(hashTable_t* table, void* value) {
    char** keys = (char**)calloc(table->num_entries + 1, sizeof(char*));
    if (!keys) {
        perror("[HASHTABLE] Failed to allocate memory for keys");
        return NULL;
    }

    size_t count = 0;
    for (size_t i = 0; i < table->size; i++) {
        if (!table->entries[i].is_valid) {
            continue;
        }

        if (table->entries[i].value == value) {
            keys[count++] = table->entries[i].key;
        }
    }

    return keys;
}
