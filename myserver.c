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
#include <time.h> // for srand()
#include <sys/time.h>

/**
 * @brief A basic FTP Server System
 * Author: Kaiwei Zhang, Xiao Zheng
 * Function Established: AUTH, PWD, CWD, MKD, DELE, LIST, PORT, STOR, RETR, RNFR, RNTO, QUIT, PASV
 * !Bug exists: In passive mode, ls cannot be executed twice, and put and set cannot be executed either.
 * 
 * Should be compiled in LINUX system.
 * Compile command: gcc myserver.c -o myserver -lcrypt
 * Execute command: sudo myserver
 * 
 * You also need another terminal of the same host to open a ftp client(using system ftp client)
 * Execute command: ftp 127.0.0.1 (loop IP, the server can only interact with the same host, as it is defined in the program)
 */

char* username;
char namebuf[256];
char dir[256];
int ser_sock2;

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

    // bind socket with local address
    if (bind(sockfd, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) < 0) 
    {
        close(sockfd);
        perror("bind() error"); 
        return -1; 
    }     

    // (listen() puts server socket in passive mode)
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
    printf("Accept client %s on TCP Port %d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    return sockfd;
}

void auth(int sock){
    struct spwd *sp;
    int result;
    char buf[256];
    memset(buf,'\0',sizeof(buf)); 

    char tip[] = "Login failed\n";
    char tip220[] = "220 Welcome to Easy FTP server. Please enter username.\n"; //220 status code
    char tip331[] = "331 Please specify the password.\n"; 
    char tip230[] = "230 Login successful.\n"; 
    char tip530[] = "530 Login incorrect.\n"; 
    char tip215[] = "215 UNIX Type: L8.\r\n"; //L8: system type
    
    send(sock, tip220, sizeof(tip220)- 1, 0); //print:220 Please enter username
    
    //验证Linux用户与密码
   
    if ( (result = recv(sock, namebuf, sizeof(namebuf), 0)) > 0 ){
        namebuf[result-2] = '\0'; 
        printf("Receive username: %s\n\n", namebuf+5);
        username = namebuf+5;
        if((sp = getspnam(username)) == NULL){
            send(sock, tip530, sizeof(tip530) - 1, 0); //print: Login incorrect
            send(sock, tip, sizeof(tip), 0); //print: Login failed
            printf("Username Authentication Failed. Program exits.\n");
            exit(1);
            close(sock);
        }
        
        send(sock, tip331, sizeof(tip331) - 1, 0); //print: 331 Please specify the password
        result = recv(sock, buf, sizeof(buf), 0);
        buf[result-2] = '\0';
        if(strcmp(sp->sp_pwdp, (char*)crypt(buf+5, sp->sp_pwdp))==0){
            send(sock, tip230, sizeof(tip230) - 1, 0); //230 Login successful
            if(recv(sock, buf, sizeof(buf), 0) > 0) //recv SYST
                send(sock, tip215, sizeof(tip215) - 1, 0); //215 UNIX Type: L8\r\n
            //get current directory
            memset(dir, '\0', sizeof(dir));
            getcwd(dir, sizeof(dir));                 // dir = "/mnt/c/Users/ASUS/Desktop/ftp"
        }else{
            send(sock, tip530, sizeof(tip530) - 1, 0); //Login incorrect
            send(sock, tip, sizeof(tip), 0);//Login failed
            printf("Password Authentication Failed. Program exits.\n");
            exit(1);
            close(sock); 
        }
    }
    else if ( result == 0 ){
        printf("Connection closed\n");
        exit(1);
        close(sock); 
    }
    else{
        printf("recv() failed\n");
        exit(1);
        close(sock);
    }
    
}

void ser_pwd(int sock){
    printf("%s\t%s\t execute PWD\n", username, dir);
    char buf[256];
    memset(buf, '\0', sizeof(buf));
    char a[256];
    memset(a, '\0', sizeof(a));
    strcpy(a,"257 The current diretory is "); // a = "257 The current diretory is "
    getcwd(buf, sizeof(buf));                 // buf = "/mnt/c/Users/ASUS/Desktop/ftp"
    strcat(a,buf);                            // a = "257 The current diretory is /mnt/c/Users/ASUS/Desktop/ftp"
    strcat(a,"\n");                           
    send(sock, a, sizeof(a), 0);
    printf("%s\t%s\t execute PWD successfully\n\n", username, dir);
}

