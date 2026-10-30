#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BUFFER (64 * 1024)
#define BYTES_PER_LINE 16
#define PROGRAM_NAME "hexdumb"
#define VERSION "1.0.0"

/* MODE_COUNT it's always the last element, will be the total modes count that
 * sets the size of flags[]. */
typedef enum { MODE_DEFAULT, MODE_CANONICAL, MODE_COUNT } Mode;

typedef void FlagsAction(void);
typedef void LineRender(const unsigned char[], size_t, size_t);

static FlagsAction show_help, show_version;
static LineRender two_bytes, canonical;

typedef struct {
    const char short_flag;
    const char *description;
    FlagsAction *action;
    Mode mode;
} Flag;

typedef struct {
    char **filesv;
    size_t filesc;
} FileList;

typedef struct {
    short addr_width;
    LineRender *renderfn;
} DumpFmt;

static const Flag flags[] = {
    {'C', "Canonical hex+ASCII display.", NULL, MODE_CANONICAL},
    {'h', "Display this help and exit.", show_help, MODE_DEFAULT},
    {'V', "Output version information and exit.", show_version, MODE_DEFAULT},
    {'\0', NULL, NULL, 0}
};

static const DumpFmt formats[MODE_COUNT] = {
    [MODE_DEFAULT] = {7, two_bytes},
    [MODE_CANONICAL] = {8, canonical},
};

static const Flag *find_flag(const char c) {
    for (size_t i = 0; flags[i].short_flag != '\0'; i++) {
        if (flags[i].short_flag == c) {
            return &flags[i];
        }
    }
    return NULL;
}

static void show_version(void) {
    printf("%s v%s\n", PROGRAM_NAME, VERSION);
    exit(EXIT_SUCCESS);
}

static void invalid(const char *msg, const char f) {
    fprintf(stderr, "%s -%c\n", msg, f);
    exit(EXIT_FAILURE);
}

static void show_help(void) {
    printf("Usage: %s [options]... <file>...\n", PROGRAM_NAME);
    printf("Available options:\n");

    for (size_t i = 0; flags[i].short_flag != '\0'; i++) {
        printf("  -%c    %s\n", flags[i].short_flag, flags[i].description);
    }
    exit(EXIT_SUCCESS);
}
static void parse_short(const char *arg, Mode *mode) {
    for (arg++; *arg; arg++) {
        const Flag *flag = find_flag(*arg);
        if (flag == NULL)
            invalid("unknown flag", *arg);
        if (flag->action)
            flag->action();
        else
            *mode = flag->mode;
    }
}
static void parse_args(int argc, char **argv, FileList *fl, Mode *mode) {
    /* Reuse argv as the file list; non-option arguments are compacted in place.
     */
    fl->filesv = argv;
    fl->filesc = 0;
    int flags_end = 0;
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (!flags_end && arg[0] == '-' && arg[1] != '\0') {
            if (arg[1] == '-' && arg[2] == '\0') {
                flags_end = 1;
                continue;
            }
            parse_short(arg, mode);
        } else
            fl->filesv[fl->filesc++] = arg;
    }
}
static void two_bytes(const unsigned char buf[], size_t size, size_t row) {
    putchar(' ');
    for (size_t j = row; j < row + BYTES_PER_LINE; j += 2) {
        if (j < size) {
            uint16_t word;
            if (j + 1 < size)
                memcpy(&word, &buf[j], sizeof word);
            // word = (uint16_t)buf[j] | ((uint16_t)buf[j + 1] << 8);
            else
                word = buf[j];
            printf("%04x ", word);
        } else
            printf("     ");
    }
}

static void canonical(const unsigned char buf[], size_t size, size_t row) {
    putchar(' ');
    for (size_t j = row; j < row + BYTES_PER_LINE; j++) {
        if (j % 8 == 0)
            putchar(' ');
        if (j < size)
            printf("%02x ", buf[j]);
        else
            printf("   ");
    }
    printf(" |");
    for (size_t j = row; j < row + BYTES_PER_LINE && j < size; j++) {
        const unsigned char c = buf[j];
        putchar((c >= 0x20 && c <= 0x7e) ? c : '.');
    }
    putchar('|');
}

static void dump(const unsigned char buf[], size_t size, size_t offset,
                 const DumpFmt *fmt) {
    for (size_t row = 0; row < size; row += BYTES_PER_LINE) {
        printf("%0*zx", fmt->addr_width, row + offset);
        fmt->renderfn(buf, size, row);
        putchar('\n');
    }
}
static int hexdump(FILE *fp, Mode mode) {
    unsigned char buffer[MAX_BUFFER];
    const DumpFmt *fmt = &formats[mode];
    size_t size, offset = 0;
    int status = EXIT_SUCCESS;
    while ((size = fread(buffer, 1, sizeof buffer, fp)) > 0) {
        dump(buffer, size, offset, fmt);
        offset += size;
    }
    if (ferror(fp)) {
        fprintf(stderr, "%s: error while reading the file\n", PROGRAM_NAME);
        status = EXIT_FAILURE;
    }

    if (offset > 0)
        printf("%0*zx\n", fmt->addr_width, offset);
    return status;
}
int main(int argc, char **argv) {
    static_assert(MAX_BUFFER % BYTES_PER_LINE == 0,
                  "MAX_BUFFER must be a BYTES_PER_LINE multiple");
    int status = EXIT_SUCCESS;
    Mode mode = MODE_DEFAULT;
    FileList fl;
    parse_args(argc, argv, &fl, &mode);
    if (fl.filesc == 0)
        status = hexdump(stdin, mode);
    else
        for (size_t i = 0; i < fl.filesc; i++) {
            const char *current_file = fl.filesv[i];
            if (strcmp(current_file, "-") == 0) {
                if (hexdump(stdin, mode) != EXIT_SUCCESS)
                    status = EXIT_FAILURE;
                continue;
            }
            FILE *fp = fopen(current_file, "rb");
            if (fp == NULL) {
                fprintf(stderr, "%s %s: ", PROGRAM_NAME, current_file);
                perror("");
                status = EXIT_FAILURE;
                continue;
            }
            if (fl.filesc > 1)
                printf("==> %s <==\n", current_file);
            if (hexdump(fp, mode) != EXIT_SUCCESS)
                status = EXIT_FAILURE;
            fclose(fp);
        }

    return status;
}
