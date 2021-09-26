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

static struct EngineEntry gEngineTables[MAX_NUM_ENGINE];
static pthread_mutex_t gJetsonTableLock;
static int gbIsTableLockOn = 0;
static int gbAgentExiting = 0;

static char gsMyOsArch[MAX_NAME_LEN] = "";
static char gsJreHeader[MAX_NAME_LEN] = "";

static int gOsArchId = OS_ARCH_UNKNOWN;

static char *gsAgentConfFile = (char *)"jetson_agent.conf";
static char *gsMgmtPortFile = (char *)"mgmt.port";
static string gsMgmtPortStr = STR_MGMT_PORT;

char gsMyHostName[MAX_NAME_LEN] = "UNKNOWN_SERVER";
pthread_mutex_t gLogFileLock;
char gsLogFile[MAX_NAME_LEN] = "JetsonAgentErr.log";

static void *EngineInstanceRequestThread(void *data)
{
	struct ClientEntry *client = (struct ClientEntry *)data;
	SOCKET sock = client->sock;
	char *sIpAddr = client->sIpAddr;
	char *sEngineName = client->engine->sEngineName;
	char *sServIp = client->sServIpAddr;
	int bDisconnectClientNeeded = 0;
	int bIsConnected = 0;
		
	JetsonWriteLogs(">>> Entered eng_i_req from client(%s, %d) via (%s, %s)\n",
			sIpAddr, sock, sEngineName, sServIp);

	try {
#if defined(_WIN32)
		bIsConnected = ConnectNamedPipe(client->hReqPipe, NULL) ? 
			1 : (GetLastError() == ERROR_PIPE_CONNECTED);  
#else
		int fd = open(client->sReqPipe, O_WRONLY);
		if (fd >= 0) {
			client->hReqPipe = fd;//no need lock here
			bIsConnected = 1;
		}
#endif
		if (!bIsConnected)
			throw runtime_error("Unable to connect request pipe\n");
      
		//----- receive data from client socket, incoming uci command
		while (1) {
			char sockReadBuf[REQ_BUFSIZE];
			memset(sockReadBuf, 0, REQ_BUFSIZE);
                    
			int bytesReceived = recv(client->sock, sockReadBuf, REQ_BUFSIZE, 0);
			if (bytesReceived < 1 || bytesReceived >= REQ_BUFSIZE) {
				JetsonWriteLogs("Closing (%d) from client(%s) for engine(%s)\n", sock, sIpAddr, sEngineName);
                    	
				//----- TODO: lookup client sock in table, find the client, kill engine, clear client entry
				bDisconnectClientNeeded = 1;
				FD_CLR(client->sock, client->pMaster);
				CloseSocket(client->sock);
				
				break;
			}
                    
			//ATTN: here have to add \n to force cmd to execute and then flush out pipe
			//bytesReceived is up to REQ_BUFSIZE-1 so no worry of buffer overrun.
			sockReadBuf[bytesReceived] = '\n';

			JetsonWriteLogs("Client (%s, %d, %s, %s) received UCI cmd >> %s",
				sIpAddr, sock, sEngineName, sServIp, sockReadBuf); //'\n' already in sockReadBuf

			int cbReplyBytes = bytesReceived+1;//number of bytes to write
			int cbWritten = 0;//number of bytes written 
			char sWriteErr[128];
			//Note: no need use separate pipeWriteBuf, just use sockReadBuf
#if defined(_WIN32)
			BOOL fSuccess = WriteFile(client->hReqPipe, sockReadBuf, (DWORD)cbReplyBytes, (LPDWORD)&cbWritten, NULL); 
         	
         	if (!fSuccess || cbReplyBytes != cbWritten) {
				sprintf(sWriteErr, "WriteFile failed, rval(%d), rbytes(%d), wbytes(%d), GLE=%d.\n",
						fSuccess, cbReplyBytes, cbWritten, GetLastError()); 
          		throw runtime_error(sWriteErr);
			}  
#else
            cbWritten = write(client->hReqPipe, sockReadBuf, cbReplyBytes);
			  
         	if (cbReplyBytes != cbWritten) {				
				sprintf(sWriteErr, "WriteFile failed, rbytes(%d), wbytes(%d)\n",
						cbReplyBytes, cbWritten); 
          		throw runtime_error(sWriteErr);
			} 			
#endif     
		}
	} catch (exception& e) {
		if (bIsConnected)
			CloseHandle(client->hReqPipe);
		
		JetsonWriteLogs("<<< ERROR on eng_i_req for Client (%s, %d) (%s, %s): %s",
				sIpAddr, sock, sEngineName, sServIp, e.what());
	}

	if (bIsConnected)
		CloseHandle(client->hReqPipe);
	
	JetsonWriteLogs("<<< Exited eng_i_req from client(%s, %d) via (%s, %s)\n",
			sIpAddr, sock, sEngineName, sServIp);

	if (bDisconnectClientNeeded) {	
		pthread_mutex_lock(&gJetsonTableLock);
		client->bIsConnected = 0;
		pthread_mutex_unlock(&gJetsonTableLock);
	}
	return NULL;
}

