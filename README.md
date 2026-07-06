# 集群聊天服务器学习仓库

这个仓库用于跟学 B 站课程《C++ 项目-集群聊天服务器-软件分层设计和高性能服务开发》，目标是边学边整理笔记，边把代码补成一个可运行、可测试、可扩展的聊天服务器项目。

## 当前进度

- 完成第一版程序开发

## 构建

```bash
mkdir build
cd build
cmake ..
make 
```

## 目录

```text
include/            公共头文件
bin/                二进制源码
src/                源码
tests/              单元测试
docs/               学习笔记和开发计划
third_party         第三方库(json库、nginx、muduo)
```

## 配置

## muduo库安装

### 一、先安装依赖（必须）

```bash
sudo apt-get update
sudo apt-get install -y cmake libboost-all-dev
```
### 二、下载 muduo 源码（推荐稳定版）
进入你项目的 third_party 目录：
``` bash
 cd ~/CHATSERVER/third_party
```
下载源码：
```bash 
git clone https://github.com/chenshuo/muduo.git
```
### 三、编译 & 安装（一步到位）
```bash
cd muduo
mkdir build 
cd build
cmake ..
#安装（把库安装到系统目录）
make && make install
```
## nginx 安装
### 一、安装依赖
```bash
 sudo apt install libpcre3 libpcre3-dev -y
 ```
### 二、下载源码
进入third_party文件夹
``` bash
 cd ~/CHATSERVER/third_party
```
解压缩源码
```bash
 tar -zxvf nginx-1.30.2.tar.gz
 ```
### 三、编译 & 安装
```bash
./configure --with-stream 
make && make install

```
#### 四、修改配置文件&&启动
```bash
vim /usr/local/nginx/conf/nginx.conf
```
写入聊天服务器负载均衡配置：
```
stream {
    upstream chat_server {
        #配置服务器 ip和端口 ，weight=1权重为1 ，max_fails=3 fail_timeout=30s 每隔30s发送一个心跳包，心跳包超时三次，认为服务器端口
       server 127.0.0.1:6000 weight=1 max_fails=3 fail_timeout=30s;
       server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
        #least_conn;#最少连接调度算法：谁当前的连接数最少，Nginx 就把新客户端分发给谁。（也可以用这种，默认是轮询）
    }

    server {
        listen 8000; #客户端发送到的端口
        proxy_pass chat_server;#把客户端发过来的所有流量，转发给上面配置好的 chat_server 服务器集群。
    }
}
```
启动
```bash
#启动
/usr/local/nginx/sbin/nginx 
#平滑启动
/usr/local/nginx/sbin/nginx -s reload 
#检查是否运行成功
ps -ef | grep nginx
#停止
/usr/local/nginx/sbin/nginx -s stop
```

## redis 安装
### redis服务器安装
```bash
 sudo apt install redis-server
 ```
 ### hireids库安装
解压缩源码
```bash
unzip hiredis-master.zip
 ```
编译 & 安装
```bash
cd hiredis-master
make && make install
#刷新动态库缓存
ldconfig 

```
