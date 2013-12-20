all: tftpd.exe

tftpd.obj : tftpd.c
  icc /Ti /Sp1 tftpd.c

tftpd.exe : tftpd.obj
  ilink /DEBUG /ST:65535 tftpd.exe tcpip32.lib
