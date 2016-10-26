#define BASE_PORT "10010"
#define GID 11

typedef uint8_t GID_t;
typedef uint16_t magicnum_t;

struct join_request {
  GID_t gid;
  magicnum_t magicnum;
};
