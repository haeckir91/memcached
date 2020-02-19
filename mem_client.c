#include <libmemcached/memcached.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

char keys[10][50];

static void rand_str(char *dest, size_t length) {
    char charset[] = "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}


int main(int argc, char **argv) {
  //memcached_servers_parse (char *server_strings);
    memcached_server_st *servers = NULL;
    memcached_st *memc;
    memcached_return rc;
    char *key = "keystring";
    char *value = "keyvalue";

    char *retrieved_value;
    size_t value_length;
    uint32_t flags;

    memc = memcached_create(NULL);
    servers = memcached_server_list_append(servers, "localhost", 11211, &rc);
    rc = memcached_server_push(memc, servers);
    if (rc == MEMCACHED_SUCCESS) {
        fprintf(stderr, "Added server successfully\n");
    } else {
        fprintf(stderr, "Couldn't add server: %s\n", memcached_strerror(memc, rc));
    }

    srand(time(NULL));
    for (int i = 0; i < 10; i++) {
        rand_str(keys[i], 50);
    }
    
    for (int i = 0 ; i < 1000; i++) {
        //key = keys[i % 10];
        rc = memcached_set(memc, key, strlen(key), value, strlen(value), (time_t)0, (uint32_t)0);

        if (rc == MEMCACHED_SUCCESS) {
            fprintf(stderr, "Key stored successfully\n");
        } else {
            fprintf(stderr, "Couldn't store key: %s\n", memcached_strerror(memc, rc));
        }

        retrieved_value = memcached_get(memc, key, strlen(key), &value_length, &flags, &rc);
        printf("Yay!\n");

        if (rc == MEMCACHED_SUCCESS) {
            fprintf(stderr, "Key retrieved successfully\n");
            printf("The key '%s' returned value '%s'.\n", key, retrieved_value);
            free(retrieved_value);
        } else {
            fprintf(stderr, "Couldn't retrieve key: %s\n", memcached_strerror(memc, rc));
        }
    }
    return 0;
}
