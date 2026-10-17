FROM python:3.11-slim
RUN apt-get update && apt-get install -y --no-install-recommends cmake ninja-build g++ qt6-base-dev && rm -rf /var/lib/apt/lists/*
COPY source.tar /tmp/source.tar
RUN mkdir /source && tar -xf /tmp/source.tar -C /source
COPY CMakeLists.txt /harness/CMakeLists.txt
RUN cmake -S /harness -B /build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build /build --target profile_test -j2
WORKDIR /source
CMD ["/build/profile_test"]
