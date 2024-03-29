### August 17, 2022: Lc0 benchmark for Jetson AGX Orin
* [JetPack 5.0.2](https://developer.nvidia.com/embedded/downloads) released on 8/15/2022. I benchmark latest [Lc0 development version](https://github.com/LeelaChessZero/lc0) with weights 256x20-t40-1541.pb.gz, and performance reaches 19359 nps. AGX Orin shows 8x performance boost than Xavier NX. [Lc0 networks](https://lczero.org/play/networks/bestnets/) has removed t40-1541. Please use the latest networks for engine evaluations.

```
jetson@agxorin:~/lc0/build/release$ ./lc0 benchmark --backend=cuda-fp16
       _
|   _ | |
|_ |_ |_| v0.30.0-dev+git.dirty built Aug 17 2022
Loading weights file from: ./256x20-t40-1541.pb.gz
Creating backend [cuda-fp16]...
CUDA Runtime version: 11.4.0
Latest version of CUDA supported by the driver: 11.4.0
GPU: Orin
GPU memory: 29.8203 Gb
GPU clock frequency: 1300 MHz
GPU compute capability: 8.7
. . . . . .
. . . . . .

===========================
Total time (ms) : 341324
Nodes searched  : 6607875
Nodes/second    : 19359
```

## Version 2.1

### September 26, 2021: Cloud engine enhancement
* Added customized management port so that port forwarding can re-direct scan/query requests to multiple backend nodes. 

### August 26, 2021: Lc0 benchmark from different JP/cuDNN combinations
* With latest JetPack 4.6 + cuDNN 8.2 released on 8/4/2021, a major performance issue with cuDNN 8.0 is fixed. I benchmark latest [Lc0 0.28.0](https://github.com/LeelaChessZero/lc0/releases/tag/v0.28.0) with weights 256x20-t40-1541.pb.gz. Surprisingly, older JP4.4/cuDNN 7.6 still performs best.
```
JetPack    cuDNN        PowerMode           Performance (Nodes/second)

JP 4.6     cuDNN 8.2    8 (20W, 6-Core)     1693

                        6 (20W, 2-Core)     1936

                        0 (15W, 2-Core)     1777
			
           cuDNN 7.6    6 (20W, 2-Core)     1972
			
JP 4.4     cuDNN 7.6    0 (15W, 2-Core)     2400
```

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
