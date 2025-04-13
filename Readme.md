# Random Function Generator using ASTs

Generates random function based on the provided grammar and produces
respective fragment shader code to render the output of the function
for each pixel coordinate. @tsoding

### Run

- Generate random image into a file (depth is optional)

```bash
cd src
./nob run -depth <depth>
```

- Generate random shader code and render it into a gui using raylib.
  Both depth and grammar are optional, and order does not matter. If not specified, uses default values.

```bash
cd src
./nob gui -grammar <path> -depth <depth>
```
