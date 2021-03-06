#ifndef __MSG_DISPATCHER_H__
#define __MSG_DISPATCHER_H__

#include <assert.h>
#include <pthread.h>
#include <ext/hash_map>
#include "net_commu.h"

/*
msg_dispatcher类的作用就是根据不同的消息类型调用相应的回调函数
msg_dispatcher数据成员中有两个map
_dispatcher中保存着cmdid与msg_callback的映射
_args中保存着cmdid与msg_callback的参数的映射
*/

typedef void msg_callback(const char* data, uint32_t len, int cmdid, net_commu* commu, void* usr_data);

class msg_dispatcher
{
public:
    msg_dispatcher() {}
    //添加消息id对应的回调函数
    int add_msg_cb(int cmdid, msg_callback* msg_cb, void* usr_data)
    {
        if (_dispatcher.find(cmdid) != _dispatcher.end()) return -1;//如果已经设置了该消息类型的回调函数
        _dispatcher[cmdid] = msg_cb;
        _args[cmdid] = usr_data;
        return 0;
    }
    //判断该消息类型是否设置了回调函数
    bool exist(int cmdid) const { return _dispatcher.find(cmdid) != _dispatcher.end(); }
    //执行消息id对应的回调函数
    void cb(const char* data, uint32_t len, int cmdid, net_commu* commu)
    {
        assert(exist(cmdid));
        msg_callback* func = _dispatcher[cmdid];
        void* usr_data = _args[cmdid];
        func(data, len, cmdid, commu, usr_data);
    }

private:
    //消息id和其回调函数对应的map
    __gnu_cxx::hash_map<int, msg_callback*> _dispatcher;
    //消息id和其回调函数参数对应的map
    __gnu_cxx::hash_map<int, void*> _args;
};

#endif
