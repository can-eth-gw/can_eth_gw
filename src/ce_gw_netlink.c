/**
 * @file ce_gw_netlink.c
 * @brief Control Area Network - Ethernet - Gateway - Netlink
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
#include <net/genetlink.h>
#include <net/netlink.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include "ce_gw_main.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
#include <uapi/linux/netlink.h>
#endif
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>

/**
 * Netlink Family Settings
 */
#define CE_GW_GE_FAMILY_NAME "CE_GW"
#define CE_GW_GE_FAMILY_VERSION 2
#define CE_GW_USER_HDR_SIZE 0 /**< user header size */
#define CE_GW_NO_FLAG 0

/**
 * @enum
 * @brief Data which can be send and received.
 * @details Set int values as Identifiers for userspace application.
 * @ingroup net
 */
enum {
	CE_GW_A_UNSPEC, /**< Only a Dummy to skip index 0. */
	CE_GW_A_DATA,	/**< NLA_STRING */
	CE_GW_A_SRC,	/**< NLA_STRING */
	CE_GW_A_DST,	/**< NLA_STRING */
	CE_GW_A_ID,	/**< NLA_U32 */
	CE_GW_A_FLAGS,	/**< NLA_U32 */
	CE_GW_A_TYPE,	/**< NLA_U8 */
	CE_GW_A_HNDL,	/**< NLA_U32 Handled Frames */
	CE_GW_A_DROP,	/**< NLA_U32 Dropped Frames */
	__CE_GW_A_MAX,	/**< Maximum Number of Attribute + 1 */
};
#define CE_GW_A_MAX (__CE_GW_A_MAX - 1) /**< Maximum Number of Attribute */

/**
 * @brief Netlink Policy - Defines the Type for the Netlink Attributes
 * @ingroup net
 */
static struct nla_policy ce_gw_genl_policy[CE_GW_A_MAX + 1] = {
	[CE_GW_A_DATA] = { .type = NLA_NUL_STRING },
	[CE_GW_A_SRC] = { .type = NLA_NUL_STRING },
	[CE_GW_A_DST] = { .type = NLA_NUL_STRING },
	[CE_GW_A_ID] = { .type = NLA_U32 },
	[CE_GW_A_FLAGS] = { .type = NLA_U32 },
	[CE_GW_A_TYPE] = { .type = NLA_U8 },
	[CE_GW_A_HNDL] = { .type = NLA_U32 },
	[CE_GW_A_DROP] = { .type = NLA_U32 },
};

/**
 * @brief Generic Netlink Family
 * @ingroup net
 */
static struct genl_family ce_gw_genl_family = {
	/*@{*/
	.id = GENL_ID_GENERATE,		/**< generated unique Identifier */
	.hdrsize = CE_GW_USER_HDR_SIZE,	/**< Size of user header */
	.name = CE_GW_GE_FAMILY_NAME,	/**< unique human readable name */
	.version = CE_GW_GE_FAMILY_VERSION,	/**< Version */
	.maxattr = CE_GW_A_MAX,		/**< Maximum Attribute Number */
	.netnsok = false,
	.pre_doit = NULL,
	.post_doit = NULL,
	/*@}*/
};

/**
 * @enum
 * @brief Generic Netlink Commands
 * @details Set int values as Identifiers for userspace application.
 */
enum {
	CE_GW_C_UNSPEC,/**< Only a Dummy to skip index 0. */
	CE_GW_C_ECHO, /**< Sends a message back. Calls ce_gw_nl_echo(). */
	CE_GW_C_ADD,  /**< Add a gateway. Calls ce_gw_netlink_add(). */
	CE_GW_C_DEL,  /**< Delate a gateway. Calls ce_gw_netlink_del(). */
	CE_GW_C_LIST,  /**< list active gateways. Calls ce_gw_netlink_list(). */
	__CE_GW_C_MAX,/**< Maximum Number of Commands plus 1 */
};
#define CE_GW_C_MAX (__CE_GW_C_MAX - 1) /**< Maximum Number of Commands */

