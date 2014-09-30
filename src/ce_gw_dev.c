/**
 * @file ce_gw_dev.c
 * @brief Control Area Network - Ethernet - Gateway - Device
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
# include <uapi/linux/can.h>
# else
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
#   include <linux/can.h>
#  else
#   error Only Linux Kernel 3.6 and above are supported
#  endif
#endif
#include "ce_gw_dev.h"
#include "ce_gw_main.h"

#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>

HLIST_HEAD(ce_gw_dev_allocated); /**< list of all allocated ethernet devices */
HLIST_HEAD(ce_gw_dev_registered);/**< list of all registered ethernet devices */
static struct kmem_cache *ce_gw_dev_cache __read_mostly; /**< cache for lists */

/**
 * @struct ce_gw_dev_list
 * @brief internal list of all registered and allocated devices.
 * @details This list is used on module exit for unregistering and freeing all
 * devices.
 */
struct ce_gw_dev_list {
	struct hlist_node list_alloc;
	struct hlist_node list_reg;
	struct rcu_head rcu;
	struct net_device *dev;
};

/**
 * @fn int ce_gw_dev_open(struct net_device *dev)
 * @brief called by the OS on device up
 * @param dev correspondening eth device
 * @retval 0 on success
 * @retval <0 on failure
 * @ingroup dev
 */
int ce_gw_dev_open(struct net_device *dev)
{
	printk("ce_gw: ce_gw_open called\n");

	if (!netif_device_present(dev)) {
		pr_err("ce_gw_dev_open: Device not registered");
		return -1;
	}

	netif_start_queue(dev);
	return 0;
}

/**
 * @fn int ce_gw_dev_stop(struct net_device *dev)
 * @brief called by the OS on device down
 * @param dev correspondening eth device
 * @retval 0 on success
 * @retval <0 on failure
 * @ingroup dev
 */
int ce_gw_dev_stop(struct net_device *dev)
{
	printk ("ce_gw_dev: ce_gw_release called\n");
	netif_stop_queue(dev);
	return 0;
}

/**
 * @fn static int ce_gw_dev_start_xmit(struct sk_buff *skb,
 *                              struct net_device *dev)
 * @brief called by the OS if a package is sent to the device
 * @param dev correspondening eth device
 * @param skb sk buffer from OS
 * @retval 0 on success
 * @retval <0 on failure
 * @ingroup dev
 */
static int ce_gw_dev_start_xmit(struct sk_buff *skb,
                                struct net_device *dev)
{
	printk ("ce_gw_dev: dummy xmit function called....\n");
	/* TODO: Get right gw_job and push it to eth_skb (instead of NULL) */
	struct ce_gw_job_info *priv = netdev_priv(dev);

	struct ce_gw_job *job = NULL;
	struct hlist_node *node;

#	if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	hlist_for_each_entry_safe(job, node, &priv->job_src, list_dev) {

#	else
	struct hlist_node *pos;
	hlist_for_each_entry_safe(job, pos, node, &priv->job_src,
	                          list_dev) {
#	endif
		ce_gw_eth_rcv(skb, job);
	}

	dev_kfree_skb(skb);

	/*here is my test for ce_gw_eth_to_canfd*/
	/*	struct sk_buff *can_skb;*/
	/*	__u32 id = 0xF65C034B;*/
	/*	__u8 flags = 0x04;*/
	/*	__u8 res0 = 0xF3;*/
	/*	__u8 res1 = 0x00;*/
	/*	can_skb = ce_gw_eth_to_canfd(id, flags, res0, res1, skb, dev);*/
	return 0;
}

/**
 * @fn static int ce_gw_dev_init(struct net_device *dev)
 * @brief called by the OS on device registered
 * @param dev correspondening eth device
 * @retval 0 on success
 * @retval <0 on failure
 * @ingroup dev
 */
int ce_gw_dev_init(struct net_device *dev) {
	printk ("ce_gw_dev: device init called\n");
	return 0;
}

/**
 * @brief Defined Functions of Ethernet device
 */
