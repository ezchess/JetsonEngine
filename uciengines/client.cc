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

#include "../common/common.h"

using namespace std;

static int gbClientExiting = 0;
static int gbScanNeeded = 0;
static int gbQueryNeeded = 0;

static char gsScanBuffer[RSP_BUFSIZE];
static char gsQueryBuffer[QUERY_BUFSIZE];

static SOCKET gServSock;

#if defined(_WIN32)
static char *gsJetsonScanFile = (char *)"jetson_scan.exe";
#else
static char *gsJetsonScanFile = (char *)"jetson_scan";
#endif

char gsMyHostName[MAX_NAME_LEN] = "UNKNOWN_HOST";
pthread_mutex_t gLogFileLock;
char gsLogFile[MAX_NAME_LEN] = "JetsonErr_";

static void *ClientReciverThread(void *data)
{
	JetsonWriteLogs(">>> Entered receiver thread\n");
	
	if (gbScanNeeded)
		memset(gsScanBuffer, 0, RSP_BUFSIZE);
		
	if (gbQueryNeeded)
		memset(gsQueryBuffer, 0, QUERY_BUFSIZE);

	try {	
		while(1) {
			fd_set reads;
			FD_ZERO(&reads);
			FD_SET(gServSock, &reads);
#if !defined(_WIN32)
			FD_SET(0, &reads);
#endif
			struct timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 100000;
				
			if (gbClientExiting)
				break;

			if (select(gServSock+1, &reads, 0, 0, &timeout) < 0) {
				JetsonWriteLogs("select() failed. (%d)\n", GetSockErrno());
				throw runtime_error("select() failed\n");
			}

			if (FD_ISSET(gServSock, &reads)) {
				char sSockReadBuf[RSP_BUFSIZE];
				memset(sSockReadBuf, 0, RSP_BUFSIZE);
				int bytes_received = recv(gServSock, sSockReadBuf, RSP_BUFSIZE, 0);
				if (bytes_received < 1) {
					gbClientExiting = 1;
					break;
				}

				std::string line = sSockReadBuf;
			
				if (gbScanNeeded) {
				   	strncat(gsScanBuffer, sSockReadBuf, RSP_BUFSIZE);

					if (strstr(line.c_str(), "scanisdone"))
				   		break;
				}
				else if (gbQueryNeeded) {
					cout << sSockReadBuf;
					JetsonWriteLogs("%s", sSockReadBuf);
					if (strstr(line.c_str(), "querydone")) {
						gbClientExiting = 1;
						break;
					}
				}
				else
		   			cout << sSockReadBuf;//uci response data to ChessBase
			}
		}//while()

		if (gbScanNeeded) {
			char *sBaseFile = (char *)data;
			char cCurrentPath[FILENAME_MAX];

			if (!GetCurrDir(cCurrentPath, sizeof(cCurrentPath)))
				throw runtime_error("unable to get current path\n");

			cCurrentPath[sizeof(cCurrentPath) - 1] = '\0'; /* not really required */

			char * line = strtok(strdup(gsScanBuffer), "\n");
			while(line) {
				cout << line << endl;
				JetsonWriteLogs("%s\n", line);
   			
				if (strstr(line, "scanisdone"))
					break;
				else {
					ostringstream ossNewFile;
#if defined(_WIN32)
					ossNewFile << cCurrentPath << "\\" << line << ".exe";
#else
					ossNewFile << cCurrentPath << "/" << line;
#endif

					ofstream ifs (ossNewFile.str());
					if (!ifs.is_open()) {
						cout << "ERROR: file " << ossNewFile.str() << " is open" << endl;
						JetsonWriteLogs("ERROR: file %s is open\n", ossNewFile.str().c_str());
					}
					else
					{
						ifs.close();
						ostringstream ossCmdline;

#if defined(_WIN32)						
						ossCmdline << "copy " << cCurrentPath << "\\" << sBaseFile << " " << cCurrentPath << "\\" << line << ".exe";
#else
						ossCmdline << "cp " << cCurrentPath << "/" << sBaseFile << " " << cCurrentPath << "/" << line;
						ossCmdline << "; " << "chmod a+x " << cCurrentPath << "/" << line;
#endif
						JetsonWriteLogs("exec cmd is(%s)\n", ossCmdline.str().c_str());
						cout << "exec cmd is(" << ossCmdline.str() << ")" << endl;
						system(ossCmdline.str().c_str());
						SleepMsec(100);
					}
   				}
				
				line  = strtok(NULL, "\n");
			}
		
			gbClientExiting = 1;
		}
	} catch (exception& e) {		
		JetsonWriteLogs("<<< ERROR: %s", e.what());
	}
	
	JetsonWriteLogs("<<< Exited receiver thread\n");
	
	return NULL;
}

