#ifndef MONIOS_NET_H
#define MONIOS_NET_H

#include <stdint.h>
#include <stddef.h>

/* ===== 常量与宏 ===== */
#define ETH_ALEN      6
#define IP_ALEN       4
#define ETH_HLEN      14
#define IP_HLEN       20
#define ICMP_HLEN     8
#define ARP_HLEN      28

#define ETH_P_IP      0x0800
#define ETH_P_ARP     0x0806
#define IP_PROTO_ICMP 1

/* 选择 NE2000 ISA 缺省 I/O（推荐用 ne2k_isa）。若用 ne2k_pci，请先枚举 PCI 拿 BAR0 修改此值 */
#define NIC_IO_BASE   0x300
#define NIC_IRQ       10

/* ===== 结构体 ===== */
#pragma pack(push, 1)
typedef struct {
    uint8_t  dest_mac[ETH_ALEN];
    uint8_t  src_mac[ETH_ALEN];
    uint16_t ethertype;
} eth_header_t;

typedef struct {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint8_t  saddr[IP_ALEN];
    uint8_t  daddr[IP_ALEN];
} ip_header_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} icmp_header_t;
#pragma pack(pop)

/* 对外 API */
int  init_network(void);
void send_packet(const void *data, size_t len);
void receive_packet(void);       /* 轮询接收（可选） */
void net_interrupt_handler(void);
void do_ping_impl(const char *ipstr);

#endif
