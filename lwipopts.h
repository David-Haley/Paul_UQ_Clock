// lwIP configuration for Paul_UQ_Clock.
// NO_SYS=0: full FreeRTOS integration (pico_cyw43_arch_lwip_sys_freertos), giving
// blocking sockets from any task and automatic servicing of the cyw43 driver and
// lwIP stack from their own dedicated (core-1-pinned) FreeRTOS task.
// Based on the SDK's lib/lwip/contrib/examples/example_app/lwipopts.h, trimmed to
// what this project actually uses (IPv4 + UDP/DHCP/DNS + sockets; no TCP server,
// no IPv6, no stats/PPP/SNMP).

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define NO_SYS                      0
#define LWIP_SOCKET                 (NO_SYS == 0)
#define LWIP_NETCONN                (NO_SYS == 0)
#define LWIP_NETIF_API              (NO_SYS == 0)
#define LWIP_TCPIP_CORE_LOCKING     1
#define SYS_LIGHTWEIGHT_PROT        (NO_SYS == 0)

#define LWIP_IPV4                   1
#define LWIP_IPV6                   0

#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_RAW                    1
#define LWIP_ICMP                   1

#define LWIP_UDP                    1
#define UDP_TTL                     255

#define LWIP_TCP                    1
#define TCP_TTL                     255
#define TCP_MSS                     1460
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * TCP_SND_BUF) / TCP_MSS)

#define LWIP_DHCP                   1
#define LWIP_DHCP_GET_NTP_SRV       1
#define DHCP_DOES_ARP_CHECK         0

#define LWIP_DNS                    1
#define LWIP_COMPAT_SOCKETS         1
#define LWIP_SO_RCVTIMEO            1
// Use newlib's <sys/time.h> struct timeval instead of lwIP's own copy.
#define LWIP_TIMEVAL_PRIVATE        0

#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1

/* ---------- Memory options ---------- */
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    4000
#define MEMP_NUM_TCP_SEG             32
#define MEMP_NUM_ARP_QUEUE           10
#define MEMP_NUM_SYS_TIMEOUT         12
#define MEMP_NUM_NETCONN              8
#define PBUF_POOL_SIZE                24

/* ---------- Mailbox / thread sizing (NO_SYS==0) ---------- */
#define TCPIP_THREAD_STACKSIZE            2048
// Numeric literal (not tskIDLE_PRIORITY + 3): lwipopts.h is included from plain C
// lwIP sources that don't otherwise pull in FreeRTOS's task.h. tskIDLE_PRIORITY is
// 0, so this is equivalent.
#define TCPIP_THREAD_PRIO                 3
#define TCPIP_MBOX_SIZE                    8
#define DEFAULT_THREAD_STACKSIZE          1024
#define DEFAULT_RAW_RECVMBOX_SIZE          8
#define DEFAULT_UDP_RECVMBOX_SIZE          8
#define DEFAULT_TCP_RECVMBOX_SIZE          8
#define DEFAULT_ACCEPTMBOX_SIZE            8

/* ---------- Stats / debug ---------- */
#define LWIP_STATS                   0
#define MEM_STATS                    0
#define SYS_STATS                    0
#define LINK_STATS                   0

#define LWIP_CHKSUM_ALGORITHM        3

#endif /* LWIPOPTS_H */
