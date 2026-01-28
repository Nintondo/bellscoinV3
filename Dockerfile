# Build stage
FROM debian:trixie AS builder

ARG BUILD_JOBS=0
ARG RUN_TESTS=0
ARG PREFIX=/opt/bellscoin

WORKDIR /usr/src/app

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential libtool autotools-dev automake pkg-config \
        bsdmainutils curl ca-certificates ccache rsync git procps \
        bison cmake libxcb-xinerama0 libxcb-icccm4-dev libxcb-image0-dev \
        libxcb-keysyms1-dev libxcb-render-util0 libxcb-shape0 libxcb-xkb1 \
        libxkbcommon-x11-0 python3 libxml2-utils qtbase5-dev qttools5-dev-tools && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Download, extract, and clean up Bellscoin source
RUN curl -o bellscoin.tar.gz -Lk "https://github.com/Nintondo/bellscoinV3/archive/refs/heads/dev.tar.gz" && \
    tar -xf bellscoin.tar.gz && \
    mv bellscoinV3-dev/* ./ && \
    rm -rf bellscoinV3-dev && \
    rm -f bellscoin.tar.gz

# Build dependencies, build bellscoin, run tests
RUN if [ "${BUILD_JOBS}" = "0" ] || [ -z "${BUILD_JOBS}" ]; then BUILD_JOBS="$(nproc)"; fi && \
    mkdir build && cd depends && make -j"${BUILD_JOBS}" && \
    cd .. && \
    ./autogen.sh && \
    CONFIG_SITE="$PWD/depends/x86_64-pc-linux-gnu/share/config.site" \
    ./configure --prefix="${PREFIX}" --with-boost="$PWD/depends/x86_64-pc-linux-gnu" && \
    make -j"${BUILD_JOBS}"

RUN if [ "$RUN_TESTS" = "1" ]; then make check; fi

RUN mkdir -p /build && \
    make DESTDIR=/build install

# Runtime stage
FROM debian:trixie-slim AS runner

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        gosu \
        libc6 \
        libgcc-s1 \
        tini && \
    rm -rf /var/lib/apt/lists/*

RUN groupadd --system --gid 1001 appuser && \
    useradd --system --uid 1001 --gid appuser --home /home/appuser --shell /usr/sbin/nologin appuser && \
    mkdir -p /home/appuser/.cache /data && \
    chown -R 1001:1001 /home/appuser /data

WORKDIR /app

COPY --from=builder /build/opt/bellscoin/bin/bellsd /app/
COPY --from=builder /build/opt/bellscoin/bin/bells-cli /app/
COPY --from=builder /build/opt/bellscoin/bin/bells-qt /app/
COPY --from=builder /build/opt/bellscoin/bin/bells-util /app/
COPY --from=builder /build/opt/bellscoin/bin/bells-wallet /app/
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 19918

ENTRYPOINT ["/usr/bin/tini", "--", "/entrypoint.sh"]
CMD ["/app/bellsd"]
