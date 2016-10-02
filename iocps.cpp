

#include <stdio.h>
#include <process.h>
#include <assert.h>
#include <list>
#include <Winsock2.h>
#include <mstcpip.h>
#include <mswsock.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock")

enum {
	TRIGGER_NONE,
	TRIGGER_ACCEPT,
	TRIGGER_CONNECT,
	TRIGGER_SEND,
	TRIGGER_RECV
};

enum {
	IO_OK,
	IO_ERR
};

#define TRIGGER_BUF_SIZE (64*1024)

static int sendcounter;

static int oseri;

static int sendoverlapped = 1;

struct trigger {

    trigger() {
        type = TRIGGER_NONE;
        state = IO_OK;
        trans = 0;
        err = 0;
        sock = 0;
    }

    void prepare() {
		memset(&ol, 0, sizeof(ol));
        state = IO_OK;
        err = 0;
        trans = 0;
    }

    OVERLAPPED      ol;
    int             type;
    int             state;
    int             err;
    int             trans;
    int             sock;
	char            buf[TRIGGER_BUF_SIZE];
    int             size;
    int             seri;
	char			abuf[128];
};

struct autolock{
    autolock() {
		InitializeCriticalSection(&locker);
    }
    ~autolock() {
		DeleteCriticalSection(&locker);
    }
    void lock() {
		EnterCriticalSection(&locker);
    }
    void unlock() {
		LeaveCriticalSection(&locker);
    }
    CRITICAL_SECTION locker;
};

struct scopelocker {
    scopelocker(autolock *l) {
		locker = l;
		locker->lock();
    }
    ~scopelocker() {
		locker->unlock();
    }
    autolock    *locker;
};


int         lsock = 0;
trigger     *at;
HANDLE      port;
HANDLE      threadlist[4];
typedef std::list<trigger *>  tlist;
tlist       tactivelist;
tlist       tfreelist;
autolock    ollock;

trigger* gettriger() {
    scopelocker locker(&ollock);
	printf("thread get locker : %d \n", GetCurrentThreadId());
    trigger *t = NULL;
	if (tfreelist.size() == 0) {
		t = new trigger();
	} else {
        t = tfreelist.front();
        tfreelist.pop_front();
    }
    tactivelist.push_back(t);
	printf("thread free locker : %d \n", GetCurrentThreadId());
	printf("get overlapped addr : %08x \n", t);
    return t;
}

struct conninfo {
    int         host;
    int         peer;
};

void makestr(char *buf, int len) {
    for (int i = 0; i < len; i++) {
        buf[i] = '0'+i%5+1;
    }
}

void freetrigger(trigger *t) {
    scopelocker locker(&ollock);
    trigger *t1 = NULL;
    for (tlist::iterator ite = tactivelist.begin(); ite != tactivelist.end(); ++ite) {
        if ((trigger *)*ite == t) {
            tactivelist.erase(ite);
            t1 = t;
            break;
        }
    }

    if (t1 == NULL)
        assert(false);
	printf("free overlapped addr : %08x \n", t1);
    tfreelist.push_back(t1);
}

int sockapi_create() {
	int s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (s == INVALID_SOCKET) {
		return 0;
	}
	return s;
}


conninfo getacceptexaddr() {
    static LPFN_GETACCEPTEXSOCKADDRS lpfgetaddrex = NULL;
    static GUID guidaddrex = WSAID_GETACCEPTEXSOCKADDRS;
    if (lpfgetaddrex == NULL) {
        DWORD bytes = 0;
        int err = WSAIoctl(at->sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
                &guidaddrex,
                sizeof(guidaddrex),
                &lpfgetaddrex,
                sizeof(lpfgetaddrex),
                (DWORD *)&bytes,
                0,
                0);
        int lasterr = WSAGetLastError();
        if (err == SOCKET_ERROR) {
            assert(false);
        }
    }

    struct sockaddr_in *remote = NULL;
    struct sockaddr_in *local = NULL;
    int addrlen = sizeof(struct sockaddr_in);
	lpfgetaddrex(at->abuf + 4, 0, addrlen + 16, addrlen + 16, 
		(struct sockaddr **)&local, &addrlen, 
		(struct sockaddr **)&remote, &addrlen);

	conninfo info = { local->sin_addr.s_addr, remote->sin_addr.s_addr };
	return info;
}

