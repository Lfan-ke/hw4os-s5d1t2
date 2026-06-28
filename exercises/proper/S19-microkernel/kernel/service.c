/* S19 · 服务核心实现（给定，承接 S07 简易文件系统的心智）。
 * 一张定长内存文件表：create 占第一个空槽、read 按 id 取值。
 * 它不知道自己是被内核直接调用、还是被一个用户态服务任务代理——这正是“机制与策略分离”。 */
#include "service.h"

struct SvcFile {
    int      used;
    uint32_t key;
    uint32_t val;
};

static struct SvcFile g_table[SVC_MAXFILE];

void svc_reset(void) {
    int i;
    for (i = 0; i < SVC_MAXFILE; i++) {
        g_table[i].used = 0;
        g_table[i].key  = 0;
        g_table[i].val  = 0;
    }
}

int svc_create(uint32_t key, uint32_t val) {
    int i;
    for (i = 0; i < SVC_MAXFILE; i++) {
        if (!g_table[i].used) {
            g_table[i].used = 1;
            g_table[i].key  = key;
            g_table[i].val  = val;
            return i;                 /* 文件 id = 槽号 */
        }
    }
    return -1;                        /* 表满 */
}

int svc_read(int id, uint32_t *out) {
    if (id < 0 || id >= SVC_MAXFILE) return 0;
    if (!g_table[id].used)           return 0;
    *out = g_table[id].val;
    return 1;
}

/* 标准工作负载：前 4 条建文件（key=100+i, val=(i+1)*11），后 4 条按 id 回读。
 * 宏/微两路调用同一发生器 → 同一请求序列 → 结果可逐项对比。 */
void svc_demo_req(int i, int *op, uint32_t *a0, uint32_t *a1) {
    if (i < 4) {
        *op = OP_CREATE;
        *a0 = (uint32_t)(100 + i);          /* key */
        *a1 = (uint32_t)((i + 1) * 11);     /* val */
    } else {
        *op = OP_READ;
        *a0 = (uint32_t)(i - 4);            /* id = 0..3 */
        *a1 = 0;
    }
}