static void *EngineInstanceResponseThread(void *data)
{
	struct ClientEntry *client = (struct ClientEntry *)data;
	SOCKET sock = client->sock;
	char *sIpAddr = client->sIpAddr;
	char *sEngineName = client->engine->sEngineName;
	char *sServIp = client->sServIpAddr;
	int bIsConnected = 0;
		
	JetsonWriteLogs(">>> Entered eng_i_rsp from client(%s, %d) via (%s, %s)\n",
			sIpAddr, sock, sEngineName, sServIp);	

	try {
#if defined(_WIN32)
		bIsConnected = ConnectNamedPipe(client->hRspPipe, NULL) ? 
			1 : (GetLastError() == ERROR_PIPE_CONNECTED); 
 
#else
		int fd = open(client->sRspPipe, O_RDONLY);
		if (fd >= 0) {
			client->hRspPipe = fd;//no need lock here
			bIsConnected = 1;
		}		
#endif
		if (!bIsConnected)
			throw runtime_error("Unable to connect response pipe\n");

		while (1) {
			char pipeReadBuf[RSP_BUFSIZE];
			memset(pipeReadBuf, 0, RSP_BUFSIZE);

			// Read client requests from the pipe. This simplistic code only allows messages
			// up to RSP_BUFSIZE characters in length.
      		//SleepMsec(300);
			int cbBytesRead = 0; // number of bytes read
			char sReadErr[128];
#if defined(_WIN32)			
			BOOL fSuccess = ReadFile(client->hRspPipe, pipeReadBuf, RSP_BUFSIZE*sizeof(TCHAR), (LPDWORD)&cbBytesRead, NULL); 

			if (!fSuccess || cbBytesRead == 0) {
				if (GetLastError() == ERROR_BROKEN_PIPE) {
					sprintf(sReadErr, "ReadFile failed, client disconnected, rval(%d), rbytes(%d), GLE=%d.\n",
						fSuccess, cbBytesRead, GetLastError()); 
				}
				else {
					sprintf(sReadErr, "ReadFile failed, rval(%d), rbytes(%d), GLE=%d.\n",
						fSuccess, cbBytesRead, GetLastError()); 
				}
				
				throw runtime_error(sReadErr);
			}
#else
			cbBytesRead = read(client->hRspPipe, pipeReadBuf, RSP_BUFSIZE);
			
			if (cbBytesRead <= 0) {
				sprintf(sReadErr, "ReadFile failed, rbytes(%d)\n", cbBytesRead); 
				throw runtime_error(sReadErr);
			}			
#endif
    		
			//-----hijack id name and save actual response string to oss
			//Note: pipeReadBuf is converted to ossSockWriteBuf
			ostringstream ossSockWriteBuf;
		
			if (strstr(pipeReadBuf, "id name")) {
				string sRspStr = pipeReadBuf;
				char *sIdStr = (char *)"id name ";
				size_t idPos = sRspStr.find(sIdStr);
				size_t insertPos = idPos + strlen(sIdStr);			
				string sOrigIdStr = sRspStr.substr(insertPos);
			
				ossSockWriteBuf << "id name " << gsJreHeader << client->sServIpAddr << "_" 
					<< client->engine->sEngineName << "##" << sOrigIdStr;
				cbBytesRead = ossSockWriteBuf.str().length();
			}
			else {
				ossSockWriteBuf << pipeReadBuf;
			}
		
			if (client->bIsDataLogOn) {
				//TODO: change a bit behavior (0d 0a) for neat output to log file but keep return message intact
			}
		
			send(client->sock, ossSockWriteBuf.str().c_str(), cbBytesRead, 0);
		}
	} catch (exception& e) {
		if (bIsConnected)
			CloseHandle(client->hRspPipe);
		
		JetsonWriteLogs("<<< ERROR on eng_i_rsp for Client (%s, %d) (%s, %s): %s",
				sIpAddr, sock, sEngineName, sServIp, e.what());	
	}
	
	if (bIsConnected)
		CloseHandle(client->hRspPipe);
	
	JetsonWriteLogs("<<< Exited eng_i_rsp from client(%s, %d) via (%s, %s)\n",
			sIpAddr, sock, sEngineName, sServIp);	
	return NULL;
}

void JetsonSignalHandler( int signal_num )
{ 	
	gbAgentExiting = 1;
	JetsonWriteLogs("<<<<<<<<<< Server terminated with sig(%d).\n", signal_num);
	JetsonWriteLogs("<<<<<<<<<<\n");
	exit(signal_num);
}

