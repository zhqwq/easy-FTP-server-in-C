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
   
    if((result = recv(sock, buf, sizeof(buf), 0)) > 0 ){
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
            send(sock, tip230, sizeof(tip230), 0); //print: 220 Welcome to Easy FTP server. Please enter username.
            if(recv(sock, buf, sizeof(buf), 0) > 0) //接受SYST
                send(sock, tip215, sizeof(tip215), 0); //print: 215 UNIX Type: L8\r\n
        }else{
            send(sock, tip530, sizeof(tip530), 0); //print: 530 Login incorrect
            send(sock, tip, sizeof(tip), 0); //print: Login failed
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
    
    strcpy(buf,getcwd(NULL,0));
    strcat(buf,path);
    if(mkdir(buf, S_IRWXU)==0){
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
    char tip257[256] = "257 ";
    
    strcpy(buf,getcwd(NULL,0));
    strcat(buf,path);
    if(mkdir(buf, S_IRWXU)==0){
        strcat(tip257,buf);
        strcat(tip257," created \n");
        send(sock, tip257,strlen(tip257), 0);
    }else{
        send(sock, tip550,sizeof(tip550), 0);
    }
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
    while(1){
        memset(buf, '\0', sizeof(buf));
        result = recv(clt_sock, buf, sizeof(buf), 0); //example: PORT 127,0,0,1,249,108 所以 port = 249*256 + 108
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
    }
    
    close(clt_sock);
}