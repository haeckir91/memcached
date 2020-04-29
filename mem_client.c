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

#define NUM_KEYS 10
#define KEY_LENGTH 10

#define NSEC_PER_SEC    1000000000L


struct bench_record {
    uint64_t start;
    uint64_t end;
};

static int num_ops = 2000;
static int num_threads = 1;
static bool non_block = false;
static char* server = "127.0.0.1";
static int key_range = 15000000;
static int count = 0;


static pthread_t* threads; // all threads
static pthread_barrier_t barrier1;
static pthread_barrier_t barrier2;
static pthread_barrier_t barrier3;

static struct bench_record* bench;

static void parse_cmdline(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "s:r:t:k:b")) != -1) {
        switch (opt) {
        case 's': 
            printf("Using server %s \n", optarg);
            server = strdup(optarg);
            break;
        case 'r': 
            printf("Sending %d set requests \n", atoi(optarg));
            num_ops = atoi(optarg);
            break;
        case 't': 
            printf("Starting %d client threads \n", atoi(optarg));
            num_threads = atoi(optarg);
            break;
        case 'b': 
            printf("Sets are non blocking \n");
            non_block = true;
            break;
        case 'k': 
            printf("Sets key range to %d \n", atoi(optarg));
            key_range = atoi(optarg);
            non_block = true;
            break;

        default:
            fprintf(stderr, "Usage: %s [-s] <Server IP Address> [-r] <Number of Requests> \n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
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
        fprintf(stderr, "Added server successfully\n");
    } else {
        fprintf(stderr, "Couldn't add server: %s\n", memcached_strerror(memc, rc));
    }

    srand(time(NULL));
    
    if (non_block) {
        rc = memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_NO_BLOCK, 1);
        if (rc != MEMCACHED_SUCCESS) {
            printf("Failed setting memcached behaviour as non blocking\n");
            exit(EXIT_FAILURE);
        }
    }

    pthread_barrier_wait(&barrier1);

    if (t_id == 0) {
        char len_str[32];
        sprintf(len_str, "%d", num_ops);
        rc = memcached_set(memc, "start_benchmark", strlen("start_benchmark"), "2000", 
                           strlen("2000"), (time_t)0, (uint32_t)0);
        if (rc == MEMCACHED_SUCCESS) {
            printf("Starting Benchmark \n");
        } else {
            fprintf(stderr, "Starting Benchmark FAILED\n");
            exit(EXIT_FAILURE);
        }
    }

    printf("Thread %lu waiting to start benchmark \n", t_id);
    pthread_barrier_wait(&barrier2);
    
    char* value;
    char key[128];
    int req = 0;
    size_t value_length;
    uint32_t flags;
    struct timespec start;
    struct timespec end;
    while (count < num_ops) {
        int old =__atomic_fetch_add(&count, 1, __ATOMIC_SEQ_CST);
        int ret = sprintf(key, "%d", rand() % key_range);
        clock_gettime(CLOCK_REALTIME, &start);
        value = memcached_get(memc, key, ret, &value_length, &flags, &rc);
        clock_gettime(CLOCK_REALTIME, &end);
        if (rc != MEMCACHED_SUCCESS) {
            fprintf(stderr, "Couldn't get key: %s\n", memcached_strerror(memc, rc));
        }
        free(value);
        req++;
        bench[old].start = start.tv_sec*NSEC_PER_SEC + start.tv_nsec;
        bench[old].end = end.tv_sec*NSEC_PER_SEC + end.tv_nsec;
    }

    pthread_barrier_wait(&barrier3);

    printf("Thread %lu finished benchmark after sending %d requests\n", 
           t_id, req);

    if (t_id == 0) {
        rc = memcached_set(memc, "end_benchmark", strlen("end_benchmark"), "1000", 
                           strlen("1000"), (time_t)0, (uint32_t)0);
    }
    memcached_free(memc);
    memcached_server_list_free(servers);
    return NULL;

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
    
    pthread_barrier_init(&barrier1, NULL, num_threads);
    pthread_barrier_init(&barrier2, NULL, num_threads);
    pthread_barrier_init(&barrier3, NULL, num_threads);

    char *retrieved_value;
    size_t value_length;
    uint32_t flags;

    FILE *fp = fopen("memcached-client-kernel-tcp.csv", "ab+");

    bench = calloc(num_ops, sizeof(struct bench_record));

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
        pthread_attr_destroy(&attr);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);
    
    FILE* output;

    output = fopen("memcached-kernel-tcp-client00.csv", "w+");
    assert(output);
    fprintf(output, "id,start,end,latency\n");
    for (int i = 0; i < num_ops; i++) {
        fprintf(output, "%d,%lu,%lu,%lu\n", i+1, bench[i].start,
                bench[i].end, bench[i].end - bench[i].start);
    }
    fflush(output);
    fclose(output);

    printf("Ending Benchmark \n");
    return 0;
}
