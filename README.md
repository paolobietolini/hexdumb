# hexdumb

My from-scratch version of `hexdump`, written while learning C. Yes, the name is on purpose.

The repo holds two versions of the same program. The first one works. The second one is 23 times faster. Two posts on my blog explain the design choices and what changed between them:

- [Writing hexdump from scratch](https://paolobietolini.com/posts/writing-hexdump-from-scratch/)
- [Making hexdump 23 times faster (and what I've learned)](https://paolobietolini.com/posts/making-hexdump-faster/)

## What's in here

| File | Binary | What it is |
|---|---|---|
| `hexdumb.c` | `hexdumb` | v1: the straightforward version |
| `hexdumb_buffered.c` | `hexbuff` | v2: buffered output, 23× faster than v1 |

## Build

You need a C compiler (gcc) and `make`.

```sh
make            # builds ./hexdumb and ./hexbuff
make clean      # removes them
```

## Usage

```sh
./hexbuff -C file.bin          # canonical hex + ASCII
./hexbuff file.bin             # default: 16-bit words
cat file.bin | ./hexbuff -C    # reads stdin when no file is given
./hexbuff -C a.bin - b.bin     # "-" means stdin
```

| Flag | Meaning |
|---|---|
| `-C` | Canonical hex + ASCII display |
| `-h` | Show help and exit |
| `-V` | Show version and exit |

```
$ ./hexbuff -C sample.txt
00000000  48 65 6c 6c 6f 2c 20 68  65 78 64 75 6d 70 21 0a  |Hello, hexdump!.|
00000010  41 41 41 41 41 41 41 41  41 41 41 41 41 41 41 41  |AAAAAAAAAAAAAAAA|
00000020  0a                                                |.|
00000021
```

## How fast

Measured with [`hyperfine`](https://github.com/sharkdp/hyperfine) on a 50 MiB file, output sent to `/dev/null`:

| Command | Mean time |
|---|---|
| `./hexbuff -C` (v2) | 75.7 ms |
| `xxd` | 759.5 ms |
| `./hexdumb -C` (v1) | 1.754 s |
| `hexdump -C` | 4.138 s |
| `hexdump -Cv` | 4.224 s |

The bit twiddling was the small part. Most of the win was 128 million stdio calls becoming 800. The [second post](https://paolobietolini.com/posts/making-hexdump-faster/) walks through it.

## Differences from util-linux `hexdump`

- Repeated lines are not collapsed into `*`, so `-C` output matches `hexdump -Cv`, not `hexdump -C`.
- Only `-C` and the default 16-bit word view exist. No `-n`, `-s` or custom format strings.
- In the default view, the padding on the last line can differ slightly.

This is a learning project, not a drop-in replacement.

## License

[MIT](LICENSE)

---

Paolo Bietolini · [paolobietolini.com](https://paolobietolini.com)
