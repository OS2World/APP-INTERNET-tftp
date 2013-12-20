#include "signal.h"
#include <stdio.h>
#include <stdlib.h>
#include <types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <tftpd.h>

char path_prefix[256];
int  default_blksize;
int  num_block;
char verbose;

int decode(char type,char *raw,int len_raw,PACKET *packet)
{
 char           *pos[20];
 int            nbr;
 int            i;

 switch( type )
  {
   case RRQ:
        nbr        = 0;
        pos[ nbr ] = &raw[0];

        for(i=0;i<len_raw;i++)
           if (raw[i] == '\0')
             pos[ ++nbr ] = &raw[i+1];
        nbr--;
        strcpy(packet->rrq.filename,pos[0]);

        packet->rrq.type_trf = '?';
        if (stricmp(pos[1],"octet") == 0)
        packet->rrq.type_trf = 'o';
        if (stricmp(pos[1],"netascii") == 0)
        packet->rrq.type_trf = 'n';
        if (stricmp(pos[1],"mail") == 0)
        packet->rrq.type_trf = 'm';
        if (packet->rrq.type_trf == '?')
          {
            printf("Fatal error unknown transfert type : %s\n\r",pos[1]);
            exit(-1);
          }
        packet->rrq.opt      = FALSE;    /* tftp without option set by default */
        packet->rrq.tsize   = -1;
        packet->rrq.blksize = -1;
        for(i=2;i<nbr;i+=2)
          {
            if (stricmp(pos[i],"tsize") == 0)
              {
                packet->rrq.tsize  = atol(pos[i+1]);
                packet->rrq.opt    = TRUE;
              }

           if (stricmp(pos[i],"blksize") == 0)
             {
               packet->rrq.blksize = atol(pos[i+1]);
               packet->rrq.opt    = TRUE;
             }
          }

        break;
   case ACK:
        break;
   case ERROR:
        packet->err.rc = raw[0]*256+raw[1];
        strcpy(packet->err.msger,&raw[2]);
        break;
  }
 return 0;
}

void break_handler( int sig_nummer )
{
   printf("TFTPD stopped succesfully\n\r");
   exit(0);
}

int main(int argc,char **argv)
{
  int                sockint;
  int                s;
  struct sockaddr_in server;
  struct sockaddr_in client;
  struct servent     *tftpd_prot;
  int                client_address_size;
  char               buf[2048];
  int                nbr_lu;
  int                nbr_ecrit;
  PACKET             packet;
  struct stat        s_stat;
  char               path[256];
  FILE               *fichin;
  int                i;

  signal(SIGINT,break_handler);
  printf("**********************************************\n\r");
  printf("*      IBM TCP/IP for OS/2                   *\n\r");
  printf("*      Advanced TFTP Server (TFTPD)          *\n\r");
  printf("*      Support blksize,tsize options         *\n\r");
  printf("*      Version: %s %s         *\n\r",__DATE__,__TIME__);
  printf("*      (C) Copyright Serge Sterck 2002       *\n\r");
  printf("**********************************************\n\r");
  printf("\n\r");
  verbose = FALSE;
  path_prefix[0] = '\0';
  if (argc > 1)
    {
      for(i=1;i<argc;i++)
       {
          if (stricmp(argv[i],"-v") == 0)
             verbose = TRUE;
          else strcpy(path_prefix,argv[1]);
       }
    }
  if ((sockint = sock_init()) != 0)
    {
       printf(" INET.SYS probably is not running");
       exit(1);
    }
loop_bind:
  if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        psock_errno("socket()");
        exit(1);
    }

  tftpd_prot = getservbyname("tftp", "udp");
  if (tftpd_prot == NULL)
    {
      printf("The tftpd/udp protocol is not listed in the etc/services file\n");
      exit(1);
    }
  server.sin_family      = AF_INET;
  server.sin_port        = tftpd_prot->s_port;
  server.sin_addr.s_addr = INADDR_ANY;

  if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
      psock_errno("bind()");
      exit(2);
    }
  client_address_size = sizeof(client);
loop:
  nbr_lu = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *) &client,&client_address_size);
  if (nbr_lu < 0)
   {
       psock_errno("recvfrom()");
       exit(4);
   }
  decode(buf[1],&buf[2],nbr_lu,&packet);
