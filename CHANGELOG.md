## Version 3.0 (In progress)

### September 26, 2021: Cloud engine enhancement
* Added customized management port so that port forwarding can re-direct scan/query requests to multiple backend nodes. 

## Version 2.0

### August 16, 2020: [Jetson Engine v2](http://www.ezchess.org/jetson_v2/UserGuide.html) build 2008.1601
* Fixed a bug in Windows agent.

### August 15, 2020: Jetson Engine v2 build 2008.1505
* Added client support for Linux. UCI engine tested in Ubuntu 18.04.
* Added client support Mac OS. UCI engine tested with HIARCS Chess Explorer in macOS Catalina.
* Fixed a few bugs.

### June 28, 2020: Jetson Engine v2 build 2006.002
* Framework changed to client-server based.
* Backend platform supports Nvidia Jetson Xavier, Windows, and Linux.
* Backend agent module is separated from engine as a standalone program (jetson_agent) to simultaneously communicate with multiple engines.
* Any UCI-compliant engines can be launched in backend server. 
* Frontend client renamed as jetson_scan.exe. UCI engine tested with ChessBase in Windows 10.

## Version 1.0

### June 7, 2020: Add support for [Stockfish](https://stockfishchess.org/)
* [Stockfish source code](https://github.com/official-stockfish/Stockfish) is modified to add backend agent module.
* Frontend client for Stockfish jetsonsf.exe is UCI compliant and tested with ChessBase in Windows 10.

### May 31, 2020: Jetson Engine v1 (aka [Jetson Lc0](http://www.ezchess.org/jetson_v1/JetsonLc0UserGuide.html)) initial release. 
* Backend platform supports [Nvidia Jetson Xavier](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-xavier-nx/).
* Backend engine supports [Leela Chess Zero](https://lczero.org/).
* [LCZero source code](https://github.com/LeelaChessZero/lc0) is modified to add backend agent module.
* Frontend client jetsonlc0.exe is UCI compliant and tested with ChessBase in Windows 10.
