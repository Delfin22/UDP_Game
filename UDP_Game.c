#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#define MAXNICKNAME 50
#define BUFLEN 10

char nickname [MAXNICKNAME];
char ip[INET6_ADDRSTRLEN]; 
int pid;

struct Shared{ //struct with data shared between parrent and child processes
	int points;
	int tour;
	int playerPoints;
	int oppPoints;
	int isEnd;
};

struct Shared *shared;



void getinfo(struct sockaddr *their_addr, char*data){ //conversion ipv4 address to presentation address
	char presentation_addr[INET6_ADDRSTRLEN];
	inet_ntop(their_addr->sa_family,  &(((struct sockaddr_in*)their_addr)->sin_addr),
			presentation_addr, sizeof(presentation_addr));
	strcpy(data,presentation_addr);
	data[INET6_ADDRSTRLEN] = '\0';
}


int setup_server(char* port){ //listening on port choosen in argv[2]
	int sockfd, rv;
	struct addrinfo hints, *servinfo, *p;
	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_INET; 
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; //use your own ip

	if((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0){ //fulfilling address fields using getaddrinfo
		perror("Getaddrinfo");
		return 1;
	}

	for(p = servinfo; p != NULL; p = p->ai_next){
		if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			close(sockfd);
			perror("Socket");
			continue;
		}

		if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(sockfd);
			perror("Bind");
			continue;
		}
		break;
	}

	if(p == NULL){
		fprintf(stderr, "failed to bind socket\n");
		return 2;
	}
	freeaddrinfo(servinfo);
	return sockfd;
}
int listen_for_int(int sockfd, int *buf){ //function for receiving integers
	int numbytes;
	struct sockaddr_storage their_addr;
	size_t addr_len = sizeof(struct sockaddr_storage);
	if ((numbytes = recvfrom(sockfd, buf, sizeof(int), 0,
				(struct sockaddr *)&their_addr, (socklen_t *)&addr_len)) == -1){

		perror("recvfrom");
		exit(0);
	}
	return 0;
}

int listen_for_nickname(int sockfd, char*buf){ //function for receiving array of chars
	char data[MAXNICKNAME];
	int numbytes;
	struct sockaddr_storage their_addr;
	size_t addr_len = sizeof(struct sockaddr_storage);
	if ((numbytes = recvfrom(sockfd, data, MAXNICKNAME, 0,
				(struct sockaddr *)&their_addr, (socklen_t *)&addr_len)) == -1){

		perror("recvfrom");
		exit(0);
	}
	data[numbytes] = '\0';
	if(strcmp(data,"no_nickname") == 0){
		getinfo((struct sockaddr*)&their_addr, data);
	}


	strcpy(buf,data);

	return 0;
}
void send_int(char* address, char* port, int x){ //function for sending integer to choosen address
	int *data = &x;
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes = sizeof(int);
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if((rv = getaddrinfo(address,port,&hints,&servinfo)) != 0){
		perror("getaddrinfo");
		exit(0);
	}
	for(p = servinfo; p != NULL; p = p->ai_next){
		if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1){
			perror("socket");
			continue;
		}
		break;
	}
	if(p == NULL){
		perror("getaddrinfo");
		exit(0);
	}
	if(sendto(sockfd, data, numbytes, 0, p->ai_addr, p->ai_addrlen) == -1){
		perror("Sendto");
		exit(0);
	}
	
	close(sockfd);
	freeaddrinfo(servinfo);
}

void send_nickname(char* address, char* port, char* data){ //function for sending array of chars
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes = strlen(data);
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if((rv = getaddrinfo(address,port,&hints,&servinfo)) != 0){
		perror("getaddrinfo");
		exit(0);
	}
	for(p = servinfo; p != NULL; p = p->ai_next){
		if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1){
			perror("socket");
			continue;
		}
		break;
	}
	if(p == NULL){
		perror("getaddrinfo");
		exit(0);
	}
	if(sendto(sockfd, data, numbytes, 0, p->ai_addr, p->ai_addrlen) == -1){
		perror("Sendto");
		exit(0);
	}
	
	close(sockfd);
	freeaddrinfo(servinfo);
}

void exchange_nicknames(int argc, char**argv,int sockfd){ //function for receiving nickname and saving it locally
	send_nickname(argv[1],argv[2],"no_nickname"); //if user decided not to share his nickname default data will be no_nickname
	listen_for_nickname(sockfd,ip);
	printf("Rozpoczeto gre z %s\n",ip);

	if(argc == 3){
		send_nickname(argv[1],argv[2],"no_nickname");
	}
	else{
		send_nickname(argv[1],argv[2],argv[3]);
	}
	listen_for_nickname(sockfd,nickname);
	printf("%s dolaczyl do gry!\n",nickname);
}