/**
 * @fn int ce_gw_netlink_echo(struct sk_buff *skb_info, struct genl_info *info)
 * @brief Generic Netlink Command - Sends a massage back
 * @param skb_info Netlink Socket Buffer with Message
 * @param info Additional Netlink Information
 * @retval 0 if parsing is finished
 * @ingroup net
 */
int ce_gw_netlink_echo(struct sk_buff *skb_info, struct genl_info *info)
{
	struct sk_buff *skb;
	int err;
	void *user_hdr;

	if (info == NULL) {
		printk(KERN_ERR "ce_gw: info attribute is missing."\
		       " No Massage received.\n");
		return -1;
	}

	struct nlattr *nla_a_msg = info->attrs[CE_GW_A_DATA];
	char *nla_a_msg_pay = (char *) nla_data(nla_a_msg);

	if (nla_a_msg_pay == NULL) {
		pr_warning("ce_gw: String Message is missing.\n");
	} else {
		pr_info("ce_gw: Messege received: %s\n", nla_a_msg_pay);
	}

	/* send a message back*/
	skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (skb == NULL) {
		err = -ENOMEM;
		pr_err("ce_gw: Socket allocation failed.\n");
		goto ce_gw_add_error;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
	user_hdr = genlmsg_put(skb, info->snd_pid, info->snd_seq,
	                       &ce_gw_genl_family, CE_GW_NO_FLAG, CE_GW_C_ECHO);
#else
	user_hdr = genlmsg_put(skb, info->snd_portid, info->snd_seq,
	                       &ce_gw_genl_family, CE_GW_NO_FLAG, CE_GW_C_ECHO);
#endif

	if (user_hdr == NULL) {
		err = -ENOMEM;
		pr_err("ce_gw: Error during putting haeder\n");
		goto ce_gw_add_error;
	}

	err = nla_put_string(skb, CE_GW_A_DATA,
	                     "hello world from kernel space   \n");
	if (err != 0) {
		pr_err("ce_gw: Putting Netlink Attribute Failed.\n");
		goto ce_gw_add_error;
	}

	genlmsg_end(skb, user_hdr);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
	err = genlmsg_unicast(genl_info_net(info),skb,info->snd_pid);
#else
	err = genlmsg_unicast(genl_info_net(info),skb,info->snd_portid);
#endif
	if (err != 0) {
		pr_err("ce_gw: Message sending failed.\n");
		goto ce_gw_add_error;
	}

	return 0;

ce_gw_add_error:
	kfree_skb(skb);
	return err;
};

/**
 * @fn int ce_gw_netlink_add(struct sk_buff *skb_info, struct genl_info *info)
 * @brief add a virtual ethernet device or a route
 * @param skb_info Netlink Socket Buffer with Message
 * @param info Additional Netlink Information
 * @deatils will be called by ce_gw_add() in userspace in cegwctl.
 * @details Has multiple netlink Attributes.
 * + #CE_GW_A_SRC: The textual name of the device wich will be the src. Must be
 *                 NULL if you want do add a device.
 * + #CE_GW_A_DST: The textual name of the device wich will be the dst OR the
 *                 name of the device, if you want to add a device.
 * + #CE_GW_A_TYPE: The Type of the route. For adding dev some settings
 *                  according to the type will be set.
 * + #CE_GW_A_FLAGS: The Flags of the route. For adding dev some settings
 *                  according to the type will be set. See netlink.h for the
 *                  falgs.
 * @ingroup net
 * @retval 0 on success
 * @retval <0 on failure
 */
int ce_gw_netlink_add(struct sk_buff *skb_info, struct genl_info *info)
{
	int err = 0;
	if (info == NULL) {
		err = -EINVAL;
		pr_err("ce_gw: info attribute is missing."\
		       " No Massage received.\n");
		return err;
	}

	struct nlattr *nla_src = info->attrs[CE_GW_A_SRC];
	char *nla_src_data = (char *) nla_data(nla_src);

	struct nlattr *nla_dst = info->attrs[CE_GW_A_DST];
	char *nla_dst_data = (char *) nla_data(nla_dst);

	struct nlattr *nla_flags = info->attrs[CE_GW_A_FLAGS];
	u32 *nla_flags_data = (u32 *) nla_data(nla_flags);

	struct nlattr *nla_type = info->attrs[CE_GW_A_TYPE];
	u8 *nla_type_data = (u8 *) nla_data(nla_type);

	if (nla_src == NULL) { /* add dev is called in userspace */
		pr_info("ce_gw: add dev is called\n");
		struct net_device *dev;

		if (nla_dst == NULL ||
		    nla_type == NULL || nla_flags == NULL) {
			pr_err("ce_gw: DST or TYPE is missing.\n");
			err = -ENODATA;
			goto ce_gw_add_error;
		}

		dev = ce_gw_dev_create(*nla_type_data, *nla_flags_data,
		    nla_dst_data);
		if (dev == NULL) {
			pr_err("ce_gw_netlink: Device allocation failed.");
			err = -ENOMEM;
			goto ce_gw_add_error;
		}

		err = ce_gw_dev_register(dev);
		if (err != 0) {
			pr_err("ce_gw_netlink: Device registration failed.");
			ce_gw_dev_free(dev);
			goto ce_gw_add_error;
		}

	} else { /* add route is called in userspace*/
		if (nla_src == NULL || nla_dst == NULL ||
		    nla_type == NULL || nla_flags == NULL) {
			pr_err("ce_gw: SRC or DST or TYPE is missing.\n");
			err = -ENODATA;
			goto ce_gw_add_error;
		}
		
			pr_info("ce_gw: Add : from %s to %s"
			        "(Type %d; Flags %d)\n", nla_src_data,
			        nla_dst_data, *nla_type_data, *nla_flags_data);

		/* get device index by their names */
		struct net_device *src_dev;
		src_dev = dev_get_by_name(&init_net, nla_src_data);
		if (src_dev == NULL) {
			err = -ENODEV;
			pr_err("ce_gw_netlink: src dev not found: %d\n", err);
			goto ce_gw_add_error;
		}
		int src_dev_ifindex = src_dev->ifindex;
		dev_put(src_dev);

		struct net_device *dst_dev;
		dst_dev = dev_get_by_name(&init_net, nla_dst_data);
		if (dst_dev == NULL) {
			err = -ENODEV;
			pr_err("ce_gw_netlink: dst dev not found: %d\n", err);
			goto ce_gw_add_error;
		}
		int dst_dev_ifindex = dst_dev->ifindex;
		dev_put(dst_dev);

		err = ce_gw_create_route(src_dev_ifindex, dst_dev_ifindex,
		                         *nla_type_data, *nla_flags_data);
		if (err != 0) {
			goto ce_gw_add_error;
		}
	}

ce_gw_add_error:
	/* TODO if you send two add messages A abd B in in very short
	 * short distance (e.g. with option -b in userspace) than the
	 * ACK Sequence number of Message B is the same of the Massage
	 * A. So the ACK for Message B could not correct assign to
	 * Message B in userspace and the userpace think there is an
	 * error, but all is correct. This seems to be a bug in this
	 * function of netlink */
	netlink_ack(skb_info, info->nlhdr, -err);
	return err;
}

/**
 * @fn int ce_gw_netlink_del(struct sk_buff *skb_info, struct genl_info *info)
 * @brief Deletes a device by name or a route by ID
 * @param skb_info Netlink Socket Buffer with Message
 * @param info Additional Netlink Information
 * @pre #CE_GW_A_ID must be == 0 OR #CE_GW_A_DST must be == NULL
 * @details will be called by ce_gw_add() in userspace in cegwctl.
 * @details Has multiple netlink Attributes:
 * + #CE_GW_A_ID The id of the route you want to delete. CE_GW_A_DST must be
 *        NULL when you want to delete a route.
 * + #CE_GW_A_DST The textual name of the virtual device you want to delete.
 *        The device must be previously added by ce_gw_add(). CE_GW_A_ID must be
 *        0 if you want to delete a device.
 * @ingroup net
 * @retval 0 on success
 * @retval <0 on failure
 */
int ce_gw_netlink_del(struct sk_buff *skb_info, struct genl_info *info)
{
	int err = 0;
	if (info == NULL) {
		err = -ENODATA;
		pr_err("ce_gw: info attribute is missing."\
		       " No Massage received: %d\n", err);
		return err;
	}

	struct nlattr *nla_id = info->attrs[CE_GW_A_ID];
	__u32 *nla_id_data = (__u32 *) nla_data(nla_id);

	/* DST Contains the device name to delete */
	struct nlattr *nla_dst = info->attrs[CE_GW_A_DST];
	char *nla_dst_data = (char *) nla_data(nla_dst);

	if (nla_dst == NULL) { /* del route is called in userspace */
		if (*nla_id_data == 0) {
			err = -ENODATA;
			pr_warning("ce_gw: ID is missing: %d\n", err);
			goto ce_gw_del_error;
		} else
			pr_debug("ce_gw: del device: %d\n", *nla_id_data);

		err = ce_gw_remove_route(*nla_id_data);

	} else { /* del dev is called in userspace */

		struct net_device *dev;
		dev = dev_get_by_name(&init_net, nla_dst_data);
		if (dev == NULL) {
			err = -ENODEV;
			pr_err("ce_gw_netlink: No such Device: %d\n", err);
			goto ce_gw_del_error;
		}
		dev_put(dev);

		ce_gw_dev_unregister(dev);
		ce_gw_dev_free(dev);
	}

ce_gw_del_error:
	netlink_ack(skb_info, info->nlhdr, -err);
	return err;
}

/**
 * @fn int ce_gw_netlink_list(struct sk_buff *skb_info, struct genl_info *info)
 * @brief Send informations of one or more routes to userspace.
 * @param skb_info Netlink Socket Buffer with Message
 * @param info Additional Netlink Information
 * @details will be called by ce_gw_list() in userspace in cegwctl.
 * @details get Netlink Attribute:
 * + #CE_GW_A_ID set it to 0 if you want to send all routes.
 * Else set it to the route id you want to send.
 * @details Send multiple netlink Attributes back:
 * + #CE_GW_A_SRC
 * + #CE_GW_A_DST
 * + #CE_GW_A_ID
 * + #CE_GW_A_FLAGS
 * + #CE_GW_A_TYPE
 * + #CE_GW_A_HNDL
 * + #CE_GW_A_DROP
 * @ingroup net
 * @retval 0 on success
 * @retval <0 on failure
 */
int ce_gw_netlink_list(struct sk_buff *skb_info, struct genl_info *info)
{
	struct sk_buff *skb;
	int err = 0;
	void *user_hdr;

	pr_debug("ce_gw_netlink: ce_gw_netlink_list is called.\n");

	struct nlattr *nla_id = info->attrs[CE_GW_A_ID];
	__u32 *nla_id_data = (__u32 *) nla_data(nla_id);

	/* TODO perhaps pare arguments like ID to only transmit a special GW */
	struct ce_gw_job *cgj;
	struct hlist_node *node;

#	if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	hlist_for_each_entry_safe(cgj, node, ce_gw_get_job_list(), list) {
#	else

	struct hlist_node *pos;
	hlist_for_each_entry_safe(cgj, pos, node, ce_gw_get_job_list(), list) {
#	endif
		if (*nla_id_data != 0 && cgj->id != *nla_id_data) {
			continue;
		}

		pr_debug("ce_gw_netlink: Job List entry is send.\n");

		skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
		if (skb == NULL) {
			pr_err("ce_gw: Socket allocation failed.\n");
			goto ce_gw_list_error;
		}

#		if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
		user_hdr = genlmsg_put(skb, info->snd_pid,
		                       info->snd_seq, &ce_gw_genl_family,
		                       NLM_F_MULTI, CE_GW_C_ECHO);

#		else
		user_hdr = genlmsg_put(skb, info->snd_portid,
		                       info->snd_seq, &ce_gw_genl_family,
		                       NLM_F_MULTI, CE_GW_C_ECHO);
#		endif
		if (user_hdr == NULL) {
			err = -ENOMEM;
			pr_err("ce_gw: Error during putting haeder\n");
			goto ce_gw_list_error;
		}

		err = nla_put_string(skb, CE_GW_A_SRC, cgj->src.dev->name);
		err += nla_put_string(skb, CE_GW_A_DST, cgj->dst.dev->name);
		err += nla_put_u32(skb, CE_GW_A_ID, cgj->id);
		err += nla_put_u32(skb, CE_GW_A_FLAGS, cgj->flags);
		err += nla_put_u8(skb, CE_GW_A_TYPE, cgj->type);
		err += nla_put_u32(skb, CE_GW_A_HNDL, cgj->handled_frames);
		err += nla_put_u32(skb, CE_GW_A_DROP, cgj->dropped_frames);
		if (err != 0) {
			pr_err("ce_gw: Putting Netlink Attribute Failed.\n");
			goto ce_gw_list_error;
		}

		genlmsg_end(skb, user_hdr);

#		if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
		err = genlmsg_unicast(genl_info_net(info), skb,
		                      info->snd_pid);
#		else

		err = genlmsg_unicast(genl_info_net(info), skb,
		                      info->snd_portid);
#		endif
		if (err != 0) {
			pr_err("ce_gw: Message sending failed.\n");
			goto ce_gw_list_error;
		}
	}

	/* send a DONE Message (of a Multimessage Series) back */
	struct nlmsghdr *nlhdr;
	skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);

	if (skb == NULL) {
		pr_err("ce_gw: Socket allocation failed.\n");
		goto ce_gw_list_error;
	}

#	if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
	nlhdr = nlmsg_put(skb, info->snd_pid, info->snd_seq,
	                  NLMSG_DONE, 0, CE_GW_NO_FLAG);
#	else

	nlhdr = nlmsg_put(skb, info->snd_portid, info->snd_seq,
	                  NLMSG_DONE, 0, CE_GW_NO_FLAG);
#	endif
	if (nlhdr == NULL) {
		err = -ENOMEM;
		pr_err("ce_gw: Error during putting haeder\n");
		goto ce_gw_list_error;
	}

	nlmsg_end(skb, nlhdr);

#	if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
	err = nlmsg_unicast(genl_info_net(info)->genl_sock, skb,
	                    info->snd_pid);

#	else
	err = nlmsg_unicast(genl_info_net(info)->genl_sock, skb,
	                    info->snd_portid);
#	endif
	if (err != 0) {
		pr_err("ce_gw: Message sending failed.\n");
		goto ce_gw_list_error;
	}

	return 0;

ce_gw_list_error:
	kfree_skb(skb);
	return err;
}

