# Introduction
BUPT 2021 互联网应用大作业 用C语言在Linux环境中实现一个FTP服务器(socket编程)
## Implemented Function:
基本命令: Auth, PWD, CWD, LISt, MKDIR, DEL, RN <br>
主动模式Active mode 下载与上传 <br>
服务器展示: Username, IP, Action, speed, total trafic. <br>

## How to use in Linux terminal
gcc myserver.c -o myserver -lcrypt  <br>
sudo ./myserver  <br>
还没学会写makefile <br>
使用vsftpd连接至local服务器 ftp 127.0.0.1 <br>

## Socket Structure
 ![image](https://user-images.githubusercontent.com/56614895/121205636-6c2fce80-c8aa-11eb-805b-b0f93569c887.png)

## 参考资料：
0. https://www.cnblogs.com/wanghao-boke/p/11930651.html C语言实现FTP服务器 
1. https://www.geeksforgeeks.org/socket-programming-cc/ Socket Programming in C/C++ 
