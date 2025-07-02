#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include <assert.h>

#define ROW_SIZE 60
#define LENGHT_SIZE 100

char cell_image[2] = {
  [0] = ' ',
  [1] = '*'
};

typedef enum {
  O = 0,
  I = 1
}Cell;

Cell patterns[8] = {
  [0b000] = O,
  [0b001] = I,
  [0b010] = I,
  [0b011] = I,
  [0b100] = O,
  [0b101] = I,
  [0b110] = I,
  [0b111] = O,
};

typedef struct {
  Cell cells[ROW_SIZE];
}Row;

Row next_row(Row prev) {
  Row next = {};

  for (int i = 1; i<ROW_SIZE-1; ++i) {
    int pattern_index = prev.cells[i-1]<<2 | prev.cells[i]<<1 | prev.cells[i+1];
    next.cells[i] = patterns[pattern_index];
  }

  return next;
}
 
void print_row(Row row) {
  putc('|', stdout);
  for (int i=0; i< ROW_SIZE; ++i) {
    putc(cell_image[row.cells[i]], stdout);
  }
  putc('|', stdout);
  putc('\n', stdout);
}

void line(void) {
  for (int j=0; j<ROW_SIZE + 2; j++) { 
    putc('-',stdout);
  }
  putc('\n',stdout);
}

Row random_row(void) {
  Row result = {};

  for (int i=0; i< ROW_SIZE; ++i) {
    result.cells[i] = rand() % 2;
  }
  return result;
}

int main() {
  srand(time(0));
  Row first = random_row();
  line();
  for (int j=0; j<LENGHT_SIZE; j++) { 
    print_row(first);
    first = next_row(first);
  }
  line();
  return 0;
}