static int post_accept() {
    static LPFN_ACCEPTEX acceptfunc = NULL;
    if (acceptfunc == NULL) {
        int bytes = 0;
        GUID guid_accept = WSAID_ACCEPTEX;

		int err = WSAIoctl(lsock, SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guid_accept,
                    sizeof(guid_accept),
                    &acceptfunc,
                    sizeof(acceptfunc),
                    (DWORD *)&bytes,
                    0,
                    0
                );
    }

    int client = sockapi_create();
    
    at->prepare();
    at->type = TRIGGER_ACCEPT;
    at->sock = lsock;

    int *c = (int *)at->abuf;
    *c = client;
    char *buf = (char *)(c + 1);
    
	int addrsize = sizeof(sockaddr)+16;
	int err = acceptfunc(lsock, client, buf, 0, addrsize, addrsize, (DWORD *)&at->trans, (OVERLAPPED *)at);
	int lasterr = WSAGetLastError();
    if (err == 0) {
        if (WSAGetLastError() != WSA_IO_PENDING)
            assert(false);
//         else
//             assert(false);
    }
    
    return 0;
}


static int post_send(trigger *t) {
    printf("post_send thread:%u sock:%d seri:%d \n", GetCurrentThreadId(), t->sock, t->seri);
    WSABUF wsb;
    wsb.buf = t->buf;
    wsb.len = t->size;

    t->prepare();
    t->type = TRIGGER_SEND;
    
	printf("send counter %d \n", InterlockedAdd((long *)&sendcounter, 1));
    int err = WSASend(t->sock, &wsb, 1, (DWORD *)&(t->trans), 0, (OVERLAPPED *)t, NULL);
	int lasterr = WSAGetLastError();
	if (err == SOCKET_ERROR) {
		if (lasterr != WSA_IO_PENDING)
            printf("send err not pending \n ");
		else {
			printf("send pending \n");
			t->buf[0] = 1;
			//assert(false);
			err = 10101;
		}
             
	}
	else 
		err = 0;

	return err;
}

static int post_recv(trigger *t) {
    printf("post_recv thread:%u t->sock:%d seri:%d \n", GetCurrentThreadId(), t->sock, t->seri);
    WSABUF wsb;
    wsb.buf = t->buf;
	wsb.len = TRIGGER_BUF_SIZE;

    t->prepare();
    t->type = TRIGGER_RECV;
    
    int flag = 0;
    int err = WSARecv(t->sock, &wsb, 1, (DWORD *)&(t->trans), (DWORD *)&flag, (OVERLAPPED *)t, NULL);
	int lasterr = WSAGetLastError();
    if (err == SOCKET_ERROR) {
		if (lasterr != WSA_IO_PENDING)
            printf("recv err not pending \n");
        else {
            printf("recv err pending \n");
        }
    }

    return 0;
}

void onrecv(trigger *t) {
    printf_s(t->buf, sizeof(t->buf), "recv msg : %s \n", t->buf);
}

void onsend(trigger *t) {

}


int send_noenoughbuf(trigger *t) {

    __int64 val = 5;
	int len = sizeof(__int64);

	getsockopt(t->sock, SOL_SOCKET, SO_SNDBUF, (char *)&val, &len);

	val = 5;
    int err = setsockopt(t->sock, SOL_SOCKET, SO_SNDBUF, (const char*)&val, sizeof(val));

	__int64 val1 = 0;
	getsockopt(t->sock, SOL_SOCKET, SO_SNDBUF, (char *)&val1, &len);

	t->size = TRIGGER_BUF_SIZE;
    makestr(t->buf, t->size);
    err = post_send(t);
	return err;
}

