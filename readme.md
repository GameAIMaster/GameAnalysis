# 前言

​        自18年始，加入畅游天龙工作室，开始完整的制作一款MMO大航海游戏。众所周知，畅游主打游戏天龙八部从07年正式开测，13年来依旧是公司的主要营收项目。所以MMO游戏基因一直深入到公司文化当中。项目决定继承天龙开发模式，客户端采用C#+Lua开发，引擎使用的Unity引擎，渲染借助了UE4加速渲染，服务器架构采用的是天龙端游手游上线部署的，运营方面由畅游子公司17173发行。

​	本文指在具体介绍分析天龙架构，各个功能模块实现方案，也会查阅当下资料对比优缺点，方便读者全面了解相关知识。代码中的功能不断积累，文中不能面面俱到，很多知识也有自己的主观见解，如果有错误希望指正。

​	收到文笔限制，也是自己第一次系统的总结知识，读起来可能会枯燥乏味，我会尽可能通俗易懂的解释概念

​	

​        