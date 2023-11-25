/**
 * Author: Winston Trinh, November 2023
 */

#include <getopt.h>  // getopt, optarg
#include <stdlib.h>  // exit, atoi, malloc, free
#include <stdio.h>   // printf, fprintf, stderr, fopen, fclose, FILE
#include <limits.h>  // ULONG_MAX
#include <string.h>  // strcmp, strerror
#include <errno.h>   // errno

/* fast base-2 integer logarithm */
#define INT_LOG2(x) (31 - __builtin_clz(x))
#define NOT_POWER2(x) (__builtin_clz(x) + __builtin_ctz(x) != 31)

/* tag_bits = ADDRESS_LENGTH - set_bits - block_bits */
#define ADDRESS_LENGTH 64

/**
 * Print program usage
 */
static void print_usage() {
    printf("Usage: csim [-hv] -S <num> -K <num> -B <num> -p <policy> -t <file>\n");
    printf("Options:\n");
    printf("  -h           Print this help message.\n");
    printf("  -v           Optional verbose flag.\n");
    printf("  -S <num>     Number of sets.           (must be > 0)\n");
    printf("  -K <num>     Number of lines per set.  (must be > 0)\n");
    printf("  -B <num>     Number of bytes per line. (must be > 0)\n");
    printf("  -p <policy>  Eviction policy. (one of 'FIFO', 'LRU')\n");
    printf("  -t <file>    Trace file.\n\n");
    printf("Examples:\n");
    printf("$ ./csim    -S 16  -K 1 -B 16 -p LRU -t traces/yi2.trace\n");
    printf("$ ./csim -v -S 256 -K 2 -B 16 -p LRU -t traces/yi2.trace\n");
}

/* Parameters set by command-line args (no need to modify) */
int verbose = 0;   // print trace if 1
int S = 0;         // number of sets
int K = 0;         // lines per set
int B = 0;         // bytes per line

typedef enum { FIFO = 1, LRU = 2 } Policy;
Policy policy;     // 0 (undefined) by default

FILE *trace_fp = NULL;

int B_temp = 0;
int S_temp = 0;

/**
 * Parse input arguments and set verbose, S, K, B, policy, trace_fp.
 */
static void parse_arguments(int argc, char **argv) {
    char c;
    while ((c = getopt(argc, argv, "S:K:B:p:t:vh")) != -1) {
        switch(c) {
            case 'S':
                S = atoi(optarg);
                if (NOT_POWER2(S)) {
                    fprintf(stderr, "ERROR: S must be a power of 2\n");
                    exit(1);
                }
                break;
            case 'K':
                K = atoi(optarg);
                break;
            case 'B':
                B = atoi(optarg);
                break;
            case 'p':
                if (!strcmp(optarg, "FIFO")) {
                    policy = FIFO;
                }
                // parse LRU, exit with error for unknown policy
                if (!strcmp(optarg, "LRU")) {
                    policy = LRU;
                }
                break;
            case 't':
                // open file trace_fp for reading
                trace_fp = fopen(optarg, "r");
                if (!trace_fp) {
                    fprintf(stderr, "ERROR: %s: %s\n", optarg, strerror(errno));
                    exit(1);
                }
                break;
            case 'v':
                // TODO
                verbose = 1;
                break;
            case 'h':
                // TODO
                print_usage();
                exit(0);
            default:
                print_usage();
                exit(1);
        }
    }

    /* Ensure that all required command line args were specified and valid */
    if (S <= 0 || K <= 0 || B <= 0 || policy == 0 || !trace_fp) {
        printf("ERROR: Negative or missing command line arguments\n");
        print_usage();
        if (trace_fp) {
            fclose(trace_fp);
        }
        exit(1);
    }

    /* Other setup */
    c = INT_LOG2(S);
    B_temp = INT_LOG2(B);
}

/**
 * Cache data structures
 */
typedef struct Block {
    unsigned long tagBits;
    long countBlock;
    char validBits;
} BlockCache;

unsigned long setMask;

unsigned int num_fifo = 1;
unsigned int num_lru = 1;

typedef BlockCache *SetCache;
typedef SetCache *Cache;
Cache cache;

/**
 * Allocate cache data structures.
 *
 * This function dynamically allocates (with malloc) data structures for each of
 * the `S` sets and `K` lines per set.
 *
 */
static void allocate_cache() {
    cache = (SetCache*)malloc(sizeof(SetCache) * S);
    setMask = (unsigned long)(S - 1);
    for (int idx = 0; idx < S; ++idx) {
        cache[idx] = (BlockCache*)malloc(sizeof(BlockCache) * K);
        for (int jdx = 0; jdx < K; ++jdx) {
            cache[idx][jdx].tagBits = 0;
            cache[idx][jdx].countBlock = 0;
            cache[idx][jdx].validBits = 0;
        }
    }
}

/**
 * Deallocate cache data structures.
 *
 * This function deallocates (with free) the cache data structures of each
 * set and line.
 *
 */
static void free_cache() {
    for (int idx = 0; idx < S; ++idx) {
        free(cache[idx]);
    }
    free(cache);
}