void handleio(trigger *t) {

    int err;
    if (t->type == TRIGGER_ACCEPT) {

		int *csock = (int *)t->abuf;
		getacceptexaddr();

        printf("accept sock %d \n", *csock);

		CreateIoCompletionPort((HANDLE)*csock, port, 0, 0);

		trigger *client;

		client = gettriger();
		client->sock = *csock;
        client->seri = InterlockedAdd((long *)&oseri, 1);
        err = post_recv(client);

        client = gettriger();
		client->sock = *csock;
        client->seri = InterlockedAdd((long *)&oseri, 1);
		err = send_noenoughbuf(client);
        
        //post_recv(client);
        //err = post_accept();
    } else if (t->type == TRIGGER_RECV) {
        if (t->state == 0) {
            trigger *t1 = gettriger();
            t1->sock = t->sock;
            t1->seri = InterlockedAdd((long *)&oseri, 1); 
        } else {
            freetrigger(t);
        }
        
        //onrecv(t);
		err = 0;
    } else if (t->type == TRIGGER_SEND) {

        if (t->state == 0) {
            trigger *t1 = gettriger();
		    t1->sock = t->sock;
		    t1->seri = InterlockedAdd((long *)&oseri, 1);

            err = send_noenoughbuf(t1);
            if (err == 10101) {
                 if (sendoverlapped-- > 0)
                     handleio(t);
            }
        } else {
            freetrigger(t);
        }
        
		err = 0;
    }

    if (err)
        assert(false);
}

static unsigned int __stdcall threadfunc(void *param) {

    while (true) {
        trigger *t = NULL;
        OVERLAPPED *ol = NULL;
        DWORD trans = 0;
        ULONG_PTR key = 0;
        int err = 0;

        err = GetQueuedCompletionStatus(port, (DWORD *)&trans, &key, &ol, INFINITE);
        t = (trigger *)ol;

		//printf("\n\niocp : %u \n", GetCurrentThreadId());
		if (t)
			//printf("iocp operation sock:%d seri:%d \n", t->sock, t->seri);
			NULL;
		else
			//printf("iocp NULL operation sock:%d seri:%d \n");
			NULL;

		if (key == (ULONG_PTR)-1 && ol == NULL) {
			assert(false);
			break;
		}

		int lasterr = WSAGetLastError();
        //printf("iocp last err:%d \n",lasterr);
			
        if (!err) {
            if (!t)
                break;
            else {
                t->state = IO_ERR;
                t->trans = trans;
                t->err = WSAGetLastError();
            }
        } else {
            t->state = IO_OK;
            t->trans = trans;
            t->err = WSAGetLastError();
        }

        handleio(t);        
    }

	return 0;
}

int main() {

    WSADATA wsa;
    int err = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (err != 0) {
        printf("init err \n");
        return 0;
    }

    port =  CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,(ULONG_PTR)0,4);
    lsock = sockapi_create();
    

    sockaddr_in addr;
    addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("192.168.221.1");
    addr.sin_port = htons(9100);
	int addrlen = sizeof(addr);

    err = bind(lsock, (const struct sockaddr *)&addr, sizeof(addr));
    err = listen(lsock, 5);

	//int client = accept(lsock, (struct sockaddr *)&addr, &addrlen);

    at = new trigger();

	CreateIoCompletionPort((HANDLE)lsock, port, 0, 0);
    post_accept();

    for (int i = 0; i < 4; i++) {
        unsigned int threadid = 0;
        HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, threadfunc, NULL, 0, &threadid);
        if (thread == NULL)
            assert(false);
        threadlist[i] = thread;
    }
    
    while (true) {
        int cmd;
        scanf_s("%d", &cmd);

        if (cmd == 0) {

            PostQueuedCompletionStatus(port, 0, -1, NULL);
            WaitForMultipleObjects(4, threadlist, true, INFINITE);

            break;
        }
    }
    

    assert(false);

    WSACleanup();
    return 0;
}