/**
 * @brief details of ce_gw_netlink_echo()
 * @ingroup net
 */
struct genl_ops ce_gw_genl_ops_echo = {
	.cmd = CE_GW_C_ECHO,
	.internal_flags = CE_GW_NO_FLAG,
	.flags = CE_GW_NO_FLAG,
	.policy = ce_gw_genl_policy,
	.doit = ce_gw_netlink_echo,
	.dumpit = NULL,
	.done = NULL,
};

/**
 * @brief details of ce_gw_netlink_add()
 * @ingroup net
 */
struct genl_ops ce_gw_genl_ops_add = {
	.cmd = CE_GW_C_ADD,
	.internal_flags = CE_GW_NO_FLAG,
	.flags = CE_GW_NO_FLAG,
	.policy = ce_gw_genl_policy,
	.doit = ce_gw_netlink_add,
	.dumpit = NULL,
	.done = NULL,
};

/**
 * @brief details of ce_gw_netlink_del()
 * @ingroup net
 */
struct genl_ops ce_gw_genl_ops_del = {
	.cmd = CE_GW_C_DEL,
	.internal_flags = CE_GW_NO_FLAG,
	.flags = CE_GW_NO_FLAG,
	.policy = ce_gw_genl_policy,
	.doit = ce_gw_netlink_del,
	.dumpit = NULL,
	.done = NULL,
};

