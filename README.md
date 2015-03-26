# net_simulator
A REALLY simple C program to simulate delay, packet loss and speed limit between 2 Linux boxes. Good for performing internet program testing in LAN.

---

### Features
 * Use UDP between hosts.
 * Create point-to-point tunnel with tun devices.
 * Flow control with TBF(Token Bucket Filter)
 * Let me think...

---

### Build and install
Edit Makefile , change the above lines.

Then just
```
make
make install
```

---

### Configure
Just take a look at example.conf and guess... Yes, it's in json.

* "RemoteAddress" and "RemotePort" indicates the peer side socket address. If these 2 entries are ommited, the program will be running in passive mode which means it will use the source address/port of the first packet it receives as the peer side address/port, and it will discard all packets before this.

* "LocalPort" indicates the local socket port, "0.0.0.0" is always used.

* "TunnelLocalAddr" and "TunnelPeerAddr" indicates the IP addresses would be configured to the tun device after it is created. 
* "DropRate" indicates packet loss probability, value: [0-1]

* "TBF_Bps" and "TBF_burst" indicates the TBF(Token Bucket Filter) parameter for speed limit. What is TBF? Google it if needed. "TBF_Bps" value is BYTEs per second. "TBF_burst" value is also in BYTEs.

* "Delay" indicates the time delay of each packet, value in ms(millisecond). Note: You need erally big RAM if you configred a very big delay in very big Bps. 


NOTE: All configures are working while SENDING packets! Receiving packet procedure is not controlled at all! I have told you!

---

### Start!
* Host1 (Passive side)
configure file:
```json
{
	"LocalPort"			:	60000,
	"TunnelLocalAddr"	:	"172.16.111.1",
	"TunnelPeerAddr"	:	"172.16.111.2",

	"DropRate"			:	0.05,
	"TBF_Bps"			:	10000000,
	"TBF_burst"			:	100000000,
	"Delay"				:	100
}
```
You type:
```shell
wormhole -c your_configure_file
```

* Host2 (Active side)
configure file:
```json
{
  "RemoteAddress" : "IP_ADDRESS_OF_PASSIVE_SIDE",
  "RemotePort" : 60000,
	"LocalPort"			:	60000,
	"TunnelLocalAddr"	:	"172.16.111.2",
	"TunnelPeerAddr"	:	"172.16.111.1",

	"DropRate"			:	0.05,
	"TBF_Bps"			:	10000000,
	"TBF_burst"			:	100000000,
	"Delay"				:	100
}
```

You type:
```shell
wormhole -c your_configure_file
```

OK, you can type
```
ping 172.16.111.2
```
at Host1 to check if it's OK.

---

Have FUN!