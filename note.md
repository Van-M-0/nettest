
1. overlapped 必须要初始化 (提示过6：invalid handle)
2. getqueuedcompletionstatus 中的参数必须要改正确(提示过998: access invalid memory)
3. scopelocker中的参数不能为引用


1. acceptex 中addrsize为sizeof(sockaddr)+16



3个缓冲区
user buffer, tcp/ip buffer, netcard buffer
- 一般情况下，bufer的流向是 uesr buffer -> tcp/ip buffer -> nectcard buffer

出现pending的情况是
    - wsasend, if userbuffer size > tcpipbuffer size, 出现pending, 此时多余的userbuffer
      内容则会被锁定，用户不应该修改这个，当tcpipbuffer空闲时，会通iocp，用户可继续发送
      *但是调试的时候修改了内容，也不会出现错误，不知道锁定是什么意思*
      wsasend如果成功，则userbuffer已经拷贝到tcpipbuffer，这个buffer可以随意使用
      wsasend如果失败且不为pending，则发送错误，应该关闭套接字相关的资源(**仔细考虑**)
    - wsarecv, 如果tcpbuffer没有可读内容时，则出现pending
      如果没有错误，则tcpipbuffer的内容会被拷贝到userbuffer里面来
      如果错误，我们应该关闭socket的资源


当发送队列满了，wsasend出现pending时候
- 如果客户端非法或正常关闭，则iocp线程会受到消息，ol不为null，trans为0，lasterr为64(netname not avaliable),
  如果在调用wsasend，则会出现10054(connection was forced closed by remote host)
  每个overlapped都会受到一次通知


- wsasend发送数据成功后，就是ret为0后，完成端口会立即受到通知
- wsarecv提交接收数据后，如果没有数据，则会为pending，否则立即将tcpipbuffer的数据拷贝到userbuffer中
- 



- 用一个overlapped结构既投递读，又和写，结果还不知道

- 套接字套接字关闭了，有多少个未决的io，就会有多少完成通知

- 理论上，完成端口收到通知的顺序是完成端口上未决的项，但是由于线程的干扰，最终顺序可能会变

- 




