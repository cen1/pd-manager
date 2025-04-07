FROM debian:bookworm AS build

RUN apt-get update && apt-get install -y \
    git cmake build-essential libmysqlcppconn-dev libmariadb-dev-compat \
    libboost-filesystem-dev libboost-system-dev libboost-chrono-dev \
    libboost-thread-dev libboost-date-time-dev libboost-regex-dev \
    curl gnupg ca-certificates

# Add xpam repository for bncsutil
RUN curl -fsSL https://repo.xpam.pl/repository/repokeys/public/debian-bookworm-xpam.asc | tee /etc/apt/keyrings/debian-bookworm-xpam.asc \
    && echo "Types: deb\nURIs: https://repo.xpam.pl/repository/debian-bookworm-xpam/\nSuites: bookworm\nComponents: main\nSigned-By: /etc/apt/keyrings/debian-bookworm-xpam.asc" \
    | tee /etc/apt/sources.list.d/xpam.sources > /dev/null \
    && apt-get -y update \
    && apt-get install -y bncsutil

WORKDIR /app
COPY CMake ./CMake
COPY ghost ./ghost
COPY test ./test
COPY CMakeLists.txt .

RUN ls -la
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --config Release

FROM debian:bookworm AS runtime

RUN apt-get update && apt-get -y install ca-certificates

COPY --from=build /etc/apt/keyrings/debian-bookworm-xpam.asc /etc/apt/keyrings/debian-bookworm-xpam.asc
COPY --from=build /etc/apt/sources.list.d/xpam.sources /etc/apt/sources.list.d/xpam.sources

RUN apt-get update && apt-get install -y \
    libmysqlcppconn7v5 \
    libboost-filesystem1.74 libboost-system1.74 libboost-chrono1.74 \
    libboost-thread1.74 libboost-date-time1.74 libboost-regex1.74 \
    bncsutil \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/build/pd-manager-1.0.0 .

COPY ip-to-country.csv gameloaded.txt gameover.txt language.cfg ipblacklist.txt pd-manager-docker.cfg .

CMD ["/app/pd-manager-1.0.0", "pd-manager-docker.cfg"]
