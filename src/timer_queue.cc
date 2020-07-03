#include <unistd.h>
#include <strings.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include "timer_queue.h"
#include "print_error.h"

timer_queue::timer_queue(): _count(0), _next_timer_id(0), _pioneer(-1/*= uint32_t max*/)
{
    //构造函数中初始化了一个timerfd，使用了系统实时时间，非阻塞
    //CLOCK_REALTIME:系统实时时间,随系统实时时间改变而改变,即从UTC1970-1-1 0:0:0开始计时,
    //中间时刻如果系统时间被用户改成其他,则对应的时间相应改变
    _timerfd = ::timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);//使用了timerfd

    exit_if(_timerfd == -1, "timerfd_create()");
}

timer_queue::~timer_queue()
{
    //析构函数中关闭
    ::close(_timerfd);
}
//像定时器堆中添加定时事件
int timer_queue::add_timer(timer_event& te)
{
    te.timer_id = _next_timer_id++;
    heap_add(te);

    if (_event_lst[0].ts < _pioneer)
    {
        _pioneer = _event_lst[0].ts;
        reset_timo();
    }
    return te.timer_id;
}
//根据定时事件id，删除定时器事件
void timer_queue::del_timer(int timer_id)
{
    mit it = _position.find(timer_id);
    if (it == _position.end())
    {
        error_log("no such a timerid %d", timer_id);
        return ;
    }
    int pos = it->second;
    heap_del(pos);

    if (_count == 0)    //如果容器中没有了定时器
    {
        _pioneer = -1;
        reset_timo();
    }
    else if (_event_lst[0].ts < _pioneer)
    {
        _pioneer = _event_lst[0].ts;
        reset_timo();
    }
}
//得到相同时间的定时事件数组，就是每次相同时间的定时器事件只触发一次，
//然后执行整个传出数组的定时到期回调函数。
void timer_queue::get_timo(std::vector<timer_event>& fired_evs)
{
    std::vector<timer_event> _reuse_lst;

    while (_count != 0 && _pioneer == _event_lst[0].ts)
    {
        timer_event te = _event_lst[0];
        //fired_evs是一个传出数组；
        fired_evs.push_back(te);
        if (te.interval)//如果是循环定时器
        {
            te.ts += te.interval;
            _reuse_lst.push_back(te);
        }
        heap_pop();
    }
    for (vit it = _reuse_lst.begin(); it != _reuse_lst.end(); ++it)
    {
        add_timer(*it);
    }
    //reset timeout
    if (_count == 0)
    {
        _pioneer = -1;
        reset_timo();
    }
    else//_pioneer != _event_lst[0].ts
    {
        _pioneer = _event_lst[0].ts;
        reset_timo();
    }
}
//将最近的到时事件加到timerfd上
void timer_queue::reset_timo()
{
    struct itimerspec old_ts, new_ts;
    ::bzero(&new_ts, sizeof(new_ts));

    if (_pioneer != (uint64_t)-1)
    {
        //_pioneer的单位是毫秒，而tv_sec的单位是秒
        new_ts.it_value.tv_sec = _pioneer / 1000;
        //1毫秒=1000 000纳秒
        new_ts.it_value.tv_nsec = (_pioneer % 1000) * 1000000;
    }
    //when _pioneer = -1, new_ts = 0 will disarms the timer
    ::timerfd_settime(_timerfd, TFD_TIMER_ABSTIME, &new_ts, &old_ts);
}

void timer_queue::heap_add(timer_event& te)
{
    _event_lst.push_back(te);
    //update position
    _position[te.timer_id] = _count;

    int curr_pos = _count;
    _count++;

    int prt_pos = (curr_pos - 1) / 2;
    while (prt_pos >= 0 && _event_lst[curr_pos].ts < _event_lst[prt_pos].ts)
    {
        timer_event tmp = _event_lst[curr_pos];
        _event_lst[curr_pos] = _event_lst[prt_pos];
        _event_lst[prt_pos] = tmp;
        //update position
        _position[_event_lst[curr_pos].timer_id] = curr_pos;
        _position[tmp.timer_id] = prt_pos;

        curr_pos = prt_pos;
        prt_pos = (curr_pos - 1) / 2;
    }
}

void timer_queue::heap_del(int pos)
{
    timer_event to_del = _event_lst[pos];
    //update position
    _position.erase(to_del.timer_id);

    //rear item
    timer_event tmp = _event_lst[_count - 1];
    _event_lst[pos] = tmp;
    //update position
    _position[tmp.timer_id] = pos;

    _count--;
    _event_lst.pop_back();

    heap_hold(pos);
}

void timer_queue::heap_pop()
{
    if (_count <= 0) return ;
    //update position
    _position.erase(_event_lst[0].timer_id);
    if (_count > 1)
    {
        timer_event tmp = _event_lst[_count - 1];
        _event_lst[0] = tmp;
        //update position
        _position[tmp.timer_id] = 0;

        _event_lst.pop_back();
        _count--;
        heap_hold(0);
    }
    else if (_count == 1)
    {
        _event_lst.clear();
        _count = 0;
    }
}

void timer_queue::heap_hold(int pos)
{
    int left = 2 * pos + 1, right = 2 * pos + 2;
    int min_pos = pos;
    if (left < _count && _event_lst[min_pos].ts > _event_lst[left].ts)
    {
        min_pos = left;
    }
    if (right < _count && _event_lst[min_pos].ts > _event_lst[right].ts)
    {
        min_pos = right;
    }
    if (min_pos != pos)
    {
        timer_event tmp = _event_lst[min_pos];
        _event_lst[min_pos] = _event_lst[pos];
        _event_lst[pos] = tmp;
        //update position
        _position[_event_lst[min_pos].timer_id] = min_pos;
        _position[tmp.timer_id] = pos;

        heap_hold(min_pos);
    }
}