static void *EngineInstanceThread(void *data)
{
	struct ClientEntry *newClient = (struct ClientEntry *)data;
	SOCKET sock = newClient->sock;
	char *sIpAddr = newClient->sIpAddr;
	char *sEngineName = newClient->engine->sEngineName;
	char *sServIp = newClient->sServIpAddr;
	char *arguments = newClient->engine->arguments;

	JetsonWriteLogs(">>> Entered eng_i from client(%s, %d) via (%s, %s)\n",
			sIpAddr, sock, sEngineName, sServIp);

	try {	
		ostringstream ossCmdline;
		ostringstream ossLog;
	
		//get command line arguments
		ostringstream ossArgs;
		ossArgs.str(""); ossArgs.clear();
		string args = arguments;
		
		JetsonWriteLogs("engine args[%s]\n", args.c_str());
		
		if (args.length() > 0) {
			string delimiter = ":";
			size_t pos = 0;
			string token;
			ossArgs << " ";
			while ((pos = args.find(delimiter)) != string::npos) {
				token = args.substr(0, pos);
				ossArgs << token << " ";
				args.erase(0, pos + delimiter.length());
			}
			ossArgs << args << " ";
		}

#if defined(_WIN32)	
		ossCmdline << "cd " << newClient->engine->sEngineDir << " && " << newClient->sEngInstName << ossArgs.str()
				<< " < " << newClient->sReqPipe << " > " << newClient->sRspPipe;
#else
		ossCmdline << "cd " << newClient->engine->sEngineDir << "; ./" << newClient->sEngInstName << ossArgs.str()
				<< " < " << newClient->sReqPipe << " > " << newClient->sRspPipe;
#endif
		
		//system() call may not be a good idea
		JetsonWriteLogs(">>> (%s) is launched\n", ossCmdline.str().c_str());
	
		int rval = system(ossCmdline.str().c_str());
	
		JetsonWriteLogs("<<< (%s) is ended, rval=%d\n", ossCmdline.str().c_str(), rval);
	} catch (exception& e) {
		JetsonWriteLogs("<<< ERROR on eng_i for Client (%s, %d) (%s, %s): %s",
				sIpAddr, sock, sEngineName, sServIp, e.what());	
	}
	
	JetsonWriteLogs("<<< Exited eng_i from client(%s, %d) via (%s, %s)\n",
			sIpAddr, sock, sEngineName, sServIp);
	return NULL;
}	

static char *GetServIp(SOCKET sock)
{
	struct sockaddr_in localSin;
	socklen_t localSinLen = sizeof(localSin);
	getsockname(sock, (struct sockaddr*)&localSin, &localSinLen);
	return (inet_ntoa(localSin.sin_addr));
}

