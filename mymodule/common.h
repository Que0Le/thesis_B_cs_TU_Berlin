// #define DEBUG_KM_10
// #define DEBUG_KM_INCOMING_PACKETS
// #define DEBUG_UP_INCOMING_PACKETS
// #define DEBUG_US_INCOMING_PACKETS
// #define DEBUG_EBPF_INCOMING_PACKETS
// #define DEBUG_READ_FROM_US

const char *path_prefix = "/proc/";
const char *proc_filename = "intercept_mmap";
const char *path_km = "/home/que/Desktop/mymodule/logs/KM_kern.txt";
const char *path_km_user = "/home/que/Desktop/mymodule/logs/KM_user.txt";

const char *path_km_server_linuxsocket = "/home/que/Desktop/mymodule/logs/KM_socket_server.txt";
const char *path_ebpf_server_linuxsocket = "/home/que/Desktop/mymodule/logs/EBPF_socket_server.txt";

const char *path_xdp_kern = "/home/que/Desktop/mymodule/logs/XDP_kern.txt";
const char *path_xdp_us = "/home/que/Desktop/mymodule/logs/XDP_user.txt";

const char *path_ebpf_kern = "/home/que/Desktop/mymodule/logs/EBPF_kern.txt";
const char *path_ebpf_us = "/home/que/Desktop/mymodule/logs/EBPF_user.txt";

enum { 
    BUFFER_SIZE = 4096,
    PKT_BUFFER_SIZE = 128,
    PKTS_PER_BUFFER = BUFFER_SIZE/PKT_BUFFER_SIZE,                // = BUFFER_SIZE / PKT_BUFFER_SIZE
    SIZE_OF_PAGE_HC = 4096,             // hardcode
    MAX_PKT = 200,
    KM_FIND_BUFF_SLOT_MAX_TRY = 200,
    /* SHIFT x == 2^(x+1) 15=65.536 16=131.072 17=262.144 18=524.288 19=1.048.576 */
    MAX_LOG_ENTRY_SHIFT = 14,
    MAX_LOG_ENTRY = 2 << MAX_LOG_ENTRY_SHIFT,
    /* PAGES_PER_LOG_BUFF = 2^PAGES_ORDER = 64 pages */
    PAGES_ORDER = 6,
    /* 64pages * 4KB/page = 2^18B (262144B) = 32768 unsigned long */
    MAX_ENTRIES_PER_LOG_BUFF = (2 << (PAGES_ORDER-1))*SIZE_OF_PAGE_HC/8,
    NUM_LOG_BUFF = MAX_LOG_ENTRY / MAX_ENTRIES_PER_LOG_BUFF
};

/* Rate of while-loop */
enum {
    /* This rates are based on the dev machine. Adapt them to your machine! */
    CLIENT_RATE = 2,   // rate=3k pps
    // CLIENT_RATE = 4,   // rate=6k pps
    // CLIENT_RATE = 50,   // rate=50k pps
    /* 
    r20~30pkt/ms
    r50~48pkt/ms
    */
    USER_PROCESSING_RATE = 10
};

#define DEST_PORT 8080
#define DEST_IPADDR "192.168.1.24"

enum  {
    PL_KEEP_ALIVE = 1,
    PL_DATA = 2,
    PL_LOG = 3
};

struct Payload {
    unsigned long client_uid;
    unsigned long uid;
    unsigned long type;
    unsigned long created_time;         //nsec
    unsigned long ks_time_arrival_1;    //nsec
    unsigned long ks_time_arrival_2;    //nsec
    unsigned long us_time_arrival_1;    //nsec
    unsigned long us_time_arrival_2;    //nsec
    // Extra data
    char data[960];  // 1024k
    // char data[448];  // 512k
};
