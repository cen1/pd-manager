# pd-manager
Manager bot by myubernick, cen

Based on ghost++.

This bot sits in a bnet channel and accepts player's commands. It delegates game hosting to "slave" hostbots. It tracks games in lobby, games in play and collects the game results.

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
libboost-thread-dev libboost-date-time-dev libboost-regex-dev bncsutil
cmake -B build
cmake --build build --config Release
cd build
ctest --verbose -C Release
```

## Build on Windows

Using vcpkg
```
cmake --preset vcpkg
cmake --build build --config Release
```

Using conan v2
```
conan install . -of build -s build_type=Release -o *:shared=False --build=missing
cmake --preset conan
cmake --build build --config Release
```

## Run

### Binary

Run with `./pd-manager-1.0.0 <name-of-your-config-file>.cfg`

### Docker

A `docker-compose.yml` file is provided which starts a MySQL database, runs the migrations and starts the manager.

You should prepare `pd-manager-docker.cfg` which will be volume mounted into the container.

You need to configure at a minimum these options from the example cfg:

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
| db_mysql_* | Connection options to your MySQL database.																												 |

Default config expect the game files in `./war3_126a` and all you need to provide is essentially the username and password to get started.

Run with `docker compose up -d`.

### Systemd
See `pd-manager.service` for a sample service file.

## Debugging production problems
```
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
gdb --args pd_manager-1.0.0 default.cfg
r
```

## Command list
`ds`
Disable all bots from accepting games to host.

`ds <num>`
Disable slave hosting by number. Number is taken from username, e.g. lagabuse.com.20 = 20.
