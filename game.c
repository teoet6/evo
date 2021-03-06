#include "pishtov.h"

#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#define SIMULATIONS_PER_GENERATION 300
#define NUM_CELLS                  1000
#define NUM_GENES                  4
#define WEIGHT_AMPLITUDE           4.0f
#define MUTATION_RARITY            10000
#define POISON_DEATH_RARITY        1
#define FIELD_X                    128
#define FIELD_Y                    128

typedef enum Neuron_Id Neuron_Id;
enum Neuron_Id {
    INPUT_ALWAYS_ONE,
    INPUT_POS_X,
    INPUT_POS_Y,
    INPUT_TIME,

    OUTPUT_MOVE_X,
    OUTPUT_MOVE_Y,

    INTERNAL_A,

    NUM_NEURONS,
};

typedef enum Program_State Program_State;
enum Program_State {
    POPULATE,
    SIMULATE,
    SELECT_SURVIVORS,
};

typedef struct Gene Gene;
struct Gene {
    uint8_t src;
    uint8_t dst;
    int16_t val;
};

typedef struct Cell Cell;
struct Cell {
    Gene  genome [NUM_GENES];
    float neurons[NUM_NEURONS];
};

typedef struct Petri Petri;
struct Petri {
    Cell cells[NUM_CELLS];
    int field[FIELD_Y][FIELD_X];
};

Program_State program_state;
int program_speed = 1;

int simulation_step;

int generations = 0;

Petri petri;

int is_food_position(int y, int x) {
    return x < FIELD_X / 16;
    return x < FIELD_X / 32 || x >= FIELD_X - FIELD_X / 32;
}

int is_poison_position(int y, int x) {
    return 0;
    if      (simulation_step < SIMULATIONS_PER_GENERATION * 1 / 4) return 0;
    else if (simulation_step < SIMULATIONS_PER_GENERATION * 2 / 4) return x <  FIELD_X / 2;
    else                                                           return 0;
    // else                                                           return x >= FIELD_X / 2;
}


void init() {
    srand(time(0));

    for (int i = 0; i < NUM_CELLS; ++i) {
        for (int j = 0; j < NUM_GENES; ++j) {
            petri.cells[i].genome[j].src = rand() % NUM_NEURONS;
            petri.cells[i].genome[j].dst = rand() % NUM_NEURONS;
            petri.cells[i].genome[j].val = rand() & 0xffff;
        }
    }
}

void populate() {
    generations++;
    printf("Generation: %d\n", generations);

    for (int i = 0; i < FIELD_Y; ++i) {
        for (int j = 0; j < FIELD_X; ++j) {
            petri.field[i][j] = -1;
        }
    }

    for (int i = 0; i < NUM_CELLS; ++i) {
        int y, x;
        do {
            y = rand() % FIELD_Y;
            x = rand() % FIELD_X;
        } while(petri.field[y][x] != -1);
        petri.field[y][x] = i;
    }

    simulation_step = 0;

    program_state = SIMULATE;
}

float gene_val_to_weight(int16_t val) {
    // printf("%f\n", (float)val / (float)0xffff);
    return ((float)val / (float)0xffff) * 2.f * WEIGHT_AMPLITUDE;
}

