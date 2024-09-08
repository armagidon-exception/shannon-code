#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stdio.h>
#include "vec.h"

struct code_s {
  uint64_t code;
  uint8_t length;
};

struct codetable_s {
  uint64_t occurrences[256];
  struct code_s codes[256];
  uint64_t total;
  char_vector* registered_chars;
};

struct codetable_s encoder_build_code_table(FILE* file);

int encoder_read8(FILE* file, uint8_t* o);
int encoder_read64(FILE* file, uint64_t* o);

void encoder_decompress_to_stream(FILE* input, FILE* output);

int encoder_write8(FILE* file, uint8_t byte);
int encoder_write64(FILE* file, uint64_t l);

void encoder_compress_to_stream(FILE* input, struct codetable_s* codetable, FILE* output);


#endif /* end of include guard: ENCODER_H */
