/*****************************************************************************
 * This file is part of Jetson Engine.
 * 
 * Copyright (C) 2020 Evelyn Zhu
 * 
 * Jetson Engine is a client-server based chess engine framework designed for
 * chess software like ChessBase or Fritz to remotely access UCI-compliant 
 * chess engines that run on external devices or computer nodes.
 *
 * More information about Jetson Engine can be found at http://ezchess.org/
 *
 * Jetson Engine is a free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Jetson Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Jetson Engine. If not, see <http://www.gnu.org/licenses/>.
 *
 * If you modify this Program, or any covered work, by linking or
 * combining it with third-party libraries, proprietary libraries, or
 * non-GPL compatible libraries, you must obtain licenses from the 
 * authors of the libraries before conveying the result work. You are
 * not authorized to redistribute these libraries, whether in binary
 * forms or source codes, individually or together with this Program
 * as a whole unless the terms of the respective license agreements
 * grant you additional permissions.
 *
 * The above copyright notice, permission notice, and license agreement
 * information shall be included in all copies or substantial portions
 * of this Program.
 *
 * Author: Evelyn J. Zhu <info@ezchess.org>
 *
 ****************************************************************************/

#ifndef _JET_COMMON_H
#define _JET_COMMON_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <pthread.h>
#include <csignal>
#include <time.h>
#include <cstdarg>

#if defined(_WIN32)
	#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x0600
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#pragma comment(lib, "ws2_32.lib")
	#include <windows.h>
	#include <direct.h>
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <unistd.h>
	#include <errno.h>
	#include <fcntl.h> 
	#include <sys/utsname.h>
	#include <sys/stat.h>
	#include <thread>
	#include <cstring>
#endif

#if defined(_WIN32)
	#define IsSockValid(s) ((s) != INVALID_SOCKET)
	#define CloseSocket(s) closesocket(s)
	#define GetSockErrno() (WSAGetLastError())
	#define GetCurrDir _getcwd
	#define SleepMsec(msec) Sleep(msec)
#else
	#define IsSockValid(s) ((s) >= 0)
	#define CloseSocket(s) close(s)
	#define GetSockErrno() (errno)
	#define GetCurrDir getcwd
	#define SleepMsec(msec) this_thread::sleep_for(chrono::milliseconds(msec))
	#define CloseHandle(h) close(h)
	#define SOCKET int
	#define HANDLE int
#endif

#define MAX_NAME_LEN				256	//max length for all types of names
#define MAX_NUM_ENGINE				32	//max numbers of engine folders allowed to set in server
#define MAX_NUM_LOGI_PER_ENGINE		64	//max numbers of client connections for each individual

struct EngineEntry;

struct ClientEntry {
	int bIsConnected;
	int bIsDataLogOn;
	char sReqPipe[MAX_NAME_LEN];
	char sRspPipe[MAX_NAME_LEN];
	HANDLE hReqPipe;
	HANDLE hRspPipe;
	SOCKET sock;
	char sIpAddr[MAX_NAME_LEN];
	char sServIpAddr[MAX_NAME_LEN];
	char sEngInstName[MAX_NAME_LEN];//engine instance name
	struct EngineEntry *engine;
	fd_set *pMaster;
} __attribute__((aligned(8)));

struct EngineEntry {
	int bIsAllocated;
	int reserved;
	char sEngineDir[MAX_NAME_LEN];
	char sEngineName[MAX_NAME_LEN];
	char sEngineExeName[MAX_NAME_LEN];
	char sEngienPort[MAX_NAME_LEN];
	char arguments[MAX_NAME_LEN];
	struct ClientEntry clients[MAX_NUM_LOGI_PER_ENGINE];
} __attribute__((aligned(8)));

enum SocketType {
	SOCK_TYPE_MGMT = 1,
	SOCK_TYPE_ENGINE = 2
};

enum OsArch {
	OS_ARCH_UNKNOWN	= 0,
	OS_ARCH_XAVIER_ARM64 = 1,	//Nvidia Xavier Tegra, ARM64
	OS_ARCH_WINDOWS_X64 = 2,	//Windows 10, X86-64
	OS_ARCH_LINUX_X64 = 3		//Ubuntu Linux, X86-64
};

#define RSP_BUFSIZE 8192			//uci command, mgmt command
#define REQ_BUFSIZE	1024			//uci response
#define PIPE_BUFSIZE RSP_BUFSIZE	//pipe read/write
#define QUERY_BUFSIZE 32768			//query response

#define STR_OS_ARCH_WIN		(char *)"Windows X86-64"
#define STR_OS_ARCH_LINUX	(char *)"Linux X86-64"
#define STR_OS_ARCH_XAVIER	(char *)"Xavier ARM64"

#define STR_JRE_HDR_WIN		(char *)"JRE_X64WIN_"
#define STR_JRE_HDR_LINUX	(char *)"JRE_X64LNX_"
#define STR_JRE_HDR_XAVIER	(char *)"JRE_XAVIER_"

#define STR_MGMT_PORT (char *)"53350"

#define BUILD_NUMBER (char *)"v2.2008.1505"

static inline const std::string GetCurrentDateTime()
{
	time_t now = time(0);
	struct tm tstruct;
	char buf[80];
	tstruct = *localtime(&now);
	strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
	return buf;
}

static inline bool IsFileExist(const char *fileName)
{
    std::ifstream inFile(fileName);
    return inFile.good();
}

extern char gsMyHostName[];
extern pthread_mutex_t gLogFileLock;
extern char gsLogFile[];
static inline void JetsonWriteLogs(const char *fmt, ...)
{
	//Please ensure sErrMsg ended with '\n'
	pthread_mutex_lock(&gLogFileLock);
	FILE *p = fopen(gsLogFile, "a");
	if (p!= NULL) {
		std::ostringstream oss;
		oss << GetCurrentDateTime() << " [" << gsMyHostName << "] " << fmt;
		
		va_list args;
		va_start(args, fmt);
		vfprintf(p, oss.str().c_str(), args);
		va_end(args);
		
		fclose(p);
	}
	pthread_mutex_unlock(&gLogFileLock);
}

#endif	//_JET_COMMON_H
