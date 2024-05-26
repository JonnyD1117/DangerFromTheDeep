FROM ubuntu:18.04

RUN echo "Updating Ubuntu"
RUN apt-get update && apt-get upgrade -y

RUN echo "Installing dependencies..."
RUN apt install -y \
			ccache \
			clang \
			clang-format \
			clang-tidy \
			cppcheck \
			texlive \
            texlive-latex-recommended \
			doxygen \
			gcc \
			git \
			graphviz \
			make \
            libsdl2 \
            libsdl2-dev \
            libsdl2-image \
            libsdl2-image-dev \
            libsdl2-mixer \ 
            libsdl2-mixer-dev \
            libfftw3 \
            libfftw3-bin \
            libfftw3-dev \
            libfftw3-single3 \
            cmake \
            cmake-extras \
            extra-cmake-modules

RUN echo "Installing dependencies not found in the package repos..."

# Disabled pthread support for GTest due to linking errors
RUN git clone https://github.com/google/googletest.git --branch release-1.10.0 && \
        cd googletest && \
        cmake -Bbuild -Dgtest_disable_pthreads=1 && \
        cmake --build build --config Release && \
        cmake --build build --target install --config Release

