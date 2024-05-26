# End users

Dependencies for end users:
 
 * SDL2
 * SDL2_image
 * SDL2_mixer
 * SDL2_ttf
 * FFTW3 (float)
 * GLEW

# Developers

Besides the same dependencies and their development versions (append a *-dev* or *-devel* for ```.deb``` or ```.rpm``` based distributions more or less), it would be useful to have
 
 * clang-tidy
 * cppcheck
 * Doxygen (for automatic developer documentation)
 * LaTeX/latexpdf compiler (for dev docs)
 * [cmake-format](https://github.com/cheshirekow/cmake_format)
 * a compiler that supports the C++17 standard (the project baseline).

# Notes

Some developers use Ubuntu 20.04 LTS, so it should be relatively well supported in this environment.
There's no finalized Dockerfile yet for integrated CI/CD.


