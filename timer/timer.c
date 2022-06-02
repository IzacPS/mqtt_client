#include "timer.h"

void timer_init(struct timer_t *timer)
{
	timer->_start_time.tv_sec = 0;
	timer->_millis = 0;
}

void timer_start(struct timer_t *timer, uint32_t msec)
{
    gettimeofday(&timer->_start_time, 0);
    timer->_millis = msec;
}

bool timer_is_time_up(struct timer_t *timer)
{
    struct timeval curtime;
    uint32_t secs, usecs;
    if (timer->_start_time.tv_sec == 0)
    {
        return false;
    }
    else
    {
        gettimeofday(&curtime, 0);
        secs  = (curtime.tv_sec  - timer->_start_time.tv_sec) * 1000;
        usecs = (curtime.tv_usec - timer->_start_time.tv_usec) / 1000.0;
        return ((secs + usecs) > (uint32_t)timer->_millis);
    }
}

void timer_stop(struct timer_t *timer)
{
    timer->_start_time.tv_sec = 0;
    timer->_millis = 0;
}