static int JetsonClientLogin(struct EngineEntry *engEntry, SOCKET sock, const char *sIpAddr, fd_set *pMaster)
{
	ostringstream ossReqPipe;
	ostringstream ossRspPipe;
	ostringstream ossParam;
	ostringstream ossNewEngExeName;

	try {
#if defined(_WIN32)
		HANDLE hReqPipe = INVALID_HANDLE_VALUE;
		HANDLE hRspPipe = INVALID_HANDLE_VALUE;
	
		ossReqPipe << "\\\\.\\pipe\\" << engEntry->sEngineName << "_req_" << sIpAddr;
		ossRspPipe << "\\\\.\\pipe\\" << engEntry->sEngineName << "_rsp_" << sIpAddr;
						
		JetsonWriteLogs("Creating request pipe (%s)\n", ossReqPipe.str().c_str());	
		hReqPipe = CreateNamedPipe( 
				ossReqPipe.str().c_str(),	// pipe name 
				PIPE_ACCESS_DUPLEX,			// read/write access 
				PIPE_TYPE_MESSAGE |			// message type pipe 
				PIPE_READMODE_MESSAGE |		// message-read mode 
				PIPE_WAIT,					// blocking mode 
				PIPE_UNLIMITED_INSTANCES,	// max. instances  
				PIPE_BUFSIZE,				// output buffer size 
				PIPE_BUFSIZE,				// input buffer size 
				0,							// client time-out 
				NULL);						// default security attribute 

		if (hReqPipe == INVALID_HANDLE_VALUE) {
			JetsonWriteLogs("CreateReqPipe (%s) failed, GLE=%d.\n", ossReqPipe.str().c_str(), GetLastError()); 
			throw runtime_error("CreateReqPipe failed\n");
		}

		JetsonWriteLogs("Creating response pipe, pipe_name=(%s)\n", ossRspPipe.str().c_str());	
		hRspPipe = CreateNamedPipe( 
			ossRspPipe.str().c_str(),					// pipe name 
			PIPE_ACCESS_DUPLEX,			// read/write access 
			PIPE_TYPE_MESSAGE |			// message type pipe 
			PIPE_READMODE_MESSAGE |		// message-read mode 
			PIPE_WAIT,					// blocking mode 
			PIPE_UNLIMITED_INSTANCES,	// max. instances  
			PIPE_BUFSIZE,				// output buffer size 
			PIPE_BUFSIZE,				// input buffer size 
			0,							// client time-out 
			NULL);						// default security attribute 

		if (hRspPipe == INVALID_HANDLE_VALUE) {
			JetsonWriteLogs("CreateRspPipe (%s) failed, GLE=%d.\n", ossRspPipe.str().c_str(), GetLastError()); 
			throw runtime_error("CreateRspPipe failed\n");
		}
						
		ossNewEngExeName << "jei_" << sIpAddr << "_" << engEntry->sEngineName << ".exe";
		ossParam << "cd " << engEntry->sEngineDir << " && "
			<< "copy " << engEntry->sEngineExeName << " ";	    				
#else
		int hReqPipe;
		int hRspPipe;
	
		ossReqPipe << engEntry->sEngineDir << engEntry->sEngineName << "_req_" << sIpAddr;
		ossRspPipe << engEntry->sEngineDir << engEntry->sEngineName << "_rsp_" << sIpAddr;

		JetsonWriteLogs("Creating request pipe, pipe_name=(%s)\n", ossReqPipe.str().c_str());	
		mkfifo(ossReqPipe.str().c_str(), 0666);

		JetsonWriteLogs("Creating response pipe, pipe_name=(%s)\n", ossRspPipe.str().c_str());	
		mkfifo(ossRspPipe.str().c_str(), 0666);

		ossNewEngExeName << "jei_" << sIpAddr << "_" << engEntry->sEngineName;
		ossParam << "cd " << engEntry->sEngineDir << "; "
			<< "cp " << engEntry->sEngineExeName << " ";
#endif

		JetsonWriteLogs("new engine instance name=(%s)\n", ossNewEngExeName.str().c_str());
	    				
		ossParam << ossNewEngExeName.str();
	
		//change permission to executable
#if !defined(_WIN32)
		ossParam << "; " << "chmod a+x " << ossNewEngExeName.str();
#endif
	    				
		JetsonWriteLogs("copy instance (%s)\n", ossParam.str().c_str());
	    				
		int rval = system(ossParam.str().c_str());
		if (rval != 0) {
			JetsonWriteLogs("ERROR: cmd exec rval=%d\n", rval);
			throw runtime_error("cmd exec failed\n");
		}

		//----- add new client login and engine instance
		//TODO: need to check duplicate client???
		struct ClientEntry *newClient = NULL;
		pthread_mutex_lock(&gJetsonTableLock);
		for (int i=0; i<MAX_NUM_LOGI_PER_ENGINE; i++) {
			struct ClientEntry *thisClient = &engEntry->clients[i];
		
			if (thisClient->bIsConnected)
				continue;
		
			//get a free entry
			thisClient->bIsConnected = 1;
			thisClient->sock = sock;
			strncpy(thisClient->sIpAddr, sIpAddr, MAX_NAME_LEN);
			strncpy(thisClient->sEngInstName, ossNewEngExeName.str().c_str(), MAX_NAME_LEN);
			thisClient->engine = engEntry;
			thisClient->hReqPipe = hReqPipe;
			thisClient->hRspPipe = hRspPipe;
			strncpy(thisClient->sReqPipe, ossReqPipe.str().c_str(), MAX_NAME_LEN);
			strncpy(thisClient->sRspPipe, ossRspPipe.str().c_str(), MAX_NAME_LEN);
			thisClient->pMaster = pMaster;
		
			char *sSrvIp = GetServIp(sock);
			strncpy(thisClient->sServIpAddr, sSrvIp, MAX_NAME_LEN);

			newClient = thisClient;
			break;
		}
		pthread_mutex_unlock(&gJetsonTableLock);

		if (newClient != NULL) {
			int rc;
			
			JetsonWriteLogs("connected socket = %d, from %s\n", sock, sIpAddr);

			pthread_t engineInstanceReqThreadId;
			rc = pthread_create(&engineInstanceReqThreadId, NULL, EngineInstanceRequestThread, (void *)newClient);
			if (rc != 0)
				throw runtime_error("Unable to create engine instance request thread\n");

			pthread_t engineInstanceRspThreadId;
			rc = pthread_create(&engineInstanceRspThreadId, NULL, EngineInstanceResponseThread, (void *)newClient);
			if (rc != 0)
				throw runtime_error("Unable to create engine instance response thread\n");
	    
			pthread_t engineInstanceThreadId;
			rc = pthread_create(&engineInstanceThreadId, NULL, EngineInstanceThread, (void *)newClient);
			if (rc != 0)
				throw runtime_error("Unable to create engine instance thread\n");
		}
	} catch (exception& e) {
  		//TODO: better to clean up resources here!!!
		JetsonWriteLogs("<<< ERROR on login for Client (%s, %d) (%s): %s",
				sIpAddr, sock, engEntry->sEngineName, e.what());	
	}
	
	return 0;
}

static int JetsonFindEngine(const char *sEngName)
{
	int bIsEngineExist = 0;
	
	pthread_mutex_lock(&gJetsonTableLock);
	for (int i=0; i<MAX_NUM_ENGINE; i++) {
		struct EngineEntry *thisEng = &gEngineTables[i];
		
		if (!thisEng->bIsAllocated)
			continue;
		
		if (strcmp(thisEng->sEngineName, sEngName) == 0)
		{
			bIsEngineExist = 1;
			break;
		}
	}
	pthread_mutex_unlock(&gJetsonTableLock);

	return bIsEngineExist;	
}

