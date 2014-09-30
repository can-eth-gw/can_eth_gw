/**
 * @defgroup files Core
 * @defgroup trans Translation
 * @defgroup alloc Create & Deleting
 * @defgroup proc Processing
 * @defgroup get Getter & Setter
 * @defgroup dev Device
 * @defgroup net Netlink
 * @file ce_gw_main.c
 * @brief Control Area Network - Ethernet - Gateway - Device
 * @author Stefan Smarzly (stefan.smarzly@in.tum.de)
 * @author Fabian Raab (fabian.raab@tum.de)
 * @date May, 2013
 * @copyright GNU Public License v3 or higher
 * @ingroup files
 * @{
 */

/*****************************************************************************
 * (C) Copyright 2013 Fabian Raab, Stefan Smarzly
 *
 * This file is part of CAN-Eth-GW.
 *
 * CAN-Eth-GW is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CAN-Eth-GW is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CAN-Eth-GW.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

/**
* @code
*   _______________________________________________________
*  |                                                       |
*  |       CAN Ethernet Gateway Kernel Module (ce_gw)      |
*  |_______________________________________________________|
*  |                                                       |
*  |           Ethernet Frame    +----------------------+  |  CAN Frame
*  |           +-----o---------->|CAN - Ethernet Gateway|<-+-------o----+
*  |           |                 |     ce_gw_main.c     |  |            |
*  |           |                 +----------------------+  |            v
*  |           v                                           |       +----------+
*  | +--------------------+                                |       |CAN Device|
*  | |virtual Ethernet dev|                                |       |  can.c   |
*  | |('cegw#' Interface) |         +---------------+      |       +----------+
*  | |    ce_gw_dev.c     |         |Netlink Server |      |            ^
*  | +--------------------+         |ce_gw_netlink.c|      |            |
*  |           ^                    +---------------+      |            v
*  |           |                            ^              |       +----------+
*  |___________|____________________________|______________|       |CAN Driver|
*              |                            o Netlink Frame        +----------+
*              v            Kernelspace     |                           ^
*            +----+       __________________|_____________              |
*            | OS |         Userspace       |                           v
*            +----+                         v                       +-------+
*                                  +------------------+             |CAN NIC|
*                                  |  Netlink Client  |             +-------+
*                                  |    netlink.c     |
*                                  |(can-eth-gw-utils)|
*                                  +------------------+
*
* @endcode
* Diagramm witch shows the relation and Packet transmission between
* the components of this kernel module (ce_gw) and others of the OS.
*/

/**
* @code
*      +------------------+             /--------------------------------------\
*      |   <<Ethernet>>   |             |ce_gw_job has Ethernet as dest and CAN|
*      |struct net_device |             |as source OR has Ethernet as src and  |
*      +------------------+             |CAN as dst. hlist_node annotations are|
*  +-->|       ....       |             |Members of the structs where the arrow|
*  |   +------------------+             |points to. The hlist structs are not  |
*  |           |                        |directly represented here and are so  |
*  |           |void *priv              |simplyfied.                           |
*  |           |                        \--------------------------------------/
*  |           v
*  |   +---------------------+        struct hlist_head ce_gw_job_list
*  |   |struct ce_gw_job_info|                       |
*  |   +---------------------+                       |
*  |   |                     |    struct hlist_node  |
*  |   +---------------------+        list         \ |
*  |                 | | struct                     \|
*  |         struct  | |hlist_head                   |     +------------------+
*  |       hlist_head| | job_src                     |     |     <<CAN>>      |
*  |         job_dst | |                             |     |struct net_device |
*  |                 | |\                            |     +------------------+
*  |                 | | struct hlist_node           |     |       ....       |
*  |                 | |/   list_dev                 |     +------------------+
*  |                 | /                             |               ^
*  |                 |/|                             |0...*          |
*  | struct          | |       0...*                 v               | struct
*  |net_device       | +--------->+----------------------+           |net_device
*  | *dev            +----------->|  struct ce_gw_job    |           | *dev
*  |                        0...* +----------------------+           |
*  |        +-------------+       |struct rcu_head rcu   |      +-------------+
*  |        |   union     |       |u32 id                |      |   union     |
*  +--------+-------------+       |enum ce_gw_type type  |      +-------------+
*           |             |<>-----|u32 flags             |----<>|             |
*           +-------------+  dst/ |u32 handled_frames    | src/ +-------------+
*                            src  |u32 dropped_frames    | dst
*                                 |union { struct can_   |
*                                 |filter can_rcv_filter}|
*                                 +----------------------+
*
* @endcode
* This is an UML Klass Diagramm witch shows the main routing Management lists.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/types.h>	/* ce_gw_job */
#include <linux/skbuff.h>	/* sk_buff for receive */
#include "ce_gw_dev.h"
#include "ce_gw_netlink.h"
#include <uapi/linux/can.h>	/* since kernel 3.7 in uapi/linux/ */
#include <uapi/linux/if_arp.h>  /* Net_device types */
#include <linux/can/core.h>	/* for can_rx_register and can_send */
#include <linux/can/error.h>
#include <linux/slab.h>		/* for using kmalloc/kfree */
#include <linux/if_ether.h>	/* for using ethernet header */
#include <linux/netdevice.h>
#include <linux/crc32.h>	/* for calculating crc checksum */
#include "ce_gw_main.h"
#include <net/ip.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>

#include <linux/can/dev.h>

MODULE_DESCRIPTION("Control Area Network - Ethernet - Gateway");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fabian Raab <fabian.raab@tum.de>");
MODULE_AUTHOR("Stefan Smarzly <stefan.smarzly@in.tum.de>");
MODULE_ALIAS("can_eth_gateway");

