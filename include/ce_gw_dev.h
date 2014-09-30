/**
 * @file ce_gw_dev.h
 * @brief Control Area Network - Ethernet - Gateway - Device Header
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

#ifndef __CE_GW_DEV_H__
#define __CE_GW_DEV_H__

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>

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
#include "ce_gw_main.h"

enum ce_gw_type;
struct ce_gw_job;

/**
 * @struct ce_gw_job_info
 * @brief The private Field of ether struct net_device with pointer to its job.
 * @details The private Field of ethernet net_device. struct hlist_head job_src;
 *          and struct hlist_head job_dst; are Lists with struct ce_gw_job as
 *          as its elements. They Point to to the struct ce_gw_job where the
 *          dev is part of it. So there exist a Pointer from the
 *          struct ce_gw_job to the net_device and from struct ce_gw_job_info
 *          back to the same struct ce_gw_job.
 */
struct ce_gw_job_info {
	struct hlist_head job_src; /**< List where the dev is the src in job */
	struct hlist_head job_dst; /**< List where the dev is the dst in job */
};

/**
 * @fn int ce_gw_is_allocated_dev(struct net_device *eth_dev)
 * @brief check if the param eth_dev is allocated by this module
 * @param eth_dev the device wich should be checked
 * @retval 0 param eth_dev was allocated by this module
 * @retval -ENODEV param eth_dev was NOT allocated by this module
 * @retval others error occured
 * @see ce_gw_is_registered_dev() should you normally use
 * @ingroup dev
 */
extern int ce_gw_is_allocated_dev(struct net_device *eth_dev);

/**
 * @fn int ce_gw_is_registered_dev(struct net_device *eth_dev)
 * @brief check if the param eth_dev is registered by this module
 * @param eth_dev the device wich should be checked
 * @retval 0 param eth_dev was registered by this module
 * @retval -ENODEV param eth_dev was NOT registered by this module
 * @retval others error occured
 * @ingroup dev
 */
extern int ce_gw_is_registered_dev(struct net_device *eth_dev);

/**
 * @fn int ce_gw_has_min_mtu(struct net_device *dev, enum ce_gw_type type,
 *        u32 flags)
 * @brief checks if the given device has enough mtu to use the type and flags
 * @param dev The net_device which schould be checked
 * @param type The type which sould be compared to
 * @param flags needed dor the #CE_GW_F_CAN_FD flag, to check if the type is
 * CANfd capable
 * @retval true The mtu of dev is bigger then the nedded mtu by the type
 * @retval false if not
 */
int ce_gw_has_min_mtu(struct net_device *dev, enum ce_gw_type type, u32 flags);

/**
 * @fn void ce_gw_dev_job_src_add(struct ce_gw_job *job)
 * @brief Adds an pointer to the net_device internal list where it is the src.
 * @details The Function will add a Pointer to the list in the net_device
 *          private field back to the param job. The pointer in union src.dev in
 *          the param job be set to the param eth_dev.
 * @pre union src.dev in param job must already point to the ethernet device.
 * @param job The struct ce_gw_job where union src will points to the param
 *            eth_dev. A Pointer to this param will be added.
 * @ingroup dev
 */
void ce_gw_dev_job_src_add(struct ce_gw_job *job);

/**
 * @fn void ce_gw_dev_job_dst_add(struct ce_gw_job *job)
 * @brief Adds an pointer to the net_device internal list where it is the dst.
 * @details The Function will add a Pointer to the list in the net_device
 *          private field back to the param job. The pointer in union dst.dev in
 *          the param job be set to the param eth_dev.
 * @pre union dst.dev in param job must already point to the ethernet device.
 * @param job The struct ce_gw_job where union dst will points to the param
 *            eth_dev. A Pointer to this param will be added.
 * @ingroup dev
 */
extern void ce_gw_dev_job_dst_add(struct ce_gw_job *job);

/**
 * @fn void ce_gw_dev_job_add(struct net_device *eth_dev,
 *       struct ce_gw_job *job)
 * @brief Adds an pointer to the net_device internal list where it is part of.
 * @details The Function will add a Pointer to the list in the net_device
 *          private field back to the param job. There must be a pointer
 *          in union src or union dst in the param job to the
 *          struct net_device *eth_dev.
 * @pre union src.dev or union dst.dev in param job must already point to
 *      the param eth_dev.
 * @param eth_dev The ethernet struct net_device witch is pointed from
 *                union src or union dst in param job.
 * @param job The struct ce_gw_job where union src or union dst points to the
 *            param eth_dev. A Pointer to this param will be added.
 * @retval 0 success
 * @retval <0 failure
 * @ingroup dev
 */
extern int ce_gw_dev_job_add(struct net_device *eth_dev, struct ce_gw_job *job);