static struct EngineEntry* JetsonAddNewEngine(const char *sEngDir, const char *sEngExeName, 
		const char *sEngPort, const char *sEngName, const char *arguments)
{
	struct EngineEntry *pAddedEng = NULL;
	
	//FIXME: add new engine should be one time thing, need to check duplicate?
	pthread_mutex_lock(&gJetsonTableLock);
	for (int i=0; i<MAX_NUM_ENGINE; i++) {
		struct EngineEntry *thisEng = &gEngineTables[i];
		
		if (thisEng->bIsAllocated)
			continue;
		
		//get a free entry
		thisEng->bIsAllocated = 1;
		strncpy(thisEng->sEngineDir, sEngDir, MAX_NAME_LEN);
		strncpy(thisEng->sEngineName, sEngName, MAX_NAME_LEN);
		strncpy(thisEng->sEngineExeName, sEngExeName, MAX_NAME_LEN);
		strncpy(thisEng->sEngienPort, sEngPort, MAX_NAME_LEN);
		strncpy(thisEng->arguments, arguments, MAX_NAME_LEN);
		pAddedEng = thisEng;
		break;
	}
	pthread_mutex_unlock(&gJetsonTableLock);
	
	return pAddedEng;
}

static void JetsonScanAndLoadEngines(SOCKET sockClient, int bIsScan);
static void JetsonQueryEngines(SOCKET sockClient);
static int JetsonSocket(int sockType, const char *sEngDir, const char *sEngExeName, const char *sEngPort, const char *sEngName, const char *arguments)
{
	if (sockType != SOCK_TYPE_ENGINE &&
		sockType != SOCK_TYPE_MGMT) {
		JetsonWriteLogs("ERROR: invalid sockType(%d)\n", sockType);
		return 0;
	}
	
	if (sockType == SOCK_TYPE_MGMT)
		JetsonWriteLogs(">>> MGMT creating listening socket...\n");
	else
		JetsonWriteLogs(">>> Engine (%s) creating listening socket...\n", sEngName);
	
	int bIsSockListenValid = 0;
    SOCKET sockListen;
		
	try {
		struct addrinfo localAddr;
		memset(&localAddr, 0, sizeof(localAddr));
		localAddr.ai_family = AF_INET;
		localAddr.ai_socktype = SOCK_STREAM;
		localAddr.ai_flags = AI_PASSIVE;

		struct addrinfo *bindAddr;
		getaddrinfo(0, sEngPort, &localAddr, &bindAddr);

		sockListen = socket(bindAddr->ai_family,
				    bindAddr->ai_socktype, bindAddr->ai_protocol);
		if (!IsSockValid(sockListen)) {
			JetsonWriteLogs("socket() failed. (%d)\n", GetSockErrno());
			throw runtime_error("socket() failed\n");
		}
		bIsSockListenValid = 1;

		if (bind(sockListen, bindAddr->ai_addr, bindAddr->ai_addrlen)) {
			JetsonWriteLogs("bind() failed. (%d)\n", GetSockErrno());
			throw runtime_error("bind() failed\n");
		}
		freeaddrinfo(bindAddr);

		if (listen(sockListen, 10) < 0) {
			JetsonWriteLogs("listen() failed. (%d)\n", GetSockErrno());
			throw runtime_error("listen() failed\n");
		}

		fd_set master;
		FD_ZERO(&master);
		FD_SET(sockListen, &master);
		SOCKET maxSock = sockListen;

		struct EngineEntry *pNewEng = NULL;

		if (sockType == SOCK_TYPE_MGMT)
			printf("MGMT waiting for connections...\n");
		else {
			printf("Engine (%s) waiting for connections...\n", sEngName);
		
			pNewEng = JetsonAddNewEngine(sEngDir, sEngExeName, sEngPort, sEngName, arguments);
			if (pNewEng == NULL) {
				JetsonWriteLogs("Unable to add new engine for %s\n", sEngName);
				throw runtime_error("add engine failed\n");
			}
		}

		while(1) {
			struct timeval timeout;
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
				   	
			fd_set reads;
			reads = master;
			if (select(maxSock+1, &reads, 0, 0, &timeout) < 0) {
				JetsonWriteLogs("select() failed. (%d)\n", GetSockErrno());
				throw runtime_error("select() failed\n");
			}

			if (gbAgentExiting)
				break;

			SOCKET i;
			for(i = 1; i <= maxSock; ++i) {		
				if (gbAgentExiting)
					break;
					
				if (FD_ISSET(i, &reads)) {
					if (i == sockListen) {
						struct sockaddr_storage clientAddr;
						socklen_t clientLen = sizeof(clientAddr);
						
						SOCKET sockClient = accept(sockListen,
									(struct sockaddr*) &clientAddr,
									&clientLen);
						if (!IsSockValid(sockClient)) {
							JetsonWriteLogs("accept() failed. (%d)\n", GetSockErrno());
							throw runtime_error("accept() failed\n");
						}

						FD_SET(sockClient, &master);
						if (sockClient > maxSock)
							maxSock = sockClient;

						char sLocalIp[100];
						getnameinfo((struct sockaddr*)&clientAddr,
									clientLen,
									sLocalIp, sizeof(sLocalIp), 0, 0,
									NI_NUMERICHOST);
										
						char *sServIp = GetServIp(sockClient);

						if (sockType == SOCK_TYPE_MGMT)
							JetsonWriteLogs("MGMT Received new connection from %s via %s\n", sLocalIp, sServIp);
						else {
							JetsonWriteLogs("Engine (%s)(ServIP:%s) received new connection from %s\n", sEngName, sServIp, sLocalIp);
							JetsonClientLogin(pNewEng, sockClient, sLocalIp, &master);
						}
					}
					else {
						//nothing need to do here for engine, receiver in separate thread
						if (sockType == SOCK_TYPE_MGMT) {
							char sSockReadBuf[REQ_BUFSIZE];
							memset(sSockReadBuf, 0, REQ_BUFSIZE);
                    
							int bytesReceived = recv(i, sSockReadBuf, REQ_BUFSIZE, 0);
							if (bytesReceived < 1) {
								JetsonWriteLogs("MGMT closing socket (%d)\n", i);
                    	                    	
								FD_CLR(i, &master);
								CloseSocket(i);
							
								continue;
							}
    
							JetsonWriteLogs("MGMT received cmd=%s\n", sSockReadBuf);
                    
							if (strncmp(sSockReadBuf, "scan", 4) == 0)
								JetsonScanAndLoadEngines(i, 1);
							else if (strcmp(sSockReadBuf, "query") == 0)
								JetsonQueryEngines(i);
						}
					} //receive socket data
				} //if FD_ISSET
			} //for i to maxSock
		} //while(1)
		
		// before exiting out, cleanup all resources

	} catch (exception& e) {
		//TODO: better to clean up resources here!!!
		if (sockType == SOCK_TYPE_MGMT)
			JetsonWriteLogs("<<< ERROR on mgmt socket: %s", e.what());
		else	
			JetsonWriteLogs("<<< ERROR on engine socket: %s", e.what());	
	}
		
	if (bIsSockListenValid)
		CloseSocket(sockListen);
	
	if (sockType == SOCK_TYPE_MGMT)
		JetsonWriteLogs("<<< MGMT closing listening socket...\n");
	else
		JetsonWriteLogs("<<< Engine (%s) closing listening socket...\n", sEngName);

	return 0;
}

