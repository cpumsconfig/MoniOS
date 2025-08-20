#include "drivers/net.h"
#include "monios/common.h"     /* inb/outb */
#include "drivers/gdtidt.h"    /* idt_set_gate */
#include "log.h"

#include <string.h>
#include <stdint.h>

/* ===================== NE2000/DP8390 寄存器定义 ===================== */
/* 通用寄存器（页位于 CR 的 PS1..0） */
#define REG_CR     0x00  /* Command */
#define REG_PSTART 0x01  /* Page Start (P0) */
#define REG_PSTOP  0x02  /* Page Stop  (P0) */
#define REG_BNRY   0x03  /* Boundary   (P0) */
#define REG_TSR_TPSR 0x04/* Read:TSR(P0) / Write:TPSR(P0) */
#define REG_TBCR0  0x05  /* Transmit Byte Count Low  (P0) */
#define REG_TBCR1  0x06  /* Transmit Byte Count High (P0) */
#define REG_ISR    0x07  /* Interrupt Status (P0) */
#define REG_RSAR0  0x08  /* Remote Start Address Low  (P0) */
#define REG_RSAR1  0x09  /* Remote Start Address High (P0) */
#define REG_RBCR0  0x0A  /* Remote Byte Count Low  (P0) */
#define REG_RBCR1  0x0B  /* Remote Byte Count High (P0) */
#define REG_RCR    0x0C  /* Receive Configuration (P0) */
#define REG_TCR    0x0D  /* Transmit Configuration (P0) */
#define REG_DCR    0x0E  /* Data Configuration (P0) */
#define REG_IMR    0x0F  /* Interrupt Mask (P0) */

#define REG_PAR0   0x01  /* Physical Address (P1..P1+5) */
#define REG_CURR   0x07  /* Current Page (P1) */

#define REG_MAR0   0x08  /* Multicast Address (P1..P1+7) */

/* 数据端口与复位端口（NE2000 兼容） */
#define REG_DATA   0x10  /* NE2000 data port (word) */
#define REG_RESET  0x1F  /* NE2000 reset port (read to reset) */

/* CR 位 */
#define CR_STP     0x01
#define CR_STA     0x02
#define CR_TXP     0x04
#define CR_RD0     0x08  /* Remote DMA command bits */
#define CR_RD1     0x10
#define CR_RD2     0x20
#define CR_PS0     0x40  /* Page Select */
#define CR_PS1     0x80

/* ISR 位 */
#define ISR_PRX    0x01  /* Packet Received */
#define ISR_PTX    0x02  /* Packet Transmitted */
#define ISR_RXE    0x04
#define ISR_TXE    0x08
#define ISR_OVW    0x10  /* Overwrite Warning */
#define ISR_CNT    0x20
#define ISR_RDC    0x40  /* Remote DMA Complete */
#define ISR_RST    0x80  /* Reset Status */

/* IMR 位（使能对应 ISR） */
#define IMR_PRXE   0x01
#define IMR_PTXE   0x02
#define IMR_RXEE   0x04
#define IMR_TXEE   0x08
#define IMR_OVWE   0x10
#define IMR_CNTE   0x20
#define IMR_RDCE   0x40

/* RCR 位 */
#define RCR_AB     0x04  /* Accept Broadcast */
#define RCR_PRO    0x10  /* Promiscuous (可选) */

/* TCR 位 */
#define TCR_LB0    0x02  /* Internal loopback (init 时可用) */

/* DCR 位（常用配置：0x49 => WTS=1, LS=0, AR=0, FT1:0=10） */
#define DCR_VAL    0x49

/* ===================== 驱动参数 ===================== */
static uint8_t my_mac[ETH_ALEN]  = {0x52,0x54,0x00,0x12,0x34,0x56};
static uint8_t my_ip[IP_ALEN]    = {192,168,10,19};

/* 内存页（每页 256 字节）。典型配置：TX 占用 6 页（1536B），RX 环占用其余 */
#define PAGE_SIZE      256
#define RX_START       0x46
#define RX_STOP        0x80
#define TX_START       0x40      /* 0x40..0x45 用作发送缓冲 */
#define TX_PAGES       6

/* 简易缓冲区（构帧用） */
static uint8_t tx_buf[1518];
static uint8_t rx_buf[1600];

/* ping 状态（与之前一致，演示用） */
static volatile uint8_t ping_received = 0;

/* ===================== 小工具 ===================== */
static inline uint16_t bswap16(uint16_t v){ return (v<<8)|(v>>8); }
static inline uint16_t htons(uint16_t v){ return bswap16(v); }
static inline uint16_t ntohs(uint16_t v){ return bswap16(v); }

