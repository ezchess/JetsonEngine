# Jetson Engine
Jetson Engine is a client-server based chess engine framework designed for chess software like ChessBase or Fritz to remotely access UCI-compliant chess engines that run on external devices or computer nodes. This README shows how to compile backend server agent and frontend client UCI engines. More information about Jetson Engine can be found at http://www.ezchess.org/.

Backend server agent supports:
* [Nvidia Jetson Xavier Device](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-xavier-nx/) (arm64, Linux)
* Windows Server (x64, Windows 10)
* Linux Server (x64, Ubuntu Linux)

Frontend client UCI engines supports:
* Windows Host (x64, Windows 10)
* Linux Host (x64, Ubuntu Linux)
* Mac Host (x64, maxOS)

## 1. Downloading Source
```
git clone https://github.com/ezchess/JetsonEngine.git
```

## 2. Building Jetson Backend Agent
Jetson backend servers support three platforms: Nvidia Xavier devices, Windows 10, and Ubuntu Linux.

### 2.1 Xavier and Linux
```
g++ -o jetson_agent agent.cc -lpthread
```

### 2.2 Windows

1. Download and setup [MSYS2 Software Distribution and Building Platform for Windows](https://www.msys2.org/).

2. Under MSYS2, install development tools for MSYS2:
```
unzip -v &> /dev/null || pacman -S --noconfirm unzip
make -v &> /dev/null || pacman -S --noconfirm make
g++ -v &> /dev/null || pacman -S --noconfirm mingw-w64-x86_64-gcc
```

3. Under MSYS2, compile jetson agent:
```
g++ -o jetson_agent.exe agent.cc -liphlpapi -lws2_32 -lpthread -static
```

## 3. Building Jetson Frontend Client
Jetson frontend clients support three platforms: Windows 10, Ubuntu Linux, and Mac OS.

### 3.1 Linux and Mac OS
```
g++ -o jetson_scan client.cc -lpthread
```

### 3.2 Windows
MSYS2 is required for compiling client. Please follow the steps mentioned in section 2.2. After MSYS2 is set up run the following command:
```
g++ -o jetson_scan.exe client.cc -liphlpapi -lws2_32 -lpthread -static
```

## 4. Running Jetson Engine
Please follow the [Jetson Engine User Guide](http://www.ezchess.org/jetson_v2/UserGuide.html) to set up and launch agent and client. For Nvidia Xavier device backend, please follow this [special procedure](http://www.ezchess.org/jetson_v2/XavierUserGuide.html). 

## 5. License
Jetson Engine is a free software. You can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

Jetson Engine is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Jetson Engine. If not, see <http://www.gnu.org/licenses/>.

If you modify this Program, or any covered work, by linking or combining it with third-party libraries, proprietary libraries, or non-GPL compatible libraries, you must obtain licenses from the authors of the libraries before conveying the result work. You are not authorized to redistribute these libraries, whether in binary forms or source codes, individually or together with this Program as a whole unless the terms of the respective license agreements grant you additional permissions.

Author: Evelyn J. Zhu <info@ezchess.org>
