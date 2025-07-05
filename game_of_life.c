#include <stdio.h>
#include <unistd.h>

#define ROWS 50
#define COLS 50
#define CELLS (ROWS*COLS)
#define ALIVE '*'
#define DEAD ' '

/* Translate the x,y grip point into the inex in the linar array.
 * It implements wrapping so both positive and negative x,y coordinates will work*/
int cell_to_index(int x, int y) {
  if (x>=COLS) x %=COLS;
  if (y>=COLS) y %=ROWS;

  if (x<0) {
    x = (-x) % COLS;
    x = COLS - x;
  };
  if (y<0) {
    y = (-y) % ROWS;
    y = ROWS - y;
  };
  return y*ROWS+x;
}

/* The function sets the specified cell at x,y to the specified state. */
void set_cell(char *grid, int x, int y, char state) {
  grid[cell_to_index(x,y)] = state;
}

/* The function returns the state at x,y. */
char get_cell(char *grid, int x, int y) {
  return grid[cell_to_index(x, y)];
}

/* Print the grid on the screen, clearing the terminal using the required VT100 escape sequence. */
void print_grid(char *grid) {
  printf("\x1b[H\x1b[J");
  for (int y=-1; y<=ROWS; y++) {
    printf("|");
    for (int x=0; x<COLS; x++) {
      if (y==-1 | y==ROWS) {
        printf("-");
      } else {
        printf("%c", get_cell(grid, x,y));
      }
    }
    printf("|");
    printf("\n");
  }
}

/* Set all the grid cells to the specified state.*/
void set_grid(char *grid, char state) {
  for (int y=0; y<ROWS; y++) {
    for (int x=0; x<COLS; x++) {
      set_cell(grid, x, y, state);
    }
  }
}

/* Return the number of living cells neigbors of x,y. */
int count_living_neighbors(char *grid, int x, int y) {
  int alive = 0;
  for (int yo=-1; yo<=1; yo++) {
    for (int xo=-1; xo<=1; xo++) {
      if (xo == 0 && yo == 0) continue;
      if(get_cell(grid,x+xo,y+yo) == ALIVE) alive++;
    }
  }
  return alive;
}

/* Compute the new state of game of life accoring to its rules*/
void compute_new_state(char *old, char *new) {
  for (int y=0; y<ROWS; y++) {
    for (int x=0; x<COLS; x++) {
      int n_alive = count_living_neighbors(old,x,y);
      int new_state = DEAD;
      if (get_cell(old,x,y) == ALIVE) {
        if (n_alive == 2 || n_alive == 3) new_state = ALIVE;
      } else {
          if (n_alive == 3) new_state =ALIVE;
      }
      set_cell(new,x,y,new_state);
    }
  }
}

int main() {
    char old_grid[CELLS];
    char new_grid[CELLS];
    set_grid(old_grid, DEAD);

    // Gosper Glider Gun (top-left corner, around 5x1)
    int gun[][2] = {
        {5,1},{5,2},{6,1},{6,2},
        {5,11},{6,11},{7,11},
        {4,12},{8,12},
        {3,13},{9,13},{3,14},{9,14},
        {6,15},
        {4,16},{8,16},
        {5,17},{6,17},{7,17},
        {6,18},
        {3,21},{4,21},{5,21},
        {3,22},{4,22},{5,22},
        {2,23},{6,23},
        {1,25},{2,25},{6,25},{7,25},
        {3,35},{4,35},{3,36},{4,36}
    };
    for (int i = 0; i < sizeof(gun)/sizeof(gun[0]); i++)
        set_cell(old_grid, gun[i][0], gun[i][1], ALIVE);

    // Glider (top-right)
    set_cell(old_grid, 1, 70, ALIVE);
    set_cell(old_grid, 2, 71, ALIVE);
    set_cell(old_grid, 3, 69, ALIVE);
    set_cell(old_grid, 3, 70, ALIVE);
    set_cell(old_grid, 3, 71, ALIVE);

    // Pulsar (center)
    int pulsar[][2] = {
        {10, 30}, {10, 31}, {10, 32}, {10, 36}, {10, 37}, {10, 38},
        {12, 30}, {12, 31}, {12, 32}, {12, 36}, {12, 37}, {12, 38},
        {14, 30}, {14, 31}, {14, 32}, {14, 36}, {14, 37}, {14, 38},
        {11, 28}, {12, 28}, {13, 28}, {11, 33}, {12, 33}, {13, 33},
        {11, 35}, {12, 35}, {13, 35}, {11, 40}, {12, 40}, {13, 40}
    };
    for (int i = 0; i < sizeof(pulsar)/sizeof(pulsar[0]); i++)
        set_cell(old_grid, pulsar[i][0], pulsar[i][1], ALIVE);

    // Lightweight spaceship (bottom left)
    int lwss[][2] = {
        {20, 1}, {20, 4},
        {21, 0}, {22, 0},
        {23, 0}, {23, 4},
        {24, 0}, {24, 1}, {24, 2}, {24, 3}
    };
    for (int i = 0; i < sizeof(lwss)/sizeof(lwss[0]); i++)
        set_cell(old_grid, lwss[i][0], lwss[i][1], ALIVE);

    // Main loop
    while (1) {
        compute_new_state(old_grid, new_grid);
        print_grid(new_grid);
        usleep(100000);
        compute_new_state(new_grid, old_grid);
        print_grid(old_grid);
        usleep(100000);
    }
    return 0;
}