/* 仅支持 "%d.%d.%d.%d" 这种格式 */
int mini_sscanf(const char *str, const char *fmt, int *a, int *b, int *c, int *d) {
    if (!str || !fmt) return 0;

    /* 必须是固定格式 "%d.%d.%d.%d" */
    if (strcmp(fmt, "%d.%d.%d.%d") != 0) return 0;

    int vals[4] = {0};
    int idx = 0;
    int num = 0;
    int has_digit = 0;

    while (*str && idx < 4) {
        char c = *str++;
        if (c >= '0' && c <= '9') {
            num = num * 10 + (c - '0');
            if (num > 255) return 0;   /* IP 段必须 0-255 */
            has_digit = 1;
        } else if (c == '.' || c == '\0') {
            if (!has_digit) return 0;
            vals[idx++] = num;
            num = 0;
            has_digit = 0;
        } else {
            return 0;  /* 非法字符 */
        }
    }

    if (has_digit && idx < 4) {
        vals[idx++] = num;
    }

    if (idx != 4) return 0;

    *a = vals[0];
    *b = vals[1];
    *c = vals[2];
    *d = vals[3];
    return 4;  /* 成功解析 4 个数字 */
}


static uint16_t ip_checksum(const void *buf, size_t len){
    const uint16_t *p = (const uint16_t*)buf;
    uint32_t sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8_t*)p;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (uint16_t)~sum;
}

/* 切页辅助：CR 页位（PS1..0） */
static inline void set_page(int page){
    uint8_t cr = inb(NIC_IO_BASE + REG_CR);
    cr &= ~(CR_PS0|CR_PS1);
    if (page == 1) cr |= CR_PS0;
    else if (page == 2) cr |= CR_PS1;
    outb(NIC_IO_BASE + REG_CR, cr);
}

/* 远程 DMA：写（把内存数据写到卡内存 addr=page*256+offset） */
static void rdm_write(uint16_t card_addr, const void *src, uint16_t len){
    /* 设置地址与长度 */
    outb(NIC_IO_BASE + REG_RSAR0, card_addr & 0xFF);
    outb(NIC_IO_BASE + REG_RSAR1, card_addr >> 8);
    outb(NIC_IO_BASE + REG_RBCR0, len & 0xFF);
    outb(NIC_IO_BASE + REG_RBCR1, len >> 8);

    /* CR: STA | Remote Write (RD1=1, RD0=0) */
    uint8_t cr = (CR_STA | CR_RD1);
    outb(NIC_IO_BASE + REG_CR, cr);

    /* 通过数据端口写入（16-bit 对齐；这里按字节写入更通用） */
    const uint8_t *p = (const uint8_t*)src;
    for (uint16_t i = 0; i < len; ++i){
        outb(NIC_IO_BASE + REG_DATA, p[i]);
    }

    /* 等待 RDC 完成 */
    while (!(inb(NIC_IO_BASE + REG_ISR) & ISR_RDC)) /* spin */ ;
    /* 清 RDC */
    outb(NIC_IO_BASE + REG_ISR, ISR_RDC);
}

/* 远程 DMA：读（从卡内存 addr 读 len 字节到 dst） */
static void rdm_read(uint16_t card_addr, void *dst, uint16_t len){
    outb(NIC_IO_BASE + REG_RSAR0, card_addr & 0xFF);
    outb(NIC_IO_BASE + REG_RSAR1, card_addr >> 8);
    outb(NIC_IO_BASE + REG_RBCR0, len & 0xFF);
    outb(NIC_IO_BASE + REG_RBCR1, len >> 8);

    /* CR: STA | Remote Read (RD0=1) */
    uint8_t cr = (CR_STA | CR_RD0);
    outb(NIC_IO_BASE + REG_CR, cr);

    uint8_t *p = (uint8_t*)dst;
    for (uint16_t i = 0; i < len; ++i){
        p[i] = inb(NIC_IO_BASE + REG_DATA);
    }

    /* 等待 RDC 完成 */
    while (!(inb(NIC_IO_BASE + REG_ISR) & ISR_RDC)) /* spin */ ;
    outb(NIC_IO_BASE + REG_ISR, ISR_RDC);
}