static struct net_device_ops ce_gw_ops = {
	.ndo_init 	= ce_gw_dev_init,
	.ndo_open 	= ce_gw_dev_open,
	.ndo_stop	= ce_gw_dev_stop,
	.ndo_start_xmit	= ce_gw_dev_start_xmit,
	0
};

int ce_gw_is_allocated_dev(struct net_device *eth_dev) {

	struct ce_gw_dev_list *dl = NULL;
	struct hlist_node *node;

#	if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	hlist_for_each_entry_safe(dl, node, &ce_gw_dev_allocated, list_alloc) {

#	else
	struct hlist_node *pos;
	hlist_for_each_entry_safe(dl, pos, node, &ce_gw_dev_allocated,
	                          list_alloc) {
#	endif
		if (dl->dev == eth_dev)
			return 0;
	}

	return -ENODEV;
}

int ce_gw_is_registered_dev(struct net_device *eth_dev) {

	struct ce_gw_dev_list *dl = NULL;
	struct hlist_node *node;

#	if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	hlist_for_each_entry_safe(dl, node, &ce_gw_dev_registered, list_reg) {

#	else
	struct hlist_node *pos;
	hlist_for_each_entry_safe(dl, pos, node, &ce_gw_dev_registered,
	                          list_reg) {
#	endif
		if (dl->dev == eth_dev)
			return 0;
	}

	pr_debug("ce_gw_is_registered_dev: Device not registered\n");
	return -ENODEV;
}

int ce_gw_has_min_mtu(struct net_device *dev, enum ce_gw_type type, u32 flags) {

	int mtu = ETH_DATA_LEN; /* Standart Ethernet Value */

	switch (type) {
	case CE_GW_TYPE_NONE:
		/* Do Nothing (default Ethernet MTU will be set) */
		break;
	case CE_GW_TYPE_ETH:
		if ((flags & CE_GW_F_CAN_FD) == CE_GW_F_CAN_FD) {
			mtu = CANFD_MAX_DLEN;
		} else {
			mtu = CAN_MAX_DLEN;
		}
		break;
	case CE_GW_TYPE_NET:
		if ((flags & CE_GW_F_CAN_FD) == CE_GW_F_CAN_FD) {
			mtu = sizeof(struct canfd_frame);
		} else {
			mtu = sizeof(struct can_frame);
		}
		break;
	case CE_GW_TYPE_TCP:
		/* TODO nothing yet */
		break;
	case CE_GW_TYPE_UDP:
		/* TODO nothing yet */
		break;
	default:
		pr_err("ce_gw_dev: Type not defined.");
	}

	if (dev->mtu >= mtu) {
		return true;
	}

	return false;
}

void ce_gw_dev_job_src_add(struct ce_gw_job *job) {
	struct ce_gw_job_info *priv = netdev_priv(job->src.dev);
	hlist_add_head_rcu(&job->list_dev, &priv->job_src);
}

void ce_gw_dev_job_dst_add(struct ce_gw_job *job) {
	struct ce_gw_job_info *priv = netdev_priv(job->dst.dev);
	hlist_add_head_rcu(&job->list_dev, &priv->job_dst);
}


int ce_gw_dev_job_add(struct net_device *eth_dev, struct ce_gw_job *job) {
	if (job->src.dev == eth_dev) {
		ce_gw_dev_job_src_add(job);
	} else if (job->dst.dev == eth_dev) {
		ce_gw_dev_job_dst_add(job);
	} else {
		pr_err("ce_gw_dev_job_add: Invalid Arguments");
		return -1;
	}
	return 0;
}

void ce_gw_dev_job_remove(struct ce_gw_job *job) {
	hlist_del_rcu(&job->list_dev);
}

struct net_device *ce_gw_dev_alloc(char *dev_name) {
	pr_debug("ce_gw_dev: Alloc Device\n");
	struct net_device *dev;

	dev = alloc_netdev(sizeof(struct ce_gw_job_info),
	                   dev_name, ether_setup);
	if (dev == NULL) {
		pr_err("ce_gw_dev: Error allocation etherdev.");
		goto ce_gw_dev_create_error;
	}

