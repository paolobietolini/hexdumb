#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define MAX_BUFFER (64 * 1024)
#define BYTES_PER_LINE 16
#define PROGRAM_NAME "hexdumb"
#define VERSION "1.0.0"

/* ADDRESS(8) + (ASCII_LEN + BYTES * 2) + SPACES + SEPARATORS + TERMINATOR and
 * NL*/
#define MAX_LINE ((8) + (3 * BYTES_PER_LINE) + (BYTES_PER_LINE + 4) + (2) + (2))

/* MODE_COUNT it's always the last element, will be the total modes count that
 * sets the size of flags[]. */
typedef enum { MODE_DEFAULT, MODE_CANONICAL, MODE_COUNT } Mode;

typedef void FlagsAction(void);
typedef size_t LineRender(unsigned char dst[], size_t dst_size,
                          const unsigned char buf[], size_t len, size_t addr);
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
    LineRender *renderfn;
} DumpFmt;

static const Flag flags[] = {
    {'C', "Canonical hex+ASCII display.", NULL, MODE_CANONICAL},
    {'h', "Display this help and exit.", show_help, MODE_DEFAULT},
    {'V', "Output version information and exit.", show_version, MODE_DEFAULT},
    {'\0', NULL, NULL, 0}};

static const DumpFmt formats[MODE_COUNT] = {
    [MODE_DEFAULT] = {two_bytes},
    [MODE_CANONICAL] = {canonical},
};

static const char hex[] = "0123456789abcdef";

static size_t min(size_t a, size_t b) {
    return (a < b) ? a : b;
}

static void write_offset(unsigned char line[], size_t addr_width,
                         size_t offset) {
    for (ssize_t i = addr_width - 1; i >= 0; i--) {
        line[i] = hex[offset & 0x0f];
        offset >>= 4;
    }
}

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
static void write_bytes(unsigned char *dst, const unsigned char c) {
    dst[0] = hex[c >> 4];
    dst[1] = hex[c & 0x0f];
}

static size_t two_bytes(unsigned char dst[], size_t dst_size,
                        const unsigned char buf[], size_t len, size_t addr) {

    const size_t addr_width = 7;
    memset(dst, ' ', dst_size);
    size_t pos = addr_width + 1;
    write_offset(dst, addr_width, addr);
    for (size_t i = 0; i < len; i += 2) {
        /* Full compatibility with hexdump */
        //uint16_t word = 0;
        // memcpy(&word, buf + i, min(2, len - i));
        // write_bytes(dst + pos,  word >> 8);
        // write_bytes(dst + pos + 2, word & 0xff);
        write_bytes(dst + pos + 2, buf[i]);
        if (i + 1 < len)
            write_bytes(dst + pos, buf[i + 1]);
        else
            write_bytes(dst + pos, 0);
        pos += 5;
    }
    dst[pos - 1] = '\n';
    return pos;
}
static size_t canonical(unsigned char dst[], size_t dst_size,
                        const unsigned char buf[], size_t len, size_t addr) {
    const size_t addr_width = 8;
    memset(dst, ' ', dst_size);

    size_t hex_start = addr_width + 2;
    size_t ascii_pos = hex_start + (3 * BYTES_PER_LINE) + 2;

    write_offset(dst, addr_width, addr);
    if (len == 0) {
        dst[addr_width] = '\n';
        return addr_width + 1;
    }

    dst[ascii_pos++] = '|';
    for (size_t i = 0; i < BYTES_PER_LINE; i++) {
        size_t pos = hex_start + i * 3 + (i / 8);

        if (i < len) {
            write_bytes(dst + pos, buf[i]);
            dst[ascii_pos++] =
                (buf[i] >= 0x20 && buf[i] <= 0x7e) ? buf[i] : '.';
        }
    }

    dst[ascii_pos++] = '|';
    dst[ascii_pos++] = '\n';
    return ascii_pos;
}
static size_t dump(unsigned char out[], const size_t dst_size,
                   const DumpFmt *fmt, const unsigned char buf[], size_t size,
                   size_t offset) {
    size_t written = 0;
    for (size_t row = 0; row < size; row += BYTES_PER_LINE) {
        size_t line_size = min(BYTES_PER_LINE, size - row);
        written +=
            fmt->renderfn(out + written, min(MAX_LINE, dst_size - written),
                          buf + row, line_size, offset + row);
    }
    return written;
}
static int hexdump(FILE *fp, Mode mode) {
    unsigned char buffer[MAX_BUFFER];
    static unsigned char out[MAX_LINE * (MAX_BUFFER / BYTES_PER_LINE)];
    const DumpFmt *fmt = &formats[mode];
    size_t size = 0, offset = 0;
    int status = EXIT_SUCCESS;
    while ((size = fread(buffer, 1, sizeof buffer, fp)) > 0) {
        size_t len = dump(out, sizeof out, fmt, buffer, size, offset);
        fwrite(out, 1, len, stdout);
        offset += size;
    }
    if (ferror(fp)) {
        fprintf(stderr, "%s: error while reading the file\n", PROGRAM_NAME);
        status = EXIT_FAILURE;
    }
    if (offset > 0) {
        size_t len = fmt->renderfn(out, sizeof out, NULL, 0, offset);
        fwrite(out, 1, len, stdout);
    }

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