/* ===================== 发送/接收 ===================== */
/* 把 tx_buf 中的数据写到 TX_START 页，并触发发送 */
static void ne2k_transmit(const void *data, uint16_t len){
    if (len < 60) len = 60;                 /* 以太网最小帧 */
    if (len > 1518) len = 1518;

    /* 1) 把帧写到显存：card_addr = TX_START * 256 */
    uint16_t card_addr = TX_START * PAGE_SIZE;
    rdm_write(card_addr, data, len);

    /* 2) 配置发送参数并触发发送 */
    outb(NIC_IO_BASE + REG_TSR_TPSR, TX_START);    /* TPSR */
    outb(NIC_IO_BASE + REG_TBCR0, (uint8_t)(len & 0xFF));
    outb(NIC_IO_BASE + REG_TBCR1, (uint8_t)(len >> 8));

    /* CR: 置 TXP 位（在保持 STA 的同时） */
    uint8_t cr = inb(NIC_IO_BASE + REG_CR);
    outb(NIC_IO_BASE + REG_CR, (uint8_t)((cr & ~(CR_STP)) | CR_TXP | CR_STA));
}

/* 环形缓冲的“包头”格式（位于每个接收帧前 4 字节） */
typedef struct {
    uint8_t  status;
    uint8_t  next_page;
    uint8_t  len_lo;
    uint8_t  len_hi;
} rx_hdr_t;

/* 处理环缓中的所有数据包 */
static void ne2k_rx_drain(void){
    /* 页 1 读取 CURR（硬件当前写入页） */
    set_page(1);
    uint8_t curr = inb(NIC_IO_BASE + REG_CURR);
    set_page(0);

    uint8_t bnry = inb(NIC_IO_BASE + REG_BNRY);
    uint8_t page = (uint8_t)(bnry + 1);
    if (page >= RX_STOP) page = RX_START;

    while (page != curr){
        /* 每个包以 4 字节包头开始，位于 page*256 偏移 */
        uint16_t pkt_addr = (uint16_t)page * PAGE_SIZE;
        rx_hdr_t hdr;
        rdm_read(pkt_addr, &hdr, sizeof(hdr));

        uint16_t totlen = (uint16_t)hdr.len_lo | ((uint16_t)hdr.len_hi << 8);
        if (totlen < 60 || totlen > sizeof(rx_buf)){
            /* 异常：丢弃并复位接收区 */
            printf("ne2k: bad rx len=%u, reset ring\n", totlen);
            /* 直接把 BNRY 追到 CURR-1，防止卡死 */
            set_page(1);
            curr = inb(NIC_IO_BASE + REG_CURR);
            set_page(0);
            uint8_t new_bnry = (uint8_t)(curr == RX_START ? (RX_STOP - 1) : (curr - 1));
            outb(NIC_IO_BASE + REG_BNRY, new_bnry);
            break;
        }

        /* 读取整个帧（含 4 字节头）到 rx_buf */
        uint16_t read_len = (uint16_t)(totlen + sizeof(rx_hdr_t));
        if (read_len > sizeof(rx_buf)) read_len = sizeof(rx_buf);

        /* 包可能跨越 RX_STOP，需要两段读取 */
        uint16_t first = (uint16_t)((RX_STOP * PAGE_SIZE) - pkt_addr);
        if (first > read_len) first = read_len;

        rdm_read(pkt_addr, rx_buf, first);
        if (first < read_len){
            /* 剩余从 RX_START 开头接着读 */
            rdm_read((uint16_t)(RX_START * PAGE_SIZE),
                     rx_buf + first,
                     (uint16_t)(read_len - first));
        }

        /* 解析以太类型并做最基本处理（ICMP 回显的 echo reply 通知） */
        if (read_len >= sizeof(rx_hdr_t) + sizeof(eth_header_t)){
            const uint8_t *frame = rx_buf + sizeof(rx_hdr_t);
            const eth_header_t *eth = (const eth_header_t*)frame;
            uint16_t type = ntohs(eth->ethertype);

            if (type == ETH_P_IP){
                if (read_len >= sizeof(rx_hdr_t) + ETH_HLEN + sizeof(ip_header_t)){
                    const ip_header_t *ip = (const ip_header_t*)(frame + ETH_HLEN);
                    if (ip->protocol == IP_PROTO_ICMP){
                        if (read_len >= sizeof(rx_hdr_t) + ETH_HLEN + IP_HLEN + sizeof(icmp_header_t)){
                            const icmp_header_t *icmp = (const icmp_header_t*)(frame + ETH_HLEN + IP_HLEN);
                            if (icmp->type == 0 /* echo reply */){
                                ping_received = 1;
                            }
                        }
                    }
                }
            }
        }

        /* 推进 BNRY 到本帧的前一页（硬件从 next_page 开始写下一帧） */
        uint8_t next = hdr.next_page;
        if (next < RX_START || next >= RX_STOP){
            /* 异常，直接把 BNRY 追到 curr-1 */
            set_page(1);
            curr = inb(NIC_IO_BASE + REG_CURR);
            set_page(0);
            uint8_t new_bnry = (uint8_t)(curr == RX_START ? (RX_STOP - 1) : (curr - 1));
            outb(NIC_IO_BASE + REG_BNRY, new_bnry);
            break;
        }

        uint8_t new_bnry = (uint8_t)(next - 1);
        if (new_bnry < RX_START) new_bnry = (uint8_t)(RX_STOP - 1);
        outb(NIC_IO_BASE + REG_BNRY, new_bnry);

        /* 更新 page，继续读下一帧 */
        page = next;
        set_page(1);
        curr = inb(NIC_IO_BASE + REG_CURR);
        set_page(0);
    }
}