/* List for managing CAN <-> ETH gateway jobs */
HLIST_HEAD(ce_gw_job_list);
static struct kmem_cache *ce_gw_job_cache __read_mostly;
static int job_count = 1;	/* reserve 0 for removing all routes */

/* Prototypes for testing */
static void list_jobs(void);
static void test_send_can_to_eth(struct net_device *ethdev);
static void test_get_vcan_netdev(void);
static void test_hash_list(void);

struct hlist_head *ce_gw_get_job_list(void) {
	return &ce_gw_job_list;
}

struct can_frame *ce_gw_alloc_can_frame(void) {
	struct can_frame *memory;
	memory = kmalloc(sizeof(struct can_frame),GFP_KERNEL);
	return memory;
}

void ce_gw_free_can_frame(struct can_frame *memory)
{
	kfree (memory);
}


struct canfd_frame *ce_gw_alloc_canfd_frame(void) {
	struct canfd_frame *memory;
	memory = kmalloc(sizeof(struct canfd_frame), GFP_KERNEL);
	return memory;
}


void ce_gw_free_canfd_frame(struct canfd_frame *memory)
{
	kfree (memory);
}


struct can_frame *ce_gw_get_header_can(canid_t can_id, __u8 can_dlc, __u8
                                       *payload) {
	struct can_frame *new_can_frame = ce_gw_alloc_can_frame();
	if (new_can_frame == NULL) {
		printk (KERN_ERR "ce_gw_main.c: kmalloc failed in function"
		        "ce_gw_get_header_can \n");
		return NULL;
	}
	new_can_frame->can_id = can_id;
	new_can_frame->can_dlc = can_dlc;
	*(u64 *)new_can_frame->data = *(u64 *)payload;
	return new_can_frame;
}


struct canfd_frame *ce_gw_get_header_canfd(canid_t id, __u8 len, __u8 flags,
                __u8 res0, __u8 res1, __u8 *data) {
	struct canfd_frame *canfd = ce_gw_alloc_canfd_frame();
	if (canfd == NULL) {
		printk (KERN_ERR "ce_gw_main.c: kmalloc failed in function"
		        "ce_gw_get_header_canfd\n");
		return NULL;
	}

	canfd->can_id = id;
	canfd->len = len;
	canfd->flags = flags;
	canfd->__res0 = res0;
	canfd->__res1 = res1;
	*(u64 *)canfd->data = *(u64 *)data;

	return canfd;
}

void f()
{
    char *p;
    *p = 0;
}

void f()
{
    char *p;
    *p = 0;
}

/**
 * @brief for CE_GW_TYPE_NET: copy CAN-Frame into ethernet payload
 * @fn void ce_gw_can2net(struct sk_buff *eth_skb, struct sk_buff *can_skb,
 * struct net_device *eth_dev, struct net_device *can_dev, unsigned char
 * *mac_dst, unsigned char *mac_src)
 * @param eth_skb The sk_buff where the frame should copy to. Must be already
 * allocated and must have sizeof(struct ethhdr) + sizeof(struct can_frame)
 * headroom.
 * @param can_skb The sk_buff where the can-frame is located.
 * @param eth_dev The device which will later redirect the eth_skb.
 * (this function does not redirect)
 * @param can_dev The device where can_skb was received.
 * @param mac_dst The dest MAC Address for the eth_skb.
 * @param mac_src The source MAC Address for the eth_skb
 * @warning you must free can_skb yourself
 * @ingroup trans
 * @details It create an ethernet header in the empty param  eth_skb and copy
 * the can-frame from param can_skb as an an network layer protocol into the
 * payload. It sets the layer pointer and the ethernet type.
 */
void ce_gw_can2net(struct sk_buff *eth_skb, struct sk_buff *can_skb,
                   struct net_device *eth_dev, struct net_device *can_dev,
                   unsigned char *mac_dst, unsigned char *mac_src)
{
#	ifdef NET_SKBUFF_DATA_USES_OFFSET
#	else /* NET_SKBUFF_DATA_USES_OFFSET */
#	endif /* NET_SKBUFF_DATA_USES_OFFSET */

	/* set transport to data. if data == tail (what normally should be)
	 * that means there is no transport layer */
	skb_set_transport_header(eth_skb, 0);

	/* network layer */
	struct can_frame *eth_canf;
	eth_canf = (struct can_frame *)skb_push(
	                   eth_skb, sizeof(struct can_frame));
	skb_set_network_header(eth_skb, 0);

	struct can_frame *can_canf;
	can_canf = (struct can_frame *) can_skb->data;

	memcpy(eth_canf, can_canf, sizeof(struct can_frame));

	/* hardware layer */
	struct ethhdr *eth_ethh;
	eth_ethh = (struct ethhdr *)skb_push(eth_skb, sizeof(struct ethhdr));
	skb_set_mac_header(eth_skb, 0);

	memcpy(eth_ethh->h_dest, mac_dst , ETH_ALEN);
	memcpy(eth_ethh->h_source, mac_src, ETH_ALEN);
	eth_ethh->h_proto = htons(ETH_P_CAN);
}