void ser_cwd(int sock, char* path){
    printf("%s\t%s\t execute CWD %s\n", username, dir, path);
    char tip250[] = "250 Directory successfully changed.\n";
    char tip550[] = "550 Failed to change directory!\n";
    if(chdir(path) >= 0) {
        //更换目录成功
        send(sock, tip250, sizeof(tip250) - 1,0);
        memset(dir, '\0', sizeof(dir));
        getcwd(dir, sizeof(dir));                
    }
    else{
        send(sock, tip550, sizeof(tip550) - 1,0);
    }
    
    printf("%s\t%s\t execute CWD %s successfully\n\n", username, dir, path);
}

void ser_mkd(int sock, char* path){
    printf("%s\t%s\t execute MKD %s\n", username, dir, path);
    char buf[256];
    memset(buf, '\0', sizeof(buf));
    char tip550[256] = "550 Create directory operation failed.\n";
    char tip257[256] = "257  ";
    
    strcpy(buf,getcwd(NULL,0));//buf = work dir
    strcat(buf,"/");
    strcat(buf,path);
    if(mkdir(path, S_IRWXU)==0){
        strcat(tip257,buf);
        strcat(tip257," created.\n");
        send(sock, tip257,strlen(tip257), 0);
        printf("%s\t%s\t execute MKD %s successfully\n\n", username, dir, path);
    }else{
        send(sock, tip550,sizeof(tip550) - 1, 0);
    }
}

void ser_del(int sock, char* path){
    printf("%s\t%s\t execute DEL %s\n", username, dir, path);
    char tip550[256] = "550 Delete operation failed.\n";
    char tip250[256] = "250 Delete operation successful.\n";
    
    if(remove(path)==0){
        send(sock, tip250,strlen(tip250) - 1, 0);
    }else{

        send(sock, tip550,sizeof(tip550) - 1, 0);
        printf("%s\t%s\t execute DEL %s successfully\n\n", username, dir, path);
    }
}

void ser_rnfr(int sock, char* name){
    //RNFR a.c
    //350 Ready for RNTO
    //RNTO b.c
    //250 Rename successful
    printf("%s\t%s\t execute RNFR %s\n", username, dir, name);
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
        printf("%s\t%s\t execute RNFR %s successfully\n\n", username, dir, name);
    }
}

void ser_rnto(int sock, char* oldname, char* newname){
    printf("%s\t%s\t execute RNTO %s %s\n", username, dir, oldname, newname);
    char buf[256];
    if(rename(oldname,newname)==0){
        strcpy(buf, "250 Rename successful.\n");
        send(sock, buf, strlen(buf), 0);
        printf("%s\t%s\t execute RNTO %s %s successfully\n\n", username, dir, oldname, newname);
    }else{
        strcpy(buf,"550 Failed to rename.\n");
        send(sock, buf, strlen(buf), 0);
    }
}

