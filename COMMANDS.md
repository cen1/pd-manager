# Manager and hostbot command list

Commands to manager should be whispered through bnet: `/w manager_username !command_name ...`

Command trigger is usually `!` or `.` depending on the config.

## Inside Channel - through whisper (everyone)

| Command | Description |
|---|---|
| `!checkban [name]` | Check whether you are banned on the current realm, optionally add `[name]` to check someone else. |
| `.dota <game name>` | Creates a public custom DotA game using the ladder DotA map. |
| `.dotaobs <game name>` | Creates a public custom DotA game using the ladder DotA map with observer slots (only enabled for tours). |
| `!game <number>` | Displays information about a game in progress. |
| `!games` | Displays information about all games. |
| `!gnames <number>` | Displays players in a game number `<number>`. |
| `!regions` | Displays available hostbot regions. Use the 2-letter region code with commands below. |
| `!priv<region_code> <game name>` | Creates a private ladder DotA game. If region code is not specified, defaults to EU host. E.g., `!privas` for Asian host. |
| `!privl<region_code> <game name>` | Creates a private ladder DotA game with a legacy DotA map. |
| `.priv<region_code> <game name>` | Creates a private custom game using the map loaded with `.map`. |
| `!privobs<region_code> <game name>` | Creates a private ladder DotA game with observer slots (this game cannot be rehosted to a public game). |
| `!pub<region_code> <game name>` | Creates a public DotA ladder game. If region code is not specified, defaults to EU host. E.g., `!pubas` for Asian host. |
| `!publ<region_code> <game name>` | Creates a public DotA ladder game with a legacy DotA map. |
| `.pub<region_code> <game name>` | Creates a public custom game using the map loaded with `.map`. |
| `!lobby <number>` | Displays information about a lobby in progress. |
| `!lobbies` | Displays all open lobbies, slot occupation, and players in each lobby. |
| `.map <map name>` | Loads a map file; leave blank to see the current map. |
| `!names <number>` | Displays players in a lobby number `<number>`. |
| `!priv <game name>` | Alias to `!gopriv`. |
| `!privl <game name>` | Alias to `!goprivl`. |
| `.priv <game name>` | Alias to `.gopriv`. |
| `!privobs <game name>` | Alias to `!goprivobs`. |
| `!q [number]` | Alias to `!queue`. |
| `!queue [number]` | Shows information about the queue, optionally add `[number]` to check details for a game in the queue. |
| `!sd [name]` | Alias to `!statsdota`. |
| `!statsdota [name]` | Displays your ladder DotA stats, optionally add `[name]` to display stats for another player. |
| `!unqueue` | Removes your game from the queue (leaving the channel has the same effect as using `!unqueue`). |
| `!users` | Displays how many users are currently using lagabuse.com. |
| `!version` | Displays version information. |
| `!where <name>` | Checks if player `<name>` is playing on the lagabuse.com bot. |

---
## In-game lobby (everyone)

| Command | Description |
|---|---|
| `!check <name>` | Checks a player's status (tries to do a partial match). |
| `!checkme` | Checks your own status. |
| `!f` | Alias to `!from`. |
| `!from` | Displays the country each player is from. |
| `!gameinfo` | Shows some game information. |
| `!gamename` | Displays the current game name. |
| `!gn` | Alias to `!gamename`. |
| `!hcl` | Displays the current HCL command string (aka. game mode). |
| `!mode` | Alias to `!hcl`. |
| `!owner` | Displays who the current game owner is. |
| `!p` | Alias to `!ping`. |
| `!ping` | Shows everyone's ping. |
| `!psr [name]` | Shows your PSR, optionally add `[name]` to check another player (tries to do a partial match). |
| `!r [name]` | Alias to `!psr`. |
| `!rall` | Shows everyone's PSR. |
| `!rankall` | Alias to `!rall`. |
| `!rateall` | Alias to `!rall`. |
| `!rating [name]` | Alias to `!psr`. |
| `!ratings` | Shows win chance % and the team's balance. |
| `!rs` | Alias to `!ratings`. |
| `!region` | Prints the hostbot region. |
| `!sc` | Shows how to spoof check manually. |
| `!sd [name]` | Alias to `!statsdota`. |
| `!statsdota [name]` | Displays your ladder DotA stats, optionally add `[name]` to display stats for another player (tries to do a partial match). |
| `!version` | Displays version information. |
| `!votekick <name>` | Starts a votekick against a player (tries to do a partial match). |
| `!yes` | Votes yes to kick a player after someone used `!votekick`. |