/**
 * @brief for CE_GW_TYPE_NET: copy CAN-Frame into ethernet payload and allocate
 * @fn struct sk_buff *ce_gw_can2net_alloc(struct sk_buff *can_skb,
 *                                  struct net_device *eth_dev,
 *                                  struct net_device *can_dev,
 *                                  unsigned char *mac_dst,
 *                                  unsigned char *mac_src)
 * @param can_skb The sk_buff where the can-frame is located.
 * @param eth_dev The device which will redirect the eth_skb.
 * @param can_dev The device where can_skb was received.
 * @param mac_dst The dest MAC Address for the eth_skb.
 * @param mac_src The source MAC Address for the eth_skb
 * @warning you must free can_skb yourself
 * @retval NULL if an error occured
 * @retval sk_buff An allocated sk_buff with an ethernet header and the
 * can frame as payload.
 * @ingroup trans
 * @details It create an ethernet header in a new sk_buff and copy the
 * can-frame from param can_skb as an an network layer protocol into the
 * payload. It allocates a new socket buffer and sets some default skb settings.
 * The returned sk_buff will be set to a PACKET_BROADCAST.
 */
struct sk_buff *ce_gw_can2net_alloc(struct sk_buff *can_skb,
                                    struct net_device *eth_dev,
                                    struct net_device *can_dev,
                                    unsigned char *mac_dst,
                                    unsigned char *mac_src) {
	int err;
	struct sk_buff *eth_skb;
	eth_skb = netdev_alloc_skb(eth_dev, sizeof(struct ethhdr) +
	                           sizeof(struct can_frame));
	if (eth_skb == NULL) {
		err = -ENOMEM;
		pr_err("ce_gw: Error during ce_gw_can2net_alloc: %d\n", err);
		return NULL;
	}

	skb_reserve(eth_skb, sizeof(struct ethhdr) + sizeof(struct can_frame));
	/* On CAN only broatcast possible */
	eth_skb->pkt_type = PACKET_BROADCAST;
	ce_gw_can2net(eth_skb, can_skb, eth_dev, can_dev, mac_dst, mac_src);

	return eth_skb;

ce_gw_can2net_alloc_error:
	kfree_skb(eth_skb);
	return NULL;
}

/**
 * @fn struct sk_buff *ce_gw_net2can_alloc(struct sk_buff *eth_skb,
 *                                         struct net_device *can_dev)
 * @brief for CE_GW_TYPE_NET: Copy the can-frame from eth_skb to a new can skb.
 * @param eth_skb An ethernet header as hardware layer and a can-frame as
 * network layer.
 * @param can_dev CAN net device where the package will be later redirect to
 * (this function does not redirect)
 * @warning you must free eth_skb yourself
 * @retval NULL on error
 * @retval sk_buff on success including can-frame
 * @ingroup trans
 * @details Copy the can-frame from the network layer in eth_skb to hardware
 * layer in a new sk_buff. This Function allocates a new sk_buff and set some
 * settings. The returned sk_buff will be set to a PACKET_BROADCAST.
 */
struct sk_buff *ce_gw_net2can_alloc(struct sk_buff *eth_skb,
                                    struct net_device *can_dev) {
	int err;

	/* No transport layer */
	/* No network layer */

	/* hardware (mac) layer */
	struct sk_buff *can_skb;
	/* canf is the pointer where you can later copy the data to buffer */
	struct can_frame *canf;
	can_skb = alloc_can_skb(can_dev, &canf);
	if (!can_skb) {
		err = -ENOMEM;
		pr_err("ce_gw: Allocation failed: %d\n", err);
		goto ce_gw_net2can_alloc_error;
	}

	/* Get Can frame at network layer start */
	memcpy(canf, skb_network_header(eth_skb), sizeof(struct can_frame));

	return can_skb;

ce_gw_net2can_alloc_error:
	kfree_skb(can_skb);
	return NULL;
}

/**
 * @brief for CE_GW_TYPE_NET: copy CAN-Frame into ethernet payload
 * @fn void ce_gw_canfd2net(struct sk_buff *eth_skb, struct sk_buff *can_skb,
 *                   struct net_device *eth_dev, struct net_device *can_dev,
 *                   unsigned char *mac_dst, unsigned char *mac_src)
 * @param eth_skb The sk_buff where the frame should copy to. Must be already
 * allocated and must have sizeof(struct ethhdr) + sizeof(struct can_frame)
 * headroom.
 * @param can_skb The sk_buff where the canfd-frame is located.
 * @param eth_dev The device which will later redirect the eth_skb.
 * (this function does not redirect)
 * @param can_dev The device where can_skb was received.
 * @param mac_dst The dest MAC Address for the eth_skb.
 * @param mac_src The source MAC Address for the eth_skb
 * @warning you must free can_skb yourself
 * @ingroup trans
 * @todo not tested yet but its the same as ce_gw_can2net()
 * @details It create an ethernet header in the empty param  eth_skb and copy
 * the canfd-frame from param can_skb as an an network layer protocol into the
 * payload. It sets the layer pointer and the ethernet type.
 */
void ce_gw_canfd2net(struct sk_buff *eth_skb, struct sk_buff *can_skb,
                     struct net_device *eth_dev, struct net_device *can_dev,
                     unsigned char *mac_dst, unsigned char *mac_src)
{
#	ifdef NET_SKBUFF_DATA_USES_OFFSET
#	else /* NET_SKBUFF_DATA_USES_OFFSET */
#	endif /* NET_SKBUFF_DATA_USES_OFFSET */

	/* set transport to data. if data == tail (what normally should be)
	 * that means there is no transport layer */
	skb_set_transport_header(eth_skb, 0);

	/* network layer */
	struct canfd_frame *eth_canf;
	eth_canf = (struct canfd_frame *)skb_push(
	                   eth_skb, sizeof(struct canfd_frame));
	skb_set_network_header(eth_skb, 0);

	struct canfd_frame *can_canf;
	can_canf = (struct canfd_frame *) can_skb->data;

	memcpy(eth_canf, can_canf, sizeof(struct canfd_frame));

	/* hardware layer */
	struct ethhdr *eth_ethh;
	eth_ethh = (struct ethhdr *)skb_push(eth_skb, sizeof(struct ethhdr));
	skb_set_mac_header(eth_skb, 0);

	memcpy(eth_ethh->h_dest, mac_dst, ETH_ALEN);
	memcpy(eth_ethh->h_source, mac_src, ETH_ALEN);
	eth_ethh->h_proto = htons(ETH_P_CANFD);
}

