/**
 * @file ce_gw_netlink.h
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

#ifndef __CE_GW_NETLINK_H__
#define __CE_GW_NETLINK_H__

/**
 * @fn int ce_gw_netlink_init(void)
 * @brief Must called once at module init.
 * @details  During init of module this function must called. It registers
 *           the family and its operations.
 * @retval 0 when init successful
 * @retval <0 if an error occurred.
 * @ingroup net
 */
int ce_gw_netlink_init(void);

/**
 * @fn static void ce_gw_netlink_exit(void)
 * @brief Must called once at module exit.
 * @ingroup net
 * @details     During exit of module this function must called. It unregisters
 *              the family and its operations.
 */
void ce_gw_netlink_exit(void);

#endif

/**@}*/
