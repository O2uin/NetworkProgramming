/*
* 조건
* 1.IPv4, IPv6 TCP/UDP 동시 제공(총 4버전)
* 2.클라이언트 GUI제작(루프백 사용)
* 3.기능:서버 접속, 파일 전송, 메시지 전송, 모든 클라이언트 실시간 그리기/지우기, 2개 클라이언트 GUI
* 4.서버는 공인 ip주소 사용(채팅)-180.69.249.39
*/

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32")

#define SERVERPORT 9000
#define BUFSIZE    256

void err_quit(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char*)&lpMsgBuf, 0, NULL);
	MessageBoxA(NULL, (const char*)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

void err_display(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char*)&lpMsgBuf, 0, NULL);
	printf("[%s] %s\n", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

void err_display(int errcode)
{
	LPVOID lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, errcode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char*)&lpMsgBuf, 0, NULL);
	printf("[오류] %s\n", (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}


typedef struct _SOCKETINFO
{//소켓 정보 저장
	SOCKET sock;
	bool   isIPv6;
	bool   isUDP;
	char   buf[BUFSIZE + 1];
	int    recvbytes;
} SOCKETINFO;

int nTotalSockets = 0;
SOCKETINFO* SocketInfoArray[FD_SETSIZE];

bool AddSocketInfo(SOCKET sock, bool isIPv6, bool isUDP);
void RemoveSocketInfo(int nIndex);

int main(int argc, char* argv[])
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	//소켓 초기화
	//TCP_IPv4
	SOCKET listen_sock4 = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock4 == INVALID_SOCKET) err_quit("socket()");

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);//여기에 공인 ip를 넣는건가?
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock4, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	retval = listen(listen_sock4, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");


	//TCP_IPv6
	SOCKET listen_sock6 = socket(AF_INET6, SOCK_STREAM, 0);
	if (listen_sock6 == INVALID_SOCKET) err_quit("socket()");

	struct sockaddr_in6 serveraddr6;
	memset(&serveraddr6, 0, sizeof(serveraddr6));
	serveraddr6.sin6_family = AF_INET6;
	serveraddr6.sin6_addr = in6addr_any;
	serveraddr6.sin6_port = htons(SERVERPORT);
	retval = bind(listen_sock6, (struct sockaddr*)&serveraddr6, sizeof(serveraddr6));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	retval = listen(listen_sock6, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");


	//UDP_IPv4
	SOCKET udp_sock4 = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_sock4 == INVALID_SOCKET) err_quit("socket()");

	struct sockaddr_in u_serveraddr4;
	memset(&u_serveraddr4, 0, sizeof(u_serveraddr4));
	u_serveraddr4.sin_family = AF_INET;
	u_serveraddr4.sin_addr.s_addr = htonl(INADDR_ANY);
	u_serveraddr4.sin_port = htons(SERVERPORT);
	retval = bind(udp_sock4, (struct sockaddr*)&u_serveraddr4, sizeof(u_serveraddr4));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	//UDP_IPv6
	SOCKET udp_sock6 = socket(AF_INET6, SOCK_DGRAM, 0);
	if (udp_sock6 == INVALID_SOCKET) err_quit("socket()");

	struct sockaddr_in6 u_serveraddr6;
	memset(&u_serveraddr6, 0, sizeof(u_serveraddr6));
	u_serveraddr6.sin6_family = AF_INET6;
	u_serveraddr6.sin6_addr = in6addr_any;
	u_serveraddr6.sin6_port = htons(SERVERPORT);
	retval = bind(udp_sock6, (struct sockaddr*)&u_serveraddr6, sizeof(u_serveraddr6));
	if (retval == SOCKET_ERROR) err_quit("bind()");


	// 데이터 통신에 사용할 변수(공통)
	fd_set rset;
	SOCKET client_sock;
	int addrlen;
	// 데이터 통신에 사용할 변수(IPv4)
	struct sockaddr_in clientaddr4;
	// 데이터 통신에 사용할 변수(IPv6)
	struct sockaddr_in6 clientaddr6;

	char buf[BUFSIZE + 1];

	while (1) {
		FD_ZERO(&rset);
		FD_SET(listen_sock4, &rset);
		FD_SET(listen_sock6, &rset);
		for (int i = 0; i < nTotalSockets; i++) {
			FD_SET(SocketInfoArray[i]->sock, &rset);
		}

		retval = select(0, &rset, NULL, NULL, NULL);
		if (retval == SOCKET_ERROR) err_quit("select()");


		if (FD_ISSET(listen_sock4, &rset)) {  // TCP/IPv4
			addrlen = sizeof(clientaddr4);
			client_sock = accept(listen_sock4,
				(struct sockaddr*)&clientaddr4, &addrlen);
			if (client_sock == INVALID_SOCKET) {
				err_display("accept()");
				break;
			}
			else {
				char addr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &clientaddr4.sin_addr, addr, sizeof(addr));
				printf("\n[TCP/IPv4 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
					addr, ntohs(clientaddr4.sin_port));

				if (!AddSocketInfo(client_sock, false, false))
					closesocket(client_sock);
			}
		}
		if (FD_ISSET(listen_sock6, &rset)) { // TCP/IPv6
			addrlen = sizeof(clientaddr6);
			client_sock = accept(listen_sock6,
				(struct sockaddr*)&clientaddr6, &addrlen);
			if (client_sock == INVALID_SOCKET) {
				err_display("accept()");
				break;
			}
			else {
				// 클라이언트 정보 출력
				char addr[INET6_ADDRSTRLEN];
				inet_ntop(AF_INET6, &clientaddr6.sin6_addr, addr, sizeof(addr));
				printf("\n[TCP/IPv6 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
					addr, ntohs(clientaddr6.sin6_port));
				// 소켓 정보 추가: 실패 시 소켓 닫음
				if (!AddSocketInfo(client_sock, true, false))
					closesocket(client_sock);
			}
		}
		if (FD_ISSET(udp_sock4, &rset)) {// UDP/IPv4
			
			addrlen = sizeof(clientaddr4);
			retval = recvfrom(udp_sock4, buf, BUFSIZE, 0,
				(struct sockaddr*)&clientaddr4, &addrlen);
			if (retval == SOCKET_ERROR) {
				err_display("recvfrom()");
				break;
			}			
				
			buf[retval] = '\0';			
			char addr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &clientaddr4.sin_addr, addr, sizeof(addr));
			printf("\n[UDP/IPv4 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
				addr, ntohs(clientaddr4.sin_port));

			retval = sendto(udp_sock4, buf, retval, 0,
				(struct sockaddr*)&clientaddr4, sizeof(clientaddr4));
			if (retval == SOCKET_ERROR) {
				err_display("sendto()");
				break;	
			}

			if (!AddSocketInfo(udp_sock4, false, true))
				closesocket(udp_sock4);
		}
		if (FD_ISSET(udp_sock6, &rset)) {// UDP/IPv6
			addrlen = sizeof(clientaddr6);
			retval = recvfrom(udp_sock6, buf, BUFSIZE, 0,
				(struct sockaddr*)&clientaddr6, &addrlen);
			if (retval == SOCKET_ERROR) {
				err_display("recvfrom()");
				break;
			}

			buf[retval] = '\0';
			char addr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET6, &clientaddr6.sin6_addr, addr, sizeof(addr));
			printf("\n[UDP/IPv6 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
				addr, ntohs(clientaddr6.sin6_port));

			retval = sendto(udp_sock6, buf, retval, 0,
				(struct sockaddr*)&clientaddr6, sizeof(clientaddr6));
			if (retval == SOCKET_ERROR) {
				err_display("sendto()");
				break;
			}
			if (!AddSocketInfo(udp_sock6, true, true))
				closesocket(udp_sock6);
		}

		// 소켓 셋 검사(2): 데이터 통신
		for (int i = 0; i < nTotalSockets; i++) {
			SOCKETINFO* ptr = SocketInfoArray[i];
			if (FD_ISSET(ptr->sock, &rset)) {
				// 데이터 받기
				retval = recv(ptr->sock, ptr->buf + ptr->recvbytes,
					BUFSIZE - ptr->recvbytes, 0);
				if (retval == 0 || retval == SOCKET_ERROR) {
					RemoveSocketInfo(i);
					continue;
				}

				// 받은 바이트 수 누적
				ptr->recvbytes += retval;

				if (ptr->recvbytes == BUFSIZE) {
					// 받은 바이트 수 리셋
					ptr->recvbytes = 0;
					// 현재 접속한 모든 클라이언트에 데이터 전송
					for (int j = 0; j < nTotalSockets; j++) {
						SOCKETINFO* ptr2 = SocketInfoArray[j];
						retval = send(ptr2->sock, ptr->buf, BUFSIZE, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(j);
							--j; // 루프 인덱스 보정
							continue;
						}
					}
				}
			}
		} /* end of for */
	} /* end of while (1) */

	// 소켓 닫기
	closesocket(listen_sock4);

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 소켓 정보 추가
bool AddSocketInfo(SOCKET sock, bool isIPv6, bool isUDP)
{
	if (nTotalSockets >= FD_SETSIZE) {
		printf("[오류] 소켓 정보를 추가할 수 없습니다!\n");
		return false;
	}
	SOCKETINFO* ptr = new SOCKETINFO;
	if (ptr == NULL) {
		printf("[오류] 메모리가 부족합니다!\n");
		return false;
	}
	ptr->sock = sock;
	ptr->isIPv6 = isIPv6;
	ptr->isUDP = isUDP;
	ptr->recvbytes = 0;
	SocketInfoArray[nTotalSockets++] = ptr;
	return true;
}