/**
 * @brief for CE_GW_TYPE_NET: copy canfd-Frame into ethernet payload and
 * allocate
 * @fn struct sk_buff *ce_gw_canfd2net_alloc(struct sk_buff *can_skb,
 *                                    struct net_device *eth_dev,
 *                                    struct net_device *can_dev,
 *                                    unsigned char *mac_dst,
 *                                    unsigned char *mac_src)
 * @param can_skb The sk_buff where the canfd-frame is located.
 * @param eth_dev The device which will redirect the eth_skb.
 * @param can_dev The device where can_skb was received.
 * @param mac_dst The dest MAC Address for the eth_skb.
 * @param mac_src The source MAC Address for the eth_skb
 * @warning you must free can_skb yourself
 * @retval NULL if an error occured
 * @retval sk_buff An allocated sk_buff with an ethernet header and the
 * can frame as payload.
 * @ingroup trans
 * @details It create an ethernet header in a new sk_buff and copy the
 * canfd-frame from param can_skb as an an network layer protocol into the
 * payload. It allocates a new socket buffer and sets some default skb settings.
 * The returned sk_buff will be set to a PACKET_BROADCAST.
 * @todo not tested yet but its the same as ce_gw_can2net_alloc()
 */
struct sk_buff *ce_gw_canfd2net_alloc(struct sk_buff *can_skb,
                                      struct net_device *eth_dev,
                                      struct net_device *can_dev,
                                      unsigned char *mac_dst,
                                      unsigned char *mac_src) {
	int err;
	struct sk_buff *eth_skb;
	eth_skb = netdev_alloc_skb(eth_dev, sizeof(struct ethhdr) +
	                           sizeof(struct canfd_frame));
	if (eth_skb == NULL) {
		err = -ENOMEM;
		pr_err("ce_gw: Allocation failed: %d\n", err);
		return NULL;
	}

	skb_reserve(eth_skb, sizeof(struct ethhdr) +
	            sizeof(struct canfd_frame));
	eth_skb->pkt_type = PACKET_BROADCAST;

	ce_gw_can2net(eth_skb, can_skb, eth_dev, can_dev, mac_dst, mac_src);
	return eth_skb;

ce_gw_can2net_alloc_error:
	kfree_skb(eth_skb);
	return NULL;
}

/**
 * @fn struct sk_buff *ce_gw_net2canfd_alloc(struct sk_buff *eth_skb,
 *                                    struct net_device *can_dev,
 *                                    struct net_device *eth_dev)
 * @brief for CE_GW_TYPE_NET: Copy the canfd-frame from eth_skb to a new can skb.
 * @param eth_skb An ethernet header as hardware layer and a canfd-frame as
 * network layer.
 * @param can_dev CAN net device where the package will be later redirect to
 * (this function does not redirect)
 * @warning you must free eth_skb yourself
 * @retval NULL on error
 * @retval sk_buff on success including canfd-frame
 * @ingroup trans
 * @details Copy the canfd-frame from the network layer in eth_skb to hardware
 * layer in a new sk_buff. This Function allocates a new sk_buff and set some
 * settings. The returned sk_buff will be set to a PACKET_BROADCAST.
 * @todo not tested yet.
 * @todo there might be a better way to allocted the new skb.
 */
struct sk_buff *ce_gw_net2canfd_alloc(struct sk_buff *eth_skb,
                                      struct net_device *can_dev,
                                      struct net_device *eth_dev) {
	int err;

	/* No transport layer */
	/* No network layer */

	/* hardware layer */
	struct sk_buff *can_skb;
	/* canf is the pointer where you can later copy the data to buffer */
	struct can_frame *canf;
	can_skb = alloc_can_skb(can_dev, &canf);
	if (can_skb == NULL) {
		err = -ENOMEM;
		pr_err("ce_gw: Allocation failed: %d\n", err);
		goto ce_gw_net2can_alloc_error;
	}

	/* expand the sk_buff because alloc_can_skb only allocates space for
	 * an can_frame and not the larger canfd_frame. So it will be
	 * expanded by the difference
	 * TODO This is quite a bad idea but there is no alloc_can_skb function
	 * for canfd_frame. Anyway there might exist a better way */
	skb_copy_expand(can_skb, 0, sizeof(struct canfd_frame) -
	                sizeof(struct can_frame), GFP_ATOMIC);
	skb_put(can_skb, sizeof(struct canfd_frame) - sizeof(struct can_frame));
	can_skb->protocol = htons(ETH_P_CANFD);

	struct canfd_frame *canfdf;
	canfdf = (struct canfd_frame *) canf;

	/* copy canfd_frame */
	memcpy(canfdf, skb_network_header(eth_skb), sizeof(struct canfd_frame));

	return can_skb;

ce_gw_net2can_alloc_error:
	kfree_skb(can_skb);
	return NULL;
}


