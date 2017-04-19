# brute_tun
一个用于在IP网络中的两个Linux系统之间抵抗丢包的隧道程序

---

### Features
 * 主机间使用UDP通信。
 * 利用tun功能建立点到点隧道。
 * 完全无状态
 
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

* "LocalPort" 定义套接字的本地bind端口,可以用LIST定义多个端口值。默认之是60001。

* "TunnelLocalAddr" 和 "TunnelPeerAddr" 定义tun隧道建立好之后的地址配置。

* "DupLevel" 定义荣誉发送级别，越大抵抗丢包能力越强。

---

### 开始吧！
* Host1 (被动端)
配置文件：
```json
{
	"LocalPort"			:	[60000],
	"TunnelLocalAddr"	:	"172.16.111.1",
	"TunnelPeerAddr"	:	"172.16.111.2",

	"DupLevel"			:	2
}
```
运行：
```shell
brutun -c your_configure_file
```

* Host2 (主动端)
配置文件:
```json
{
  "RemoteAddress" : "被动端的IP地址",
  "RemotePort" : 60000,
	"LocalPort"			:	[60000],
	"TunnelLocalAddr"	:	"172.16.111.2",
	"TunnelPeerAddr"	:	"172.16.111.1",

	"DupLevel"			:	3
}
```

运行:
```shell
brutun -c your_configure_file
```

OK现在在Host1上运行
```
ping 172.16.111.2
```
看看是不是正常。

---

玩好！
