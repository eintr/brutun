# net_simulator
一个用于在LAN中的两个Linux系统之间模拟延迟、丢报和限速的简单的C程序。适用于在LAN环境下进行互联网应用程序的测试

---

### Features
 * 主机间使用UDP通信。
 * 利用tun功能建立点到点隧道。
 * 流控算法采用TBF(令牌桶过滤器)
 * 我再想想...

---

### Build and install
编辑Makefile , 修改前面几行

然后
```
make
make install
```

---

### Configure
先打开example.conf猜猜看……没错，就是个json。

* "RemoteAddress" 和 "RemotePort" 定义对端地址与端口。如果省略这两个配置，则程序将工作于被动模式。也就是说程序将使用它第一次收到的数据包的源地址作为对段地址，在此之前程序将会丢弃所要发送的数据。

* "LocalPort" 定义套接字的本地bind端口, 本地地址永远是"0.0.0.0"。

* "TunnelLocalAddr" 和 "TunnelPeerAddr" 定义tun隧道建立好之后的地址配置。

* "DropRate" 定义丢包概率, 取值: [0-1] 。

* "TBF_Bps" 和 "TBF_burst" 定义TBF(令牌桶过滤器)的限速参数。什么是TBF? 去Google一下。"TBF_Bps"取值单位是每秒字节数。"TBF_burst"取值单位是字节.

* "Delay" 定义每个数据包的延迟，取值以毫秒(millisecond)为单位。注意：如果你定义的带宽限速速率和延迟都很大的话，你需要有很多内存才行。


注意: 所有的配置都仅仅用于发送数据包！接受数据的过程是完全不受控制的！我强调过了！

---

### 开始吧！
* Host1 (被动端)
配置文件：
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
运行：
```shell
wormhole -c your_configure_file
```

* Host2 (主动端)
配置文件:
```json
{
  "RemoteAddress" : "被动端的IP地址",
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

运行:
```shell
wormhole -c your_configure_file
```

OK现在在Host1上运行
```
ping 172.16.111.2
```
看看是不是正常。

---

玩好！
