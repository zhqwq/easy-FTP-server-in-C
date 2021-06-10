#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h> //for atoi()
#include <string.h>
#include <unistd.h>
#include <shadow.h> // for getspnam()
#include <errno.h>

int socket_create(int port){
    int sockfd;
    int yes = 1;
    struct sockaddr_in sock_addr;

    //socket(domain, type, protocol) AF_INET:ipv4, SOCK_STREAM:TCP, 0:IP
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0))<0){
        perror("socket() error");
        return -1;
    }

    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port); 
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY); //localhost
    
    // reuse address and port. Prevents error such as: “address already in use”.
    // SO - Socket, SOL - Socket Level
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
    {
        close(sockfd);
        perror("setsockopt() error");
        return -1; 
    }

    // 绑定本地套接字地址到套接字
    if (bind(sockfd, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) < 0) 
    {
        close(sockfd);
        perror("bind() error"); 
        return -1; 
    }     

    // 将套接字设置为监听状态(listen() puts server socket in passive mode)
    int backlog = 5; //max length to pend connection for socket to grow.
    if (listen(sockfd, backlog) < 0) 
    {
        close(sockfd);
        perror("listen() error");
        return -1;
    }

    return sockfd;
}

int socket_accept(int sock){
    int sockfd;
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    if((sockfd = accept(sock, (struct sockaddr *)&client_addr, &len))<0){
        perror("accept() error");
        return -1;
    }
    printf("Accept client %s on TCP Port %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    return sockfd;
}

void auth(int sock){
    struct spwd *sp;
    int result;
    char buf[256];
    memset(buf,'\0',sizeof(buf)); //填充'0'结束符 防止printf输出奇奇怪怪的东西

    //服务器向Client发送响应码
    char tip[] = "Login failed\n";
    char tip220[] = "220 Welcome to Easy FTP server. Please enter username.\n"; //220 status code
    char tip331[] = "331 Please specify the password\n"; //要求密码
    char tip230[] = "230 Login successful\n"; //成功登录
    char tip530[] = "530 Login incorrect\n"; // 登陆错误
    char tip215[] = "215 UNIX Type: L8\r\n"; //系统类型
    
    send(sock, tip220, sizeof(tip220), 0); //print:220 Please enter username
    
    //验证Linux用户与密码
   
    if ( (result = recv(sock, buf, sizeof(buf), 0)) > 0 ){
        buf[result-2] = '\0'; // 替换末尾 /r/n 为\0\n 使得字符串正确结尾
        printf("Receive username: %s\n", buf+5);
        if((sp = getspnam(buf+5)) == NULL){
            send(sock, tip530, sizeof(tip530), 0); //print: Login incorrect
            send(sock, tip, sizeof(tip), 0); //print: Login failed
            close(sock);
        }
        send(sock, tip331, sizeof(tip331), 0); //print: 331 Please specify the password
        result = recv(sock, buf, sizeof(buf), 0);
        buf[result-2] = '\0';
        if(strcmp(sp->sp_pwdp, (char*)crypt(buf+5, sp->sp_pwdp))==0){
            send(sock, tip230, sizeof(tip230), 0);
            if(recv(sock, buf, sizeof(buf), 0) > 0) //接受SYST
                send(sock, tip215, sizeof(tip215), 0); 
        }else{
            send(sock, tip530, sizeof(tip530), 0);
            send(sock, tip, sizeof(tip), 0);
            close(sock); 
        }
    }
    else if ( result == 0 ){
        printf("Connection closed\n");
        close(sock); 
    }
    else{
        printf("recv failed: \n");
        close(sock);
    }
    
}

void ser_pwd(int sock){
    char buf[256];
    memset(buf, '\0', sizeof(buf));
    char a[256];
    memset(a, '\0', sizeof(a));
    strcpy(a,"257 The current diretory is "); // a = "257 The current diretory is "
    getcwd(buf, sizeof(buf));                 // buf = "/mnt/c/Users/ASUS/Desktop/ftp"
    strcat(a,buf);                            // a = "257 The current diretory is /mnt/c/Users/ASUS/Desktop/ftp"
    strcat(a,"\n");                           
    send(sock, a, sizeof(a), 0);
}

void ser_cwd(int sock, char* path){
    char tip250[] = "250 Directory successfully changed\n";
    char tip550[] = "550 Failed to change directory!\n";
    if(chdir(path) >= 0) //更换目录成功
        send(sock, tip250, sizeof(tip250),0);
    else{
        send(sock, tip550, sizeof(tip550),0);
        perror("550");
    }
}

void ser_mkd(int sock, char* path){
    char buf[256];
    memset(buf, '\0', sizeof(buf));
    char tip550[256] = "550 Create directory operation failed.\n";
    char tip257[256] = "257  ";
    
    strcpy(buf,getcwd(NULL,0));//buf = work dir
    strcat(buf,"/");
    strcat(buf,path);
    if(mkdir(path, S_IRWXU)==0){
        strcat(tip257,buf);
        strcat(tip257," created \n");
        send(sock, tip257,strlen(tip257), 0);
    }else{
        send(sock, tip550,sizeof(tip550), 0);
    }
}

void ser_del(int sock, char* path){
    char tip550[256] = "550 Delete operation failed.\n";
    char tip250[256] = "250 Delete operation successful.\n";
    
    if(remove(path)==0){
        send(sock, tip250,strlen(tip250), 0);
    }else{
        send(sock, tip550,sizeof(tip550), 0);
    }
}

void ser_rnfr(int sock, char* name){
    char buf[256];
    memset(buf,'\0',sizeof(buf));
    FILE *fp;
    if((fp=fopen(name,"r"))==NULL){
        strcpy(buf,"550 Failed to rename.\n");
        send(sock, buf, sizeof(buf),0);
    } 
    else{
        strcpy(buf,"350 Ready for RNTO.\n");
        send(sock, buf, sizeof(buf),0);
    }
}

void ser_rnto(int sock, char* oldname, char* newname){
    char buf[256];
    if(rename(oldname,newname)==0){
        strcpy(buf, "250 Rename successful.\n");
        send(sock, buf, strlen(buf), 0);
    }else{
        strcpy(buf,"550 Failed to rename\n");
        send(sock, buf, strlen(buf), 0);
    }
}

int data_connect(int sock, int port){
    printf("进行Active数据连接\n");
    int data_sock;
    char buf[256];
    memset(buf, 0, sizeof(buf));
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    { 
            perror("error: creating socket.\n");
            return -1;
    }

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(data_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0 )
    {
        perror("error: connecting to server.\n");
        return -1;
    }

    return data_sock;
}

int ser_port(int sock, char* addr){
    char buf[256];
    memset(buf,'\0',sizeof(buf));
    printf("准备连接客户端端口\n");
    int address[7];
    memset(address,'\0',7);
    char* token = strtok(addr, ",");
    address[0] = atoi(token);
    int i = 1;
    while (token!=NULL && i<=5)
    {
        token = strtok(NULL,",");
        address[i] = atoi(token);
        i++;
    }
    
    int port = address[4]*256 + address[5];
    int data_sock = data_connect(sock, port);
    printf("最后两位数%d,%d,新端口号为%d\n", address[4],address[5],port);
    strcpy(buf, "200 PORT command successful.\n");
    send(sock, buf, sizeof(buf), 0);
    return data_sock;
}

void ser_ls(int clt_sock, int data_sock){
    char tip150[256] = "150 Here comes the directory listing.\n";
    char tip226[256] = "226 Directory send OK.\n";
    char data[256];
    size_t num_read;    
    memset(data,'\0', sizeof(data));
    system("ls -l | tail -n+2 > ls.txt");
    send(clt_sock,tip150,sizeof(tip150),0); //send 150 to clitent
    FILE* fd = fopen("ls.txt","r");
    fseek(fd, SEEK_SET, 0);

    /* 通过数据连接，发送ls.txt 文件的内容 */
    while ((num_read = fread(data, 1, 256, fd)) > 0) 
    {
        if (send(data_sock, data, num_read, 0) < 0) 
            perror("err");
        memset(data, 0, 256);
    }
    fclose(fd);
    close(data_sock);
    send(clt_sock, tip226,sizeof(tip226),0);    // 发送应答码 226（关闭数据连接，请求的文件操作成功）  

}

void ser_get(int clt_sock, int date_sock, char* name){
    char tip150[256] = "150 Opening ASCII mode data connection for ";
    strcat(tip150, name);
    //150 Opening BINARY mode data connection for file.hole (50 bytes).
    char tip226[256] = "Transfer complete";
}

int main(int argc,char *argv[]){
    //initial
    int port = 21; // port 21 for FTP
    int ser_sock = socket_create(port); // socket() - setsockopt() - bind() - lienten()
    int clt_sock = socket_accept(ser_sock); // accept()
    char tip221[] = "221 GoodBye!\n";

    //authentication
    auth(clt_sock);

    //循环等待命令
    int result;
    char buf[256];
    char oldname[256];
    int data_sock;
    while(1){
        memset(buf, '\0', sizeof(buf));
        result = recv(clt_sock, buf, sizeof(buf), 0); 
        if(buf!=NULL)
            printf("recv:%s\n",buf);
        if(strcmp(buf,"QUIT\r\n")==0){
            send(clt_sock, tip221, sizeof(tip221), 0);
            break;
        }
        if(strcmp(buf,"PWD\r\n")==0){
            ser_pwd(clt_sock);
        }
        if(strstr(buf, "CWD")!=NULL){ // 不严谨但省事
            buf[result - 2] = '\0';
            ser_cwd(clt_sock, buf + 4);
        }
        if(strstr(buf, "MKD")!=NULL){
            buf[result - 2] = '\0';
            ser_mkd(clt_sock, buf + 4);
        }
        if(strstr(buf, "DELE")!=NULL){
            buf[result - 2] = '\0';
            ser_del(clt_sock, buf + 5);
        }
        if(strstr(buf, "RNFR")!=NULL){
            //RNFR a.c
            //350 Ready for RNTO
            //RNTO b.c
            //250 Rename successful
            buf[result - 2] = '\0';
            strcpy(oldname,buf + 5);
            ser_rnfr(clt_sock, buf + 5);
        }
        if(strstr(buf, "RNTO")!=NULL){
            buf[result - 2] = '\0';
            ser_rnto(clt_sock, oldname, buf + 5);
        }
        if(strstr(buf, "PORT")!=NULL){
            buf[result - 2] = '\0';
            printf("%s\n",buf+5);
            data_sock = ser_port(clt_sock, buf+5);

            //PORT 127,0,0,1,249,108  (port = 249*256 + 108)
            //200 PORT command successful
            //LIST
            //150 Here comes the directory listing.
            //data
            //226 Directory send OK.
        }
        if(strstr(buf, "LIST")!=NULL){
            ser_ls(clt_sock, data_sock);
        }

        if(strstr(buf, "RETR")!=NULL){
            ser_get(clt_sock, data_sock, buf + 4);
        }
    }
    
    close(clt_sock);
}