#if defined(_WIN32)
static bool FileExists(const string &fileName)
{
    return _access_s(fileName.c_str(), 0) == 0;
}
#endif

static void *EngineLaunchThread(void *data)
{
	struct EngineEntry *pTmpEngEntry = (struct EngineEntry *)data;
	char *sBackendDir = pTmpEngEntry->sEngineDir;
	char *sEngName = pTmpEngEntry->sEngineName;
	char *sEngExe = pTmpEngEntry->sEngineExeName;
	char *sEngPort = pTmpEngEntry->sEngienPort;
	char *arguments = pTmpEngEntry->arguments;
	
	ostringstream ossEngDir;
#if defined(_WIN32)
	ossEngDir << sBackendDir << "\\" << sEngName << "\\";
#else
	ossEngDir << sBackendDir << "/" << sEngName << "/";
#endif
	
	string sEngDir = ossEngDir.str();
	
	JetsonWriteLogs(">>> Entered eng_launch (%s)\n", sEngName);
	
	//check if engine exe exists
	ostringstream ossEngExeFullPath;
	ossEngExeFullPath << ossEngDir.str() << sEngExe;
	if (!FileExists(ossEngExeFullPath.str().c_str())) {
		JetsonWriteLogs("ERROR: engine executable path (%s) not exist\n", ossEngExeFullPath.str().c_str());		
	}
	else {
		if (JetsonFindEngine(sEngName)) {
			JetsonWriteLogs("Engine (%s) exist!\n", sEngName);
		}
		else {
			JetsonWriteLogs("Launching new engine (%s)\n", sEngName);
			JetsonSocket(SOCK_TYPE_ENGINE, sEngDir.c_str(), sEngExe, sEngPort, sEngName, arguments);
		}
	}
	
	JetsonWriteLogs("<<< Exited eng_launch for engine(%s)\n", sEngName);
	free(pTmpEngEntry);
	
	return NULL;
}	

