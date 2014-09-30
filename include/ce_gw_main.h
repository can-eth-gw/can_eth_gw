/**
 * @file ce_gw_main.h
 * @brief Control Area Network - Ethernet - Gateway - Haeder
 * @author Stefan Smarzly (stefan.smarzly@in.tum.de)
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

#ifndef __CE_GW_MAIN_H__
#define __CE_GW_MAIN_H__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/types.h>	/* ce_gw_job */
#include <linux/skbuff.h>	/* sk_buff for receive */
#include "ce_gw_dev.h"
#include "ce_gw_netlink.h"
#include <uapi/linux/can.h>	/* since kernel 3.7 in uapi/linux/ */
#include <uapi/linux/if_arp.h>	/* Net_device types */
#include <linux/can/core.h>	/* for can_rx_register and can_send */
#include <linux/can/error.h>
#include <linux/slab.h>		/* for using kmalloc/kfree */
#include <linux/if_ether.h>
#include <linux/netdevice.h>

/** ce_gw_job.flags: is Gateway CANfd compatible */
#define CE_GW_F_CAN_FD 0x00000001 

/**
 * @enum ce_gw_type
 * @brief Type of the Gateway
 */
enum ce_gw_type {
	CE_GW_TYPE_NONE,/**< No Type. Should normally not be used. */
	CE_GW_TYPE_ETH, /**< Convert CAN header to Ethernet header
			 * Payload of CAN header must contain an IP header */
	CE_GW_TYPE_NET, /**< Copy complete CAN Frame into Ethernet payload
			 * e.g use CAN as Network Layer (Layer 3) protocol. */
	CE_GW_TYPE_TCP, /**< Convert CAN header into an IP/TCP package */
	CE_GW_TYPE_UDP, /**< Convert CAN header into an IP/UDP package */
	__CE_GW_TYPE_MAX, /**< Maximum Type Number + 1 */
};
#define CE_GW_TYPE_MAX (__CE_GW_TYPE_MAX - 1) /**< Maximum Type Number */

/**
 * @struct ce_jw_job
 * @brief Mapping and statistics for CAN <-> ETH gateway jobs
 * (Based on can/gw.c, rev 20101209)
 */
struct ce_gw_job {
	struct hlist_node list;	/**< List entry for ce_gw_job_list main list */
	struct rcu_head rcu;	/**< Lock monitor */
	struct hlist_node list_dev;	/**< List entry from the ETH device */
	u32 id;			/**< Unique Identifier of Gateway */
	enum ce_gw_type type;	/**< Translation type of the Gateway */
	u32 flags;		/**< Flags with settings of the Gateway */
	u32 handled_frames;	/**< counter for handles frames */
	u32 dropped_frames;	/**< counter for dropped_frames */

	union {
		struct net_device *dev;
	} src; 		/**< CAN / ETH frame data source */
	union {
		struct net_device *dev;
	} dst;		/**< CAN / ETH frame data destination */
	union {
		struct can_filter can_rcv_filter;
		/* TODO: Add ethernet receive filter (eth_rcv_filter) */
	}; /**< Filter incoming packet */
};

/**
 * @fn struct hlist_head *ce_gw_get_job_list(void);
 * @brief getter for HLIST_HEAD(ce_gw_job_list)
 * @returns Pointer to ce_gw_job_list.
 */
struct hlist_head *ce_gw_get_job_list(void);

/**
 * @fn struct can_frame *ce_gw_alloc_can_frame(void)
 * @brief allocates memory for the can frame
 * @return pointer to the memory allocated
 * @ingroup alloc
 * @todo not tested yet
 */
extern struct can_frame *ce_gw_alloc_can_frame(void);

/**
 * @fn void ce_gw_free_can_frame(struct can_frame *memory)
 * @brief frees memory allocated for can_frame
 * @param memory can frame that should be freed
 * @ingroup alloc
 * @todo not tested yet
 */
extern void ce_gw_free_can_frame(struct can_frame *memory);

/**
 * @fn struct canfd_frame *ce_gw_alloc_canfd_frame(void)
 * @brief allocates memory for a canfd frame
 * @return pointer to the memory allocated
 * @ingroup alloc
 * @todo not tested yet
 */
extern struct canfd_frame *ce_gw_alloc_canfd_frame(void);

/**
 * @fn void ce_gw_free_canfd_frame(struct canfd_frame *memory)
 * @brief frees memory allocated for can_frame
 * @param memory canfd_frame that should be freed
 * @ingroup alloc
 * @todo not tested yet
 */
extern void ce_gw_free_canfd_frame(struct canfd_frame *memory);

/**
 * @fn struct can_frame *ce_gw_get_header_can(canid_t can_id, __u8 can_dlc,
 * __u8 *payload)
 * @brief builds a can header in SFF (standart frame format) or EFF
 * (extended frame formate) with the given information
 * @param can_id_t identifier (11/29 bits) + error flag (0=data, 1= error) +
 *  remote transmission flag (1=rtr frame) + frame format flag (0=SFF, 1=EFF)
 * @param can_dlc data length code
 * @param payload data (maximum 64 bit in an 8*8 array)
 * @see include/linux/can.h
 * @retval can frame if successful
 * @retval NULL if unsuccessful
 * @todo not tested yet
 * @ingroup get
 */
extern struct can_frame *ce_gw_get_header_can(canid_t can_id, __u8 can_dlc,
  __u8 *payload);

/**
 * @fn struct canfd_frame *ce_gw_get_header_canfd(canid_t id, __u8 len, __u8
 * flags, __u8 res0, __u8 res1, __u8 *data)
 * @brief builds canfd_frame with the given information
 * @param id identifier: 32 bit (CAN_ID + EFF + RTR + ERR flag)
 * @param len frame payload length in byte
 * @param flags additional flags for CAN FD
 * @param res0 reserved / padding
 * @param res1 reserved / padding
 * @param data payload
 * @retval canfd frame if successful
 * @retval NULL if unsuccessful
 * @todo not tested yet
 * @ingroup get
 */
