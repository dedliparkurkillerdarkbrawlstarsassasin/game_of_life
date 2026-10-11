#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>

#define FIELD_WIDTH 80
#define FIELD_HEIGHT 25
#define ALIVE_CHAR '1'
#define INITIAL_DELAY 200
#define MIN_DELAY 20
#define MAX_DELAY 1000
#define DELAY_STEP 20

typedef struct s_field {
    int width;
    int height;
    char *cells;
} t_field;

int wrap(int value, int limit) {
    value %= limit;
    if (value < 0) value += limit;
    return (value);
}

t_field *field_create(int width, int height) {
    char *cells;
    t_field *field;

    cells = calloc((size_t)width * (size_t)height, sizeof(char));
    if (!cells) return (NULL);
    field = malloc(sizeof(t_field));
    if (!field) {
        free(cells);
        return (NULL);
    }
    field->width = width;
    field->height = height;
    field->cells = cells;
    return (field);
}

void field_destroy(t_field *field) {
    if (!field) return;
    free(field->cells);
    free(field);
}

int field_get(const t_field *field, int row, int col) {
    row = wrap(row, field->height);
    col = wrap(col, field->width);
    return (field->cells[row * field->width + col]);
}

void field_set(t_field *field, int row, int col, int value) {
    field->cells[row * field->width + col] = (char)value;
}

void discard_line_remainder(int has_newline) {
    int ch;
    if (has_newline) return;
    ch = fgetc(stdin);
    while (ch != '\n' && ch != EOF) ch = fgetc(stdin);
}

int field_read_stdin(t_field *field) {
    char line[FIELD_WIDTH + 2];
    int row = 0;

    while (row < field->height && fgets(line, sizeof(line), stdin)) {
        int col = 0;
        int has_newline = 0;

        while (col < field->width && line[col] && line[col] != '\n' && line[col] != '\r') {
            field_set(field, row, col, line[col] == ALIVE_CHAR ? 1 : 0);
            col++;
        }

        if (line[col] == '\n' || line[col] == '\r') has_newline = 1;

        discard_line_remainder(has_newline);

        if (col != field->width || !has_newline) return 0;

        row++;
    }

    if (row != field->height) return 0;

    return 1;
}

int count_neighbors(const t_field *field, int row, int col) {
    static const int offsets[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};
    int count;
    int i;

    count = 0;
    i = 0;
    while (i < 8) {
        count += field_get(field, row + offsets[i][0], col + offsets[i][1]);
        i++;
    }
    return (count);
}

void field_next_generation(const t_field *current, t_field *next) {
    int row;
    int neighbors;
    int alive;

    row = 0;
    while (row < current->height) {
        int col = 0;
        while (col < current->width) {
            neighbors = count_neighbors(current, row, col);
            alive = field_get(current, row, col);
            if (alive && (neighbors == 2 || neighbors == 3))
                field_set(next, row, col, 1);
            else if (!alive && neighbors == 3)
                field_set(next, row, col, 1);
            else
                field_set(next, row, col, 0);
            col++;
        }
        row++;
    }
}

void draw_field(const t_field *field, int generation, int delay) {
    int row;

    erase();
    row = 0;
    while (row < field->height) {
        int col = 0;
        while (col < field->width) {
            if (field_get(field, row, col))
                mvaddch(row, col, 'O');
            else
                mvaddch(row, col, '.');
            col++;
        }
        row++;
    }
    mvprintw(field->height, 0, "Gen: %-6d  Speed: %4dms  [A] faster  [Z] slower  [Space] quit", generation,
             delay);
    refresh();
}

void handle_input(int ch, int *delay, int *running) {
    if (ch == ' ')
        *running = 0;
    else if (ch == 'a' || ch == 'A') {
        *delay -= DELAY_STEP;
        if (*delay < MIN_DELAY) *delay = MIN_DELAY;
    } else if (ch == 'z' || ch == 'Z') {
        *delay += DELAY_STEP;
        if (*delay > MAX_DELAY) *delay = MAX_DELAY;
    }
}

void init_screen(void) {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
}

void cleanup_screen(void) { endwin(); }

int screen_too_small(void) {
    int rows;
    int cols;

    getmaxyx(stdscr, rows, cols);
    return (rows < FIELD_HEIGHT + 1 || cols < FIELD_WIDTH);
}

int init_fields(t_field **current, t_field **next) {
    *current = field_create(FIELD_WIDTH, FIELD_HEIGHT);
    *next = field_create(FIELD_WIDTH, FIELD_HEIGHT);
    if (!*current || !*next) {
        field_destroy(*current);
        field_destroy(*next);
        return (0);
    }
    return (1);
}

void run_game(t_field *current, t_field *next) {
    int delay;
    int running;
    int generation;
    t_field *tmp;

    delay = INITIAL_DELAY;
    running = 1;
    generation = 0;
    while (running) {
        draw_field(current, generation, delay);
        timeout(delay);
        handle_input(getch(), &delay, &running);
        if (running) {
            field_next_generation(current, next);
            tmp = current;
            current = next;
            next = tmp;
            generation++;
        }
    }
}

int main(void) {
    t_field *current;
    t_field *next;

    if (!init_fields(&current, &next)) {
        fprintf(stderr, "game_of_life: allocation failed\n");
        return (1);
    }

    if (!field_read_stdin(current)) {
        fprintf(stderr,
                "game_of_life: input must be exactly %dx%d "
                "with each line terminated by newline\n",
                FIELD_WIDTH, FIELD_HEIGHT);
        field_destroy(current);
        field_destroy(next);
        return (1);
    }

    if (!freopen("/dev/tty", "r", stdin)) {
        field_destroy(current);
        field_destroy(next);
        fprintf(stderr, "game_of_life: cannot reopen /dev/tty for input\n");
        return (1);
    }

    init_screen();
    if (screen_too_small()) {
        cleanup_screen();
        field_destroy(current);
        field_destroy(next);
        fprintf(stderr, "game_of_life: terminal too small (need %dx%d)\n", FIELD_WIDTH, FIELD_HEIGHT + 1);
        return (1);
    }

    run_game(current, next);
    cleanup_screen();
    field_destroy(current);
    field_destroy(next);
    return (0);
}