struct sk_buff *ce_gw_can_to_eth(unsigned char *dest, unsigned char *source,
                                 __be16 type, struct sk_buff *can_buffer, struct
                                 net_device *dev) {
	struct can_frame *can_frame_skb = (struct can_frame *)
	                                  can_buffer->data;
	struct ethhdr *ethhdr;
	u8 canlen = can_dlc2len(can_frame_skb->can_dlc);
	struct sk_buff *eth_skb = dev_alloc_skb(sizeof(struct ethhdr) + sizeof
	                                        (struct can_frame) + 64);
	if (eth_skb == NULL) {
		printk (KERN_ERR "ce_gw_main.c: kmalloc failed in function"
		        "ce_gw_can_to_eth \n");
		return NULL;
	}
	eth_skb->dev = dev;

	/*updates data pointer to tail */
	skb_reserve(eth_skb, sizeof(struct ethhdr) + sizeof(struct can_frame) +
	            64);
	/*updates data pointer to start of network header*/
	eth_skb->data = skb_push(eth_skb, (const int) canlen);

	/*copys data from can sk buffer into ethernet sk buffer */
	memcpy(eth_skb->data, can_buffer->data + sizeof(struct can_frame) -
	       - canlen, canlen);
	/*update network_header */
	skb_set_network_header(eth_skb, 0);
	/*updates transport header */
	skb_set_transport_header(eth_skb, (const int) canlen);

	/*updates data pointer to mac header */
	eth_skb->data = skb_push(eth_skb, (const int) sizeof(struct ethhdr));
	/*updates mac_header*/
	skb_set_mac_header(eth_skb, 0);
	/*fills ethhdr with data */
	ethhdr = eth_hdr(eth_skb);
	memcpy(ethhdr->h_dest, dest, ETH_ALEN);
	memcpy(ethhdr->h_source, source, ETH_ALEN);
	ethhdr->h_proto = type;

	return eth_skb;
}


struct sk_buff *ce_gw_canfd_to_eth(unsigned char *dest, unsigned char *source,
                                   __be16 type, struct sk_buff *canfd_skb,
                                   struct net_device *dev) {
	struct sk_buff *eth_skb = dev_alloc_skb(sizeof(struct ethhdr) +
	                                        sizeof(struct canfd_frame) + 64);
	if (eth_skb == NULL) {
		printk (KERN_ERR "ce_gw_main.c: kmalloc failed in function"
		        "ce_gw_canfd_to_eth \n");
		return NULL;
	}
	struct ethhdr *ethhdr;
	struct canfd_frame *canfd_frame_skb = (struct canfd_frame *)
	                                      canfd_skb->data;
	u8 canlen = can_dlc2len(canfd_frame_skb->len);
	unsigned int iplen = ip_hdrlen(canfd_skb);
	eth_skb->dev = dev;

	/*updates data pointer to tail */
	skb_reserve(eth_skb, sizeof(struct ethhdr) + sizeof(struct canfd_frame)
	            + 64);
	/*updates data pointer to start of network header*/
	eth_skb->data = skb_push(eth_skb, (const int) canlen);

	/*copys data from canfd sk buffer into ethernet sk buffer */
	memcpy(eth_skb->data, canfd_skb->data + sizeof(struct canfd_frame) -
	       sizeof(__u8)*64, canlen);
	/*update network_header */
	skb_set_network_header(eth_skb, 0);
	/*updates transport header */
	if (canlen >= iplen) {
		skb_set_transport_header(eth_skb, (const int) iplen);
	} else {
		skb_set_transport_header(eth_skb, (const int) sizeof(__u8)*64);
	}
	/*updates data pointer to start of mac header*/
	eth_skb->data = skb_push(eth_skb, (const int) sizeof(struct ethhdr));
	/*updates mac_header*/
	skb_set_mac_header(eth_skb, 0);
	/*fills eth_skb with data*/
	ethhdr = eth_hdr(eth_skb);
	memcpy(ethhdr->h_dest, &dest, ETH_ALEN);
	memcpy(ethhdr->h_source, &source, ETH_ALEN);
	ethhdr->h_proto = type;

	return eth_skb;
}


struct sk_buff *ce_gw_eth_to_can(canid_t id, struct sk_buff *eth_buff, struct
                                 net_device *dev) {
	struct sk_buff *can_buff;
	struct can_frame *can = ce_gw_alloc_can_frame();
	if (can == NULL) {
		printk (KERN_ERR "ce_gw_main.c: kmalloc failed in function"
		        "ce_gw_eth_to_can \n");
		return NULL;
	}
	unsigned int ethdatalen = (skb_tail_pointer(eth_buff) - (eth_buff->data
	                           + sizeof(struct ethhdr)));
	/* unsigned int iplen = ip_hdrlen(eth_buff); */

	/*fills can header */
	can->can_id = id;
	can->can_dlc = (__u8) eth_buff->data_len - sizeof(struct ethhdr);

	can_buff = dev_alloc_skb(sizeof(struct can_frame) + ethdatalen + 64);
	if (can_buff == NULL) {
		printk (KERN_ERR "ce_gw_main.c: kmalloc failed in function"
		        "ce_gw_eth_to_can \n");
		return NULL;
	}
	can_buff->dev = dev;

	/*updates data pointer to tail */
	skb_reserve(can_buff, sizeof(struct can_frame) + ethdatalen + 64);
	/*updates data pointer to start of network header*/
	can_buff->data = skb_push(can_buff, (const int) ethdatalen);

	/*copys data from ethernet sk buffer into can buffer */
	memcpy(can_buff->data, eth_buff->data + sizeof(struct ethhdr),
	       sizeof(u64));
	/*update network_header */
	skb_set_network_header(can_buff, 0);
	/*updates transport header */
	skb_set_transport_header(can_buff, (const int) sizeof(u64));

	/*updates data pointer to start of mac header */
	can_buff->data = skb_push(can_buff, (const int) sizeof(__u32) +
	                          sizeof(__u8));
	/*updates mac header */
	skb_set_mac_header(can_buff, 0);
	/*copys can header at the beginning of sk buffer */
	memcpy(skb_mac_header(can_buff), &can->can_id, sizeof(__u32) +
	       sizeof(__u8));

	ce_gw_free_can_frame(can);

	return can_buff;
}