loop_req:
  switch( buf[1] )
   {
     case RRQ:
          if (verbose)
            printf("<< %15s :Read %s\n\r",inet_ntoa(client.sin_addr),packet.rrq.filename);
          printf("Download file : %s\n\r",packet.rrq.filename);
          if (packet.rrq.opt == TRUE)     /* we receive a read with a option */
            {
              if (packet.rrq.tsize == 0)  /* client ask for the filesize     */
                {
                  sprintf(path,"%s\\%s",path_prefix,packet.rrq.filename);
                  if (stat(path,&s_stat) != 0)
                    {
                      printf("\n\rcannot stat %s\n\r",path);
                      buf[0] = 0;
                      buf[1] = ERROR;
                      buf[2] = 0;
                      buf[3] = 0;
                      sprintf(&buf[4],"Cannot stat %s",path);
                      nbr_ecrit = sendto(s, buf, 4+strlen(&buf[4])+1, 0, (struct sockaddr *) &client,client_address_size);
                      if (nbr_ecrit < 0)
                        {
                          psock_errno("sendto()");
                          exit(4);
                        }
                      goto loop;
                    }
                  if (verbose)
                    printf(">> %15s :Oack size of %s = %d\n\r",inet_ntoa(server.sin_addr),packet.rrq.filename,s_stat.st_size);
                  buf[0] = 0;
                  buf[1] = OACK;
                  strcpy(&buf[2],"tsize");
                  buf[7] = '\0';
                  sprintf(&buf[8],"%d",s_stat.st_size);
                  nbr_ecrit = sendto(s, buf, 8+strlen(&buf[8])+1, 0, (struct sockaddr *) &client,client_address_size);
                  if (nbr_ecrit < 0)
                    {
                      psock_errno("sendto()");
                      exit(4);
                     }
                }
              if (packet.rrq.blksize != -1)  /* Initiate read with blkize       */
                   default_blksize = packet.rrq.blksize;
              else default_blksize = 512;
              if (verbose)
                printf("                   :blksize set to :%d\n\r",default_blksize);
            }
          else
            {
              buf[0] = 0;      /* send ack block = 0 */
              buf[1] = ACK;
              buf[2] = 0;
              buf[3] = 0;
              nbr_ecrit = sendto(s, buf, 4, 0, (struct sockaddr *) &client,client_address_size);
              if (nbr_ecrit < 0)
                {
                  psock_errno("sendto()");
                  exit(4);
                }
            }
          sprintf(path,"%s\\%s",path_prefix,packet.rrq.filename);
          fichin    = fopen(path,"rb");
          num_block = 1;
          buf[1] = DATA;
          goto loop_req;
          break;
     case DATA:
          buf[0] = 0;
          buf[1] = DATA;
          buf[2] = num_block/256;
          buf[3] = num_block % 256;
          nbr_lu = fread(&buf[4],1,default_blksize,fichin);
          if (verbose)
            printf(">> %15s :Data block :%d\n\r",inet_ntoa(server.sin_addr),num_block);
          if (nbr_lu == default_blksize)
               nbr_ecrit = sendto(s, buf, 4+default_blksize, 0, (struct sockaddr *) &client,client_address_size);
          else
              {
                nbr_ecrit = sendto(s, buf, 4+nbr_lu, 0, (struct sockaddr *) &client,client_address_size);
                fclose(fichin);
                soclose(s);
                goto loop_bind;
              }
          if (nbr_ecrit < 0)
            {
              psock_errno("sendto()");
              exit(4);
            }
          break;
     case ACK:
          if (verbose)
            printf("<< %15s :Ack block :%d \n\r",inet_ntoa(client.sin_addr),buf[2]*256+buf[3]);
          if (buf[2] == 0 && buf[3] == 0)
            goto loop;    /* we ack the block 0 */
          buf[1] = DATA;
          num_block++;
          goto loop_req;
     case ERROR:
          if (verbose)
            printf("<< %15s :Error rc = %d msger = %s\n\r",inet_ntoa(client.sin_addr),packet.err.rc,packet.err.msger);
          else printf("                Error --> %s\n\r",packet.err.msger);
          default_blksize = 512;
          fclose(fichin);
          break;
   }
  goto loop;
  soclose(s);
  return 0;
}
