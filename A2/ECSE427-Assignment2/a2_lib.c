//
//  a2_lib.c
//  ECSE427-Assignment2
//
//  Created by Shao-Wei Liang on 2018-10-29.
//  Copyright © 2018 Shao-Wei Liang. All rights reserved.
//

#include "a2_lib.h"

sem_t *db;
sem_t *mutex;
char *kvStoreInfoAddr;

int kv_store_create(char *name) {
    
    // Creates and opens a new, or opens an existing, POSIX shared memory object.
    int fd = shm_open(name, O_CREAT | O_RDWR, S_IRWXU);
    if (fd < 0) {
        perror("Error... Opening shm\n");
        return -1;
    }
    
    // Resize the shared memory object to a specific length (Key-value store PLUS total size of all potential key-value pair
    size_t sizeToAllocate = (sizeof(kvStore) + maxKeyValuePairs * keyValuePairSize);
    ftruncate(fd, sizeToAllocate);
    kvStoreInfoAddr = (char *) mmap(NULL, sizeToAllocate, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // Initialize Semaphores then does error check.
    mutex = sem_open("260606721_a2_mutex", O_CREAT, S_IRWXU ,1);
    if (mutex == SEM_FAILED) {
        perror("mutex semaphore failed.");
        return(-1);
    }
    
    db = sem_open("260606721_a2_db", O_CREAT, S_IRWXU, 1);
    if (db == SEM_FAILED) {
        perror("db semaphore failed.");
        return -1;
    }
    
    // Initialize a local variable kvStoreInfo so we can access the attributes within
    kvStore* kvStoreInfo = (kvStore *)kvStoreInfoAddr;

    // Initialized the KV-store info (Book Keeping)
    if (kvStoreInfo->initialized == 0) {
        for (int i = 0; i < numberOfPods; i++) {
            kvStoreInfo->podNums[i] = 0;
        }
        for (int j = 0; j < podSize; j++) {
            kvStoreInfo->podSlots[j] = 0;
        }
        kvStoreInfo->initialized = 1;
        kvStoreInfo->readCounter = 0;
    }
    
    close(fd);
    return 0;
}

unsigned long hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    
    while(c = *str++) {
        hash = ((hash << 5) + hash) + c;
    }
    hash = (hash > 0) ? hash : -(hash);
    return hash % numberOfPods;
}

int kv_store_write(char *key, char *value) {
    
    // Determine the pod number a key belongs in
    unsigned long podNum = hash(key);
    
    // Determine the starting location of the pod that contains the key
    size_t podLocation = podSize * keyValuePairSize * podNum;

    // Initailize kvStoreInfo to access podNums
    kvStore* kvStoreInfo = (kvStore *)kvStoreInfoAddr;
    
    // kvStoreInfo->podNums[podNum] returns an int which indicates the number of key-value pair within the given pod.
    size_t offset = keyValuePairSize * kvStoreInfo->podNums[podNum];
    
    sem_wait(db);
    
    // Store the given key and value into the shared memory
    memcpy(kvStoreInfoAddr + sizeof(kvStore) + offset + podLocation, key, keySize);
    memcpy(kvStoreInfoAddr + sizeof(kvStore) + offset + podLocation + keySize, value , valueSize);
    
    // Key-Value written into the pod, increment the count of key-value pairs within this pod
    kvStoreInfo->podNums[podNum]++;
    
    // If the current count value is greater than the pod size, we will loop back to the start of the pod
    //  so that the next write will replace the existing (oldest) entry.
    kvStoreInfo->podNums[podNum] = kvStoreInfo->podNums[podNum] % podSize;

    sem_post(db);
    
    return 0;
}

