#define BASE_PORT "10010"
#define GID 11
#define MAGIC_NUMBER 0x1234

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