/* Counters used to record cache statistics */
int miss_count     = 0;
int hit_count      = 0;
int eviction_count = 0;

/**
 * Simulate a memory access.
 *
 * If the line is already in the cache, increase `hit_count`; otherwise,
 * increase `miss_count`; increase `eviction_count` if another line must be
 * evicted. This function also updates the metadata used to implement eviction
 * policies (LRU, FIFO).
 *
 */
static void access_data(unsigned long addr) {
    unsigned long num_tag = addr >> (S_temp + B_temp);
    unsigned long num_set = setMask & (addr >> B_temp); 

    SetCache set_cache = cache[num_set];

    if (policy == LRU) {
        for (int idx = 0; idx < K; ++idx) {
            if (set_cache[idx].tagBits == num_tag && set_cache[idx].validBits) {
                ++hit_count;
                set_cache[idx].countBlock = ++num_lru;
                if(verbose == 1) {
                    printf(" HIT ");
                }
                return;
            }
        }

        ++miss_count;
        if (verbose == 1) {
            printf(" MISS ");
        }
    
        int inv = -1;
        for (int idx = 0; idx < K; ++idx) {
            if (!set_cache[idx].validBits) {
                inv = idx;
                break;
            }
        }

        if (inv != -1) {
            set_cache[inv].tagBits = num_tag;
            set_cache[inv].countBlock = ++num_lru;
            set_cache[inv].validBits = 1;
        } else {
            if(verbose == 1) {
                printf(" EVICTION ");
            }
            ++eviction_count;
            long lowest_count = set_cache[0].countBlock;
            int lowest_count_idx = 0;
            for(int idx = 0; idx < K; ++idx){
                if (lowest_count > set_cache[idx].countBlock) {
                    lowest_count = set_cache[idx].countBlock;
                    lowest_count_idx = idx;
                }
            }
            set_cache[lowest_count_idx].countBlock = ++num_lru;
            set_cache[lowest_count_idx].tagBits = num_tag;
        }
    } else if (policy == FIFO) {
        for (int idx = 0; idx < K; ++idx) {
            if (set_cache[idx].tagBits == num_tag && set_cache[idx].validBits){
                if(verbose == 1) {
                    printf(" HIT ");
                }
                ++hit_count;
                return;
            }
        }

        if (verbose == 1) {
            printf(" MISS ");
        }
        ++miss_count;
        
        int inv = -1;
        for (int idx = 0; idx < K; ++idx) {
            if (!set_cache[idx].validBits) {
                inv = idx;
                break;
            }
        }

        if (inv != -1) {
            set_cache[inv].tagBits = num_tag;
            set_cache[inv].countBlock = ++num_fifo;
            set_cache[inv].validBits = 1;
        } else {
            if(verbose == 1) {
                printf(" EVICTION ");
            }
            ++eviction_count;
            int recent = 0;
            for(int idx = 0; idx < K; ++idx) {
                if (set_cache[recent].countBlock > set_cache[idx].countBlock) {
                    recent = idx;
                }
            }
            set_cache[recent].tagBits = num_tag;
            set_cache[recent].countBlock = ++num_fifo;
            set_cache[recent].validBits = 1;
        }
    }
}

/**
 * Replay the input trace.
 *
 * This function:
 * - reads lines (e.g., using fgets) from the file handle `trace_fp` (a global variable)
 * - skips lines not starting with ` S`, ` L` or ` M`
 * - parses the memory address (unsigned long, in hex) and len (unsigned int, in decimal)
 *   from each input line
 * - calls `access_data(address)` for each access to a cache line
 *
 */
static void replay_trace() {
    char trace_cmd;
    unsigned long address;
    long unsigned int size;
    while (fscanf(trace_fp, " %c %lx,%ld", &trace_cmd, &address, &size) == 3) {
        if(trace_cmd == 'L' || trace_cmd == 'S') {
            if(verbose == 1) {
                printf("ADDRESS: %lx", address);
            }
            if (size > 1) {
                for (long unsigned int idx = address; idx < address + size; ++idx) {
                    if (idx == address || idx % B == 0) {
                        access_data(idx);
                    }
                }
            } else {
                access_data(address);
            }
        }
        // For 'M', we need to access blocks twice for size checks
        // To do this, we can call access_data twice
        else if(trace_cmd == 'M') {
            if(verbose == 1) {
                printf("ADDRESS: %lx", address);
            }
            if (size > 1) {
                for (long unsigned int idx = address; idx < address + size; ++idx) {
                    if (idx == address || idx % B == 0) {
                        access_data(idx);
                        access_data(idx);
                    }
                }
            } else {
                access_data(address);
                access_data(address);
            }
        }
    }
}

/**
 * Print cache statistics
 */
static void print_summary(int hits, int misses, int evictions) {
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
}

int main(int argc, char **argv) {
    parse_arguments(argc, argv);  // set global variables used by simulation
    allocate_cache();             // allocate data structures of cache
    replay_trace();               // simulate the trace and update counts
    free_cache();                 // deallocate data structures of cache
    fclose(trace_fp);             // close trace file
    print_summary(hit_count, miss_count, eviction_count);  // print counts
    return 0;
}