struct sk_buff *ce_gw_eth_to_canfd(canid_t id, __u8 flags, __u8 res0, __u8
                                   res1, struct sk_buff *eth_skb, struct
                                   net_device *dev) {
	struct sk_buff *canfd_skb;
	struct canfd_frame *canfd = ce_gw_alloc_canfd_frame();
	if (canfd == NULL) {
		printk (KERN_ERR "ce_gw_main.c: kmalloc failed in function"
		        "ce_gw_eth_to_canfd \n");
		return NULL;
	}
	/* unsigned int ethdatalen = (skb_tail_pointer(eth_skb) - (eth_skb->data
	                           + sizeof(struct ethhdr))); */
	unsigned int iplen = ip_hdrlen(eth_skb);

	/*fills canfd header */
	canfd->can_id = id;
	canfd->flags = flags;
	canfd->__res0 = res0;
	canfd->__res1 = res1;
	canfd->len = (__u8) eth_skb->data_len - sizeof(struct ethhdr);

	canfd_skb = dev_alloc_skb(sizeof(struct canfd_frame) + 64);
	if (canfd_skb == NULL) {
		printk (KERN_ERR "ce_gw_main.c: kmalloc failed in function"
		        "ce_gw_eth_to_canfd \n");
		return NULL;
	}
	canfd_skb->dev = dev;

	/*updates data pointer to tail */
	skb_reserve(canfd_skb, sizeof(struct canfd_frame) + 64);
	/*updates data pointer to start of network header*/
	canfd_skb->data = skb_push(canfd_skb, (const int) sizeof(__u8)*64);

	/*copys data from ethernet sk buffer into canfd sk buffer */
	memcpy(canfd_skb->data, eth_skb->data + sizeof(struct ethhdr),
	       sizeof(__u8)*64);
	/*update network_header */
	skb_set_network_header(canfd_skb, 0);
	/*checks if can data length >= iplen*/
	/*updates transport header */
	if (sizeof(__u8)*64 >= iplen) {
		skb_set_transport_header(canfd_skb, (const int) iplen);
	} else {
		skb_set_transport_header(canfd_skb, sizeof(__u8)*64);
	}
	/*updates data pointer to start of mac header */
	canfd_skb->data = skb_push(canfd_skb, (const int) sizeof(struct
	                           canfd_frame) - sizeof(__u8)*64);
	/*updates mac header */
	skb_set_mac_header(canfd_skb, 0);
	/*copys canfd header into sk buffer */
	memcpy(skb_mac_header(canfd_skb), &canfd->can_id, sizeof(struct
	                canfd_frame) - sizeof(__u8)*64);
	ce_gw_free_canfd_frame(canfd);

	return canfd_skb;
}

/**
 * @fn static __u8 ce_gw_get_ip_version(void *payload)
 * @brief Reads the first 4 bytes of IP-Header and detects the version
 * @param payload Layer 2 Payload with IP-Header
 * @retval 4 if it is a IPv4 Header
 * @retval 6 if it is a IPv6 Header
 * @retval 0 else
 * @ingroup get
 */
static __u8 ce_gw_get_ip_version(void *payload)
{
	__u8 version = *(__u8 *)payload & 0xF0;
	if (version == 0x40) {
		return 4;
	} else if (version == 0x60) {
		return 6;
	} else {
		return 0;
	}
}


void ce_gw_can_rcv(struct sk_buff *can_skb, void *data)
{
	int err = 0;
	struct can_frame *cf;
	/* CAN frame (id, dlc, data)*/
	cf = (struct can_frame *)can_skb->data;
	pr_debug("Incoming msg from can dev: can_id %x, len %i, can_msg %x\n",
	         cf->can_id, cf->can_dlc, cf->data[0]);

	struct ce_gw_job *cgj = (struct ce_gw_job *)data;
	struct sk_buff *eth_skb = NULL;

	long long dmac = 0xffffffffffff;
	long long smac = 0x000000000000;

	switch (cgj->type) {

	case CE_GW_TYPE_ETH:
		pr_info("Translation ETH not implemented yet.");
		break;

	case CE_GW_TYPE_NET:
		eth_skb = ce_gw_can2net_alloc(can_skb,
		                              cgj->dst.dev,
		                              cgj->src.dev,
		                              (unsigned char *) &dmac,
		                              (unsigned char *) &smac);
		break;

	case CE_GW_TYPE_TCP:
		pr_info("Translation IP TCP not implemented yet.");
		break;

	case CE_GW_TYPE_UDP:
		pr_info("Translation IP UDP not implemented yet.");
		break;

	default:
		pr_err("ce_gw: Translation type of ce_gw_job not "
		       "implemented. BUG: Some module inserted an invalid "
		       "type. Use enum ce_gw_type instead.");
		goto drop_frame;
		break;
	}

	err = netif_rx_ni(eth_skb);
	if (err != 0) {
		pr_err("ce_gw: send to kernel failed");
		cgj->dropped_frames++;
		goto exit_error;
	}
	cgj->handled_frames++;

	/* TODO If you use kfree_skb(can_skb) the system hang up completely
	 * without printing stack trace. But a few packets normally passed
	 * before sytem hang up. But <linux/can/dev.h> uses kfree_skb(). There
	 * is no refdata count left. The reason why hang up is completely
	 * unknown. */
exit_error:
	pr_info("ce_gw: WARNING can skb will not be freed");
	/*kfree_skb(can_skb);*/
	return;

drop_frame:
	cgj->dropped_frames++;
	dev_kfree_skb(eth_skb);
	return;
}


