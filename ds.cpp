#include "common.h"
#include "ds.h"

// https://github.com/explosion/murmurhash/blob/master/murmurhash/MurmurHash2.cpp
u32 map_hash (const char* data) {
  const u32 m = 0x5bd1e995;
  const int r = 24;

  u32 len = strlen(data);

  u32 h = 123456 ^ len;

  while(len >= 4) {
    u32 k = *(u32*)data;

    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    data += 4;
    len -= 4;
  }

  switch(len) {
  case 3: h ^= data[2] << 16;
  case 2: h ^= data[1] << 8;
  case 1: h ^= data[0];
      h *= m;
  };

  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;
  return h;
}

bool map_equals(char const* lhs, const char* rhs) {
    return !strcmp(lhs, rhs);
}

// @POINTERSIZE - We're assuming a pointer is 64 bits
u32 map_hash(void* node) {
    union {
        void* p;
        u32 h[2];
    } u;
    u.p = node;
    return u.h[0] | u.h[1];
}

bool map_equals(void* lhs, void* rhs) {
    return lhs == rhs;
}

char* linear_alloc::alloc(u64 bytes) {
    if (remaining < bytes) {
        remaining = 64 * 1024 * 1024;
        assert(bytes < remaining);
        current = (char*)malloc(remaining);
        blocks.push(current);
        assert(current);
    }

    char* r = current;

    current += bytes;
    remaining -= bytes;

    return r;
}

void linear_alloc::free_all() {
    for (char* b : blocks)
        free(b);
}
