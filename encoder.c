#include "encoder.h"
#include "vec.h"
#include <stdint.h>
#include <stdio.h>

static void sort_codes(struct codetable_s *table) {
  uint8_t *a = table->registered_chars->array;
  uint64_t *occ = table->occurrences;
  for (int i = 0; i < table->registered_chars->size - 1; i++) {
    bool swapped = false;
    for (int j = 0; j < table->registered_chars->size - i - 1; j++) {
      if (occ[a[j]] > occ[a[j + 1]]) {
        swapped = true;
        int t = a[j];
        a[j] = a[j + 1];
        a[j + 1] = t;
      }
    }
  }
}

static void make_codes(struct codetable_s *table, struct code_s *prefix,
                       char_vector *proccessing, uint64_t total) {
  if (!proccessing->size)
    return;
  else if (proccessing->size == 1) {
    table->codes[proccessing->array[0]] =
        (struct code_s){prefix->code << 1 | 1, prefix->length + 1};
    return;
  } else if (proccessing->size == 2) {
    table->codes[proccessing->array[0]] =
        (struct code_s){prefix->code << 1, prefix->length + 1};
    table->codes[proccessing->array[1]] =
        (struct code_s){prefix->code << 1 | 1, prefix->length + 1};
    return;
  }

  long current = 0;
  char_vector *left = new_vec(128);

  while (proccessing->size) {
    uint8_t b;
    vec_pop(proccessing, &b);
    current += table->occurrences[b];
    vec_push(left, b);
    if (current >= total / 2)
      break;
  }
  vec_rev(left);

  make_codes(table, &(struct code_s){prefix->code << 1, prefix->length + 1},
             left, current);

  vec_destroy(left);
  make_codes(table, &(struct code_s){prefix->code << 1 | 1, prefix->length + 1},
             proccessing, total - current);
}

struct codetable_s encoder_build_code_table(FILE *file) {
  struct codetable_s table;
  table.registered_chars = new_vec(256);
  for (int i = 0; i < 256; i++) {
    table.codes[i] = (struct code_s){0};
    table.occurrences[i] = 0;
  }
  uint8_t byte;
  while (!feof(file) && !ferror(file)) {
    encoder_read8(file, &byte);
    if (!table.occurrences[byte]) {
      vec_push(table.registered_chars, byte);
    }
    table.occurrences[byte]++;
    table.total++;
  }
  sort_codes(&table);
  char_vector *copy = vec_copy(table.registered_chars);
  make_codes(&table, &(struct code_s){0, 0}, copy, table.total);

  vec_destroy(copy);
  return table;
}

int encoder_read8(FILE *file, uint8_t *o) {
  int c = fgetc(file);
  if (feof(file))
    return false;
  else if (ferror(file))
    return false;
  *o = (uint8_t)c;
  return true;
}


int encoder_read16(FILE *file, uint16_t *o) {
  for (int i = 1; i >= 0; i--) {
    uint8_t byte = 0;
    if (!encoder_read8(file, &byte)) {
      return false;
    }
    *o |= (byte << 8 * i);
  }
  return true;
}

int encoder_read64(FILE *file, uint64_t *o) {
  for (int i = 7; i >= 0; i--) {
    uint8_t byte = 0;
    if (!encoder_read8(file, &byte)) {
      return false;
    }
    *o |= (byte << 8 * i);
  }
  return true;
}

int encoder_write8(FILE *file, uint8_t byte) {
  uint8_t a[] = {byte};
  fwrite(a, 1, 1, file);
  return true;
}

int encoder_write16(FILE *file, uint16_t l) {
  uint8_t a[2];
  for (int i = 1; i >= 0; i--) {
    a[1 - i] = (l >> 8 * i) & 0xFF;
  }
  fwrite(a, 1, 2, file);

  return true;
}
int encoder_write64(FILE *file, uint64_t l) {
  uint8_t a[8];
  for (int i = 7; i >= 0; i--) {
    a[7 - i] = (l >> 8 * i) & 0xFF;
  }
  fwrite(a, 1, 8, file);

  return true;
}

void encoder_compress_to_stream(FILE *input, struct codetable_s *codetable,
                                FILE *output) {
  encoder_write16(output, codetable->registered_chars->size);
  for (int i = 0; i < 256; i++) {
    struct code_s code = codetable->codes[i];
    if (code.length) {
      encoder_write8(output, i);
      encoder_write8(output, code.length);
      encoder_write64(output, code.code);
    }
  }

  encoder_write64(output, codetable->total);

  uint8_t current = 0;
  int pos = 7;

  uint8_t byte;
  while (!feof(input) && !ferror(input)) {
    encoder_read8(input, &byte);
    struct code_s code = codetable->codes[byte];
    for (int i = code.length - 1; i >= 0; i--) {
      if ((code.code & (1 << i)) != 0)
        current |= (1 << pos);
      if (--pos < 0) {
        pos = 8 + pos;
        encoder_write8(output, current);
        current = 0;
      }
    }
  }
  if (pos != 7) {
    encoder_write8(output, current);
  }
}

struct code_node_s {
  uint16_t b;
  struct code_node_s *children[2];
};

void encoder_decompress_to_stream(FILE *input, FILE *output) {
  uint16_t tableSize = 0;
  encoder_read16(input, &tableSize);
  struct code_node_s root = {.b = 0xFFFF};
  for (int c = 0; c < tableSize; c++) {
    struct code_node_s *parent = &root;
    uint8_t byte, length;
    uint64_t code = 0;
    encoder_read8(input, &byte);
    encoder_read8(input, &length);
    encoder_read64(input, &code);
    for (int i = length - 1; i >= 0; i--) {
      int p = (code & (1L << i)) == 0 ? 0 : 1;
      if (!parent->children[p]) {
        parent->children[p] = malloc(sizeof(struct code_node_s));
        *parent->children[p] = (struct code_node_s){.b = 0xFFFF, .children = {0, 0}};
      }
      parent = parent->children[p];
    }
    parent->b = byte;
  }

  uint64_t total;
  encoder_read64(input, &total);

  uint8_t current;
  encoder_read8(input, &current);
  int pos = 7;
  struct code_node_s *parent = &root;
  for (int i = 0; i < total; i++) {
    while (parent && parent->b == 0xFFFF) {
      int p = (current & (1L << pos)) == 0 ? 0 : 1;
      parent = parent->children[p];
      if (--pos < 0) {
        pos = 8 + pos;
        encoder_read8(input, &current);
      }
    }
    if (parent == 0) {
      printf("PEPETS\n");
      return;
    }
    encoder_write8(output, parent->b);
    parent = &root;
  }
}
