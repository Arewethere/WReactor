#ifndef __MSG_HEAD_H__
#define __MSG_HEAD_H__

class event_loop;

struct commu_head
{
    int cmdid;//消息类型
    int length;
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
