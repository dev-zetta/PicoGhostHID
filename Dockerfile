FROM ubuntu:24.04 AS builder

WORKDIR /src
COPY scripts/build-deps.txt scripts/install_build_deps.sh scripts/
RUN ./scripts/install_build_deps.sh && rm -rf /var/lib/apt/lists/*

COPY scripts/fetch_sdk.sh scripts/
ARG PICO_SDK_VERSION
ARG PICO_SDK_REPO
RUN ./scripts/fetch_sdk.sh

COPY . .
ARG BOARD=all
ARG GHOST_USB_SERIAL
ARG CMAKE_BUILD_PARALLEL_LEVEL
RUN ./scripts/build.sh --output /out "${BOARD}"

FROM scratch AS firmware
COPY --from=builder /out/ /
