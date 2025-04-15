# pd-manager

Warcraft III hosting system based on original ghost++ project.

Created by myubernick and maintained by cen. This system hosted thousands of DotA and custom games on former playdota.eu/lagabuse.com since 2008, now known as dota.eurobattle.net.

This repository consists of:
- `pd-manager`: typically a bot sitting in bnet channel accepting player commands to host games, check lobbies and collect game stats.
- `pd-slave`: host that accepts a game host request and hosts the game lobby. Multiple slave bots can be attached to a single manager.

Copyright [2008] [Trevor Hogan]
Copyright [2010-2025] [myubernick, cen]

## Convenience Debian repo for bncsutil

```
curl -fsSL https://repo.xpam.pl/repository/repokeys/public/debian-bookworm-xpam.asc | sudo tee /etc/apt/keyrings/debian-bookworm-xpam.asc

echo "Types: deb
URIs: https://repo.xpam.pl/repository/debian-bookworm-xpam/
Suites: bookworm
Components: main
Signed-By: /etc/apt/keyrings/debian-bookworm-xpam.asc" |
sudo tee /etc/apt/sources.list.d/xpam.sources > /dev/null
```

Alternatively, you need to build and install [bncsutil](https://github.com/BNETDocs/bncsutil) from source.

## Build on Linux
```
wget -qO - https://repo.xpam.pl/repository/repokeys/public/debian-bookworm-xpam.asc | sudo apt-key add -
sudo apt-get update
sudo apt install libmysqlcppconn-dev libmariadb-dev-compat libboost-filesystem-dev libboost-system-dev libboost-chrono-dev \
libboost-thread-dev libboost-date-time-dev libboost-regex-dev bncsutil stormlib
cmake -B build
cmake --build build --config Release
cd build
ctest --verbose -C Release
```

## Build on Windows

Use Visual Studio CMD.

Using vcpkg
```
cmake --preset vcpkg
cmake --build build_vcpkg --config Release
```

Using conan v2
```
conan install . -of build_conan -s build_type=Release -o *:shared=False --build=missing
cmake --preset conan
cmake --build build_conan --config Release
```

## Run

Before you begin, you need to place `blizzard.j` and `common.j` from your W3 install into your mapcfgs directory, for example in `./data/mapcfgs`.
You also need to prepare a W3 directory with minimal set of files:
- game.dll
- Storm.dll
- war3.exe
- War3Patch.mpq

For example, in `./wc3_126a`.

### Manager Binary

Run with `./pd-manager-1.0.0 <name-of-your-config-file>.cfg`

### Slave Binary

Run with `./pd-slave-1.0.0 <name-of-your-config-file>.cfg`

### Docker

A `docker-compose.yml` file is provided which starts a MySQL database, runs the migrations, starts a single manager and two slaves.

You should prepare `pd-manager-docker.cfg` which will be volume mounted into the pdm service.

You need to configure at a minimum these options from the example manager cfg:

| Option    | Description                                                                                                                                          |
|-----------|------------------------------------------------------------------------------------------------------------------------------------------------------|
| bot_war3path    | Path to your Warcraft III game. Directory must include at least `game.dll`, `Storm.dll`, `war3.exe` and `War3Patch.mpq`                              |
| bnet_server | Bnet server                                                                                                                                          |
| bnet_serveralias | Bnet server alias name                                                                                                                               |
| bnet_username | Bnet bot username. If it is a new account, login with your game first to verify it's working. You need a dedicated account, not your player account. |
| bnet_password | Bnet bot username password. Use lowercase only.                                                                                                      |
| bnet_rootadmin | Bnet player account that will have root privileges for all commands.                                                                                 |
| bnet_custom_war3version | Set to minor W3 version, `26` or `28`.                                                                                                               |
| bnet_custom_passwordhashtype | Set to a value of `pvpgn` if you are connecting to a pvpgn server.                                                                                   |
| db_mysql_* | Connection options to your MySQL database.																												                                                                               |

Default manager config expect the game files in `./war3_126a` and all you need to provide is essentially the username and password to get started.

You should also prepare `pd-slave-docker.cfg`, `slave_cfgs/s01.cfg` and `slave_cfgs/s02.cfg`. Config options set in the two subconfig files will override the common `pd-slave-docker.cfg`.

You need to configure at a minimum these options from the example slave cfg:

| Option    | Description                                                                                                                                                |
|-----------|------------------------------------------------------------------------------------------------------------------------------------------------------------|
| bot_war3path    | Path to your Warcraft III game. Directory must include at least `game.dll`, `Storm.dll`, `war3.exe` and `War3Patch.mpq`                              |
| bnet_server | Bnet server                                                                                                                                              |
| bnet_serveralias | Bnet server                                                                                                                                         |
| bnet_username | Bnet bot username. If it is a new account, login with your game first to verify it's working. You need a dedicated account, not your player account.   |
| bnet_password | Bnet bot username password. Use lowercase only.                                                                                                        |
| bnet_rootadmin | Bnet player account that will have root privileges for all commands.                                                                                  |
| bnet_custom_war3version | Set to minor W3 version, `26` or `28`.                                                                                                       |
| bnet_custom_passwordhashtype | Set to a value of `pvpgn` if you are connecting to a pvpgn server.                                                                      |

You can leave `bnet_username` and `bnet_password` blank in `pd-slave-docker.cfg` and set them in `slave_cfgs/s01.cfg` and `slave_cfgs/s02.cfg`.

If you want to run a single bot or more than 2 bots, adjust your compose file accordingly. Default specifies a minimum 2 bots to be able to test common hosting scenarios.

Run with `docker compose up -d`.

If you are behind NAT, you need to port forward 6101, 6102, 6201 and 6202 for default hosts to be able to accept player connections once in lobby.

### Systemd
See `manager/pd-manager.service` and `ghost/pd-slave.service` for a sample service files.

## Debugging production problems
```
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
gdb --args pd_manager-1.0.0 default.cfg
r
```
