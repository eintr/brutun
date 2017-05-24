# brute_tun

意思是: Brute Tunnel

用于在IP网络中的两个Linux系统之间，通过暴力手段抵抗丢包的隧道程序


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

### 命令行参数
只有一个：-c 配置文件

是的，没有daemon运行的选项，因为那根本就没必要。

* 如果你需要以daemon方式运行，总要解决异常退出的重启对不对？所以你总要套在一个supervisor下面跑，而任何supervisor都能以daemon方式运行。
* 如果你不需要引入supervisor，只能说明你的服务可用性其实并不重要，利用一下 setsid(1) 也就够了。

---

### Configure
先打开example.conf猜猜看……没错，就是个json。

* "RemoteAddress" 和 "RemotePort" 定义对端地址与端口。如果省略这两个配置，则程序将工作于被动模式。也就是说程序将使用它第一次收到的数据包的源地址作为对段地址，在此之前程序将会丢弃所要发送的数据。

* "LocalPort" 定义套接字的本地bind端口,可以用LIST定义多个端口值。默认之是60001。

* "TunnelLocalAddr" 和 "TunnelPeerAddr" 定义tun隧道建立好之后的地址配置。

* "DupLevel" 定义冗余发送级别，越大抵抗丢包能力越强。

* "MagicWord" 验证字段。双端的配置必须一致。所有收到的 MagicWord 不匹配的包，将会被无声丢弃。

* "DefaultRoute" 指定一个路由表，隧道建立成功后，自动成为该路由表的默认路由。（如有疑惑，请学习一下Linux的iproute2机制）

* "RoutePrefix" 隧道建立成功后，接管列表中所有地址前缀的路由。当前版本的程序并没有检查路由冲突导致的操作失败，需要人工审查。


---

### 开始吧！
* Host1 (被动端)
最简配置文件：
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
最简配置文件:
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