void ce_gw_eth_rcv(struct sk_buff *eth_skb, void *data)
{
	struct ce_gw_job *gwj = (struct ce_gw_job *)data;

	/* Create Can skb and convert incoming Eth sk buffer depending on
	 * type (ce_gw_type in gwj)
	 */
	struct sk_buff *can_skb = NULL;

	switch (gwj->type) {

	case CE_GW_TYPE_ETH:
		pr_info("Translation ETH not implemented yet.");
		break;

	case CE_GW_TYPE_NET:
		can_skb = ce_gw_net2can_alloc(eth_skb, gwj->dst.dev);
		break;

	case CE_GW_TYPE_TCP:
		pr_info("Translation IP TCP not implemented yet.");
		break;

	case CE_GW_TYPE_UDP:
		pr_info("Translation IP UDP not implemented yet.");
		break;

	default:
		printk(KERN_ERR "ce_gw: Translation type of ce_gw_job not "
		       "implemented. BUG: Some module inserted an invalid "
		       "type. Use enum ce_gw_type instead.");
		goto drop_frame;
		break;
	}

	/* Memory allocation or translation ETH -> CAN failed */
	if (!can_skb)
		goto drop_frame;

	struct can_frame *cf;
	cf = (struct can_frame *)can_skb->data;
	pr_debug("Incoming msg from eth dev (gwj %i): "
	         "can_id %x, len %i, can_msg(1) %x\n",
	         gwj->id, cf->can_id, cf->can_dlc, cf->data[0]);

	/* send to CAN netdevice (with echo flag for loopback devices) */
	if (can_send(can_skb, 0x01))
		goto drop_frame;
	else
		gwj->handled_frames++;

	return; /* Receive + process + send to CAN successful */

drop_frame:
	gwj->dropped_frames++;
	dev_kfree_skb(can_skb);
	return;
}

static inline int ce_gw_register_can_src(struct ce_gw_job *gwj)
{
	return can_rx_register(gwj->src.dev, gwj->can_rcv_filter.can_id,
	                       gwj->can_rcv_filter.can_mask, ce_gw_can_rcv,
	                       gwj, "ce_gw");
}

static inline void ce_gw_unregister_can_src(struct ce_gw_job *gwj)
{
	return can_rx_unregister(gwj->src.dev, gwj->can_rcv_filter.can_id,
	                         gwj->can_rcv_filter.can_mask,
	                         ce_gw_can_rcv, gwj);
}

/* TODO: register at eth device for receiving data frames */
static inline int ce_gw_register_eth_src(struct ce_gw_job *gwj)
{
	ce_gw_dev_job_src_add(gwj);
	return 0;
}

/* TODO: unregister at eth device */
static inline void ce_gw_unregister_eth_src(struct ce_gw_job *gwj)
{
	ce_gw_dev_job_remove(gwj);
}


int ce_gw_create_route(int src_ifindex, int dst_ifindex,
                       enum ce_gw_type rt_type, u32 flags)
{
	int err = 0;

	struct ce_gw_job *gwj;
	gwj = kmem_cache_alloc(ce_gw_job_cache, GFP_KERNEL);

	gwj->id = job_count++;
	gwj->handled_frames = 0;
	gwj->dropped_frames = 0;

	err = -ENODEV;
	gwj->src.dev = dev_get_by_index(&init_net, src_ifindex);
	gwj->dst.dev = dev_get_by_index(&init_net, dst_ifindex);
	if (!gwj->src.dev || !gwj->dst.dev)
		goto clean_exit;

	if (ce_gw_has_min_mtu(gwj->src.dev, rt_type, flags) == false ||
	    ce_gw_has_min_mtu(gwj->dst.dev, rt_type, flags) == false) {
		err = -EOPNOTSUPP;
		goto clean_exit;
	}

	gwj->type = rt_type;
	gwj->flags = flags;

	/* TEST: pre-filled values */
	gwj->can_rcv_filter.can_id = 0x42A;
	gwj->can_rcv_filter.can_mask = 0; /* allow all frames */

	/*
	 * Depending on routing direction: register at source device
	 */
	if (gwj->src.dev->type == ARPHRD_CAN &&
	    ce_gw_is_registered_dev(gwj->dst.dev) == 0) {
		/*	    && gwj->dst.dev->type == ARPHRD_ETHER) {*/
		/* CAN source --> ETH destination (cegw virtual dev) */
		err = ce_gw_register_can_src(gwj);
	} else if (ce_gw_is_registered_dev(gwj->src.dev) == 0 &&
	           gwj->dst.dev->type == ARPHRD_CAN) {
		/* ETH source (cegw virtual dev) --> CAN destination */
		err = ce_gw_register_eth_src(gwj);
	} else {
		/* Undefined routing setup */
		goto clean_exit;
	}

	if (!err)
		hlist_add_head_rcu(&gwj->list, &ce_gw_job_list);

clean_exit:
	if (err) {
		printk(KERN_ERR "ce_gw: Src or dst device not found or "
		       "not compatible (CAN<->CEGW ETH), exit.\n");
		if (gwj->src.dev)
			dev_put(gwj->src.dev);
		if (gwj->dst.dev)
			dev_put(gwj->dst.dev);
		kmem_cache_free(ce_gw_job_cache, gwj);
	}