int data_connect(int sock, int port){
    int data_sock;
    struct sockaddr_in dest_addr;
    memset(&dest_addr, '\0', sizeof(dest_addr));

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    { 
        perror("error: creating socket.\n");
        return -1;
    }
    
    if(connect(data_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0 )
    {
        perror("error: connecting to server.\n");
        return -1;
    }

    return data_sock;
}

int ser_port(int sock, char* addr){
    printf("%s\t%s\t execute PORT\n", username, dir);
    char buf[256];
    memset(buf,'\0',sizeof(buf));
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
    printf("Active Mode On.\n");
    printf("Connect client %d.%d.%d.%d on TCP Port %d.\n", address[0], address[1], address[2], address[3], port);
    strcpy(buf, "200 PORT command successful. Consider using PASV.\n");
    send(sock, buf, sizeof(buf), 0);
    printf("%s\t%s\t execute PORT successfully\n\n", username, dir);
    return data_sock;
}

void ser_ls(int clt_sock, int data_sock){
    printf("%s\t%s\t execute LIST\n", username, dir);
    char tip150[256] = "150 Here comes the directory listing.\n";
    char tip226[256] = "226 Directory send OK.\n";
    char data[256];
    size_t num_read;    
    memset(data,'\0', sizeof(data));
    system("ls -l | tail -n+2 > ls.txt");
    send(clt_sock, tip150, sizeof(tip150) - 1,0); //send 150 to client
    FILE* fd = fopen("ls.txt","rb+");
    fseek(fd, SEEK_SET, 0);

    /* send ls.txt */
    while ((num_read = fread(data, 1, 256, fd)) > 0) 
    {
        if (send(data_sock, data, num_read, 0) < 0) 
            perror("ls error");
        memset(data, 0, 256);
    }
    fclose(fd);
    close(data_sock); 
    close(ser_sock2);
    send(clt_sock, tip226,sizeof(tip226) - 1,0);    // file send complete  
    printf("%s\t%s\t execute LIST successfully\n\n", username, dir);
}

void ser_type(int clt_sock, char* type){
    printf("%s\t%s\texecute TYPE %s\n", username, dir, type);
    char tip200a[256] = "200 Switching to ASCII mode.\n";
    char tip200b[256] = "200 Switching to Binary mode.\n";
    if(strcmp(type, "A") == 0)
        send(clt_sock, tip200a, sizeof(tip200a) - 1, 0);
    else if(strcmp(type, "I") == 0)
        send(clt_sock, tip200b, sizeof(tip200b) - 1, 0);
    else
        perror("TYPE Error");
    printf("%s\t%s\texecute TYPE %s successfully\n\n", username, dir, type);
}

void ser_retr(int clt_sock, int data_sock, char* filename){
    printf("%s\t%s\texecute RETR %s\n", username, dir, filename);
    char tip226[256] = "226 Transfer complete.\n";
    char tip550[256] = "550 Permission denied.\n";
    char tip150[256] = "150 Opening data connection for ";
   
    char buf[256];
    strcat(tip150, filename);
    FILE *in = fopen(filename, "r");
    fseek(in, 0, SEEK_END);
    long length = ftell(in);
    sprintf(buf,"%ld",length);
    rewind(in);
    fclose(in);
    strcat(tip150," (");
    strcat(tip150,buf);
    strcat(tip150," bytes).\n");
    FILE* fd = NULL;
    char data[256];
    size_t num_read;
    struct timeval start,end;

    if((fd = fopen(filename, "rb+"))==NULL){
        send(clt_sock,tip550,sizeof(tip550) - 1,0); //file not exist
    }else{
        send(clt_sock,tip150,sizeof(tip150) - 1,0);
        gettimeofday(&start, NULL);
        do{
            num_read = fread(data, 1, 256, fd);
            if (num_read < 0) 
                perror("error in fread().\n");

            if (send(data_sock, data, num_read, 0) < 0) // send data
                perror("error sending file.\n");
        }while(num_read > 0);
        send(clt_sock, tip226, sizeof(tip226) - 1,0); // transmission complete
        gettimeofday(&end, NULL);
        long timeuse =1000000 * ( end.tv_sec - start.tv_sec ) + end.tv_usec - start.tv_usec;
        float time = timeuse /1000000.0;
        printf("time = %f seconds\n",time);
                                        
        if( fseek(fd, 0 , SEEK_END) != 0)
               printf("fseek failed\n");
        int size = ftell(fd);
        printf("total traffic = %d bytes\n",size);
        printf("speed = %f kbps\n", (size/time)/1000.0);

        fclose(fd);
        close(data_sock);
        printf("%s\t%s\t execute RETR %s successfully\n\n", username, dir, filename);
    }
}

void ser_stor(int clt_sock, int data_sock, char* filename){
    printf("%s\t%s\texecute STOR\n", username, dir);
    char tip550[256] = "550 Requested action not taken.\n";
    char tip150[256] = "150 Ok to send data.\n";
    char tip226[256] = "226 Transfer complete.\n";

    FILE* fd = NULL;
    char data[256];
    memset(data,'\0',256);
    size_t num_write;
    struct timeval start,end;

    
    
    if((fd = fopen(filename, "wb+"))== NULL){
        send(clt_sock,tip550,sizeof(tip550) - 1,0); // file open failed
    }else{
        send(clt_sock,tip150,sizeof(tip150) - 1,0);
        gettimeofday(&start, NULL);
        while (recv(data_sock, data, sizeof(data),0)>0)
        {
            if(num_write = fwrite(data, 1, strlen(data), fd) < 0)
                perror("fwrite() error.\n");
        }
        send(clt_sock, tip226, sizeof(tip226) - 1, 0);
        gettimeofday(&end, NULL);
        long timeuse =1000000 * ( end.tv_sec - start.tv_sec ) + end.tv_usec - start.tv_usec;
        float time = timeuse /1000000.0;
        printf("time = %f seconds\n",time);
                                        
        if( fseek(fd, 0 , SEEK_END) != 0)
               printf("fseek failed\n");
        int size = ftell(fd);
        printf("total traffic = %d bytes\n",size);
        printf("speed = %f kbps\n", (size/time)/1000.0);
    }

    
    fclose(fd);
    close(data_sock);
    printf("%s\t%s\texecute STOR successfully\n", username, dir);
}

int ser_pasv(int clt_sock){
    printf("%s\t%s\texecute PASV\n", username, dir);
    char tip227[256] = "227 Entering Passive Mode (127,0,0,1,%d,%d).\n";
    char buff[256];
    int portmin = 10000;
    int portmax = 20000;
    srand((unsigned int)time(0));
    int randport = rand() % (portmax - portmin + 1) + portmin;

    ser_sock2 = socket_create(randport);

    printf("Passive Mode On.\n");
    sprintf(buff, tip227, randport / 256, randport % 256);

    send(clt_sock, buff, strlen(buff), 0);
    printf("Connect client 127.0.0.1 on TCP Port %d.\n", randport);
    
    printf("%s\n",buff);

    int clt_data_sock = socket_accept(ser_sock2); 
    
    printf("%s\t%s\texecute PASV successfully\n\n", username, dir);
    return clt_data_sock;
}

int ser_pasv(int clt_sock){
    printf("进入到ser_pasv函数\n");
    char tip227[256] = "227 Entering Passive Mode (127,0,0,1,161,147).\n";
    int ser_data_sock = socket_create(41363);
    send(clt_sock, tip227, strlen(tip227), 0);

    int clt_data_sock = socket_accept(ser_data_sock); 
    return clt_data_sock;
    
}

int main(int argc,char *argv[]){
    // initialization
    int port = 21; // port 21 for FTP
    int ser_sock = socket_create(port); // socket() - setsockopt() - bind() - lienten()
    int clt_sock = socket_accept(ser_sock); // accept()
    char tip221[] = "221 GoodBye!\n";

    //authentication
    auth(clt_sock);

    //continuously wait
    int result;
    char buf[256];
    char oldname[256];
    int data_sock;
    int mode; // 0 - active , 1 - passive
    while(1){
        memset(buf, '\0', sizeof(buf));
        result = recv(clt_sock, buf, sizeof(buf), 0); 
        buf[result - 2] = '\0'; 
        if(buf!=NULL)
            printf("recv:%s\n",buf);
        if(strstr(buf,"QUIT")!=NULL){
            printf("%s\t%s\t execute QUIT\n", username, dir);
            send(clt_sock, tip221, sizeof(tip221) - 1, 0);
            printf("%s\t%s\t execute QUIT successfully\n", username, dir);
            break;
        }
        if(strstr(buf,"PWD")!=NULL){
            ser_pwd(clt_sock);
        }
        if(strstr(buf, "CWD")!=NULL){
            ser_cwd(clt_sock, buf + 4);
        }
        if(strstr(buf, "MKD")!=NULL){
            ser_mkd(clt_sock, buf + 4);
        }
        if(strstr(buf, "DELE")!=NULL){
            ser_del(clt_sock, buf + 5);
        }
        if(strstr(buf, "RNFR")!=NULL){
            strcpy(oldname,buf + 5);
            ser_rnfr(clt_sock, buf + 5);
        }
        if(strstr(buf, "RNTO")!=NULL){
           
            ser_rnto(clt_sock, oldname, buf + 5);
        }
        if(strstr(buf, "PORT")!=NULL){ // in active mode
            data_sock = ser_port(clt_sock, buf+5);
        }
        if(strstr(buf, "LIST")!=NULL){
            ser_ls(clt_sock, data_sock);
            
        }
        if(strstr(buf, "TYPE")!=NULL){
            ser_type(clt_sock, buf + 5);
        }
        if(strstr(buf, "RETR")!=NULL){ //RETR test.txt
            ser_retr(clt_sock, data_sock, buf + 5);
        }
        if(strstr(buf, "STOR")!=NULL){ //upload
            ser_stor(clt_sock, data_sock, buf + 5);
        }
        if(strstr(buf, "PASV")!=NULL){ // in passive mode
            data_sock = ser_pasv(clt_sock);
        }

    }
    
    close(clt_sock);
}