void start(char**argv ,int argc,int sockfd){ //function setting default values and initiating connection via simple handshake with 1-0 values
	shared->oppPoints = 0;
	shared->playerPoints = 0;
	shared->points = 0;	
	shared->isEnd = 0;
	nickname[0] = '\0';
	ip[0] = '\0';

	send_int(argv[1],argv[2],1);
	printf("Propozycja gry wyslana!\n");
	listen_for_int(sockfd,&(shared->tour));
	if(shared->tour == 1){
		send_int(argv[1],argv[2],0);
	}

	exchange_nicknames(argc,argv,sockfd);
}
void sighndl(int signal){ //signal service
	shmdt(shared);
	exit(0);
}
void game(char**argv, int sockfd){ //valid game
	int temp;
	char buf[BUFLEN];
	int next = 1;

	if(shared->tour == 1){
		shared->points = rand()%10+1;
		printf("Losowa wartosc poczatkowa: %d. Podaj kolejna wartosc:\n",shared->points);
	}
	while(shared->points < 50){
		int n = read(0,buf,BUFLEN);
		buf[n] = '\0';

		if(shared->points >= 50) //if opponent won
			break;
		if(strncmp(buf,"koniec",6) == 0){
			printf("Konczenie gry...\n");
			shared->isEnd = 0;
			send_int(argv[1],argv[2],-1);
			kill(pid,SIGUSR1); //sending signal to child process to terminate it
			next = 0;
			break;
		}
		else if(strncmp(buf,"wynik",5) == 0){
			printf("Twoj wynik: %d\nWynik %s: %d\n",shared->playerPoints,nickname,shared->oppPoints);
		}
		else{
			temp = atoi(buf);
			if(shared->tour == 1){
				if((temp-shared->points > 10)||(temp-shared->points < 1)){ //validate input data
					printf("Niewlasciwa liczba!\n");
				}
				else if (temp >= 50){
					printf("Wygrales! Rozpoczynam nowa gre, tura: %s\n", nickname);
					send_int(argv[1],argv[2],temp);
					shared->points = 0;
					shared->playerPoints += 1;
					shared->tour = 0;
				}
				else{
					send_int(argv[1],argv[2],temp);
					shared->points = temp;
					shared->tour = 0;
				}
			}
			else
				printf("To nie twoja tura!\n");
		}

		if(shared->isEnd == 1){ //If opponent choose to end game and player decided to wait for another connection
			next = 0;
			break;
		}

	}

	if (next != 0) //Next round initiated
		game(argv, sockfd);	
}

void begin_game(int argc,char**argv,int sockfd){

	start(argv,argc,sockfd);

	if((pid = fork()) < 0) //After estabilished connection, child process will lisen for data to ensure asynchronicity
		perror("Fork");

	else if(pid == 0){
		signal(SIGUSR1,sighndl);

		while(8){
			listen_for_int(sockfd,&(shared->points));
			if(shared->points == -1){ //because in case of ending game, program will send -1 value to opponent
				printf("Gracz %s zakonczyl gre. Mozesz poczekac na kolejnego gracza - wcisnij enter ",nickname);
				printf("lub zakonczyc dzialanie programu - wpisz 'koniec'.\n");
				shared->isEnd = 1;
				break;
			}
			else if(shared->points >= 50){
				printf("Gracz %s wygral. Rozpoczynam nowa gre. Wcisnij enter\n",nickname);
				shared->oppPoints += 1;
				shared->tour = 1;
			}
			else{
				printf("%s podal wartosc %d, podaj kolejna wartosc:\n",nickname,shared->points);
				shared->tour = 1;
			}
		}
	}
	
	else{ 
		game(argv,sockfd);

		if(shared->isEnd == 1){
			shared->isEnd = 0;
			begin_game(argc,argv,sockfd);
		}
	}
}

int main(int argc, char *argv[]){
	int shmid,key,sockfd;
	srand(time(NULL));

	if(argc < 3 || argc > 5){
		printf("Zle argumenty!\n");
		exit(1);
	}

	key = rand();	//generate random key and prepare shared memory segment

	if((shmid = shmget(key,sizeof(struct Shared), 0600 |IPC_CREAT | IPC_EXCL)) == -1){
		perror("Shemget");
		exit(1);
	}

	shared = (struct Shared*)shmat(shmid,(void*)0,0);
	if(shared ==(void*) -1){
		perror("Shmat");
		exit(1);
	}

	sockfd = setup_server(argv[2]);
	begin_game(argc,argv,sockfd);

	shmdt(shared);
	shmctl(shmid, IPC_RMID, 0);
	return 0;

}
