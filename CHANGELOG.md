## Version 2.0

### August 15, 2020: [Jetson Engine v2](http://www.ezchess.org/jetson_v2/UserGuide.html) build 2008.1505
* Adding client support for Linux and Mac OS. HIARCS Chess Explorer is tested.
* Fixed a few bugs.

### June 28, 2020: Jetson Engine v2 build 2006.002
* Framework changed to client-server based.
* Backend platform supports Nvidia Jetson Xavier, Windows, and Linux.
* Backend agent module is built as a separate program (jetson_agent) to communicate with multiple engines.
* Any UCI-compliant engines can be launched in backend server. 
* Frontend client module is UCI compliant and renamed as jetson_scan.exe.

## Version 1.0

### June 7, 2020: Adding support for [Stockfish](https://stockfishchess.org/)
* Backend agent module for Stockfish is directly embedded into [Stockfish source code](https://github.com/official-stockfish/Stockfish).
* Front client for Stockfish jetsonsf.exe is UCI compliant and supports Windows 10.

### May 31, 2020: Jetson Engine v1 (aka [Jetson Lc0](http://www.ezchess.org/jetson_v1/JetsonLc0UserGuide.html)) initial release. 
* Backend platform supports [Nvidia Jetson Xavier](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-xavier-nx/).
* Backend engine supports [Leela Chess Zero](https://lczero.org/).
* Backend agent module is directly embedded into [LCZero source code](https://github.com/LeelaChessZero/lc0).
* Front client jetsonlc0.exe is UCI compliant and supports Windows 10.