/**
 * @fn void ce_gw_dev_job_remove(struct ce_gw_job *job)
 * @brief Removes the pointer to param job from the list in ethernet net_device.
 * @details The private field of the ethernet struct net_device contains a
 *          a pointer to the param job. The Function will remove this Pointer
 *          from the list.
 * @param job The struct ce_gw_job where union src or union dst points to the
 *            param eth_dev. A Pointer to this param will be removed.
 * @warning You must remove the pointer from union src and accordingly union dst
 *          in param job to the ethernet struct net_device yourself. Until you
 *          do this our pointer architecture is inconsistent.
 * @ingroup dev
 */

extern void ce_gw_dev_job_remove(struct ce_gw_job *job);

/**
 * @fn struct net_device *ce_gw_dev_alloc(void)
 * @brief Allocates a Ethernet Device for the Gateway.
 * @param dev_name the name of the new allocated device
 * @details Allocates a Ethernet Device with struct ce_gw_job_info as private
 *          data. ce_gw_dev_setup() should be called after this function.
 *          ce_gw_dev_free() musst be called for freeing the memory.
 * @retval NULL If an error has occured.
 * @return A pointer to the allocated device.
 * @ingroup dev
 */
extern struct net_device *ce_gw_dev_alloc(char *dev_name);

/**
 * @fn void ce_gw_dev_free(struct net_device *eth_dev)
 * @brief Free virtual ethernet device and remove from internal lists
 * @pre ce_gw_dev_unregister() was already called
 * @param eth_dev the virtual ethernet device allocated by ce_gw_dev_alloc()
 * @ingroup dev
 */
extern void ce_gw_dev_free(struct net_device *eth_dev);

/**
 * @fn void ce_gw_dev_setup(struct net_device *dev, enum ce_gw_type type,
 *                   __u32 flags)
 * @brief Sets the default attributes for the Gateway Ethernet device.
 * @param type Type of the Gateway wich will be linked to the device. A
 * sensible MTU will be set. Use CE_GW_TYPE_NONE for default ethernet MTU.
 * @param flags If CE_GW_F_CAN_FD Flag is set the MTU will be set to the
 *              CAN-FD size else to the normal CAN size.
 * @param dev the ethernet device which should be set up
 * @details Sets the default attributes for the Gateway Ethernet device. Also
 *          links the net_device operations to the net_dev structure.
 *          ce_gw_dev_register() should be called afterwards for registering,
 *          ce_gw_dev_unregister() for unregistering at the end.
 * @ingroup dev
 */
extern void ce_gw_dev_setup(struct net_device *dev, enum ce_gw_type type,
                            __u32 flags);

/**
 * @fn struct net_device *ce_gw_dev_create(enum ce_gw_type type, __u32 flags)
 * @brief Allocate the device and set standart Attributes with ce_gw_dev_setup()
 * @param type Type of the Gateway wich will be linked to the device. A
 * sensible MTU will be set. Use CE_GW_TYPE_NONE for default ethernet MTU.
 * @param flags If CE_GW_F_CAN_FD Flag is set the MTU will be set to the
 *              CAN-FD size else to the normal CAN size.
 * @param dev_name the name of the new allocated device
 * @return A pointer to the allocated and configured device.
 * @ingroup dev
 */
extern struct net_device *ce_gw_dev_create(enum ce_gw_type type, __u32 flags,
  char *dev_name);

/**
 * @fn int ce_gw_dev_register(struct net_device *eth_dev)
 * @brief Register a virtual ethernet device on the OS
 * @param eth_dev the virtual ethernet device
 * @pre param eth_dev was previously allocated by ce_gw_dev_alloc()
 * @retval 0 on success
 * @retval <0 on failure
 * @ingroup dev
 */
extern int ce_gw_dev_register(struct net_device *eth_dev);

/**
 * @fn void ce_gw_dev_unregister(struct net_device *eth_dev)
 * @brief Unregister virtual ethernet device and remove from internal lists
 * @param eth_dev the virtual ethernet device
 * @pre param eth_dev was previously registered by ce_gw_dev_register()
 * @ingroup dev
 */
extern void ce_gw_dev_unregister(struct net_device *eth_dev);

/**
 * @fn int ce_gw_dev_init_module(void)
 * @brief Initialise all objects, wich are needed by the other functions
 * @post the other functions of the file could only be used after this
 *       function was called
 * @retval 0 on success
 * @retval <0 on failure
 * @ingroup dev
 */
int ce_gw_dev_init_module(void);

/**
 * @fn void ce_gw_dev_cleanup(void)
 * @brief Deleting all objects created by ce_gw_dev_init_module() and all
 *        devices
 * @post after this function is called the other functions could not be used
 *       any more
 * @ingroup dev
 */
extern void ce_gw_dev_cleanup(void);

#endif

/**@}*/