	/* initialize private field */
	struct ce_gw_job_info *priv = netdev_priv(dev);
	memset(priv, 0, sizeof(struct ce_gw_job_info));
	priv->job_src.first = NULL;
	priv->job_dst.first = NULL;

	/* create list entry and add */
	struct ce_gw_dev_list *dl;
	dl = kmem_cache_alloc(ce_gw_dev_cache, GFP_KERNEL);
	if (dl == NULL) {
		pr_err("ce_gw_dev: cache alloc failed");
		goto ce_gw_dev_create_error_cache;
	}

	dl->dev = dev;

	hlist_add_head_rcu(&dl->list_alloc, &ce_gw_dev_allocated);

	return dev;

ce_gw_dev_create_error_cache:
	kmem_cache_free(ce_gw_dev_cache, dl);

ce_gw_dev_create_error:
	free_netdev(dev);
	return NULL;
}

void ce_gw_dev_free(struct net_device *eth_dev) {
	pr_debug("ce_gw_dev: Free Device %s\n", eth_dev->name);

	struct ce_gw_dev_list *dl = NULL;
	struct hlist_node *node;

	/* search for the List Element of eth_dev */
#	if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	hlist_for_each_entry_safe(dl, node, &ce_gw_dev_allocated, list_alloc) {

#	else
	struct hlist_node *pos;
	hlist_for_each_entry_safe(dl, pos, node, &ce_gw_dev_allocated,
	                          list_alloc) {
#	endif
		if (dl->dev == eth_dev)
			break;
	}

	if (dl == NULL || dl->dev != eth_dev) {
		pr_err("ce_gw_dev: Device not found in list\n");
	} else {
		hlist_del_rcu(&dl->list_alloc);
	}

	free_netdev(eth_dev);
	kmem_cache_free(ce_gw_dev_cache, dl);
}

void ce_gw_dev_setup(struct net_device *dev, enum ce_gw_type type,
                     __u32 flags) {
	dev->netdev_ops = &ce_gw_ops;

	/* Set sensible MTU */
	switch (type) {
	case CE_GW_TYPE_NONE:
		/* Do Nothing (default Ethernet MTU will be set) */
		break;
	case CE_GW_TYPE_ETH:
		if ((flags & CE_GW_F_CAN_FD) == CE_GW_F_CAN_FD) {
			dev->mtu = CANFD_MAX_DLEN;
		} else {
			dev->mtu = CAN_MAX_DLEN;
		}
		break;
	case CE_GW_TYPE_NET:
		if ((flags & CE_GW_F_CAN_FD) == CE_GW_F_CAN_FD) {
			dev->mtu = sizeof(struct canfd_frame);
		} else {
			dev->mtu = sizeof(struct can_frame);
		}
		break;
	case CE_GW_TYPE_TCP:
		/* TODO nothing yet */
		break;
	case CE_GW_TYPE_UDP:
		/* TODO nothing yet */
		break;
	default:
		pr_err("ce_gw_dev: Type not defined.");
	}
}

struct net_device *ce_gw_dev_create(enum ce_gw_type type, __u32 flags,
     char *dev_name) {
	struct net_device *dev;
	
	dev = ce_gw_dev_alloc(dev_name);

	ce_gw_dev_setup(dev, type, flags);

	return dev;
}

int ce_gw_dev_register(struct net_device *eth_dev) {
	pr_debug("ce_gw_dev: Register Device\n");
	int err = 0;
	err = register_netdev(eth_dev);

	struct ce_gw_dev_list *dl = NULL;
	struct hlist_node *node;

	/* search for the List Element of eth_dev */
#	if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	hlist_for_each_entry_safe(dl, node, &ce_gw_dev_allocated, list_alloc) {

#	else
	struct hlist_node *pos;
	hlist_for_each_entry_safe(dl, pos, node, &ce_gw_dev_allocated,
	                          list_alloc) {
#	endif
		if (dl->dev == eth_dev)
			break;
	}

	if (dl == NULL || dl->dev != eth_dev) {
		pr_err("ce_gw_dev: Device not found in list\n");
		goto ce_gw_dev_register_error;
	}

	hlist_add_head_rcu(&dl->list_reg, &ce_gw_dev_registered);
	if (&dl->list_reg == NULL) {
		pr_err("ce_gw_dev: Device not add to list correct\n");
	}

	return err;

ce_gw_dev_register_error:
	unregister_netdev(eth_dev);
	return err;
}

