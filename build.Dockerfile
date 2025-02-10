FROM ubuntu:focal

WORKDIR /app

ARG DEBIAN_FRONTEND=noninteractive

# add a non-privileged user for running the application
RUN groupadd --gid 10001 app && \
    useradd -g app --uid 10001 --shell /usr/sbin/nologin --create-home --home-dir /app app


RUN apt update && \
  apt -y install git cmake

# See https://marian-nmt.github.io/docs/#installation for Marian requirements
RUN apt-get install -y build-essential \
    libboost-all-dev libprotobuf17 protobuf-compiler \
    libprotobuf-dev libssl-dev libgoogle-perftools-dev

# Intel MKL - for Marian usage on CPU
RUN apt install wget && \
    wget -qO- 'https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB' |  apt-key add -
RUN sh -c 'echo deb https://apt.repos.intel.com/mkl all main > /etc/apt/sources.list.d/intel-mkl.list' && \
    apt-get update && \
    apt-get install -y intel-mkl-64bit-2020.0-088

# attempt to use openblas
#RUN apt search openblas
#RUN apt install -y libopenblas-dev
#RUN update-alternatives --config libblas.so.3

# Bergamot
# pcre2 is requried to build berrgamot-translator with -DUSE_WASM_COMPATIBLE_SOURCES=off
RUN apt-get install -y libpcre2-dev

# to speedup crow, didn't work
#RUN apt-get install -y libtcmalloc-minimal4 && ln -s /usr/lib/libtcmalloc_minimal.so.4 /usr/lib/libtcmalloc_minimal.so

RUN apt-get install -y ccache

CMD ["/bin/bash"]