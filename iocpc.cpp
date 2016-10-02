
#include <stdio.h>
#include <process.h>
#include <assert.h>
#include <list>
#include <Winsock2.h>
#include <mstcpip.h>
#include <mswsock.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock")


void makestr(char *buf, int len) {
    buf = new char[len];
    for (int i = 0; i < len; i++) {
		buf[i] = '0' + i % 5 + 1;
    }
}


int main() {

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    int csock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (csock == 0) {
        assert(false);
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("192.168.221.1");
    addr.sin_port = htons(9100);

    int err = connect(csock, (const struct sockaddr *)&addr, sizeof(addr));
	if (err == SOCKET_ERROR) {
		int lasterr = WSAGetLastError();
		assert(false);
	}
        
	int recvcounter = 0;

    while (true) {

        int cmd = 0;
        char actoin[10];

        memset(actoin, 0, sizeof(actoin));

		scanf_s("%s %d", actoin, 10, &cmd);

        if (strcmp(actoin, "send") == 0) {
            char *buf = NULL;
            makestr(buf, cmd);

            printf_s(buf, cmd, "send str %s \n", buf);
            err = send(csock, buf, cmd, 0);
            if (err == 0)
                assert(false);

            delete []buf;

        } else if (strcmp(actoin, "recv") == 0) {
#define TRIGGER_BUF_SIZE (64*1024)
			char buf[TRIGGER_BUF_SIZE] = { 0 };
			int bytes = recv(csock, buf, TRIGGER_BUF_SIZE, 0);

			if (bytes == TRIGGER_BUF_SIZE)
				printf("recv counter : %d", ++recvcounter);
			else
				assert(false);
			//sprintf_s(frmt, 2048, "data: %d %s \n", bytes, buf);
			//printf("%s\n", frmt);
			//closesocket(csock);
        } else if (strcmp(actoin, "close") == 0) {
            closesocket(csock);
        }
    }

    return 0;
}

