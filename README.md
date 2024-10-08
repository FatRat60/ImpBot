
# Discord Music Bot

A discord bot written in C++/C mostly for private use by myself and various other friends groups. Compiled and targeted for Ubuntu in order to have it run on a server.


## Features

- Streaming music
- Supports Youtube and Spotify links
- Youtube
    - Videos
    - Playlists
    - Livestreams
- Spotify
    - Tracks
    - Playlists
    - Albums
- Auto-shuffles tracks when queueing playlists
- Embedded music player with tabs for viewing queue and history of who added what songs/playlists
- Communicates with Docker Daemon to start and stop game servers (i.e. Minecraft)



## Screenshots

![Empty Player Embed](/imgs/empty_player_embed.jpg)
![Player Embed](/imgs/Player_embed.jpg)
![Queue Embed](/imgs/queue_embed.jpg)
![History Embed](/imgs/history_embed.jpg)
![Add Music Modal](/imgs/add_music_modal.jpg)


## Libraries Used

- [DPP](https://dpp.dev/index.html) - C++ Discord API Library
- [OGGZ](https://www.xiph.org/oggz/) - Used for opening and reading .opus music files
- [YAML-CPP](https://github.com/jbeder/yaml-cpp) - used to parse the docker-compose.yml file used to start a game server
