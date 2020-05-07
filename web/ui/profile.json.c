#include "ui.h"

const unsigned char profile_json[];
const unsigned int profile_json_len;

file_t file_profile_json = {
  .data = profile_json,
  .len = &profile_json_len,
  .type = (const unsigned char*)"application/json"
};

const unsigned char profile_json[] = {
  0x7b, 0x0a, 0x20, 0x20, 0x22, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x5f, 0x31,
  0x22, 0x3a, 0x20, 0x22, 0x25, 0x73, 0x22, 0x2c, 0x0a, 0x20, 0x20, 0x22,
  0x70, 0x61, 0x72, 0x61, 0x6d, 0x5f, 0x32, 0x22, 0x3a, 0x20, 0x22, 0x25,
  0x73, 0x22, 0x0a, 0x7d, 0x0a, 0x00
};
const unsigned int profile_json_len = 42;