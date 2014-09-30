<a name="top"/></a>

developer_doc.md
================

Developer documentation for the CAN Ethernet Gateway

<a name="toc"/></a>
This file contains:

 1. [Overview / What is a CAN Ethernet Gateway](#chap1)
	1. [Ethernet followed by CAN](#chap1-1)
	2. [Ethernet followed by IP and TCP/UDP](#chap1-2)
	3. [Ethernet followed by IP, TCP/UDP and CAN](#chap1-3)
 2. [CAN Ethernet Gateway concept](#chap1)
	1. [Netlink server](#chap2-1)
	2. [Virtual ethernet device](#chap2-2)
	3. [CAN - Ethernet Gateway](#chap2-3)
	4. [Netlink client](#chap2-4)
 3. [Translating CAN and ethernet](#chap3)
 	1. [Characteristics of CAN](#chap3-1)
		1. [CAN SFF and EFF](#chap3-1-1)
		2. [CAN FD](#chap3-1-2)
		3. [Length calculation](#chap3-1-3)
	2. [Translation](#chap3-2)
		1. [Ethernet to CAN](#chap3-2-1)
		2. [Ethernet to CAN FD](#chap3-2-2)
		3. [CAN to ethernet](#chap3-2-3)
		4. [CAN FD to ethernet](#chap3-2-4)
		5. [Ethernet including complete CAN / CAN FD](#chap3-2-5)
	3. [Special messages](#chap3-3)
 4. [Routing](#chap4)
	1. [Routing lists](#chap4-1)
 5. [Useful links](#chap5)
 6. [Copyright](#chap6)
 7. [References](#chap7)

<a name="chap1"/></a>

1. Overview / What is a CAN Ethernet Gateway
--------------------------------------------

The CAN Ethernet Gateway converts a CAN (controller area network) signal into
an ethernet signal. From a operation system perspective the gateway is
disguised as an ethernet device by a virtual ethernet device. Due to missing
equality of the frame structures there are some varying methods for translating
the CAN frame. In the kernel module sources the types are represented with
`enum ce_gw_type` (`ce_gw_main.h`) and in userspace with `enum gw_type`
(`netlink.h`). The Gateway connect to the CAN driver named "linux-can"
(formerly "socketCAN") which is built in since Linux Kernel 3.2 . But this
module requires at minimum Linux Kernel 3.6 . _[UP](#top)_

<a name="chap1-1"/></a>
### 1.1. Ethernet followed by CAN
The first idea is to add a CAN header and the following payload directly after 
the ethernet frame. The CAN header always uses the structure implemented in the
can.h (see 3.1) and not the original signal. This gateway type is named
`TYPE_NET` (in kernel module with an prefix) in the sources, because CAN is here
a network layer protocol.

	 +____________________+
	 |                    |
	 |   MAC (Ethernet)   |
	 +____________________+
  	 |  +______________+  |       +______________+
  	 |  |              |  |       |              |
 	 |  |  CAN header  |  |       |  CAN header  |
  	 |  +______________+<-+------>+______________+
   	 |  |              |  |       |              |
  	 |  |   payload    |  |       |   payload    |
  	 |  +______________+  |       +______________+
  	 +____________________+
 
This method is easy to implement but in return you need a special network on
side of the ethernet signals. The network of the ethernet side must not be based
on IP but on CAN. Otherwise the signal can't be interpreted.
It is appropriate for CAN brigdes, where two or more CAN networks should be
connected via a standart ethernet bridge. In principle this can also be done
with the method described in 1.3 but method 1.1 is more effictive on a local
computer. 

The MAC address can be a broadcast address for keeping things easy but this
will create a lot of traffic for the system. To keep the traffic low specific
addresses are needed. Therefore it is necessary to define a mapping table,
which definies one or more MAC address for one or more CAN identifier. The
table is not implemented yet.  _[UP](#top)_

<a name="chap1-2"/></a>
### 1.2 Ethernet followed by IP and TCP/UDP
To avoid the need for a CAN network on the side of ethernet signals an IP and
TCP/UDP header can be added after the ethernet header. They are followed by the
payload of the CAN frame. The CAN header always uses the structure implemented in the
can.h (see 3.1) and not the original signal. This gateway type is named
`TYPE_ETH` (in kernel module with an prefix) in the sources.

	 +______________________+      +______________________+
	 |                      |      |                      |
	 |    MAC (Ethernet)    |      |      CAN header      |
	 +______________________+      +______________________+
	 |  +________________+  |      |  +________________+  |
	 |  |                |  |      |  |                |  |
	 |  |   IP header    |  |      |  |   IP header    |  |
	 |  +________________+  |      |  +________________+  |
	 |  |                |  |      |  |                |  |
	 |  | UDP/TCP header |<-+------+->| UDP/TCP header |  |
	 |  +________________+  |      |  +________________+  |
	 |  |                |  |      |  |                |  |
	 |  |    payload     |  |      |  |    payload     |  |
	 |  +________________+  |      |  +________________+  |
	 +______________________+      +______________________+

For this method a CAN frame already including IP and TCP/UDP header is needed.
This special CAN frame makes it easier to implement but, of course, the gateway
will only work if all CAN frames are organized in the given way. It is also
necessary to use a CAN FD because standard and extended frame format CANs don't
own enough specificed payload for the payload of IP header, TCP/UDP and payload 
itself. It can be used for networks using different layer two protocolls.

If ethernet just includes a payload without IP or TCP/UDP header it can still be
translated into a CAN frame.

As there is no CAN header included in the ethernet frame anymore it is important
to map the ID of the CAN header on the MAC address. But in comparison with the
first approach the ethernet frames will be compatible with all networks using
IP and TCP/UDP. These are not yet implemented.  _[UP](#top)_

<a name="chap1-3"/></a>
### 1.3 Ethernet followed by IP, TCP/UDP and CAN

If an ethernet frame is requested that is compatible with networks using IP and
TCP/UDP and additionally using every CAN frame should be possible there is
another option. The CAN header always uses the structure implemented in the
can.h _[(see 3.1) ](#chap3-1)_ and not the original signal. This gateway type 
is named `TYPE_TCP` and `TYPE_UDP` (in kernel module with an prefix) in the 
sources.

	 +______________________+
	 |                      |
	 |    MAC (Ethernet)    |
	 +______________________+
	 |                      |
	 |      IP header       |
	 +______________________+
	 |                      |
	 |    UDP/TCP header    |
	 +______________________+
	 |  +________________+  |       +________________+
	 |  |                |  |       |                |
	 |  |   CAN header   |  |       |   CAN header   |
	 |  |________________|<-+------>+________________+
	 |  |                |  |       |                |
	 |  |    payload     |  |       |    payload     |
	 |  +________________+  |       +________________+
	 +______________________+

In this case any CAN frame is added into the payload of the TCP/UDP protokoll.
The ID of the CAN frame is used to map an IP and also the port for TCP or UDP.
Having the best functunallity the implementation of this stack is the hardest
out of all three options. This method can be used for a CAN bridge over an IP
network, e.g. internet. Another possibility is to use the gateway for sending a
CAN frame to an application.  _[UP](#top)_

<a name="chap2"/></a>

2. CAN Ethernet Gateway concept
-------------------------------

The CAN Ethernet Gateway consists of three parts:

*	 netlink server
*	 virtual ethernet device
*	 CAN - Ethernet Gateway

Additionally there is a netlink client in the userspace.

This graphic showes the structur of the CAN Ethernet Gateway:

~~~~~~~
  _______________________________________________________
 |                                                       |
 |       CAN Ethernet Gateway Kernel Module (ce_gw)      |
 |_______________________________________________________|
 |                                                       |
 |           Ethernet Frame    +----------------------+  |  CAN Frame
 |           +-----o---------->|CAN - Ethernet Gateway|<-+-------o----+
 |           |                 |     ce_gw_main.c     |  |            |
 |           |                 +----------------------+  |            v
 |           v                                           |       +----------+
 | +--------------------+                                |       |CAN Device|
 | |virtual Ethernet dev|                                |       |  can.c   |
 | |('cegw#' Interface) |         +---------------+      |       +----------+
 | |    ce_gw_dev.c     |         |Netlink Server |      |            ^
 | +--------------------+         |ce_gw_netlink.c|      |            |
 |           ^                    +---------------+      |            v
 |           |                            ^              |       +----------+
 |___________|____________________________|______________|       |CAN Driver|
             |                            o Netlink Frame        +----------+
             v            Kernelspace     |                           ^
           +----+       __________________|_____________              |
           | OS |         Userspace       |                           v
           +----+                         v                       +-------+
                                 +------------------+             |CAN NIC|
                                 |  Netlink Client  |             +-------+
                                 |    netlink.c     |
                                 |(can-eth-gw-utils)|
                                 +------------------+

Diagramm which shows the relation and packet transmission between
the components of this kernel module (ce_gw) and others of the OS.
  _[UP](#top)_
~~~~~~~

<a name="chap2-1"/></a>

### 2.1  Netlink server

The netlink server administrates the communication between kernel- and
userspace. It enables a configuration of the gateway by the user.
  _[UP](#top)_

<a name="chap2-2"/></a>
### 2.2 Virtual ethernet device

The virtual ethernet device theoretical disguises the CAN Ethernet Gateway as
ethernet device. Therefore it is possible to use the gateway just like every
other ethernet device without knowing the structure behind. 

<a name="chap2-3"/></a>
### 2.3 CAN - Ethernet Gateway

The CAN - Ethernet Gateway is responsible for receiving CAN frames and
translating them. Additionally it theoretical simulates an ethernet network so
that it is possible to use every ethernet function on the translated CAN frame.  _[UP](#top)_

<a name="chap2-4"/></a>
### 2.4 Netlink client

The netlink client controls the gateway in the userspace. See application
'cegwctl' in package "can-eth-gw-utils":
([Link](https://github.com/can-eth-gw/can-eth-gw-utils "Sources on Github"))  _[UP](#top)_

<a name="chap3"/></a>

3. Translating CAN and ethernet
-------------------------------

<a name="chap3-1"/></a>
### 3.1 Characteristics of CAN

While working with the CAN struct there are a few things to keep an eye on.
First thing is that there are basicly three different CAN formats:

* CAN SFF (standard frame format)
* CAN EFF (extended frame format)
* CAN FD (fexible data)

  _[UP](#top)_

<a name="chap3-1-1"/></a>
#### 3.1.1 CAN SFF and EFF

CAN SFF and EFF use the same struct (can_frame). The difference between this
two types can found in the header. While an SFF has eleven bit for the
identifier plus three additional flags, EFF uses 29 bit for the identifier plus
the same three flags. To detect which of this two types is used the
CAN_EFF_FLAG needs to be checked. The original CAN signal has a different structure
than the signal produced by the CAN struct. 

Here is the original signal:


__SFF header:__

~~~~~~~
                1   1   1
     11 bit    bit bit bit  4 bit
  +___________+___+___+___+______+___...
  |           |   |   |   |      | Raw
  |identifier |RTR|EFF|res| DLC  | Data
  +___________+___+___+___+______+___...
  |               |              |
  |<------------->|<------------>|
     Arbitration      Control
      Field            Field
~~~~~~~

__EFF header:__

~~~~~~~
                1   1                      1   1   1
     11 bit    bit bit       18 bit       bit bit bit  4 bit
  +___________+___+___+__________________+___+___+___+______+___...
  |           |sub|   | extended         |   |   |   |      | Raw
  |identifier |RTR|EFF| identifier       |RTR|res|res| DLC  | Data
  +___________+___+___+__________________+___+___+___+______+___...
  |                                          |              |
  |<---------------------------------------->|<------------>|
           Arbitration Field                   Control Field
~~~~~~~

The 13th bit decides whether the sent signal is SFF or EFF. (1 = EFF, 0 =
SFF).

The struct can_frame changes the signal:

~~~~~~~
                                 1   1   1    4     4
   11 bit          18 bit       bit bit bit  bit   bit      3 byte
+___________+__________________+___+___+___+_____+_____+________________+___...
|           | extended         |   |   |   | DLC |     |                | Raw
|identifier | identifier       |ERR|RTR|EFF| pad | DLC |    padding     | Data
+___________+__________________+___+___+___+_____+_____+________________+___...

~~~~~~~
	
To see if the message is a SFF or EFF the 32th bit needs to be checked. (1 =
EFF, 0 = SFF). As both, SFF and EFF, use the same struct the 18 bit of extended
identifier are set to 0 and ignored if the EFF flag is 0.
Also there is an extended DLC field of 8 bit but only 4 bit are used. The first
4 bit are just padded. An additional flag for an error (ERR) is added which is
not present in the original signal. There, an error is represented as a complete
different signal. After the DLC an additional padding field of 3 byte is between
the header and following data.

Both original SFF and EFF and the struct add a data field of eight byte.  _[UP](#top)_

<a name="chap3-1-2"/></a>
#### 3.1.2 CAN FD

The CAN FD allows the use of a up to 64 byte data field. It also adds some
additional flags and reserved fields. Therefore it is implemented in a new
struct (canfd_frame). The structure of identifier, the ERR, RTR, EFF and the
len field (same as DLC) is alike. After that part there are 24 bits of new FD
specific fields, followed by 64 byte data.

~~~~~~~
                                 1   1   1
   11 bit          18 bit       bit bit bit 4 bit 4 bit  8 bit  8 bit  8 bit
+___________+__________________+___+___+___+_____+_____+______+______+______+
|           | Extended         |   |   |   | len |     |      | res0 | res1 |
|identifier | identifier       |ERR|RTR|EFF| pad | len |flags | pad  | pad  |
+___________+__________________+___+___+___+_____+_____+______+______+______+
~~~~~~~
  _[UP](#top)_

<a name="chap3-1-3"/></a>
#### 3.1.3 Length calculation

As the 64 byte data length should be represented by a only eight bit long
len-field a calculation for the real length is needed. This is done by the
can_dlc2len function included in can/dev.h. That function is also need for
calculating the length of a can_frame.  _[UP](#top)_

<a name="chap3-2"/></a>
### 3.2 Translation

For sending the ethernet and CAN packages the sk buffer is used.  _[UP](#top)_

<a name="chap3-2-1"/></a>
#### 3.2.1 Ethernet to CAN

As a CAN frame can only hold eight byte of data there is no space for including
an IP or TCP/UDP header. Therefore we assume that the sk buffer includes only
payload after the ethernet header as shown in the picture:

~~~~~~~
mac header                                                          mac header
data pointer-->+----------------------+  +----------------------+<--data pointer
               |                      |  |                      |
               | MAC (Ethernet) [14 B]|  |  CAN header [5 bit]  |
     network-->+----------------------+  +----------------------+<--network
     header    |  +----------------+  |  |  +----------------+  |   header
               |  |                |  |  |  |                |  |
               |  | payload [8 B]  |--+--+->| payload [8 B]  |  |
               |  +----------------+  |  |  +----------------+  |
   transport-->+----------------------+  +----------------------+<--transport
   header                                                           header
~~~~~~~

The sk buffer has various pointers included. Here the mac header points
correctly at the beginning of the ethernet or CAN header, whereas the network
pointer dosen't point at the beginning of an IP header but at the start of the
payload. Because there is no TCP/UDP header the transport pointer will point
at the end of the data.

Even if the pointers of mac, network and transport header are set on the side of
the CAN frame there is no guarentee that they will be used.  _[UP](#top)_

<a name="chap3-2-2"/></a>
#### 3.2.2 Ethernet to CAN FD

When a ethernet frame is translated into a CAN FD it is possible to copy IP
header as well as TCP/UDP header. It is just important that the total length of
IP header, TCP/UDP header and payload is not more then 64 byte. Otherwise data
will be lost.

~~~~~~~
mac header                                                         mac header
data pointer-->+______________________+ +______________________+<--data pointer
               |                      | |                      |
               | MAC (Ethernet) [14 B]| |  CAN FD header [8 B] |
             __+______________________+ +______________________+__
   network--/--+->+________________+  | |  +________________+<-+--\--network
   header   |  |  |                |  | |  |                |  |  |  header
            |  |  |   IP header    |  | |  |   IP header    |  |  |
 transport--+--+->+________________+  | |  +________________+<-+--+--transport
 header     /  |  |                |  | |  |                |  |  \  header
    [64 B] <   |  | UDP/TCP header |--+-+->| UDP/TCP header |  |   > [64 B]
            \  |  +________________+  | |  +________________+  |  /
            |  |  |                |  | |  |                |  |  |
            |  |  |     payload    |  | |  |     payload    |  |  |
            |  |  +________________+  | |  +________________+  |  |
            \__+______________________+ +______________________+__/
~~~~~~~

The mac and network header can be set correctly to the start of the CAN frame
and IP header.
There could only occour problems if the ethernet sk buffer is not including IP
and TCP/UPD and just a short data field. Then the transport header will not be
set at the beginning of UDP/TCP but at the end of the payload like in the
ethernet to CAN translation.

Even if the pointers of mac, network and transport header are set on the side of
the CAN frame there is no guarentee that they will be used.  _[UP](#top)_

<a name="chap3-2-3"/></a>
#### 3.2.3 CAN to ethernet

To translate a CAN frame into an ethernet frame the payload will be copied at
the end of the ethernet header. In this direction there is no danger to exceed
the ethernet frame.

~~~~~~~
                                                                    mac header
data pointer-->+----------------------+  +----------------------+<--data pointer
               |                      |  |                      |
               |  CAN header [5 bit]  |  | MAC (Ethernet) [14 B]|
               +----------------------+  +----------------------+<--network
               |  +----------------+  |  |  +----------------+  |   header
               |  |                |  |  |  |                |  |
               |  | payload [8 B]  |--+--+->| payload [8 B]  |  |
               |  +----------------+  |  |  +----------------+  |
               +----------------------+  +----------------------+<--transport
                                                                    header
~~~~~~~

However it can not be assumed that all pointers are set correctly in the CAN
frame. Only the data pointer will point to the start of the CAN header. All
other pointers: mac, network and transport header could point anywhere.
Therefore it is neccessary to calculate the start and end of payload. 
Additional the transport header can still not be set correctly as there is no
TCP/UDP header. Therefore it needs to be set at the and of the payload.
  _[UP](#top)_

<a name="chap3-2-4"/></a>
#### 3.2.4 CAN FD to ethernet

The translation of CAN FD to ethernet is quite similar to ethernet to CAN FD.

~~~~~~~
                                                                   mac header
data pointer-->+______________________+ +______________________+<--data pointer
               |                      | |                      |
               |  CAN FD header [8 B] | | MAC (Ethernet) [14 B]|
             __+______________________+ +______________________+__
            /  |  +________________+  | |  +________________+<-+--\--network
            |  |  |                |  | |  |                |  |  |  header
            |  |  |   IP header    |  | |  |   IP header    |  |  |
            |  |  +________________+  | |  +________________+<-+--+--transport
            /  |  |                |  | |  |                |  |  \  header
    [64 B] <   |  | UDP/TCP header |--+-+->| UDP/TCP header |  +   > [64 B]
            \  |  +________________+  | |  +________________+  |  /
            |  |  |                |  | |  |                |  |  |
            |  |  |     payload    |  | |  |     payload    |  |  |
            |  |  +________________+  | |  +________________+  |  |
            \__+______________________+ +______________________+__/
~~~~~~~

However again it can not be assumed that all pointers are set correctly in the
CAN frame. Only the data pointer will point to the start of the CAN FD header. 
All other pointers: mac, network and transport header could point anywhere.
Therefore it is neccessary to calculate the start and end of payload as well as 
start of IP and TCP/UDP header, if they are part of the CAN FD frame.
If the CAN FD includes IP and TCP/UDP header both pointers can be set correctly
as shown in the picture. Otherwise the transport pointer will be set to the end
of the payload.  _[UP](#top)_

<a name="chap3-2-5"/></a>
#### 3.2.5 Ethernet including complete CAN / CAN FD

If an ethernet frame includes a complete CAN or CAN FD frame the data after
the ethernet header is just copied into a new sk buffer.

~~~~~~~
mac header
data pointer-->+_______________________+
               |                       |
               | MAC (Ethernet) [14 B] |
     network-->+_______________________+
     header    | +__________________+  |   +_________________+<--data pointer
               | | CAN/CAN FD header|  |   |CAN/CAN FD header|
               | |     [5 B/8 B]    |  |   |    [5 B/8 B]    |
               | +__________________+<-+-->+_________________+
               | |      payload     |  |   |     payload     |
               | |     [8 B/64 B]   |  |   |    [8 B/64 B]   |
               | +__________________+  |   +_________________+
   transport-->+_______________________+
   header
~~~~~~~

On the ethernet side mac, data and network pointer are set correctly. Only the
transport pointer is pointing to the end of the payload. On CAN or CAN FD side
only the data pointer is set. All other pointers can point anywhere.  
_[UP](#top)_

<a name="chap3-3"/></a>
### 3.3 Special messages

On both sides, ethernet and CAN, there are some special messages that can not
be translated directly. 
The ARP-requests could be managed by the gateway itself. The gateway could 
answer the requests by the mapping table described in 1.3. For the method of
1.1 and 1.2 new tables would be needed.
When the CAN header is not included in the ethernet package the error bit of
the CAN message will be lost. There is no way to translate it.
Also a ping could be realized using the RTR bit (remote transmission bit) of
the CAN header.
Beside those messages described there are some other messages that can't be
translated directly but are not relevant.
These messages are not implemented yet.  _[UP](#top)_

<a name="chap4"/></a>

4. Routing
----------

In the CAN Ethernet Gateway there are two routs. One goes from the virtual
ethernet device over the "CAN - ethernet gateway" to the CAN device. The other
one routs excatly the other way round.  _[UP](#top)_

<a name="chap4-1"/></a>
### 4.1 Routing lists

~~~~~~~
     +------------------+             /--------------------------------------\
     |   <<Ethernet>>   |             |ce_gw_job has ethernet as dst and CAN |
     |struct net_device |             |as source OR has ethernet as src and  |
     +------------------+             |CAN as dst. hlist_node annotations are|
 +-->|       ....       |             |members of the structs where the arrow|
 |   +------------------+             |points to. The hlist structs are not  |
 |           |                        |directly represented here and have    |
 |           |void *priv              |been simplyfied.                      |
 |           |                        \--------------------------------------/
 |           v
 |   +---------------------+        struct hlist_head ce_gw_job_list
 |   |struct ce_gw_job_info|                       |
 |   +---------------------+                       |
 |   |                     |    struct hlist_node  |
 |   +---------------------+        list         \ |
 |                 | | struct                     \|
 |         struct  | |hlist_head                   |     +------------------+
 |       hlist_head| | job_src                     |     |     <<CAN>>      |
 |         job_dst | |                             |     |struct net_device |
 |                 | |\                            |     +------------------+
 |                 | | struct hlist_node           |     |       ....       |
 |                 | |/   list_dev                 |     +------------------+
 |                 | /                             |               ^
 |                 |/|                             |0...*          |
 | struct          | |       0...*                 v               | struct
 |net_device       | +--------->+----------------------+           |net_device
 | *dev            +----------->|  struct ce_gw_job    |           | *dev
 |                        0...* +----------------------+           |
 |        +-------------+       |struct rcu_head rcu   |      +-------------+
 |        |   union     |       |u32 id                |      |   union     |
 +--------+-------------+       |enum ce_gw_type type  |      +-------------+
          |             |<>-----|u32 flags             |----<>|             |
          +-------------+  dst/ |u32 handled_frames    | src/ +-------------+
                           src  |u32 dropped_frames    | dst
                                |union { struct can_   |
                                |filter can_rcv_filter}|
                                +----------------------+
~~~~~~~

All routes between CAN and ethernet are saved in the ce_gw_job_list, which is
part of the ce_gw_job struct. 

Additionally the virtual ethernet device manages two lists:

* job_dst  
* job_src

Every ethernet device has it's own 2 lists.

In job_dst all entrys point to a instance of ce_gw_job. Here the saved source
must be identical to the ethernet device, that owns the list.

Job_src works in the same way. It lists all instances of ce_gw_job, that
reference the ethernet device as source.  _[UP](#top)_

<a name="chap5"/></a>

5. Useful links
---------------

* bosch CAN documentation: [http://www.bosch-semiconductors.de/media/pdf_1/canliteratur/can_fd_spec.pdf](http://www.bosch-semiconductors.de/media/pdf_1/canliteratur/can_fd_spec.pdf)
* socket can: [https://gitorious.org/linux-can](https://gitorious.org/linux-can)

<a name="chap6"/></a>

6. Copyright
------------

(C) Copyright 2013 Fabian Raab, Stefan Smarzly

This file is part of CAN-Eth-GW.

CAN-Eth-GW is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

CAN-Eth-GW is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with CAN-Eth-GW.  If not, see <http://www.gnu.org/licenses/>.

<a name="chap7"/></a>

7. References
-------------

__Sources:__
  [https://github.com/can-eth-gw/can_eth_gw/](https://github.com/can-eth-gw/can_eth_gw/ "Sources")
 
__Homepage:__
  [http://can-eth-gw.github.io/](http://can-eth-gw.github.io/ "Homepage")

__Authors:__

   + Ann Katrin Gibtner
   + Fabian Raab _<fabian.raab@tum.de>_
   + Stefan Smarzly _<stefan.smarzly@in.tum.de>_

__Date:__ 17. July 2013

