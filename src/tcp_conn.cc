#include "tcp_conn.h"
#include "msg_head.h"
#include "tcp_server.h"
#include "print_error.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <netinet/tcp.h>

static void tcp_rcb(event_loop* loop, int fd, void *args)
{
    tcp_conn* conn = (tcp_conn*)args;
    conn->handle_read();
}

static void tcp_wcb(event_loop* loop, int fd, void *args)
{
    tcp_conn* conn = (tcp_conn*)args;
    conn->handle_write();
}

void tcp_conn::init(int connfd, event_loop* loop)
{
    _connfd = connfd;
    _loop = loop;
    //set NONBLOCK
    //获得connfd的文件状态标记
    int flag = ::fcntl(_connfd, F_GETFL, 0);
    //将connfd设置为非阻塞
    ::fcntl(_connfd, F_SETFL, O_NONBLOCK | flag);

    //set NODELAY
    int opend = 1;
    //不使用Nagle算法　
    int ret = ::setsockopt(_connfd, IPPROTO_TCP, TCP_NODELAY, &opend, sizeof(opend));
    error_if(ret < 0, "setsockopt TCP_NODELAY");

    //调用用户设置的连接建立后回调函数,主要是用于初始化作为连接内变量的：parameter参数
    if (tcp_server::connBuildCb)
        tcp_server::connBuildCb(this);
    //放到epoll上监听
    _loop->add_ioev(_connfd, tcp_rcb, EPOLLIN, this);
    //tcp连接的数量加1
    tcp_server::inc_conn();
}
//读进buffer，事件驱动自动完成
void tcp_conn::handle_read()
{
    int ret = ibuf.read_data(_connfd);
    if (ret == -1)
    {
        //read data error
        error_log("read data from socket");
        clean_conn();
        return ;
    }
    else if (ret == 0)
    {
        //The peer is closed, return -2
        //如果读到0 说明对端关闭了连接
        info_log("connection closed by peer");
        clean_conn();
        return ;
    }
    //定义一个消息头，包括消息类型和消息长度
    commu_head head;
    //如果读出的消息长度大于或者等于消息头部
    while (ibuf.length() >= COMMU_HEAD_LENGTH)
    {   //将消息头复制到head中
        ::memcpy(&head, ibuf.data(), COMMU_HEAD_LENGTH);
        //如果消息头中填入的消息长度大于长度限制值或者小于零则进行相应错误处理
        if (head.length > MSG_LENGTH_LIMIT || head.length < 0)
        {
            //data format is messed up
            error_log("data format error in data head, close connection");
            clean_conn();
            break;
        }
        if (ibuf.length() < COMMU_HEAD_LENGTH + head.length)
        {
            //说明没有读完，不是一条完整的消息
            //this is half-package
            break;
        }
        //find in dispatcher
        //如果消息分发器里面没有设置相应的消息回调函数，则进行相应的错误处理
        if (!tcp_server::dispatcher.exist(head.cmdid))
        {
            //data format is messed up
            error_log("this message has no corresponding callback, close connection");
            clean_conn();
            break;
        }
        //ibuffer中删除消息头部
        ibuf.pop(COMMU_HEAD_LENGTH);
        //domain: call user callback
        //调用相应的消息回调函数处理这条消息。
        tcp_server::dispatcher.cb(ibuf.data(), head.length, head.cmdid, this);
        //删除处理之后的消息
        ibuf.pop(head.length);
    }
    ibuf.adjust();
}
//从buffer写进内核缓冲区，事件驱动自动完成
void tcp_conn::handle_write()
{
    //循环写
    while (obuf.length())
    {
        int ret = obuf.write_fd(_connfd);
        if (ret == -1)
        {
            error_log("write TCP buffer error, close connection");
            clean_conn();
            return ;
        }
        if (ret == 0)
        {
            //不是错误，仅返回为0表示此时不可继续写
            break;
        }
    }
    //如果已经写完了，则删除关注可写事件
    if (!obuf.length())
    {
        _loop->del_ioev(_connfd, EPOLLOUT);
    }
}
//就是将要发送的数据写进obuffer中
int tcp_conn::send_data(const char* data, int datlen, int cmdid)
{
    bool need_listen = false;
    //如果obuffer为空，说明上次没有已经读完，取消了监听可写事件，则设置监听可写事件
    if (!obuf.length())
        need_listen = true;
    //write rsp head first
    //填写头部相关信息
    commu_head head;
    head.cmdid = cmdid;
    head.length = datlen;
    //write head
    int ret = obuf.send_data((const char*)&head, COMMU_HEAD_LENGTH);
    if (ret != 0)
        return -1;
    //write content
    ret = obuf.send_data(data, datlen);
    if (ret != 0)
    {
        //只好取消写入的消息头
        obuf.pop(COMMU_HEAD_LENGTH);
        return -1;
    }

    if (need_listen)
    {
        _loop->add_ioev(_connfd, tcp_wcb, EPOLLOUT, this);
    }
    return 0;
}
//清除连接
void tcp_conn::clean_conn()
{
    //调用用户设置的连接释放后回调函数,主要是用于销毁作为连接内变量的：parameter参数
    if (tcp_server::connCloseCb)
        tcp_server::connCloseCb(this);
    //连接清理工作
    tcp_server::dec_conn();
    _loop->del_ioev(_connfd);
    _loop = NULL;
    ibuf.clear();
    obuf.clear();
    int fd = _connfd;
    _connfd = -1;
    ::close(fd);
}