void simulate() {
    Petri new_petri;

    for (int i = 0; i < NUM_CELLS; ++i) {
        new_petri.cells[i] = petri.cells[i];
    }
    for (int i = 0; i < FIELD_Y; ++i) {
        for(int j = 0; j < FIELD_X; ++j) {
            new_petri.field[i][j] = -1;
        }
    }

    for (int i = 0; i < NUM_CELLS; ++i) {
        for (int j = 0; j < NUM_NEURONS; ++j) {
            new_petri.cells[i].neurons[j] = 0;
        }
    }

    for (int i = 0; i < FIELD_Y; ++i) {
        for (int j = 0; j < FIELD_X; ++j) {
            const int cell_idx = petri.field[i][j];
            if (cell_idx != -1) {
                petri.cells[cell_idx].neurons[INPUT_ALWAYS_ONE] = 1;
                petri.cells[cell_idx].neurons[INPUT_POS_X] = ((float)j / FIELD_X - .5f) * 2.f;
                petri.cells[cell_idx].neurons[INPUT_POS_Y] = ((float)i / FIELD_Y - .5f) * 2.f;
                petri.cells[cell_idx].neurons[INPUT_TIME] = ((float)simulation_step / SIMULATIONS_PER_GENERATION - .5f) * 2.f;
            }
        }
    }

    for (int i = 0; i < NUM_CELLS; ++i) {
        for (int j = 0; j < NUM_GENES; ++j) {
            const uint8_t src = petri.cells[i].genome[j].src;
            const uint8_t dst = petri.cells[i].genome[j].dst;
            const float weight = gene_val_to_weight(petri.cells[i].genome[j].val);

            new_petri.cells[i].neurons[dst] += petri.cells[i].neurons[src] * weight;
        }
    }

    for (int i = 0; i < NUM_CELLS; ++i) {
        for (int j = 0; j < NUM_NEURONS; ++j) {
            if (new_petri.cells[i].neurons[j] >  1.f) new_petri.cells[i].neurons[j] =  1.f;
            if (new_petri.cells[i].neurons[j] < -1.f) new_petri.cells[i].neurons[j] = -1.f;
        }
    }

    for (int i = 0; i < FIELD_Y; ++i) {
        for (int j = 0; j < FIELD_X; ++j) {
            const int cell_idx = petri.field[i][j];
            if (cell_idx != -1) {
                int dx = 0, dy = 0;

                if (new_petri.cells[cell_idx].neurons[OUTPUT_MOVE_X] ==  1) dx += 1;
                if (new_petri.cells[cell_idx].neurons[OUTPUT_MOVE_X] == -1) dx -= 1;
                if (new_petri.cells[cell_idx].neurons[OUTPUT_MOVE_Y] ==  1) dy += 1;
                if (new_petri.cells[cell_idx].neurons[OUTPUT_MOVE_Y] == -1) dy -= 1;

                if (i + dy < 0 || i + dy >= FIELD_Y) dy = 0;
                if (j + dx < 0 || j + dx >= FIELD_X) dx = 0;

                if (    petri.field[i+dy][j+dx] != -1) dy = dx = 0;
                if (new_petri.field[i+dy][j+dx] != -1) dy = dx = 0;

                if (is_poison_position(i + dy, j + dx) && rand() % POISON_DEATH_RARITY == 0)
                    new_petri.field[i+dy][j+dx] = -1;
                else
                    new_petri.field[i+dy][j+dx] = cell_idx;
            }
        }
    }

    petri = new_petri;

    simulation_step++;
    if (simulation_step == SIMULATIONS_PER_GENERATION) {
        program_state = SELECT_SURVIVORS;
    }
}

