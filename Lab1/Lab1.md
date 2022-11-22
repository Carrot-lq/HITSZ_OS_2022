# Lab1: XV6与UNIX实用程序

本次实验实现5个Unix实用程序，目的为掌握在xv6上编写用户程序的方法。

## sleep

本程序用于理解用户程序，只需在 ``Makefile`` 文件中找到 ``UPROGS`` ，并添加行``$U/_sleep\``

## pingpong

实现思路：父进程向管道1写入 ``ping`` 后，等待从管道2读出并打印；子进程从管道2读出后打印，接着向管道1写入 ``pong`` 。

## primes

函数 ``primes`` 接收一个管道p，从管道中读出第一个非0数即为质数，并以其为基数进行筛选；新建管道pp，调用fork创建子进程，父进程将后续从p读到的所有非0数除以基数，若整除则跳过，否则将其传入pp，子进程中递归调用 ``primes`` 并传入pp。主程序中父进程将2-35传入管道并在最后添加0作为结束符，子进程调用 ``primes`` ，即可实现筛选2-35中的质数。

## find

参考 ``ls.c`` ，修改其遍历至文件时直接打印为名称匹配才打印，遍历至文件夹时新增跳过 ``.`` 与 ``..`` ，然后递归调用find查找文件夹内所有文件和子目录。

## xargs

循环从标准输入中读取字符，若无读入或仅读到 ``\n`` 退出循环；每次读至 ``\n`` 并存在有效数据时，将所读字符串添加至参数列表并执行。