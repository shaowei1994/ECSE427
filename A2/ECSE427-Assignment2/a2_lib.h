//
//  a2_lib.h
//  ECSE427-Assignment2
//
//  Created by Shao-Wei Liang on 2018-11-02.
//  Copyright © 2018 Shao-Wei Liang. All rights reserved.
//

#ifndef a2_lib_h
#define a2_lib_h

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>

int kv_store_create(char *name);
int kv_store_write(char *key, char *value);
char *kv_store_read(char *key);
char **kv_store_read_all(char *key);
int kv_delete_db(void);
unsigned long hash(const char *str);

#define DATA_BASE_NAME "my_database"

#define keySize 32
#define valueSize 256
#define keyValuePairSize (keySize + valueSize)          // (keySize + valueSize)

#define numberOfPods 256                                // number of pods within the KV-Store
#define podSize 256                                     // number of KV-Pairs per pod
#define maxKeyValuePairs (numberOfPods * podSize)       // (numberOfPods * podSize)

typedef struct {
    char key[keySize];
    char value[valueSize];
} kvPair;

typedef struct {
    int podNums[numberOfPods];
    int podSlots[podSize];
    int readCounter;
    int initialized;
} kvStore;

#endif /* a2_lib_h */