void ce_gw_dev_unregister(struct net_device *eth_dev) {
	pr_debug("ce_gw_dev: Unregister Device %s\n", eth_dev->name);
	int err = 0;

	struct ce_gw_job_info *priv = netdev_priv(eth_dev);
	pr_debug("ce_gw_dev: Deleting all Routes of %s\n", eth_dev->name);

	/* Delete all routes witch are linked to the soon unregistered device
	 * where the device is the source */
	struct ce_gw_job *job = NULL;
	struct hlist_node *node;
#	if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	hlist_for_each_entry_safe(job, node, &priv->job_src, list_dev) {

#	else
	struct hlist_node *pos;
	hlist_for_each_entry_safe(job, pos, node, &priv->job_src, list_dev) {
#	endif

		err = ce_gw_remove_route(job->id);
		if (err != 0) {
			pr_err("ce_gw_dev: route with id %u "
			       "deleting failed: %d", job->id, err);
		}
	}

	/* Delete all routes witch are linked to the soon unregistered device
	 * where the device is the dest */
	job = NULL;
	node = NULL;
#	if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	hlist_for_each_entry_safe(job, node, &priv->job_dst, list_dev) {

#	else
	pos = NULL;
	hlist_for_each_entry_safe(job, pos, node, &priv->job_dst, list_dev) {
#	endif

		err = ce_gw_remove_route(job->id);
		{
			pr_err("ce_gw_dev: route with id %u "
			       "deleting failed: %d", job->id, err);
		}
	}

	/* unregister */
	pr_debug("ce_gw_dev: Call unregister_netdev() of %s\n", eth_dev->name);
	unregister_netdev(eth_dev);

	struct ce_gw_dev_list *dl = NULL;
	node = NULL;

	/* search for the List Element of eth_dev */
#	if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	hlist_for_each_entry_safe(dl, node, &ce_gw_dev_registered, list_reg) {

#	else
	pos = NULL;
	hlist_for_each_entry_safe(dl, pos, node, &ce_gw_dev_registered,
	                          list_reg) {
#	endif
		if (dl->dev == eth_dev)
			break;
	}

	if (dl == NULL || &dl->list_reg == NULL) {
		pr_err("ce_gw_dev: Device not correct in internal list\n");
	} else {
		hlist_del_rcu(&dl->list_reg);
	}
}


int ce_gw_dev_init_module(void) {
	ce_gw_dev_cache = kmem_cache_create("can_eth_gw_dev",
	                                    sizeof(struct ce_gw_dev_list),
	                                    0, 0, NULL);
	if (!ce_gw_dev_cache)
		return -ENOMEM;

	return 0;
}

void ce_gw_dev_cleanup(void) {
	struct ce_gw_dev_list *dl = NULL;
	struct hlist_node *node;

	/* iterate over list and unregister */
#	if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	hlist_for_each_entry_safe(dl, node, &ce_gw_dev_registered, list_reg) {

#	else
	struct hlist_node *pos;
	hlist_for_each_entry_safe(dl, pos, node, &ce_gw_dev_registered,
	                          list_reg) {
#	endif
		ce_gw_dev_unregister(dl->dev);
	}

	dl = NULL;
	node = NULL;

	/* iterate over list and free */
#	if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	hlist_for_each_entry_safe(dl, node, &ce_gw_dev_allocated, list_alloc) {

#	else
	pos = NULL;
	hlist_for_each_entry_safe(dl, pos, node, &ce_gw_dev_allocated,
	                          list_alloc) {
#	endif
		ce_gw_dev_free(dl->dev);
	}

	kmem_cache_destroy(ce_gw_dev_cache);
}

/**@}*/