void maybe_mutate(Gene *g) {
    for (int i = 0; i < 8; ++i) {
        if (rand() % MUTATION_RARITY == 0) {
            g->src ^= 1 << i;
            g->src %= NUM_NEURONS;
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (rand() % MUTATION_RARITY == 0) {
            g->dst ^= 1 << i;
            g->dst %= NUM_NEURONS;
        }
    }
    for (int i = 0; i < 16; ++i) {
        if (rand() % MUTATION_RARITY == 0) {
            g->val ^= 1 << i;
            g->val &= 0xffff;
        }
    }
}

void select_survivors() {
    Petri new_petri;
    for (int i = 0; i < FIELD_Y; ++i) {
        for(int j = 0; j < FIELD_X; ++j) {
            new_petri.field[i][j] = -1;
        }
    }

    int num_survive = 0;

    for (int i = 0; i < FIELD_Y; ++i) {
        for (int j = 0; j < FIELD_X; ++j) {
            if (!is_food_position(i, j)) continue;
            const int cell_idx = petri.field[i][j];
            if (cell_idx != -1) {
                new_petri.cells[num_survive++] = petri.cells[cell_idx];
            }
        }
    }

    printf("Survivors %d%% (%d/%d)\n", num_survive * 100 / NUM_CELLS, num_survive, NUM_CELLS);

    for (int i = num_survive; i < NUM_CELLS; ++i) {
        new_petri.cells[i] = new_petri.cells[rand() % num_survive];
    }

    for (int i = 0; i < NUM_CELLS; ++i) {
        for (int j = 0; j < NUM_GENES; ++j) {
            maybe_mutate(&new_petri.cells[i].genome[j]);
        }
    }

    petri = new_petri;

    program_state = POPULATE;
}

void update() {
    for (int i = 0; i < program_speed; ++i) {
        switch (program_state) {
        case POPULATE:           populate(); break;
        case SIMULATE:           simulate(); break;
        case SELECT_SURVIVORS:   select_survivors();   break;
        }
    }
}

uint8_t get_genome_hue(Gene genome[NUM_GENES]) {
    int hue = 0;
    for (int i = 0; i < NUM_GENES; ++i) {
        hue ^= genome[i].src;
        hue ^= genome[i].dst;
        hue ^= genome[i].val >> 8;
    }
    return hue;
}

uint32_t hue_to_rgb(uint8_t hue) {
    uint8_t x = (hue % 42) * 6;

    uint8_t r, g, b;

    if      (hue >= 0   && hue <  42) r = 0xff, g = x,    b = 0;
    else if (hue >= 42  && hue <  84) r = x,    g = 0xff, b = 0;
    else if (hue >= 84  && hue < 126) r = 0,    g = 0xff, b = x;
    else if (hue >= 126 && hue < 168) r = 0,    g = x,    b = 0xff;
    else if (hue >= 168 && hue < 210) r = x,    g = 0,    b = 0xff;
    else                              r = 0xff, g = 0,    b = x;

    return r << 16 | g << 8 | b;
}

void fill_color_hex(uint32_t hex) {
    int a = hex >> 24 & 0xff;
    int r = hex >> 16 & 0xff;
    int g = hex >>  8 & 0xff;
    int b = hex       & 0xff;

    fill_color[0] = (float)r / 255;
    fill_color[1] = (float)g / 255;
    fill_color[2] = (float)b / 255;
    fill_color[3] = 1 - (float)a / 255;
}

void draw() {
    const float cell_w = fmin(window_w / FIELD_X, window_h / FIELD_Y);

    fill_color_hex(0x808080);
    fill_rect(0, 0, window_w, window_h);

    fill_color_hex(0xffffff);
    fill_rect(0, 0, FIELD_X * cell_w, FIELD_Y * cell_w);

    for (int i = 0; i < FIELD_Y; ++i) {
        for (int j = 0; j < FIELD_X; ++j) {
            if (is_food_position(i, j)) {
                fill_color_hex(0xe0ffe0);
                fill_rect(j * cell_w, i * cell_w, cell_w, cell_w);
            }
            if (is_poison_position(i, j)) {
                fill_color_hex(0xffe0e0);
                fill_rect(j * cell_w, i * cell_w, cell_w, cell_w);
            }
            if (petri.field[i][j] != -1) {
                const uint32_t hue = get_genome_hue(petri.cells[petri.field[i][j]].genome);
                const uint32_t rgb = hue_to_rgb(hue);
                fill_color_hex(rgb);
                fill_circle(j * cell_w + cell_w / 2, i * cell_w + cell_w / 2, cell_w / 2);
            }
        }
    }
}

void keydown(int key) {
    printf("Keydown %d\n", key);
    if (key == 38 && program_speed < 256) {
        if (program_speed) program_speed *= 2;
        else               program_speed  = 1;
        printf("Program speed %dx\n", program_speed);
    }
    if (key == 40 && program_speed) {
        program_speed /= 2;
        printf("Program speed %dx\n", program_speed);
    }
}

void keyup(int key) {
}

void mousedown(int button) {
}

void mouseup(int button) {
    const float cell_w = fmin(window_w / FIELD_X, window_h / FIELD_Y);
    const int y = mouse_y / cell_w;
    const int x = mouse_x / cell_w;

    printf("%d %d\n", y, x);

    if (y < 0 || y >= FIELD_Y || x < 0 || x >= FIELD_X) return;

    const int cell_idx = petri.field[y][x];
    if (cell_idx == -1) return;

#define PRINTE(E) printf("%d %s\n", E, #E)
    PRINTE(INPUT_ALWAYS_ONE);
    PRINTE(INPUT_POS_X);
    PRINTE(INPUT_POS_Y);
    PRINTE(INPUT_TIME);
    PRINTE(OUTPUT_MOVE_X);
    PRINTE(OUTPUT_MOVE_Y);
    PRINTE(INTERNAL_A);
#undef PRINTE

    for (int i = 0; i < NUM_GENES; ++i) {
        const Gene gene = petri.cells[cell_idx].genome[i];
        printf("%02d -> %02d %.2f\n", gene.src, gene.dst, gene_val_to_weight(gene.val));
    }

    printf("\n");
}
