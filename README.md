# ncalc - Terminal Scientific Calculator & CAS

**ncalc** is a powerful terminal-based scientific calculator and Computer Algebra System (CAS). Built with C++17, ncursesw, and GiNaC, it offers symbolic computation, high-precision arithmetic, and terminal-native graphing capabilities with high-resolution Braille patterns.

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

## Features

*   **Symbolic Computation:** Powered by GiNaC, ncalc can handle:
    *   **Algebraic Solving:** Solves linear and quadratic equations (e.g., `find y, y^2 = 4x`).
    *   **Differentiation:** Standard symbolic derivatives (e.g., `diff(sin(x), x)`).
    *   **Partial Derivatives:** Evaluate at specific points (e.g., `df/dx(1,0) where f(x,y) = x^2 + y^2`).
    *   **Definite Integrals:** Numeric results using Simpson's Rule (e.g., `i(2,0), x^2 dx`).
*   **Graphing:** High-resolution rendering using Unicode Braille patterns (2x4 sub-pixel grid per character). Includes automatic X and Y axes.
*   **Interactive Interface:** Keyboard-centric workflow with history navigation and UTF-8 wide-character support.

## Installation

### Prerequisites
*   Linux/Unix environment.
*   C++17 compliant compiler (g++).
*   Dependencies: `libncursesw5-dev`, `libginac-dev`, `libcln-dev`.

### Installing on Debian/Ubuntu
```bash
sudo apt update
sudo apt install build-essential libncursesw5-dev libginac-dev libcln-dev
```

### Compiling
1.  Run `make` to build the binary.
    ```bash
    make
    ```
2.  (Optional) Install system-wide:
    ```bash
    sudo make install
    ```

## Usage

### Advanced Commands
*   **Solve Equations:** `find x, 2x = 10` -> `x = 5`.
*   **Non-Linear Solving:** `find y, y^2 = 4x` -> `y = {2*sqrt(x), -2*sqrt(x)}`.
*   **Partial Derivatives:** `df/dx(1,0) where f(x,y) = x^2 + y^2` -> `2`.
*   **Definite Integrals:** `i(upper,lower), expr dvar` (e.g., `i(2,0), x^2 dx`) -> `A = 2.666667`.

### Shortcuts
*   **Ctrl + g**: Toggle Graph Mode.
*   **Ctrl + f**: Insert `find `.
*   **Ctrl + w**: Insert ` where `.
*   **Ctrl + d**: Insert `diff(`.
*   **Alt + f**: Insert `f(`.
*   **Alt + u / l / e / v**: Insert `upper`, `lower`, `expr`, `dvar` placeholders.
*   **Ctrl + p / e / n**: Insert constants `Pi`, `exp(1)`, `I`.
*   **s / c / t**: sin, cos, tan.
*   **i**: integral `i(`.

### Graphing Controls
*   **j / k** or **+ / -**: Zoom Out / In.
*   The graph automatically renders axes if $(0,0)$ is in view.

## License
Licensed under GPL-3.0-or-later

