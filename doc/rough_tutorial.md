Rough Tutorial
==============

This document is just a rough tutorial if you just want to try ca-eth-gw out and see it working. Only virtual devices are used, so there is no need for a real CAN device.

This tutorial was done on Linux Kernel 3.8.0 . It is not sure if it works on other kernels too. See `INSTALL.md` for more general details.

You need the kernel module itself `can_eth_gw` and the two userspace tools `cegwctl` and `cegwsend`. Look at the according `INSTALL.md` for how to set up them.

Lets start with loading the required kernel modules:

    modprobe -vv can
    modprobe -vv can-bcm
    modprobe -vv can-dev
    modprobe -vv can-gw
    modprobe -vv can-raw
    modprobe -vv vcan

ad of course also our own module:

    modprobe -vv ce_gw

Then set up a virtual can device and an appropiate etehrnet gateway:

	ip link add dev vcan0 type vcan
    ip link set up vcan0
    
    cegwctl add dev
    ip addr add 192.168.145.2/24 brd 192.168.145.255 dev cegw0
    ip link set up dev cegw0
    
Now you should see a virtual can device `vcan0` and a gatewayed ethernet device `cegw0`, both with state `UNKNOWN` with the command `ip addr`:

	$ ip addr
	6: vcan0: <NOARP,UP,LOWER_UP> mtu 16 qdisc noqueue state UNKNOWN
    link/can
	7: cegw0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 16 qdisc pfifo_fast state UNKNOWN qlen 1000
    link/ether 00:00:00:00:00:00 brd ff:ff:ff:ff:ff:ff
    inet 192.168.145.2/24 brd 192.168.145.255 scope global cegw0

Now add a bidirectional route between `vcan0` and `cegw0`:

	cegwctl add route --type=net vcan0 cegw0
    cegwctl add route --type=net cegw0 vcan0

The command `cegwctl route` should now show you the routes_

    $ cegwctl route
     ID       SRC    DST    TYPE   HANDLED  DROPPED  FLAGS
     2        cegw0  vcan0  NET    0        0        <>
     1        vcan0  cegw0  NET    0        0        <>

Now you need two terminals; one for sending data and one for capturing.

Start capturing on the `cegw0` device. `tshark` ist the command line version of *wireshark*, but of course you can also use wireshark directly:
*terminal1:*

	tshark -i cegw0 -S -x

Now send some random data on the virtial can device:
*terminal2:*

	cangen vcan0

You should now see on the capturing terminal the translated ethernet packages:
*terminal1:*

    Capturing on cegw0
      0.000000 00:00:00_00:00:00 -> Broadcast    LLC 30 I, N(R)=0, N(S)=0; DSAP 0xce Group, SSAP NULL LSAP Response

    0000  ff ff ff ff ff ff 00 00 00 00 00 00 00 0c cf 01   ................
    0010  00 00 08 00 00 00 f3 1c df 72 06 ca 88 34         .........r...4

Lets try vice-versa: Stop the other commans with `^c` and start capturing the virtual can device:

*terminal1:*

	candump vcan0

Now send a raw ethernet package with an can header as payload on the `cegw0` device:

*terminal2:*

	cegwsend -d ff-ff-ff-ff-ff-ff -t can -r 435f -i cegw0
    
On the capturing terminal you should see that some data is arriving:
*terminal1:*

	vcan0  000   [1]  5F
    
Congratulations, you have now successfully translated a can packet to a ethernet packet in both directions.
