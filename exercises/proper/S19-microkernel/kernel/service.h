/* S19 · 被托管的“服务”核心：一个极简内存文件服务（key→val 记录表）。
 * 这套逻辑【与形态无关】：宏内核直接在内核态调用它（MACRO 路径），
 * 微内核把它放进一个用户态服务任务、经 IPC 请求/应答间接调用（MICRO 路径）。
 * 两条路径跑同一份服务核心、同一份工作负载，结果必须逐项一致——
 * 区别只在“怎么把请求送到服务、怎么把结果取回”。 */
#ifndef S19_SERVICE_H
#define S19_SERVICE_H
#include <stdint.h>

#define SVC_MAXFILE 8

/* 清空文件表（每条路径开跑前调用，保证从干净状态出发）。 */
void svc_reset(void);

/* 新建一条记录：分配第一个空槽存 (key,val)，返回文件 id(>=0)；表满返回 -1。 */
int  svc_create(uint32_t key, uint32_t val);

/* 按 id 读出 val 到 *out：成功返回 1；id 非法/空槽返回 0（*out 不变）。 */
int  svc_read(int id, uint32_t *out);

/* —— 服务请求操作码（宏/微两路共享同一套协议）—— */
enum { OP_CREATE = 1, OP_READ = 2 };

/* 演示工作负载发生器：给定请求序号 i（0..REQ_N-1），产出一条标准请求。
 * 宏内核与微内核都调用它，从而构造【完全相同】的请求序列以便对比。 */
#define SVC_REQ_N 8
void svc_demo_req(int i, int *op, uint32_t *a0, uint32_t *a1);

#endif
