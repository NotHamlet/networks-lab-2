#define BASE_PORT 10010
#define GID 11
#define MAGIC_NUMBER 0x1234

#define MAX_MESSAGE_LENGTH 64

struct join_request {
  uint8_t gid;
  uint16_t magic_num;
} __attribute__((__packed__));


struct request_response {
  uint8_t gid;
  uint16_t magic_num;
  uint8_t rid;
  uint32_t next_IP;
} __attribute__((__packed__));

struct thread_args {
  uint32_t next_IP;
  uint8_t this_rid;
  uint8_t gid;
};

struct ring_message {
  uint8_t gid;
  uint16_t magic_num;
  uint8_t ttl;
  uint8_t rid_source;
  uint8_t rid_dest;
  char message[MAX_MESSAGE_LENGTH];
  uint8_t checksum;
} __attribute__((__packed__));
