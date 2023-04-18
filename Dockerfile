FROM ubuntu:latest

RUN apt update
RUN apt install -y make cmake curl wget python3 git clang xz-utils libomp-dev file

WORKDIR /alaska
COPY . /alaska
RUN make defconfig
RUN make deps
RUN make
CMD make sanity
