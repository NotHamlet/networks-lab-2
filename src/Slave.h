#define BASE_PORT "10010"
#define GID 11
#define MAGIC_NUMBER 0x1234

typedef uint8_t GID_t;
typedef uint16_t magic_num_t;
typedef uint8_t RID_t;
typedef uint32_t next_slave_IP_t;


struct join_request {
  GID_t gid;
  magic_num_t magic_num;
} __attribute__((__packed__));


struct request_response {
  GID_t gid;
  magic_num_t magic_num;
  RID_t rid;
  next_slave_IP_t next_IP;
} __attribute__((__packed__));
