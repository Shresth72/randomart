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

### Grammar

- `E`, `A`, `C` define each node
- `|` defines the probability of that operation
- Operations supported:
  - Unary operations: `sqrt()`, `abs()`, `sin()`
  - Binary operations: `add()`, `mult()`, `gt()`, `mod()`
  - Triple operations: `vec3()`, `if()`

```bash
# Entry Node
E | vec3(C, C, C)
  ;

# Terminal Nodes
A | random
  | x
  | y
  | t
  ;

# Binary Operations
C ||  A
  ||| add(C, C)
  ||| mult(C, C)
  ;
```

### Some beautiful examples

```c
#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform float time;
vec4 map_color(vec3 rgb) {
  return vec4((rgb + 1)/2.0, 1.0);
}
void main() {
  float x = fragTexCoord.x * 2.0 - 1.0;
  float y = fragTexCoord.y * 2.0 - 1.0;
  float t = sin(time);
  finalColor = map_color(vec3(((sqrt(abs((sqrt(abs(((t*x)*y)))*((sqrt(abs(x))+(t+t))*((t+y)+sqrt(abs(t)))))))*(-0.730462+(((sqrt(abs(y))+(t*t))+sqrt(abs(0.998753)))*((sqrt(abs(t))+sqrt(abs(-0.572576)))+((t+t)+y)))))*(sqrt(abs(((t+(sqrt(abs(x))+sqrt(abs(0.499940))))*t)))+sqrt(abs(((sqrt(abs(-0.237329))*sqrt(abs((y+y))))+((x+(t*x))+0.056313)))))),(t*sqrt(abs((-0.828350*((((t*x)*sqrt(abs(t)))*((-0.465111*t)+y))+(sqrt(abs(sqrt(abs(t))))+sqrt(abs(t)))))))),((t*(sqrt(abs((((x*0.235313)+sqrt(abs(y)))+(t+x))))+-0.055516))*x)));
}
```