static void JetsonQueryEngines(SOCKET sockClient)
{
	pthread_mutex_lock(&gJetsonTableLock);
	while (gbIsTableLockOn) {
		pthread_mutex_unlock(&gJetsonTableLock);
		SleepMsec(1000);
		pthread_mutex_lock(&gJetsonTableLock);
	}
	gbIsTableLockOn = 1;
	JetsonWriteLogs(">>> Client socket (%d) acquired lock to query engine...\n", sockClient);
	pthread_mutex_unlock(&gJetsonTableLock);

	try {			
		ostringstream oss;
	
		oss << "\n===== Engine Table Entries from Server (" << gsMyHostName
			<< ") OS-ARCH (" << gsMyOsArch << ")  =====\n";
				
		int engCnt=0;
			
		pthread_mutex_lock(&gJetsonTableLock);
		for (int i=0; i<MAX_NUM_ENGINE; i++) {
			struct EngineEntry *thisEng = &gEngineTables[i];
				
			if (!thisEng->bIsAllocated)
				continue;
				
			if (engCnt++ > 0)
				oss << "\n";
				
			//allocated engine
			oss << "Engine(" << thisEng->sEngineName << ") TCP Port(" << thisEng->sEngienPort << ")\n";
			oss << "   " << "Executable On Server(" << thisEng->sEngineDir << thisEng->sEngineExeName << ")\n";
			oss << "   " << "Connected Users:\n";
				
			for (int j=0; j<MAX_NUM_LOGI_PER_ENGINE; j++) {
				struct ClientEntry *thisClient = &thisEng->clients[j];
					
				if (!thisClient->bIsConnected)
					continue;
					
				//connected client
				oss << "      * Client IP[" << thisClient->sIpAddr << "] Socket(" 
					<< thisClient->sock << ") Server IP[" << thisClient->sServIpAddr << "] "
					<< "Engine Instance(" << thisClient->sEngInstName << ")\n";
			}
		}
			
		oss << "================================<<<querydone\n\n";
		pthread_mutex_unlock(&gJetsonTableLock);
			
		string sRetStr = oss.str();
			
		send(sockClient, sRetStr.c_str(), sRetStr.length(), 0);
			
		JetsonWriteLogs("<<< Engine query done, lock released by client socket %d.\n", sockClient);	
	} catch (exception& e) {
		JetsonWriteLogs("<<< ERROR on engine query for socket (%d): %s", sockClient, e.what());			
	}
  
	pthread_mutex_lock(&gJetsonTableLock);
	gbIsTableLockOn = 0;
	pthread_mutex_unlock(&gJetsonTableLock);

	return;
}
static void JetsonScanAndLoadEngines(SOCKET sockClient, int bIsScan)
{
	pthread_mutex_lock(&gJetsonTableLock);
	while (gbIsTableLockOn) {
		pthread_mutex_unlock(&gJetsonTableLock);
		SleepMsec(1000);
		pthread_mutex_lock(&gJetsonTableLock);
	}
	gbIsTableLockOn = 1;
	JetsonWriteLogs(">>> Client socket (%d) acquired lock to scan/load engine...\n", (bIsScan ? sockClient : 0));
	pthread_mutex_unlock(&gJetsonTableLock);

	try {
		char *sServIp = NULL;
	
		if (bIsScan) {
			sServIp = GetServIp(sockClient);
			JetsonWriteLogs("[MGMT] Local IP is (%s)\n", sServIp);
		}
	
		char cCurrentPath[MAX_NAME_LEN];
		if (!GetCurrDir(cCurrentPath, sizeof(cCurrentPath))) {
			throw runtime_error("ERROR: failed to get current path\n");
		}
		cCurrentPath[sizeof(cCurrentPath) - 1] = '\0'; /* not really required */

		string line;
		ifstream myAgentFile(gsAgentConfFile);
		if (myAgentFile) {
			while (getline( myAgentFile, line )) {
				const char *sLineStr = line.c_str();
			
				//skip comments, whitespaces, or empty lines
				if (sLineStr[0] == '#' || isspace(sLineStr[0]) || line.empty() )
					continue;
			
				string sEngName, port, sEngExe, args;
				istringstream iss(line);
				iss >> sEngName >> port >> sEngExe >> args;
			
#if defined(_WIN32)			
				if (!strstr(sEngExe.c_str(), ".exe")) {
					JetsonWriteLogs("%s doesn't have exe\n", sEngExe.c_str());
					ostringstream oss1;
					oss1 << sEngExe << ".exe";
					sEngExe = oss1.str();
				}
#endif
				//check if engine folder exists
				if (!FileExists(sEngName.c_str()))
				{
					JetsonWriteLogs("ERROR: Engine folder(%s) doesn't exist\n", sEngName.c_str());
					continue;
				}
				
				JetsonWriteLogs("Engine n(%s) p(%s) exe(%s) arg(%s)\n",
					sEngName.c_str(), port.c_str(), sEngExe.c_str(), args.c_str());
				//launch individual engine thread
				struct EngineEntry *pTmpEngEntry = (struct EngineEntry *)malloc(sizeof(struct EngineEntry));
				strncpy(pTmpEngEntry->sEngineDir, cCurrentPath, MAX_NAME_LEN);//ATTN: at this moment it is only backend dir
				strncpy(pTmpEngEntry->sEngineName, sEngName.c_str(), MAX_NAME_LEN);
				strncpy(pTmpEngEntry->sEngineExeName, sEngExe.c_str(), MAX_NAME_LEN);
				strncpy(pTmpEngEntry->sEngienPort, port.c_str(), MAX_NAME_LEN);
				strncpy(pTmpEngEntry->arguments, args.c_str(), MAX_NAME_LEN);

				pthread_t launch_thread_id;
				int rc = pthread_create(&launch_thread_id, NULL, EngineLaunchThread, (void *)pTmpEngEntry);
				if (rc != 0) {
					JetsonWriteLogs("Unable to create launch thread for engine(%s)\n", sEngName.c_str());
					continue;
				}
			
				SleepMsec(50);
			
				if (bIsScan) {
					ostringstream oss;
					oss << gsJreHeader << sServIp << "_" << port.c_str() << "_" << sEngName.c_str() << "\n";
					string sRetStr = oss.str();
				
					send(sockClient, sRetStr.c_str(), sRetStr.length(), 0);
					//SleepMsec(300);
				}
			}
		
			if (bIsScan) {
				char *scan_done = (char *)"scanisdone\n";
				send(sockClient, "scanisdone", strlen(scan_done), 0);
			}
		
			myAgentFile.close();
		
			JetsonWriteLogs("<<< Engine scan done, lock released by client socket\n", sockClient);
		}
	} catch (exception& e) {	
		
    	JetsonWriteLogs("<<< ERROR on engine scan for socket (%d): %s", sockClient, e.what());	
	}
  
	pthread_mutex_lock(&gJetsonTableLock);
	gbIsTableLockOn = 0;
	pthread_mutex_unlock(&gJetsonTableLock);

	return;
}