/* ===================== 初始化 ===================== */
static int ne2k_reset(void){
    /* 读 reset 端口触发复位，等待 ISR.RST 置位 */
    inb(NIC_IO_BASE + REG_RESET);
    for (int i = 0; i < 100000; ++i){
        if (inb(NIC_IO_BASE + REG_ISR) & ISR_RST){
            outb(NIC_IO_BASE + REG_ISR, ISR_RST); /* 清 RST */
            return 1;
        }
    }
    return 0;
}

static int ne2k_init(void){
    /* 停机 + 选择页 0 */
    outb(NIC_IO_BASE + REG_CR, CR_STP);

    if (!ne2k_reset()){
        printf("ne2k: reset failed\n");
        return 0;
    }

    /* 数据配置：16-bit 传输、适配 FIFO 等 */
    outb(NIC_IO_BASE + REG_DCR, DCR_VAL);

    /* 关闭远程 DMA 计数器 */
    outb(NIC_IO_BASE + REG_RBCR0, 0x00);
    outb(NIC_IO_BASE + REG_RBCR1, 0x00);

    /* 接收配置：接收广播（必要用于 ARP） */
    outb(NIC_IO_BASE + REG_RCR, RCR_AB);

    /* 发送配置：先内部环回（避免初始化期间发包） */
    outb(NIC_IO_BASE + REG_TCR, TCR_LB0);

    /* 设置接收环缓区 */
    outb(NIC_IO_BASE + REG_PSTART, RX_START);
    outb(NIC_IO_BASE + REG_PSTOP,  RX_STOP);
    outb(NIC_IO_BASE + REG_BNRY,   (uint8_t)(RX_START));

    /* 页 1：写 MAC 与 CURR */
    set_page(1);
    for (int i = 0; i < ETH_ALEN; ++i){
        outb(NIC_IO_BASE + (REG_PAR0 + i), my_mac[i]);
    }
    /* 关闭多播过滤（全 0） */
    for (int i = 0; i < 8; ++i){
        outb(NIC_IO_BASE + (REG_MAR0 + i), 0x00);
    }
    outb(NIC_IO_BASE + REG_CURR, (uint8_t)(RX_START + 1));
    set_page(0);

    /* 使能中断 */
    outb(NIC_IO_BASE + REG_IMR, IMR_PRXE | IMR_PTXE | IMR_RXEE | IMR_TXEE | IMR_OVWE | IMR_RDCE);

    /* 启动网卡，退出环回 */
    outb(NIC_IO_BASE + REG_CR, CR_STA);
    outb(NIC_IO_BASE + REG_TCR, 0x00);

    printf("ne2k: init ok (IO=0x%X, RX[%u..%u), TX=%u)\n",
           NIC_IO_BASE, RX_START, RX_STOP, TX_START);
    return 1;
}

/* ===================== 对外 API ===================== */
int init_network(void){
    printf("Initializing network...\n");

    if (!ne2k_init()){
        printf("Network error\n");
        return 0;
    }

    /* 安装中断门（内核 CS=0x08，属性 0x8E） */
    idt_set_gate(NIC_IRQ, (uint32_t)net_interrupt_handler, 0x08, 0x8E);

    /* 主 PIC 取消屏蔽对应 IRQ（此处简化：只动主片） */
    uint8_t imr = inb(0x21);
    imr &= (uint8_t)~(1 << (NIC_IRQ % 8));
    outb(0x21, imr);

    printf("Network ready\n");
    return 1;
}