---
## In-game lobby (game owner)

| Command | Description |
|---|---|
| `!a` | Alias to `!abort`. |
| `!abort` | Aborts the countdown. |
| `!balance` | Balances the slots based on PSR. |
| `!close <number> ...` | Closes a slot or slots. |
| `!closeall` | Closes all open slots. |
| `!hold <name> ...` | Holds (reserves) a slot for someone. |
| `!kick <name>` | Kicks a player (tries to do a partial match). |
| `!latency [number]` | Sets game latency to `[number]` (80-120), leave blank to see current latency. |
| `!max <number>` | Alias to `!maxpsr`. |
| `!maxpsr <number>` | Kicks all players with PSR greater than `[number]` (won't kick players reserved with `!hold`). |
| `!min <number>` | Alias to `!minpsr`. |
| `!minpsr <number>` | Kicks all players with PSR less than `[number]` (won't kick players reserved with `!hold`). |
| `!open <number> ...` | Opens a slot or slots. |
| `!openall` | Opens all closed slots. |
| `!p [max ping]` | Alias to `!ping`. |
| `!ping [max ping]` | Shows everyone's ping, optionally add `[max ping]` to kick players with ping above `[max ping]`. |
| `!priv [game name]` | Rehosts as a private game `[game name]`. If used without `[game name]`, the bot will automatically make a new game name. |
| `!pub [game name]` | Rehosts as a public game `[game name]`. If used without `[game name]`, the bot will automatically make a new game name. |
| `!sp` | Shuffles players. |
| `!start [force]` | Starts the game, optionally add `force` to skip checks. |
| `!swap <n1> <n2>` | Swaps slots. |
| `!sync [number]` | Alias to `!synclimit`. |
| `!synclimit [number]` | Sets the sync limit for the lag screen (50-250), leave blank to see the current sync limit. |
| `!unhost` | Unhosts the game. |
| `!comp <slot number> <difficulty from 0 (easy) to 2 (insane)>` | Sets a slot to computer. |

---
## In-game (everyone)

| Command | Description |
|---|---|
| `!autoban` | Shows autoban status, shows if you can leave the game without getting autobanned. |
| `!check <name>` | Checks a player's status (tries to do a partial match). |
| `!checkme` | Checks your own status. |
| `!f` | Alias to `!from`. |
| `!ff` | Alias to `!forfeit`. |
| `!ffcount` | Displays who voted to forfeit the game. |
| `!forfeit` | Forfeits (surrenders) the game to the opposing team. |
| `!from` | Displays the country each player is from. |
| `!gameinfo` | Shows some game information. |
| `!gamename` | Displays the current game name. |
| `!gn` | Alias to `!gamename`. |
| `!ignore [name]` | Stops receiving chat messages from a player (tries to do a partial match), leave blank to see `!ignorelist`. |
| `!ignorelist` | Displays a list of players you are ignoring and a list of players ignoring you. |
| `!owner` | Displays who the current game owner is. |
| `!p` | Alias to `!ping`. |
| `!ping` | Shows everyone's ping. |
| `!psr [name]` | Shows your PSR, optionally add `[name]` to check another player (tries to do a partial match). |
| `!r [name]` | Alias to `!psr`. |
| `!rall` | Shows everyone's PSR. |
| `!rankall` | Alias to `!rall`. |
| `!rateall` | Alias to `!rall`. |
| `!rating [name]` | Alias to `!psr`. |
| `!ratings` | Shows win chance % and the team's balance. |
| `!remake` | Starts votermk or votes in votermk. |
| `!rmk` | Alias to `!remake`. |
| `!rmkcount` | Displays who voted to remake the game. |
| `!rs` | Alias to `!ratings`. |
| `!sc` | Shows how to spoof check manually. |
| `!sd [name]` | Alias to `!statsdota`. |
| `!statsdota [name]` | Displays your ladder DotA stats, optionally add `[name]` to display stats for another player (tries to do a partial match). |
| `!unignore <name>` | Starts receiving chat messages from a player that you previously `!ignored` (tries to do a partial match). |
| `!version` | Displays version information. |
| `!votekick <name>` | Starts a votekick against a player (tries to do a partial match). |
| `!yes` | Votes yes to kick a player after someone used `!votekick`. |

---
## In-game (game owner)

| Command | Description |
|---|---|
| `!latency [number]` | Sets game latency (80-120), leave blank to see current latency. |
| `!muteall` | Mutes global chat (allied and private chat still works). |
| `!sync` | Alias to `!synclimit`. |
| `!synclimit [number]` | Sets the sync limit for the lag screen (50-250), leave blank to see current sync limit. |
| `!unmuteall` | Unmutes global chat. |

---
## Admin Commands

| Command | Description |
|---|---|
| `!announce` / `!ann <message>` | Sends an announcement. Requires access level 4001, 4050 or greater than 8000. |
| `!ban` / `!cban <name> [reason]` | Bans a user from the channel. If a reason is provided, it will be included in the ban message. |
| `!channel <channel name>` | Changes the bot's current channel. |
| `!cmd <command>` | Executes a bnet command as the manager account. |
| `!unban` / `!cunban <name>` | Unbans a user from the channel. |
| `!disable [message]` | Disables all game hosting and removes all games from the queue. An optional message can be set to give as a response. |
| `!disableladder` | Disables the ladder game hosting only. |
| `!com` | Toggles "contributor-only mode" on or off. When enabled, only contributors can create games. |
| `!comstatus` | Displays the current status of "contributor-only mode" (ON or OFF). |
| `!devoice <name>` | Removes voice privileges from a user. |
| `!devoiceme` | Removes voice privileges from the user who issued the command. |
| `!enable` | Enables game hosting. |
| `!enableladder` | Enables hosting of ladder games. |
| `!exit` / `!quit [nice/force]` | Shuts down the bot. `nice` attempts a graceful shutdown, `force` forces an immediate shutdown. |
| `!kick` / `!ckick <name> [reason]` | Kicks a user from the channel. If a reason is provided, it will be included in the kick message. |
| `!maxgames [number]` | If a number is provided, sets the maximum number of simultaneous games the bot can host. If no number is provided, displays the current maximum games. |
| `!moderate` | Moderates the bnet channel. |
| `!say <message>` | Makes the bot send a chat message to the current channel. |
| `!topic <topic text>` | Sets the channel topic. |
| `!voice <name>` | Grants temporary voice privileges to a user (expires on reconnect). |
| `!vop <name>` | Grants permanent voice privileges to a user. |
| `!voiceme` | Grants temporary voice privileges to the user who issued the command. |
| `!vopme` | Grants permanent voice privileges to the user who issued the command. |
| `!slaves` | Displays detailed information about all connected slave hostbots. |
| `!slavesl` | Displays less detailed information about all connected slave hostbots. |
| `!rsc` | Sends a command to all slave bots to reload their configurations. This reads the .cfg file again and sets new values in memory. Any config that is loaded on startup and not used dynamically will require a restart so it has limited use. It also reloads default map cfg. |
| `!ds [slave ID]` | Disables all slave bots so they can't queue any more games. If a `slave ID` is provided, prevents only that specific slave bot to host games. ID is matched against the hostbot username, 20 would disable bot `lagabuse.com.20`. This is not bot id as displayed by `!slaves` command! This command can be used to disable a subset of the slaves to wait until all their active games are finished so they can be updated and restarted. |
| `!es [slave ID]` | Enables all slave bots for hosting. If a `slave ID` is provided, enables hosting on the specific bot. Same rule for ID applies as described in `!ds`. |