#define STR_JREHDR_SIZE 16
#define STR_OSARCH_SIZE 16
#define STR_IPADDR_SIZE 32
#define STR_TCPPORT_SIZE 16

int main(int argc, char *argv[])
{
	int rc;	
	char sJreHeader[STR_JREHDR_SIZE];
	char sOsArch[STR_OSARCH_SIZE];
	char sServIp[STR_IPADDR_SIZE];
	char sServPort[STR_TCPPORT_SIZE];
	char sEngName[MAX_NAME_LEN];
	char sThisExeFileName[MAX_NAME_LEN];
	
	memset(sServIp, 0, STR_IPADDR_SIZE);
	memset(sServPort, 0, STR_TCPPORT_SIZE);

#if defined(_WIN32)
	WSADATA d;
	if (WSAStartup(MAKEWORD(2, 2), &d)) {
		std::cerr << "Failed to initialize." << endl;
		return 0;
	}
	gethostname(gsMyHostName, sizeof(gsMyHostName));
#else
	struct utsname buffer;
	if (uname(&buffer) != 0) {
		std::cerr << "Error on uname." << endl;
		return 0;
	}
   	strcpy(gsMyHostName, buffer.nodename);
#endif
	//TODO: at this moment we don't pass arguments to the backend as arguments
	//are in fact already configured individually in backend's agent.conf
	//however, we're still debating if it is better to pass down
	
	//get current executable name w/o full path info
	int startPos = 0;
	for (int i=0; i<strlen(argv[0]); i++) {
#if defined(_WIN32)
		if (argv[0][i] == '\\')
#else
		if (argv[0][i] == '/')
#endif
			startPos = i;
	}
	if (startPos > 0)
		startPos++;
	
	strncpy(sThisExeFileName, argv[0]+startPos, MAX_NAME_LEN);
	ostringstream ossLogFile;
	ossLogFile << sThisExeFileName << ".log";
	strncat(gsLogFile, ossLogFile.str().c_str(), MAX_NAME_LEN);
	
	ostringstream oss;
	oss << "Received incoming command line=";
	for (int i=0; i<argc; i++) {
		oss << argv[i] << " ";
	}
	oss << ", ver=" << BUILD_NUMBER;
	JetsonWriteLogs("%s\n", oss.str().c_str());
	
	JetsonWriteLogs("Filename = %s\n", sThisExeFileName);

	/* engine server scan or query */
	if (argc >= 3 &&
		strcmp(sThisExeFileName, gsJetsonScanFile) == 0) {
		if (strcmp(argv[1], "scan") == 0) {
			gbScanNeeded = 1;
			strncpy(sServIp, argv[2], STR_IPADDR_SIZE);
			strncpy(sServPort, STR_MGMT_PORT, STR_TCPPORT_SIZE);
			
			printf("scanning server %s on port %s\n", sServIp, sServPort);
		}
		
		if (strcmp(argv[1], "query") == 0) {
			gbQueryNeeded = 1;
			strncpy(sServIp, argv[2], STR_IPADDR_SIZE);
			strncpy(sServPort, STR_MGMT_PORT, STR_TCPPORT_SIZE);
			
			printf("query server %s on port %s\n", sServIp, sServPort);
		}
	}
	else if (strcmp(sThisExeFileName, gsJetsonScanFile) == 0) {
		printf("Incorrect syntax\n");
		printf("To scan agent run: jetson_scan scan <agent ip address>\n");
		printf("To query agent run: jetson_scan query <agent ip address>\n");
		printf("Example:\n");
		printf("jetson_scan scan 192.168.55.1\n");
		printf("jetson_scan query 192.168.55.1\n");
		return 0;
	}
	else {	//JRE engine
		for (int i=0; i<strlen(sThisExeFileName); i++) {
			if (sThisExeFileName[i] == '_')
				sThisExeFileName[i] = ' ';
		}

		sscanf(sThisExeFileName, "%s %s %s %s %s", sJreHeader, sOsArch, sServIp, sServPort, sEngName);
	}
	
	try {	
		struct addrinfo localAddr;
		memset(&localAddr, 0, sizeof(localAddr));
		localAddr.ai_socktype = SOCK_STREAM;
	
		struct addrinfo *pPeerAddr;
		if (getaddrinfo(sServIp, sServPort, &localAddr, &pPeerAddr)) {
			JetsonWriteLogs("getaddrinfo() failed. (%d)\n", GetSockErrno());
			throw runtime_error("getaddrinfo() failed\n");
		}

		char sAddrBuf[100];
		char sServBuf[100];
		getnameinfo(pPeerAddr->ai_addr, pPeerAddr->ai_addrlen,
			sAddrBuf, sizeof(sAddrBuf),
			sServBuf, sizeof(sServBuf),
			NI_NUMERICHOST);

		gServSock = socket(pPeerAddr->ai_family,
			pPeerAddr->ai_socktype, pPeerAddr->ai_protocol);
		if (!IsSockValid(gServSock)) {
			JetsonWriteLogs("socket() failed. (%d)\n", GetSockErrno());
			throw runtime_error("socket() failed\n");
		}

		if (connect(gServSock,
				pPeerAddr->ai_addr, pPeerAddr->ai_addrlen)) {
			JetsonWriteLogs("connect() failed. (%d)\n", GetSockErrno());
			throw runtime_error("connect() failed\n");
		}
		freeaddrinfo(pPeerAddr);
	
		void *pThreadData = (gbScanNeeded ? (void *)sThisExeFileName : NULL);

		pthread_t clientReceiverThreadId = 0;
		rc = pthread_create(&clientReceiverThreadId, NULL,  ClientReciverThread, pThreadData);
		if (rc != 0) {
			throw std::runtime_error("Unable to create recv_thread\n");
		}
	
		if (gbScanNeeded || gbQueryNeeded) {
			send(gServSock, argv[1], strlen(argv[1]), 0);
		
			while(1) {
				SleepMsec(1000);
				if (gbClientExiting)
					break;
			}
		}
		else {
			std::cout.setf(std::ios::unitbuf);
			std::string line;
			while (std::getline(std::cin, line)) {
				if (gbClientExiting)
					throw std::runtime_error("Connection closed by Jetson device\n");			
		
				int bytesSent = send(gServSock, line.c_str(), line.length(), 0);
			
				SleepMsec(300);

				if(!strncmp(line.c_str(), "quit", 4)) {
					gbClientExiting = 1;
					SleepMsec(500);
					break;
				}
			}
		}

		CloseSocket(gServSock);

#if defined(_WIN32)
		WSACleanup();
#endif

		return 0;
	
	} catch (std::exception& e) {
		JetsonWriteLogs("Unhandled exception: %s", e.what());
		
		CloseSocket(gServSock);
#if defined(_WIN32)
		WSACleanup();
#endif
		std::cerr << "Unhandled exception: " << e.what() << endl;
	}
}
