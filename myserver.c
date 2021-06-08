#include <stdio.h>
#include <sys/socket.h>
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
    return sockfd;
}

void auth(int sock){
    struct spwd *sp;
    int result;
    char buf[256];
    memset(buf,'\0',sizeof(buf)); //填充'0'结束符 防止printf输出奇奇怪怪的东西
    char tip[] = "Login failed";
    char tip220[] = "220 Welcome to Easy FTP server. Please enter username.\n"; //220 status code
    char tip331[] = "331 Please specify the password\n";
    char tip230[] = "230 Login successful\n";
    char tip530[] = "530 Login incorrect\n";
    
    send(sock, tip220, sizeof(tip220), 0);

    do{
        result = recv(sock, buf, sizeof(buf), 0);
        if ( result > 0 ){
            printf("Bytes received: %d\n", result);//12
            printf("buf:%s",buf+5); //USER zhqwq/r/n
            buf[result-2] = '\0';

            if((sp = getspnam(buf+5)) == NULL){// 用户名错误
                send(sock, tip530, sizeof(tip530), 0);
                send(sock, tip, sizeof(tip), 0);
                break;
            }else{ //用户名输入正确，提示输入密码
                send(sock, tip331, sizeof(tip331), 0);
            }
            result = recv(sock, buf, sizeof(buf), 0);
            buf[result-2] = '\0';
            printf("Bytes received: %d\n", result); //17
            printf("buf:%s",buf); //PASS Zhang12345
            if(strcmp(sp->sp_pwdp, (char*)crypt(buf+5, sp->sp_pwdp))==0){
                send(sock, tip230, sizeof(tip230), 0);
                break;
            }else{
                send(sock, tip530, sizeof(tip530), 0);
                send(sock, tip, sizeof(tip), 0);
                break; 
            }
        }
        else if ( result == 0 )
            printf("Connection closed\n");
        else
            printf("recv failed: \n");
    }while(result > 0);
    
}

int main(int argc,char *argv[]){
    //initial
    int port = 21; // port 21 for FTP
    int ser_sock = socket_create(port); // socket() - setsockopt() - bind() - lienten()
    int clt_sock = socket_accept(ser_sock); // accept()

    //authentication
    auth(clt_sock);
    close(clt_sock);
}