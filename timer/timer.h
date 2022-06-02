#ifndef MQTT_SUB_CLIENT_TIMER_H
#define MQTT_SUB_CLIENT_TIMER_H
#include <stdint.h>
#include <sys/time.h>
#include <stdbool.h>

struct timer_t
{
    struct timeval _start_time;
    uint32_t _millis;
};

void timer_init(struct timer_t *timer);

void timer_start(struct timer_t *timer, uint32_t msec);

bool timer_is_time_up(struct timer_t *timer);

void timer_stop(struct timer_t *timer);

#endif //MQTT_SUB_CLIENT_TIMER_H