
# Set up base image for builder
FROM ubuntu:22.04 AS builder

# Set environment variables
ARG VERSION_BELLSCOIN

# Install dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential libtool autotools-dev automake pkg-config \
        bsdmainutils curl ca-certificates ccache rsync git procps \
        bison libxcb-xinerama0 libxcb-icccm4-dev libxcb-image0-dev \
        libxcb-keysyms1-dev libxcb-render-util0 libxcb-shape0 libxcb-xkb1 \
        libxkbcommon-x11-0 python3 libxml2-utils qtbase5-dev qttools5-dev-tools && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /bellscoin

# Download, extract, and clean up Bellscoin source
RUN curl -o bellscoin.tar.gz -Lk "https://github.com/Nintondo/bellscoinV3/archive/refs/tags/v${VERSION_BELLSCOIN}.tar.gz" && \
    tar -xf bellscoin.tar.gz && \
    mv bellscoinV3-${VERSION_BELLSCOIN}/* ./ && \
    rm -rf bellscoinV3-${VERSION_BELLSCOIN} && \
    rm -f bellscoin.tar.gz

# Build dependencies, build bellscoin, run tests
RUN mkdir build && cd depends && make -j8 && \
    cd .. && \
    ./autogen.sh && \
    ./configure --prefix=$PWD/depends/x86_64-pc-linux-gnu && \
    make -j8 && \
    make DESTDIR=/bellscoin/build install && \
    make check

# Set up base image for runner
FROM debian:bookworm-slim AS runer

# Install dependencies
RUN apt-get update && apt-get install -y libc6 libgcc-s1 curl && \
    rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /bellscoin

# Copy binary files from builder to runner
COPY --from=builder /bellscoin/build/bellscoin/depends/x86_64-pc-linux-gnu/bin ./

# Entrypoint
ENTRYPOINT ["./bellsd"]