// 소켓 정보 삭제
void RemoveSocketInfo(int nIndex)
{
	SOCKETINFO* ptr = SocketInfoArray[nIndex];
	if (ptr->isUDP == false) {
		//tcp remove
		if (ptr->isIPv6 == false) {
			// 클라이언트 정보 얻기
			struct sockaddr_in clientaddr;
			int addrlen = sizeof(clientaddr);
			getpeername(ptr->sock, (struct sockaddr*)&clientaddr, &addrlen);
			// 클라이언트 정보 출력
			char addr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
			printf("[TCP/IPv4 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
				addr, ntohs(clientaddr.sin_port));
		}
		else {
			// 클라이언트 정보 얻기
			struct sockaddr_in6 clientaddr;
			int addrlen = sizeof(clientaddr);
			getpeername(ptr->sock, (struct sockaddr*)&clientaddr, &addrlen);
			// 클라이언트 정보 출력
			char addr[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &clientaddr.sin6_addr, addr, sizeof(addr));
			printf("[TCP/IPv6 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
				addr, ntohs(clientaddr.sin6_port));
		}
	}
	else {
		//udp remove
		if (ptr->isIPv6 == false) {
			// 클라이언트 정보 얻기
			struct sockaddr_in clientaddr;
			int addrlen = sizeof(clientaddr);
			getpeername(ptr->sock, (struct sockaddr*)&clientaddr, &addrlen);
			// 클라이언트 정보 출력
			char addr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
			printf("[UDP/IPv4 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
				addr, ntohs(clientaddr.sin_port));
		}
		else {
			// 클라이언트 정보 얻기
			struct sockaddr_in6 clientaddr;
			int addrlen = sizeof(clientaddr);
			getpeername(ptr->sock, (struct sockaddr*)&clientaddr, &addrlen);
			// 클라이언트 정보 출력
			char addr[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &clientaddr.sin6_addr, addr, sizeof(addr));
			printf("[UDP/IPv6 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
				addr, ntohs(clientaddr.sin6_port));
		}
	}
	// 소켓 닫기
	closesocket(ptr->sock);
	delete ptr;

	if (nIndex != (nTotalSockets - 1))
		SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets - 1];
	--nTotalSockets;
}