void net_interrupt_handler(void){
    uint8_t isr = inb(NIC_IO_BASE + REG_ISR);

    if (isr & ISR_PRX){
        ne2k_rx_drain();
        outb(NIC_IO_BASE + REG_ISR, ISR_PRX);
    }
    if (isr & ISR_PTX){
        outb(NIC_IO_BASE + REG_ISR, ISR_PTX);
    }
    if (isr & ISR_RXE){ outb(NIC_IO_BASE + REG_ISR, ISR_RXE); }
    if (isr & ISR_TXE){ outb(NIC_IO_BASE + REG_ISR, ISR_TXE); }
    if (isr & ISR_OVW){
        /* 覆盖警告：把 BNRY 追上去以恢复 */
        printf("ne2k: OVW\n");
        set_page(1);
        uint8_t curr = inb(NIC_IO_BASE + REG_CURR);
        set_page(0);
        uint8_t new_bnry = (uint8_t)(curr == RX_START ? (RX_STOP - 1) : (curr - 1));
        outb(NIC_IO_BASE + REG_BNRY, new_bnry);
        outb(NIC_IO_BASE + REG_ISR, ISR_OVW);
    }
    if (isr & ISR_RDC){
        /* 远程 DMA 完成（通常在 rdm_* 内部处理，这里兜底清一下） */
        outb(NIC_IO_BASE + REG_ISR, ISR_RDC);
    }

    /* EOI */
    outb(0x20, 0x20);
}

void receive_packet(void){
    ne2k_rx_drain();
}

/* 把 data 作为一个完整以太帧发送 */
void send_packet(const void *data, size_t len){
    if (!data || len == 0) return;
    if (len > sizeof(tx_buf)) len = sizeof(tx_buf);
    memcpy(tx_buf, data, len);
    ne2k_transmit(tx_buf, (uint16_t)len);
}

/* ===================== 简化的 ICMP Echo 请求（仍需 ARP 才能单播） ===================== */
static void build_icmp_echo(uint8_t *frame, size_t *out_len,
                            const uint8_t dst_mac[6],
                            const uint8_t dst_ip[4]){
    /* 以太头 */
    eth_header_t *eth = (eth_header_t*)frame;
    memcpy(eth->dest_mac, dst_mac, 6);
    memcpy(eth->src_mac,  my_mac,  6);
    eth->ethertype = htons(ETH_P_IP);

    /* IP + ICMP */
    ip_header_t *ip = (ip_header_t*)(frame + ETH_HLEN);
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->id = 0;
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_ICMP;
    memcpy(ip->saddr, my_ip, 4);
    memcpy(ip->daddr, dst_ip, 4);
    icmp_header_t *icmp = (icmp_header_t*)((uint8_t*)ip + IP_HLEN);
    icmp->type = 8; icmp->code = 0;
    icmp->identifier = htons(0x1234);
    icmp->sequence   = htons(1);
    uint8_t *payload = (uint8_t*)icmp + ICMP_HLEN;
    for (int i = 0; i < 32; ++i) payload[i] = (uint8_t)('A' + (i % 26));
    icmp->checksum = 0;
    icmp->checksum = ip_checksum(icmp, ICMP_HLEN + 32);

    uint16_t iplen = (uint16_t)(IP_HLEN + ICMP_HLEN + 32);
    ip->tot_len = htons(iplen);
    ip->check = 0;
    ip->check = ip_checksum(ip, IP_HLEN);

    *out_len = ETH_HLEN + iplen;
}

/* 解析 IPv4 点分字符串到 4 字节 */
static int parse_ipv4(const char *s, uint8_t out[4]) {
    int a, b, c, d;
    if (mini_sscanf(s, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return 0;
    if (a<0 || a>255 || b<0 || b>255 || c<0 || c>255 || d<0 || d>255) return 0;
    out[0]=(uint8_t)a; out[1]=(uint8_t)b; out[2]=(uint8_t)c; out[3]=(uint8_t)d;
    return 1;
}


void do_ping_impl(const char *ipstr){
    uint8_t dip[4];
    if (!parse_ipv4(ipstr, dip)){
        printf("ping: invalid ip '%s'\n", ipstr);
        return;
    }

    /* 简化：直接广播 MAC（因为没做 ARP 解析）→ 真用时请先做 ARP，拿到目标的单播 MAC */
    uint8_t dmac[6]; memset(dmac, 0xFF, 6);

    size_t framelen = 0;
    build_icmp_echo(tx_buf, &framelen, dmac, dip);
    printf("send echo to %s (len=%zu)\n", ipstr, framelen);
    ne2k_transmit(tx_buf, (uint16_t)framelen);

    /* 简单等一下，看是否收到了 Echo Reply（收到则 ping_received=1） */
    ping_received = 0;
    for (int t = 0; t < 1000 && !ping_received; ++t){
        ne2k_rx_drain();
        for (volatile int spin=0; spin<20000; ++spin) { /* small delay */ }
    }
    if (ping_received) printf("echo reply received\n");
    else               printf("timeout (note: -net user drops ICMP)\n");
}