static void *JetsonMgmtThread(void *data)
{
	JetsonWriteLogs(">>> Entered JetsonMgmtThread\n");
	JetsonSocket(SOCK_TYPE_MGMT, NULL, NULL, gsMgmtPortStr.c_str(), NULL, NULL);
	JetsonWriteLogs("<<< Exited JetsonMgmtThread\n");
	
	return NULL;
}	

int main()
{
	int rc = 1;

	try {
#if defined(_WIN32)
		strcpy(gsMyOsArch, STR_OS_ARCH_WIN);
		gOsArchId = OS_ARCH_WINDOWS_X64;
		strcpy(gsJreHeader, STR_JRE_HDR_WIN);
	
		WSADATA d;
		if (WSAStartup(MAKEWORD(2, 2), &d)) {
			throw runtime_error("Failed to initialize.\n");
		}
		
		gethostname(gsMyHostName, sizeof(gsMyHostName));		
#else
		struct utsname buffer;

		if (uname(&buffer) != 0) {
			throw runtime_error("error on uname\n");
		}
   
		if (strcmp(buffer.machine, "x86_64") == 0) {
			strcpy(gsMyOsArch, STR_OS_ARCH_LINUX);
			gOsArchId = OS_ARCH_LINUX_X64;
			strcpy(gsJreHeader, STR_JRE_HDR_LINUX);
		}
		else if (strcmp(buffer.machine, "aarch64") == 0) {
			strcpy(gsMyOsArch, STR_OS_ARCH_XAVIER);
			gOsArchId = OS_ARCH_XAVIER_ARM64;
			strcpy(gsJreHeader, STR_JRE_HDR_XAVIER);
		}
	
		strcpy(gsMyHostName, buffer.nodename);
#endif

		JetsonWriteLogs(">>>>>>>>>>\n");
		JetsonWriteLogs(">>>>>>>>>> Server started ver=%s\n", BUILD_NUMBER);

		/* set signal blocking */	
		signal(SIGABRT, JetsonSignalHandler); 
		signal(SIGINT, JetsonSignalHandler); 
		signal(SIGTERM, JetsonSignalHandler);
	
		pthread_mutex_init(&gLogFileLock, NULL);
		pthread_mutex_init(&gJetsonTableLock, NULL);
		memset((void *)gEngineTables, 0, sizeof(EngineEntry) * MAX_NUM_ENGINE);
		JetsonWriteLogs("total engine table size = %ld\n", sizeof(EngineEntry) * MAX_NUM_ENGINE);
		
		//----- load customized mgmt port -----
		ifstream myMgmtPortFile(gsMgmtPortFile);
		if (myMgmtPortFile) {
			string mgmtPortStr;
			myMgmtPortFile >> mgmtPortStr;
			if (mgmtPortStr != "" && mgmtPortStr != gsMgmtPortStr) {
				gsMgmtPortStr = mgmtPortStr;
			}
			myMgmtPortFile.close();
		}
		JetsonWriteLogs("MGMT TCP Port = %s\n", gsMgmtPortStr.c_str());
	
		//----- launch management thread for scan and query -----
		pthread_t mgmtThreadId;
		rc = pthread_create(&mgmtThreadId, NULL, JetsonMgmtThread, NULL);
		if (rc != 0)
			throw runtime_error("Unable to create management thread\n");
	
		//----- load jetson_agent.conf file and launch socket thread for each engine
		SOCKET dummySock;
		JetsonScanAndLoadEngines(dummySock, 0);

		//TODO: join all threads?
	
		//----- main program sleep forever -----
		while (1) {
			if (gbAgentExiting)
        			break;
			SleepMsec(3000);
		}

		JetsonWriteLogs("<<<<<<<<<< Server stopped\n");
		JetsonWriteLogs("<<<<<<<<<<\n");

		pthread_mutex_destroy(&gJetsonTableLock);
		pthread_mutex_destroy(&gLogFileLock);
		//ATTN: any other resource need cleanup here???
		
		rc = 0;
	} catch (exception& e) {
		JetsonWriteLogs("<<< ERROR on Main: %s", e.what());
	}

#if defined(_WIN32)
    WSACleanup();
#endif

	return rc;
}
