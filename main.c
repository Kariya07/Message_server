#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
typedef struct sockaddr_in Sockaddr_in;
typedef struct sockaddr Sockaddr;
int num_of_players = 2;
int inf = 1;
struct timeval time;
typedef struct id {
    int player_fd;
    char* buf;
    int freelen;
    int len;
    char* name;
    int nchapter;
} player_id;
typedef struct table{
    int current_num_of_players;
    player_id* names;
}players_list;
void int_list(players_list* a){
    (*a).names = (player_id*)malloc(sizeof(player_id)*(num_of_players+1));
    (*a).current_num_of_players = 0;
}
void int_id(player_id* a){
    (*a).buf=NULL;
    (*a).name=NULL;
    (*a).len=0;
    (*a).freelen=0;
    (*a).nchapter=0;
}
int expand_buf(char** a, int length){
    char* b = (char*)malloc(sizeof(char)*length*2);
    strcpy(b, *a);
    free(*a);
    *a = b;
    return length*2;
}
void freemem(player_id* a){
    if((*a).name!=NULL){
        free((*a).name);
        (*a).name=NULL;
    }
    if((*a).buf!=NULL){
        free((*a).buf);
        (*a).buf=NULL;
    }
}
int socket_create(int* fd, int port){
    Sockaddr_in addrl;
    addrl.sin_family = AF_INET;
    addrl.sin_port = htons(port);
    addrl.sin_addr.s_addr = htonl(INADDR_ANY);
    //создаем сокет
    if (((*fd) = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0){
        printf("Socket() error\n");
        return -1;
    };
    //связываем сокет с адресом
    if(bind(*fd, (Sockaddr *) &addrl, sizeof(addrl)) != 0){
        if(port < 1024){
            printf("Bind() error: Use of a privileged port number\n");
            return -1;
        }
        //порт уже кем-то используется, как проверить?
    }
    //делаем его слушающим
    if(listen(*fd,5) != 0){
        printf("No listening socket\n");
        return -1;
    }
    return 0;
}
void resort(int i, players_list* a){
    const char* byebye2 = " left the chat\n";
    for(int k=0;k<(*a).current_num_of_players;k++){
        if(k!=i && (*a).names[k].name!=NULL){
            write((*a).names[k].player_fd,(*a).names[i].name,strlen((*a).names[i].name));
            write((*a).names[k].player_fd,byebye2,strlen(byebye2));
        }
    }
    (*a).current_num_of_players--;
    int local = (*a).names[i].player_fd;
    freemem(&((*a).names[i]));
    for(int j=i; j<(*a).current_num_of_players; j++){
        (*a).names[j].name=(*a).names[j+1].name;
        (*a).names[j].len=(*a).names[j+1].len;
        (*a).names[j].freelen=(*a).names[j+1].freelen;
        (*a).names[j].player_fd=(*a).names[j+1].player_fd;
    }
    (*a).names[(*a).current_num_of_players].name=NULL;//двойной указатель - обязательно??
    (*a).names[(*a).current_num_of_players].buf=NULL;
    shutdown(local,2);
    close(local);
}
int premature_exit(int i, players_list* a, char* s){
    const char* exit = "bye";
    if(strcmp(exit, s)==0){
        free(s);
        printf("# user_%d left the chat\n",i+1);
        resort(i,a);
        return -1;
    }
    return 0;
}
int separation(int i, players_list* a, int readlen){
    char* trans=(char*)malloc((*a).names[i].len*sizeof(char));
    trans[0]='\0';
    int cur_readlen=readlen;
    for(int j=0; j<readlen; j++){
        if((*a).names[i].buf[j]=='\n'){
            const char* mes = ": ";
            const char* sep2="\n";
            if((*a).names[i].name!=NULL){
                for(int k=0;k<(*a).current_num_of_players;k++){
                    if(k!=i && (*a).names[k].name!=NULL){
                        write((*a).names[k].player_fd,(*a).names[i].name,strlen((*a).names[i].name));
                        write((*a).names[k].player_fd,mes,strlen(mes));
                        write((*a).names[k].player_fd,trans,j*sizeof(char));
                        write((*a).names[k].player_fd,sep2,strlen(sep2));
                    }
                }
                printf("[%s]: %s\n",(*a).names[i].name,trans);
            }
            trans[j-1]='\0';//вместо символа переноса каретки
            if(premature_exit(i,a,trans)==-1){
                return -2;
            }
            free(trans);
            if(j!=cur_readlen-1){
                trans = (char*)malloc(sizeof((*a).names[i].buf));
                trans[0]='\0';
                for(int k=0;k<cur_readlen-j;k++){
                    (*a).names[i].buf[k]=(*a).names[i].buf[j+1+k];
                    (*a).names[i].buf[k+1]='\0';
                }
                cur_readlen=cur_readlen-(j+1);
            }else{
                (*a).names[i].buf[cur_readlen-2] = '\0';//вместо перевода строки и переноса каретки запись конца строки
                (*a).names[i].nchapter=(*a).names[i].nchapter+(cur_readlen-2);
                return 0;
            }
        }else{
            if(j!=cur_readlen-1){
                trans[j]=(*a).names[i].buf[j];
                trans[j+1]='\0';
            }else{//если не заканчивается переводом строки
                free(trans);
                (*a).names[i].freelen=(*a).names[i].freelen-cur_readlen;
                if((*a).names[i].freelen==0){
                    (*a).names[i].len=expand_buf(&((*a).names[i].buf),(*a).names[i].len);
                }
                (*a).names[i].buf[j]='\0';
                (*a).names[i].nchapter=(*a).names[i].nchapter+cur_readlen;
                return cur_readlen-1;//индекс последней буквы
            }
        }
    }
}
int reading(int i, players_list* a){
    int readlen,ind=0;
    char* prebuf = (char*)malloc((*a).names[i].len);
    if((*a).names[i].buf!=NULL){
        readlen=read((*a).names[i].player_fd,prebuf,(*a).names[i].len);
        while((*a).names[i].buf[ind]!='\0'){ind++;}
        for(int k=0;k<readlen;k++){
            (*a).names[i].buf[k+ind]=prebuf[k];
        }
        free(prebuf);
    }
    else{
        (*a).names[i].buf = (char*)malloc(64*sizeof(char));
        (*a).names[i].len=64*sizeof(char);
        readlen=read((*a).names[i].player_fd,(*a).names[i].buf,(*a).names[i].len);
    }
    if(readlen == -1){
        free(prebuf);
        freemem(&((*a).names[i]));
        printf("Read() error\n");
        return -1;
    }
    if(readlen == 0){
        free(prebuf);
        resort(i,a);
        return -2;
    }
    free(prebuf);
    return separation(i,a,readlen);
}
int enter_name(int i, players_list* a){
    int cmp=1;
    int local_fd = (*a).names[i].player_fd;
    //вместе с признаком конца строки
    int readlen=reading(i,a);
    if(readlen==-1){
        free((*a).names[i].buf);
        (*a).names[i].buf=NULL;
        return -1;
    }
    if(readlen==-2){
        return -1;
    }
    if(readlen>0){//не дочитали до конца
        return -2;
    }
    if((*a).names[i].nchapter >= 3 && (*a).names[i].nchapter <= 16){
        for(int j=0;j<(*a).current_num_of_players;j++){
            if(j!=i && (*a).names[j].name!=NULL){
                cmp=strcmp((*a).names[j].name,(*a).names[i].buf);
            }
            if(cmp==0){
                break;
            }
        }
        if(cmp==0){
            (*a).names[i].nchapter=0;
            const char* answer2="# such name already exist\n";
            write(local_fd, answer2, strlen(answer2)+1);
            free((*a).names[i].buf);
            (*a).names[i].buf=NULL;
            return -1;
        }else{
            (*a).names[i].name=(*a).names[i].buf;
            (*a).names[i].buf=NULL;
        }
    }else{
        (*a).names[i].nchapter=0;
        const char* answer2="# invalid name\n";
        write(local_fd, answer2, strlen(answer2)+1);
        free((*a).names[i].buf);
        (*a).names[i].buf=NULL;
        return -1;
    }
    return 0;
}
void talking(int fd){
    players_list list;
    int_list(&list);
    int i=0,max_fd=fd,local_fd,readlen;
    while(inf){
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd,&fds);
        i=0;
        while(i<list.current_num_of_players){
            local_fd = list.names[i].player_fd;
            FD_SET(local_fd, &fds);
            if(local_fd > max_fd){
                max_fd = local_fd;
            }
            i++;
        }
        int res = select(max_fd+1, &fds, NULL, NULL, &time);
        time.tv_sec=5;
        time.tv_usec=0;
        if(res==0){
            printf("# chat server is running...(%d users connected)\n",list.current_num_of_players);
        }
        if(res < 0){
            printf("Error in select\n");
        }
        if(FD_ISSET(fd,&fds)){
            if((list.names[list.current_num_of_players].player_fd = accept(fd, NULL, NULL)) < 0){
                printf("Error in accept\n");
            }else{
                if(list.current_num_of_players==num_of_players){
                    const char* reject = "# sorry, server is full...\n# try later again\n";
                    write(list.names[list.current_num_of_players].player_fd,reject,strlen(reject));
                    FD_CLR(list.names[list.current_num_of_players].player_fd,&fds);
                    shutdown(list.names[list.current_num_of_players].player_fd,2);
                    close(list.names[list.current_num_of_players].player_fd);
                }else{
                    const char* answer1="Please enter your name:\n";
                    write(list.names[list.current_num_of_players].player_fd, answer1, strlen(answer1)+1);
                    int_id(&(list.names[list.current_num_of_players]));
                    list.current_num_of_players++;
                    printf("# user_%d joined the chat\n",list.current_num_of_players);
                }
            }

        }
        i=0;
        while(i < list.current_num_of_players){
            if(FD_ISSET(list.names[i].player_fd,&fds)){
                if(list.names[i].name==NULL){
                    int err = enter_name(i,&list);
                    if(err==0){
                        list.names[i].nchapter=0;//для последующих сообщений
                        const char* welcome = "Welcome, ";
                        write(list.names[i].player_fd,welcome,strlen(welcome));
                        write(list.names[i].player_fd,list.names[i].name,strlen(list.names[i].name));
                        const char* sep0 = "!\n";
                        write(list.names[i].player_fd,sep0,strlen(sep0));
                        printf("# user_%d changed name to '%s'\n",i+1,list.names[i].name);
                        const char* sep1 = "# ";
                        const char* sep2=" joined the chat\n";
                        for(int k=0;k<list.current_num_of_players;k++){
                            if(k!=i && list.names[k].name!=NULL){
                                write(list.names[k].player_fd,sep1,strlen(sep1));
                                write(list.names[k].player_fd,list.names[i].name,strlen(list.names[i].name));
                                write(list.names[k].player_fd,sep2,strlen(sep2));
                            }
                        }
                    }else{
                        i++;
                        continue;
                    }
                }else{
                    readlen=reading(i,&list);
                    if(readlen==-2){//ошибка чтения
                        printf("# user_%d left the chat\n",i+1);
                        //FD_CLR(list.)
                    }
                    if(readlen>0){//не дочитали
                        i++;
                        continue;
                    }
                    if(readlen==0){
                        free(list.names[i].buf);
                        list.names[i].buf=NULL;
                    }
                }
            }
            i++;
        }
    }
}
int main() {
    time.tv_sec=5;
    time.tv_usec=0;
    int fd=0;
    int port=6666;
    printf("Port number: ");
    scanf("%d",&port);
    int err = socket_create(&fd,port);
    if(err==0){
        talking(fd);
    }
    return 0;
}