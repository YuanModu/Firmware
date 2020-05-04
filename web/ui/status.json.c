#include "ui.h"

const unsigned char status_json[];
const unsigned int status_json_len;

file_t file_status_json = {
  .data = status_json,
  .len = &status_json_len,
  .type = (const unsigned char*)"application/json"
};

const unsigned char status_json[] = {
  0x7b, 0x0a, 0x20, 0x20, 0x22, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x5f, 0x31,
  0x22, 0x3a, 0x20, 0x22, 0x25, 0x73, 0x22, 0x2c, 0x0a, 0x20, 0x20, 0x22,
  0x70, 0x61, 0x72, 0x61, 0x6d, 0x5f, 0x32, 0x22, 0x3a, 0x20, 0x22, 0x25,
  0x73, 0x22, 0x0a, 0x7d, 0x0a, 0x00
};
const unsigned int status_json_len = 42;
