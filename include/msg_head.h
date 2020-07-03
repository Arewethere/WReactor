/*
commu_head类：封装了消息类型和消息长度
cmdid消息类型;
length消息长度;

queue_msg类：作用是给主线程给线程池中线程通信
消息类型有NEW_CONN、STOP_THD、NEW_TASK;
如果是新的连接下发connfd;
如果是新任务下发task函数;

*/

#ifndef __MSG_HEAD_H__
#define __MSG_HEAD_H__

class event_loop;
//消息头部，记录相应的消息类型和消息长度，用于处理粘包事件
struct commu_head
{
    int cmdid;//消息类型
    int length;//消息长度
};

//for accepter communicate with connections
//for give task to sub-thread
//便于主线程与子线程通信，给子线程下发任务
struct queue_msg
{
    enum MSG_TYPE   //给子线程传递消息的类型
    {
        NEW_CONN,   //新的连接
        STOP_THD,   //停止线程
        NEW_TASK,   //新的任务
    };
    MSG_TYPE cmd_type;

    union {
        int connfd;//for NEW_CONN, 向sub-thread下发新连接
        struct
        {
            void (*task)(event_loop*, void*);
            void *args;
        };//for NEW_TASK, 向sub-thread下发待执行任务
    };
};

#define COMMU_HEAD_LENGTH 8

#define MSG_LENGTH_LIMIT (65536 - COMMU_HEAD_LENGTH)

#endif