// Reference: Section 2.5.2 of the course textbook
char *kv_store_read(char *key) {

    sem_post(mutex);
    sem_post(db);
    
    // Get exclusive access to readCounter
    sem_wait(mutex);
    
    kvStore* kvStoreInfo = (kvStore *)kvStoreInfoAddr;
    size_t readCounter = kvStoreInfo->readCounter;
    readCounter++;
    if (readCounter == 1) {
        sem_wait(db);
    }
    sem_post(mutex);

    // Determine the pod number a key belongs in
    unsigned long podNum = hash(key);
    
    // Find the head of a specific pod by multiplying the pod number with size of pod
    size_t podLocation = podSize * keyValuePairSize * podNum;

    size_t offset;

    for (int i = 0; i < podSize; i++) {
        // kvStoreInfo->podSlots[podNum] returns an int which indicates the point of search.
        offset = keyValuePairSize * kvStoreInfo->podSlots[podNum];
        if (memcmp(kvStoreInfoAddr + sizeof(kvStore) + offset + podLocation, key, strlen(key)) == 0) {
            
            kvStoreInfo->podSlots[podNum]++;
            kvStoreInfo->podSlots[podNum] = kvStoreInfo->podSlots[podNum] % podSize;
            
            sem_wait(mutex);
            readCounter--;
            if (readCounter == 0) {
                sem_post(db);
            }
            sem_post(mutex);
            
            char *value = strdup((char *) (kvStoreInfoAddr + sizeof(kvStore) + offset + podLocation + keySize));
            
            return value;
        } else {
            kvStoreInfo->podSlots[podNum]++;
            kvStoreInfo->podSlots[podNum] = kvStoreInfo->podSlots[podNum] % podSize;
        }
    }
    
    sem_wait(mutex);
    readCounter--;
    if (readCounter == 0) {
        sem_post(db);
    }
    sem_post(mutex);
    
    return NULL;
}

char **kv_store_read_all(char *key) {
    
    char *value;
    char **allValues = malloc(sizeof(char *));
    int valuesCount = 0;

    // Get exclusive access to readCounter
    sem_wait(mutex);
    kvStore* kvStoreInfo = (kvStore *)kvStoreInfoAddr;
    size_t readCounter = kvStoreInfo->readCounter;
    readCounter++;
    if (readCounter == 1) {
        sem_wait(db);
    }
    sem_post(mutex);
    
    // Determine the pod number a key belongs in
    unsigned long podNum = hash(key);
    
    // Find the head of a specific pod by multiplying the pod number with size of pod
    size_t podLocation = podSize * keyValuePairSize * podNum;
    
    size_t offset;
    
    // Similar to read but instead of returning a single value, store the result in an array and return it at the end
    for (int i = 0; i < podSize; i++) {
        // kvStoreInfo->podSlots[podNum] returns an int which indicates the point of search.
        offset = keyValuePairSize * kvStoreInfo->podSlots[podNum];
        if (memcmp(kvStoreInfoAddr + sizeof(kvStore) + offset + podLocation, key, strlen(key)) == 0) {
            kvStoreInfo->podSlots[podNum]++;
            kvStoreInfo->podSlots[podNum] = kvStoreInfo->podSlots[podNum] % podSize;
            
            sem_wait(mutex);
            readCounter--;
            if (readCounter == 0) {
                sem_post(db);
            }
            sem_post(mutex);
            
            valuesCount++;
            allValues = realloc(allValues , sizeof(char *) * (valuesCount));
            value = strdup((char *) (kvStoreInfoAddr + sizeof(kvStore) + offset + podLocation + keySize));
            
            allValues[valuesCount - 1] = value;
        } else {
            kvStoreInfo->podSlots[podNum]++;
            kvStoreInfo->podSlots[podNum] = kvStoreInfo->podSlots[podNum] % podSize;
        }
    }
    
    sem_wait(mutex);
    readCounter--;
    if (readCounter == 0) {
        sem_post(db);
    }
    sem_post(mutex);
    
    // No values found within the store
    if (valuesCount == 0) {
        return NULL;
    }
    
    return allValues;
}

int kv_delete_db(){
    
    // Unlink the semaphores
    sem_unlink("260606721_a2_db");
    sem_unlink("260606721_a2_mutex");
    
    // Removes the memory mapped earlier via mmap(...)
    if(munmap(kvStoreInfoAddr, sizeof(kvStore) + maxKeyValuePairs * keyValuePairSize) == -1){
        perror("Could not delete store");
        return(-1);
    }
    
    shm_unlink(DATA_BASE_NAME);
    
    return(0);
}