extern struct canfd_frame *ce_gw_get_header_canfd(canid_t id, __u8 len,
  __u8 flags, __u8 res0, __u8 res1, __u8 *data);

/**
 * @fn struct sk_buff *ce_gw_can_to_eth(unsigned char *dest, unsigned char
 * *source, __be16 type, struct sk_buff *can_buffer, struct net_device *dev)
 * @brief converts sk buffer including can frame into sk buffer including
 * ethernet_frame
 * @param dest MAC address of destination
 * @param source MAC address of source
 * @param type type of layer3 message (example: ipv4 or ipv6 ...)
 * @param can_buffer sk buffer including a can frame
 * @param dev device of destination
 * @retval sk_buffer on success including ethernet frame
 * @retval NULL if unsuccessful
 * @ingroup trans
 * @todo not tested yet
 */
extern struct sk_buff *ce_gw_can_to_eth(unsigned char *dest,
  unsigned char *source, __be16 type, struct sk_buff *can_buffer,
  struct net_device *net);

/**
 * @fn struct sk_buff *ce_gw_canfd_to_eth(unsigned char *dest, unsigned char
 * *scource, __be16 type, struct sk_buff *canfd_skb, struct net_device *dev)
 * @brief converts sk buffer including canfd frame into sk buffer including
 * ethernet frame
 * @param dest MAC address of destination
 * @param source MAC address of source
 * @param type type of layer3 message (example: ipv4 or ipv6 ...)
 * @param canfd_skb sk buffer including a canfd_frame
 * @param dev device of the destination
 * @retval sk_buff including ethernet frame if successful
 * @retval NULL if unsuccessful
 * @ingroup trans
 * @todo not tested yet
 */
extern struct sk_buff *ce_gw_canfd_to_eth(unsigned char *dest, unsigned char *source,
                                   __be16 type, struct sk_buff *canfd_skb,
                                   struct net_device *net);

/**
 * @fn struct sk_buff *ce_gw_eth_to_can(canid_t id, struct sk_buff *eth_buff,
 * struct net_device *dev)
 * @brief converst sk_buffer including an ethernet frame to sk_buffer
 * including a can_frame
 * @param id identifier of can_frame (see ce_gw_get_header for more information
 * @param eth_buff sk_buffer including an ethernet frame
 * @param dev device of the destination
 * @return sk_buffer including a can frame
 * @ingroup trans
 * @todo not tested yet
 */
extern struct sk_buff *ce_gw_eth_to_can(canid_t id, struct sk_buff *eth_buff, struct
                                 net_device *net);

/**
 * @fn struct sk_buff *ce_gw_eth_to_canfd(canid_t id, __u8 flags, __u8 res0,
 * __u8 res1, struct sk_buff *eth_skb, struct net_device *dev)
 * @brief converst sk buffer including an ethernet frame to sk buffer
 * including a canfd frame
 * @param id identifier of canfd (see ce_gw_get_canfd_header for more
 * information)
 * @param flags additional flags for CAN FD
 * @param res0 reserved / padding
 * @param res1 reserved / padding
 * @param eth_skb sk buffer including ethernet frame
 * @param dev device of the destination
 * @return sk buffer including canfd frame
 * @ingroup trans
 * @todo not tested yet
 */
extern struct sk_buff *ce_gw_eth_to_canfd(canid_t id, __u8 flags, __u8 res0, 
  __u8 res1, struct sk_buff *eth_skb,
  struct net_device *net);

/**
 * @fn void ce_gw_can_rcv(struct sk_buff *can_skb, void *data)
 * @brief The gateway function for incoming CAN frames
 *        Receive CAN frame --> process --> send to ETH dev
 *        (skbuffer, struct receiver->data)
 * @param can_skb CAN sk buffer which should be translated to an ETH packet
 * @param data gwjob which is responsible for triggering this function
 * @ingroup proc
 * @todo check also for canfd flag not only Type and call function. You must
 * also check then if the message then is really a canfd-frame or not.
 * @todo call other translation functions. check for canfd-frame or only use
 * canfd-frame casts.
 */
extern void ce_gw_can_rcv(struct sk_buff *can_skb, void *data);

/**
 * @fn static void ce_gw_eth_rcv(struct sk_buff *eth_skb, void *data)
 * @brief The gateway function for incoming ETH frames
 *        Receive skb from ETH dev --> process --> send to CAN bus
 * @param eth_skb ETH sk buffer with CAN frame as payload. Exact location of CAN
 *        frame depends on translation type (see enum ce_gw_type)
 * @param data gwjob which is responsible for triggering this function
 * @ingroup proc
 * @todo check for canfd-frame or only use canfd-frame casts.
 */
extern void ce_gw_eth_rcv(struct sk_buff *eth_skb, void *data);

/**
 * @fn static int ce_gw_create_route(void)
 * @brief ce_gw_create_route - adds new route from CAN <-> ETH
 * @ingroup alloc
 * @retval TODO
 * @todo: add some more params: @param ce_gw_type
 */
extern int ce_gw_create_route(int src_ifindex, int dst_ifindex,
                              enum ce_gw_type rt_type, u32 flags);

/**
 * @fn static int ce_gw_remove_route(int id)
 * @brief ce_gw_remove_route - unregisters and removes CAN <-> ETH route by id
 *                             if id = 0: all routes will be removed
 * @ingroup alloc
 * @retval 0 on success
 */
extern int ce_gw_remove_route(u32 id);

#endif

/**@}*/