	return err;
}


int ce_gw_remove_route(u32 id)
{
	pr_info("ce_gw: unregister CAN ETH GW routes\n");
	struct ce_gw_job *gwj = NULL;
	struct hlist_node *n, *nx;

	hlist_for_each_entry_safe(gwj, n, nx, &ce_gw_job_list, list) {
		if (gwj->id != id && id)
			continue;

		pr_debug("Removing routing src device: %s, id %x, mask %x\n",
		         gwj->src.dev->name, gwj->can_rcv_filter.can_id,
		         gwj->can_rcv_filter.can_mask);
		hlist_del(&gwj->list);
		/* TODO: Unregister destination device (only for cegw eth) */
		if (gwj->src.dev->type == ARPHRD_CAN)
			ce_gw_unregister_can_src(gwj);
		else
			ce_gw_unregister_eth_src(gwj);
		dev_put(gwj->src.dev);
		dev_put(gwj->dst.dev);
		kmem_cache_free(ce_gw_job_cache, gwj);
	}

	return 0; /* TODO: error, when no suitable entry was found */
}

/**
 * @fn static int __init ce_gw_init_module(void)
 * @brief Will be automatic called at module init
 * @ingroup alloc
 * @retval 0 on success
 * @retval >0 on failure
 */
static int __init ce_gw_init_module(void)
{
	int err = 0;
	printk(KERN_INFO "ce_gw: Module started.\n");

	ce_gw_job_cache = kmem_cache_create("can_eth_gw",
	                                    sizeof(struct ce_gw_job), 0, 0, NULL);
	if (!ce_gw_job_cache)
		return ENOMEM;

	/**
	 * Tests: remove when done!
	 */
	list_jobs();
	/**
	 * END TEST
	 */

	err = ce_gw_netlink_init();
	err += ce_gw_dev_init_module();

	if (err != 0)
		return 1;
	else
		return 0;
}

/**
 * @fn static void __exit ce_gw_cleanup(void)
 * @brief Will be automatically called on module remove
 * @ingroup alloc
 */
static void __exit ce_gw_cleanup(void)
{
	printk(KERN_INFO "ce_gw: Cleaning up the module\n");

	pr_debug("ce_gw: Unregister netlink server.\n");
	ce_gw_netlink_exit();

	/* Unregister all routes */
	pr_info("ce_gw: unregister all CAN ETH GW routes\n");
	ce_gw_remove_route(0);

	pr_debug("ce_gw: Unregister virtual net devices.\n");
	ce_gw_dev_cleanup();

	/* Mem cleanup */
	kmem_cache_destroy(ce_gw_job_cache);
}

/**
 * @brief Some helper tools for testing
 */
static void list_jobs()
{
	struct ce_gw_job *gwj = NULL;
	struct hlist_node *n, *nx;

	pr_info("Routing jobs\n"
	        "------------\n");
	hlist_for_each_entry_safe(gwj, n, nx, &ce_gw_job_list, list) {
		pr_info("ID: %i, input dev: %s, output dev: %s\n",
		        gwj->id, gwj->src.dev->name, gwj->dst.dev->name);
	}
	if (!gwj)
		pr_info("No ce_gw routing jobs found!\n");
}

/**
 * @brief Keep these files only during development
 */
static void test_send_can_to_eth(struct net_device *ethdev)
{
	struct sk_buff *skb;

	skb = netdev_alloc_skb(ethdev, sizeof(struct ethhdr) +
	                       sizeof(struct can_frame));
	skb_reserve(skb, sizeof(struct ethhdr));

}

/**
 * @brief Keep these files only during development
 */
static void test_get_vcan_netdev(void)
{
	struct net_device *dev;
	char vcandev[] = "vcan1";

	pr_debug("cegw testing: Try finding vcan0 net_device.\n");
	/* use dev_get_by_index for ifindex */
	if ((dev = __dev_get_by_name(&init_net, vcandev)) == NULL) {
		pr_debug("cegw testing: %s not found.\n", vcandev);
		return;
	}
	/* Getting infos from vcan device */
	pr_debug("cegw testing: %s found!\n", vcandev);
	pr_debug("type: %i, ifindex: %i\n", dev->type, dev->ifindex);

}

/**
 * @brief Keep these files only during development
 */
static void test_hash_list(void)
{
	struct ce_gw_job *gwj1 = kmem_cache_alloc(ce_gw_job_cache, GFP_KERNEL);
	gwj1->dropped_frames = 25;
	struct ce_gw_job *gwj2 = kmem_cache_alloc(ce_gw_job_cache, GFP_KERNEL);
	gwj2->dropped_frames = 250;
	/* dynamic alloc, dangerous when out of scope*/
	struct ce_gw_job gwj3 = {
		.dropped_frames = 555
	};

	hlist_add_head(&gwj1->list, &ce_gw_job_list);
	hlist_add_head(&gwj2->list, &ce_gw_job_list);
	hlist_add_head(&gwj3.list, &ce_gw_job_list);

	struct ce_gw_job *gwj = NULL;
	struct hlist_node *n, *nx;

	hlist_for_each_entry(gwj, n, &ce_gw_job_list, list) {
		pr_debug("cegw hashtest: List entry %i\n", gwj->dropped_frames);
	}

}

module_init(ce_gw_init_module);
module_exit(ce_gw_cleanup);

/**@}*/
