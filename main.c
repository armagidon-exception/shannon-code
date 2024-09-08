#include "encoder.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct opts_s {
  char *filename;
  char *output_name;
  bool decompress;
};

bool read_options(int argc, char *argv[], struct opts_s *opts) {
  opts->decompress = false;
  int o;
  while ((o = getopt(argc, argv, "do:")) != -1) {
    switch (o) {
    case 'd':
      opts->decompress = true;
      break;
    case 'o': {
      int l = strlen(optarg);
      char *cpy = malloc(l + 1);
      strcpy(cpy, optarg);
      opts->output_name = cpy;
      break;
    }
    default:
      return false;
    }
  }

  if (optind == argc) {
    printf("File not specified\n");
    return false;
  }

  if (opts->output_name == NULL) {
    fprintf(stderr, "Output file not specified\n");
    return false;
  }

  return true;
}

void print_bin(uint64_t n, uint8_t l) {
  for (int i = l - 1; i >= 0; i--) {
    if ((n & (1 << i)) == 0)
      printf("%d", 0);
    else {
      printf("%d", 1);
    }
  }
}

int main(int argc, char *argv[]) {
  struct opts_s ops;
  if (!read_options(argc, argv, &ops))
    return -1;

  char *filename = argv[optind];
  FILE *file = fopen(filename, "r");
  FILE *new_file = fopen(ops.output_name, "w");
  if (!file) {
    fprintf(stderr, "File %s not found\n", filename);
    return -1;
  }

  if (!ops.decompress) {
    struct codetable_s codetable = encoder_build_code_table(file);
    rewind(file);
    encoder_compress_to_stream(file, &codetable, new_file);
    printf("Compressed to %s\n", ops.output_name);
    vec_destroy(codetable.registered_chars);
  } else {
    printf("Decompressed to %s\n", ops.output_name);
    encoder_decompress_to_stream(file, new_file);
  }

  free(ops.output_name);
  fclose(new_file);
  fclose(file);
}