/**
 * @brief details of ce_gw_netlink_list()
 * @ingroup net
 */
struct genl_ops ce_gw_genl_ops_list = {
	.cmd = CE_GW_C_LIST,
	.internal_flags = CE_GW_NO_FLAG,
	.flags = CE_GW_NO_FLAG,
	.policy = ce_gw_genl_policy,
	.doit = ce_gw_netlink_list,
	.dumpit = NULL,
	.done = NULL,
};


int ce_gw_netlink_init(void) {
	int err;

	if ((err = genl_register_family(&ce_gw_genl_family)) != 0) {
		pr_err("ce_gw: Error during registering family ce_gw: %i\n",
		       err);
		goto ce_gw_init_family_err;
	}

	err = genl_register_ops(&ce_gw_genl_family, &ce_gw_genl_ops_echo);
	if (err != 0) {
		pr_err("ce_gw: Error during registering operation echo: %i\n",
		       err);
		goto ce_gw_init_echo_err;
	}

	err = genl_register_ops(&ce_gw_genl_family, &ce_gw_genl_ops_del);
	if (err != 0) {
		pr_err("ce_gw: Error during registering operation del: %i\n",
		       err);
		goto ce_gw_init_del_err;
	}

	err = genl_register_ops(&ce_gw_genl_family, &ce_gw_genl_ops_add);
	if (err != 0) {
		pr_err("ce_gw: Error during registering operation add: %i\n",
		       err);
		goto ce_gw_init_add_err;
	}

	err = genl_register_ops(&ce_gw_genl_family, &ce_gw_genl_ops_list);
	if (err != 0) {
		pr_err("ce_gw: Error during registering operation list: %i\n",
		       err);
		goto ce_gw_init_list_err;
	}

	return 0;


	err = genl_unregister_ops(&ce_gw_genl_family, &ce_gw_genl_ops_list);

ce_gw_init_list_err:
	err = genl_unregister_ops(&ce_gw_genl_family, &ce_gw_genl_ops_add);

ce_gw_init_add_err:
	err = genl_unregister_ops(&ce_gw_genl_family, &ce_gw_genl_ops_del);

ce_gw_init_del_err:
	err = genl_unregister_ops(&ce_gw_genl_family, &ce_gw_genl_ops_echo);

ce_gw_init_echo_err:
	err = genl_unregister_family(&ce_gw_genl_family);

ce_gw_init_family_err:
	return -1;
}


void ce_gw_netlink_exit(void) {
	int err;
	err = genl_unregister_ops(&ce_gw_genl_family, &ce_gw_genl_ops_echo);
	if (err != 0) {
		pr_err("ce_gw: Error during unregistering operation echo: %i\n",
		       err);
	}

	err = genl_unregister_ops(&ce_gw_genl_family, &ce_gw_genl_ops_add);
	if (err != 0) {
		pr_err("ce_gw: Error during unregistering operation add: %i\n",
		       err);
	}

	err = genl_unregister_ops(&ce_gw_genl_family, &ce_gw_genl_ops_del);
	if (err != 0) {
		pr_err("ce_gw: Error during unregistering operation del: %i\n",
		       err);
	}

	err = genl_unregister_ops(&ce_gw_genl_family, &ce_gw_genl_ops_list);
	if (err != 0) {
		pr_err("ce_gw: Error during unregistering operation del: %i\n",
		       err);
	}

	err = genl_unregister_family(&ce_gw_genl_family);
	if (err != 0) {
		pr_err("ce_gw: Error during unregistering family ce_gw: %i\n",
		       err);
	}
}


/**@}*/
