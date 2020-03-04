#define _GNU_SOURCE
#include <libmemcached/memcached.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <assert.h>

static uint64_t num_keys = 1500000;
#define KEY_LENGTH 8
#define VALUE_LENGTH 16

static int num_threads = 1;
static bool non_block = false;
static char* server = "127.0.0.1";
static char** keys;

static pthread_t* threads; // all threads

static void parse_cmdline(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "s:t:k:")) != -1) {
        switch (opt) {
        case 's': 
            printf("Using server %s \n", optarg);
            server = calloc(1, strlen(optarg));
            strncpy(server, optarg, strlen(optarg));
            break;
        case 't': 
            printf("Starting %d client threads \n", atoi(optarg));
            num_threads = atoi(optarg);
            break;
        case 'k': 
            num_keys = atoi(optarg);
            printf("Sets number of keys before benchmark to %lu \n", num_keys);
            keys = (char**) malloc(num_keys*sizeof(char*));
            for (int i = 0; i < num_keys; i++) {
                keys[i] = (char*) malloc(KEY_LENGTH+1);
            }
            break;

        default:
            fprintf(stderr, "Usage: %s [-s] <Server IP Address> [-t] <Number of Threads> [-k] <Number of key to populate to memcached> \n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}


static void rand_str(char *dest, size_t length) {
    char charset[] = "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}


static void *client(void *arg)
{
    uint64_t t_id = (uint64_t) arg;

    memcached_server_st *servers = NULL;
    memcached_st *memc;
    memcached_return rc;

    memc = memcached_create(NULL);
    servers = memcached_server_list_append(servers, server, 11211, &rc);
    rc = memcached_server_push(memc, servers);
    if (rc == MEMCACHED_SUCCESS) {

    } else {
        fprintf(stderr, "Couldn't add server: %s\n", memcached_strerror(memc, rc));
    }

    srand(time(NULL));
    int start = t_id*(num_keys/num_threads);
    int end = (t_id+1)*(num_keys/num_threads);
    
    char value[VALUE_LENGTH+1];
    rand_str(value, VALUE_LENGTH);
    for (int i = start; i < end; i++) {
        rand_str(keys[i], KEY_LENGTH);
        rc = memcached_set(memc, keys[i], KEY_LENGTH, value, 
                           strlen(value), (time_t)0, (uint32_t)0);
        if (rc != MEMCACHED_SUCCESS) {
            fprintf(stderr, "Couldn't add key\n");
        }
    }
}

int main(int argc, char **argv) {
  //memcached_servers_parse (char *server_strings);
  //
    parse_cmdline(argc, argv);

    if ((num_threads >= 1) && (num_threads <= 100)) {
        threads = (pthread_t*) calloc(num_threads, sizeof(pthread_t));
    } else {
        fprintf(stderr, "Invalid number of threads. Should be 1 <= threads value <= 100 \n");
        exit(1);
    }

    char *retrieved_value;
    size_t value_length;
    uint32_t flags;

    uint32_t num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    for (uint64_t i = 0; i < (uint32_t) num_threads; i++) { 

	    cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i % num_cores, &cpuset);
        
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
	
        int ret = pthread_create(&threads[i], &attr, client, (void*) i);
        assert(ret == 0);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Ending Setup \n");
    return 0;
}
