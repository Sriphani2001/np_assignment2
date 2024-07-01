#include <stdio.h>
#include <stdlib.h>
/* You will to add includes here */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <map>

using namespace std;

#define DEBUG

#include "protocol.h"
#define NOCL 10000

int loopcount = 0;

bool flag = false;



int main(int argc, char *argv[]){
  
  /* Do magic */
  int sockfd[NOCL]; // Max 100 clients...
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes,numread;
  char buffer[1450];

  int DEBUGv;
  int timeout_in_seconds=1;
  
  DEBUGv=0;

  struct timeval ct1,ct2;
  
  char** tokens;
  FILE *fptr;
  
  //check out the number of the argc
  if (argc < 4 || argc >5 ) {

    fprintf(stderr,"usage: %s <HOSTNAME:PORT> <CLIENTs> <prob> <resultfile> [debug] \n",argv[0]);
    if(fptr!=NULL)
      fprintf(fptr,"ERROR OCCURED");    
    exit(1);
  }
  
  //split input to each parameters.
  /*
  char delim[]=":";
  char *Desthost=strtok(argv[1],delim);
  char *Destport=strtok(NULL,delim);
  */
	char *str = argv[1];
	char *Desthost = "";
	char *Destport = "";
	char *colon_pos = strrchr(str, ':');
	if (colon_pos != NULL) {
		*colon_pos = '\0';
		Desthost = str;
		Destport = colon_pos + 1;
	}
  int noClients=atoi(argv[2]);
  int prob=atoi(argv[3]);

  //check out the number of the clients that connected.
  if (noClients>=NOCL) {
    printf("Too many clients..Max is %d.\n", NOCL);
    printf("If you want more, change NOCL and recompile.\n");
    exit(1);
  }
  
  printf("Probability = %d \n",prob);

  if (argc==6) {
    printf("DEBUG ON\n");
    DEBUGv=1;
  } else {
    printf("DEBUG OFF\n");
    DEBUGv=0;
  }
  
//DEBUGv =1;
  
  //store information
  socklen_t addr_len;
  struct sockaddr_storage their_addr;
  addr_len = sizeof(their_addr);
  
  //write information into file
  printf("Connecting %d clients %s on port=%s \n",noClients,Desthost,Destport);
  printf("Saving to %s \n", argv[4]);
  //open or create the file in "w+" mode.
  fptr = fopen(argv[4],"w+");
  if (fptr == NULL) {
    printf("Cant write to %s, %s.\n",argv[4], strerror(errno));
  } 

  //Create a socket descriptor for the number of clients
  memset(&hints, 0, sizeof hints);
  memset(&buffer,0, sizeof(buffer));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  
  printf("%s\n\n\n\n");
  
  if ((rv = getaddrinfo(Desthost, Destport, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }
  printf("servinfo..\n");
  // loop through all the results and make a socket
  for (int i=0;i<noClients;i++){
    for(p = servinfo; p != NULL; p = p->ai_next) {
      if ((sockfd[i] = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
	perror("socket");
	continue;
      }
      break;
    }
    //    printf("servinfo prt2..\n");
    if (p == NULL) {
      fprintf(stderr, "%s: failed to create socket(%d)\n",argv[0],i);
      return 2;
    }
  }
	

  int s;
  struct sockaddr_in sa;
  socklen_t sa_len=sizeof(sa);
  
  char localIP[32];
  const char *myAdd;
  memset(&localIP,0,sizeof(localIP));
  
  int bobsMother = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
  if (bobsMother == -1 ){
    perror("Socket cant do nr2");
  } else {
    rv=connect(bobsMother,p->ai_addr,p->ai_addrlen);
    if (rv == -1 ){
      perror("Cant connect to socket..");
    } else {
      if( (s=getsockname(bobsMother,(struct sockaddr*)&sa,&sa_len) == -1) ){
	perror("getsockname failed.");
      }else {
	myAdd=inet_ntop(sa.sin_family, &sa.sin_addr, localIP,sizeof(localIP));
	//	printf("BobsMother (%s:%d) \n",localIP,ntohs(sa.sin_port));
      }
    }
  }

  close(bobsMother);
         
  //while starting, send the message to server 
  /* Ready to send messages. */
  typedef struct calcMessage cMessage;
  cMessage CM;
  CM.type=htons(22);
  CM.message=htons(0);
  CM.major_version=htons(1);
  CM.minor_version=htons(0);
  CM.protocol=htons(17);
  typedef struct calcProtocol cProtocol;

  cProtocol *ptrCM;
  cProtocol CP[NOCL];
  cMessage CMs[NOCL];
  
  int droppedClient[NOCL];

  for(int i=0;i<NOCL;i++){
    droppedClient[i]=0;
  }
  
  int myRand;
  int dropped=0;

  int OKresults=0;
  int ERRORresults=0;
  int ERRORsignup=0;

  
  printf("Sending Requests.\n");
  printf(" CM size = %d \n", sizeof(struct calcMessage));
  printf("uint16_t = %d \n", sizeof(uint16_t));
  printf("uint32_t = %d \n", sizeof(uint32_t));
  
  //let each client sends message1 to specific address
  
  

    
    
    
  int max_retries = 3; // 最大重传次数
  int retry_count_send[noClients] = {0}; 
  int retry_count_send2[noClients] = {0};
  map<int,int> records; 
  
	for(int i = 0 ; i < noClients; i++)
	{
		records.insert(pair<int,int>(i+1,0));
	}
	

  
 while(1){
  for(int i=0;i<noClients;i++){
	
	std::map<int, int>::iterator it = records.find(i + 1);
	//if the client dose not need retry
	if (it != records.end()) {
        	if(it->second == 1)
                	continue;
	}else
	{
		printf("some error in find client.\n");
	}
  	
    //send data from sockfd[i] to the target addr
   
    if ((numbytes = sendto(sockfd[i], &CM, sizeof(CM), 0,p->ai_addr, p->ai_addrlen)) == -1) {
      perror("talker: sendto");
      if(fptr!=NULL)
	fprintf(fptr,"ERROR OCCURED");    
      exit(1);
    } else {
      
      //Debug the message information
      //printf("%u %u %u %u %u\n", ntohs(CM.type),ntohl(CM.message),ntohs(CM.major_version),ntohs(CM.minor_version),ntohs(CM.protocol));

      if( (s=getsockname(sockfd[i],(struct sockaddr*)&sa,&sa_len) == -1) ){
	perror("getsockname failed.");
      }else {
	printf("Client[%d] (%s:%d) registered, sent %d bytes\n",i,localIP,ntohs(sa.sin_port),numbytes);
      }
    }

  }
	
	
  printf("\n-----RESPONSES to calcMessage (registration) ----- \n");
  
  //for each client
   for(int i=0;i<noClients;i++){
   	struct timeval timeout; 
	timeout.tv_sec = 2;  // set time out time
	timeout.tv_usec = 0;
	
   if (setsockopt(sockfd[i], SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        perror("setsockopt for receive timeout failed");
        close(sockfd[i]);
        return -1;
    }
    
    numbytes = recvfrom(sockfd[i], buffer, sizeof(buffer), 0,(struct sockaddr*)&their_addr,&addr_len);
    //receive data from sockfd[i] and store it into buffer
    if (numbytes == -1) {
    	if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // timeout, need to retry
                    if (retry_count_send[i] < max_retries) {
                        printf("Receive timeout occurred. Client[%d] Need to retransmit.\n", i);
                        retry_count_send[i]=retry_count_send[i]+1;
                        continue;
                    } else {
                        printf("Max retries exceeded for Client[%d]. Aborting.\n", i);
        	        droppedClient[i]=-1;
      			dropped++;
                        // hanlde the situation that exceed maximum times.
           	      perror("recvfrom");
		      if(fptr!=NULL)
			fprintf(fptr,"ERROR OCCURED");                  
                                           
                    }
    
  
    } 
    }else {
    printf("Client[%d] received %d bytes \n",i,numbytes);
      //DEBUG
      //printf("MSG: %s\n",buffer);
      
      records[i+1] = 1;
      printf("Client[%d] ",i);
    }   
    /* read info */
    /* Copy to internal structure */
    if (numbytes == sizeof(cProtocol)){
      memcpy(&CP[i],buffer, sizeof(cProtocol));
      printf("| calcProtocol type=%d version=%d.%d id=%d arith=%d ",ntohs(CP[i].type),ntohs(CP[i].major_version),ntohs(CP[i].minor_version),ntohl(CP[i].id), ntohl(CP[i].arith));
      switch(ntohl(CP[i].arith)){
	case 1:
		printf(" add \n");
		break;
	case 2:
		printf(" sub \n");
		break;
	case 3:
		printf(" mul \n");
		break;
	case 4:
		printf(" div \n");
		break;
	case 5:
		printf(" fadd \n");
		break;
	case 6:
		printf(" fsub \n");
		break;
	case 7:
		printf(" fmul \n");
		break;
	case 8:
		printf(" fdiv \n");
		break;
	}
      
      printf("\t  | inVal1=%d inVal2=%d inRes=%d inFloat1=%g inFloat2=%g flValue=%g \n",ntohl(CP[i].inValue1),ntohl(CP[i].inValue2),ntohl(CP[i].inResult),CP[i].flValue1,CP[i].flValue2,CP[i].flResult);
    } 
    //print if the protocol is not matched.
    else {
      printf("\t  | ODD SIZE MESSAGE. Got %d bytes, expected %d bytes (sizeof(cProtocol)) . \n",numbytes, sizeof(cProtocol));
      droppedClient[i]==-1; // Signal that this client is busted.
      ERRORresults++;
      ERRORsignup++;
    }
    
  }  

  	
  	bool flag = true;
  	for(std::map<int, int>::iterator it = records.begin();it != records.end(); it++)
  	{
  		//printf("%d\t",it->second);
  	
  		if(it->second == 0)
  		{
  			flag = false;
  			break;
  		}
  	}
  
  	printf("\n");
  
 	if(flag)
		break;
 	
	bool flagretry = false;

	for(int i = 0 ; i < noClients;i++)
	{
		if(retry_count_send[i] == 3)
		{
			flagretry = true;
			break;
		}
	}

	if(flagretry)
		break;
 

 
// sleep(10);
 
 }
 
 
  
  
  printf("\nWaiting 4s \n");
  sleep(4);



  
  printf("Doing Calculations .\n\n");
  for(int i=0;i<noClients;i++){
    //if the client has busted, do not do any calculation
    if (droppedClient[i]==-1) {
      continue;
    }
    //based on the arith in proto, do corresponding calculation
    switch(ntohl(CP[i].arith)){
    case 1: /*add */
      CP[i].inResult=htonl(ntohl(CP[i].inValue1)+ntohl(CP[i].inValue2));
      printf("[%d] %d + %d => %d ", i, ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult)); 
      break;
    case 2: /*sub */
      CP[i].inResult=htonl(ntohl(CP[i].inValue1)-ntohl(CP[i].inValue2));
      printf("[%d] %d - %d => %d ", i, ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult)); 
      break;	
    case 3: /*mul */
      CP[i].inResult=htonl(ntohl(CP[i].inValue1)*ntohl(CP[i].inValue2));
      printf("[%d] %d * %d => %d ", i, ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult)); 
      break;
    case 4: /*div */
      CP[i].inResult=htonl(ntohl(CP[i].inValue1)/ntohl(CP[i].inValue2));
      printf("[%d] %d / %d => %d ", i, ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult)); 
      break;
    case 5: /*fadd */
      CP[i].flResult=CP[i].flValue1+CP[i].flValue2;
      printf("[%d] %g + %g => %g ", i, CP[i].flValue1, CP[i].flValue2, CP[i].flResult); 
      break;
    case 6: /*fsub */
      CP[i].flResult=CP[i].flValue1-CP[i].flValue2;
      printf("[%d] %g - %g => %g ", i, CP[i].flValue1, CP[i].flValue2, CP[i].flResult); 
      break;	
    case 7: /*fmul */
      CP[i].flResult=CP[i].flValue1*CP[i].flValue2;
      printf("[%d] %g * %g => %g ", i, CP[i].flValue1, CP[i].flValue2, CP[i].flResult); 
      break;
    case 8: /*fdiv */
      CP[i].flResult=CP[i].flValue1/CP[i].flValue2;
      printf("[%d] %g / %g => %g ", i, CP[i].flValue1, CP[i].flValue2, CP[i].flResult); 
      break;
    default:
      printf(" ** SHIT unkown arithm. %d ** \n",ntohl(CP[i].arith));
      ERRORresults++;
      break;
    }
    
    //client to server
    CP[i].type=htons(2);

    //set the value of myRand, if myRand less than prob, the client is dropped.
    myRand=rand()%100;
    if( (s=getsockname(sockfd[i],(struct sockaddr*)&sa,&sa_len) == -1) ){
      perror("getsockname failed.");
    }
    
    if ( myRand<prob ) {
      printf(" | id=%d was lost. %s:%d |\n", ntohl(CP[i].id), localIP,ntohs(sa.sin_port));
      droppedClient[i]=1;
      dropped++;
      continue;
    } 

    printf(" | id=%d sending| ",ntohl(CP[i].id));
    
    if (DEBUGv==1){
	printf("\tClient Sending: \n");
	printf("\tCP.type = %d  \n", ntohs(CP[i].type));
	printf("\tCP.version = %d.%d \n", ntohs(CP[i].major_version),ntohs(CP[i].minor_version));
	printf("\tCP.id = %d \n", ntohl(CP[i].id));
	printf("\tCP.arith = %d \n", ntohl(CP[i].arith));
	printf("\tCP.inValue1= %d CP.inValue2= %d CP.inResult= %d \n ", ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult));
	printf("\tCP.flValue1= %g CP.flValue2= %g CP.flResult= %g \n", CP[i].flValue1, CP[i].flValue2, CP[i].flResult); 
    }
    
    //send the protocol back to server, while calculation is done.
    if ((numbytes = sendto(sockfd[i], &CP[i], sizeof(cProtocol), 0,p->ai_addr, p->ai_addrlen)) == -1) {
      perror("talker: sendto");
      if(fptr!=NULL)
	fprintf(fptr,"ERROR OCCURED");    
      exit(1);
    } else {
      if( (s=getsockname(sockfd[i],(struct sockaddr*)&sa,&sa_len) == -1) ){
	perror("getsockname failed.");
      }else {
	printf(" (%s:%d) sent %d bytes\n",localIP,ntohs(sa.sin_port),numbytes);
      }
    }
  }
  
  //hope to receive response
  printf("Reading server response, expecting %d replies .\n", noClients-dropped);
  struct timeval tv;

  tv.tv_sec=timeout_in_seconds;
  tv.tv_usec=0;
  
  printf("Setting a timeout of %d seconds on reads.\n", timeout_in_seconds);

  
  
  for(int i=0;i<noClients;i++){
    if (droppedClient[i]==1) {
      printf("Client %d (id = %d ) was dropped \n", i, ntohl(CP[i].id));
      continue;
    }
    //if time out 
    setsockopt(sockfd[i],SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);   
    if ((numbytes = recvfrom(sockfd[i], buffer, sizeof(buffer), 0,(struct sockaddr*)&their_addr,&addr_len)) == -1) {
      printf("Client %d (id = %d ) : (%d) %s \n", i, ntohl(CP[i].id), errno, strerror(errno));
      if (errno == ETIMEDOUT ) {
	printf("Client %d timedout.\n", i);
	continue;
      } 
      continue;

    } else {
      printf("Client[%d] | ", i );
      printf("(id=%d) ", ntohl(CP[i].id));
      printf(" Got %d bytes ",numbytes);

    }
    /* read info */
    /* Copy to internal structure */    
    memcpy(&CMs[i],buffer, sizeof(cMessage));
    switch(ntohs(CMs[i].type)){
    case 1:
      printf("S->C [ascii] ");
      break;
    case 2:
      printf("S->C [binary] ");
      break;
    case 3:
      printf("S->C [N/A] ");
      break;
    case 4:
      printf("C->S [ascii] ");
      break;
    case 5:
      printf("C->S [binary] ");
      break;
    case 6:
      printf("C->S [N/A] ");
      break;
    default:
      printf(" unknown type=%d ", ntohs(CMs[i].type));
      break;
    }
    
    printf("version=%d.%d ", ntohs(CMs[i].major_version),ntohs(CMs[i].minor_version));
    switch( ntohl(CMs[i].message)) {
    case 0:
      printf(" N/A ");
      ERRORresults++;
      break;
    case 1:
      printf(" OK ");
      OKresults++;
      break;
    case 2:
      printf(" Not OK ");
      ERRORresults++;
      break;
    default:
      printf("Unknown msg = %d ", ntohl(CMs[i].message));
      ERRORresults++;
      break;
    }


    //check out the arith and print the corresponding formulation
    switch(ntohl(CP[i].arith)){
    case 1: /*add */
      CP[i].inResult=htonl(ntohl(CP[i].inValue1)+ntohl(CP[i].inValue2));
      printf("[ %d + %d => %d ] ",  ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult)); 
      break;
    case 2: /*sub */
      CP[i].inResult=htonl(ntohl(CP[i].inValue1)-ntohl(CP[i].inValue2));
      printf("[ %d - %d => %d ] ",  ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult)); 
      break;	
    case 3: /*mul */
      CP[i].inResult=htonl(ntohl(CP[i].inValue1)*ntohl(CP[i].inValue2));
      printf("[ %d * %d => %d ] ",  ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult)); 
      break;
    case 4: /*div */
      CP[i].inResult=htonl(ntohl(CP[i].inValue1)/ntohl(CP[i].inValue2));
      printf("[ %d / %d => %d ] ",  ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult)); 
      break;
    case 5: /*fadd */
      CP[i].flResult=CP[i].flValue1+CP[i].flValue2;
      printf("[ %g + %g => %g ] ",  CP[i].flValue1, CP[i].flValue2, CP[i].flResult); 
      break;
    case 6: /*fsub */
      CP[i].flResult=CP[i].flValue1-CP[i].flValue2;
      printf("[ %g - %g => %g ] ",  CP[i].flValue1, CP[i].flValue2, CP[i].flResult); 
      break;	
    case 7: /*fmul */
      CP[i].flResult=CP[i].flValue1*CP[i].flValue2;
      printf("[ %g * %g => %g ] ",  CP[i].flValue1, CP[i].flValue2, CP[i].flResult); 
      break;
    case 8: /*fdiv */
      CP[i].flResult=CP[i].flValue1/CP[i].flValue2;
      printf("[  %g / %g => %g ] ",  CP[i].flValue1, CP[i].flValue2, CP[i].flResult); 
      break;
    default:
      printf(" ** SHIT unkown arithm. %d ** ",ntohl(CP[i].arith));
      break;
    }
    
    
    printf("\n");
  }

  //calculations done
  printf("Done, with good clients.\n");


  bobsMother = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
  if (bobsMother == -1 ){
    perror("Socket cant do nr2");
  } else {
    rv=connect(bobsMother,p->ai_addr,p->ai_addrlen);
    if (rv == -1 ){
      perror("Cant connect to socket..");
    } else {
      if( (s=getsockname(bobsMother,(struct sockaddr*)&sa,&sa_len) == -1) ){
	perror("getsockname failed.");
      }else {
	myAdd=inet_ntop(sa.sin_family, &sa.sin_addr, localIP,sizeof(localIP));
	//	printf("BobsMother (%s:%d) \n",localIP,ntohs(sa.sin_port));
      }
    }
  }

	  char myMsg[]="TEXT UDP 1.0";

	  //  printf("bob\n");
	  gettimeofday(&ct1, NULL);
	  //  printf("alice\n");

	  printf("Client will 'connect', and send a text string (rubbish). \n");
	  printf("Server should reply with an ERROR indicated.\n");
	  
	  printf("Client[X] ");
	  if ((numbytes = sendto(bobsMother, &myMsg, strlen(myMsg), 0,p->ai_addr, p->ai_addrlen)) == -1) {
	    perror("talker: sendto");
	    if(fptr!=NULL)
	      fprintf(fptr,"ERROR OCCURED");    
	    exit(1);
	  } else {
	    if( (s=getsockname(bobsMother,(struct sockaddr*)&sa,&sa_len) == -1) ){

	      perror("getsockname failed.");
	    }else {
	      printf("%s:%d sent %d bytes\n",localIP,ntohs(sa.sin_port),numbytes);
	    }
	  }

	  setsockopt(bobsMother,SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);   
	  if ((numbytes = recvfrom(bobsMother, buffer, sizeof(buffer), 0,(struct sockaddr*)&their_addr,&addr_len)) == -1) {
	    printf("Client[X] Error (%d) %s \n", errno, strerror(errno));
	    if (errno == ETIMEDOUT ) {
	      printf("Client[X] timedout.\n");
	    } 
	  } else {
	    printf("Client[X] expecting %d or %d bytes.\n", sizeof(cMessage), sizeof(cProtocol));
	    printf("Client[X] got %d bytes, ",numbytes);
	  }
	  gettimeofday(&ct2, 0);
	  //  printf("seconds : %ld\nmicro seconds : %ld", ct2.tv_sec, ct2.tv_usec);
	  double tv1,tv2;
	  tv2=(double)ct2.tv_sec+(double)(ct2.tv_usec)/1000000;
	  tv1=(double)ct1.tv_sec+(double)(ct1.tv_usec)/1000000;  
	  printf("within %g [us].\n", (tv2-tv1)*1000*1000);

	  int badCproblem=0;
	  
	  if (numbytes == sizeof(cMessage)){
	      memcpy(&CM,buffer, sizeof(cMessage));
	      switch(ntohs(CM.type)){
	      case 1:
		printf("S->C [ascii] (wrong)");
		badCproblem++;
		break;
	      case 2:
		printf("S->C [binary] (correct)");
		break;
	      case 3:
		printf("S->C [N/A] (wrong)");
		badCproblem++;
		break;
	      case 4:
		printf("C->S [ascii] (wrong)");
		badCproblem++;
		break;
	      case 5:
		printf("C->S [binary] (wrong)");
		badCproblem++;
		break;
	      case 6:
		printf("C->S [N/A] (wrong)");
		badCproblem++;
		break;
	      default:
		printf(" unknown type=%d ", ntohs(CM.type));
		badCproblem++;
		break;
	      }
	      
	      printf("version=%d.%d ", ntohs(CM.major_version),ntohs(CM.minor_version));
	      switch( ntohl(CM.message)) {
	      case 0:
		printf(" N/A  (wrong)");
		badCproblem++;
		break;
	      case 1:
		printf(" OK (wrong)");
		badCproblem++;
		break;
	      case 2:
		printf(" Not OK (correct)");
		break;
	      default:
		printf("Unknown msg = %d (wrong)", ntohl(CM.message));
		badCproblem++;
		break;
	      }
	  } else if (numbytes == sizeof(cProtocol)){
	    printf("Client[X] got a cProtocol, ");
	    memcpy(&CP[0],buffer, sizeof(cProtocol));
	    printf(" type = %d \n", ntohs(CP[0].type));
	    badCproblem++;
	  } else {    
	    printf("Client[X] got not the size that I expected.\n");
	  }
	  printf("\nDone with BAD clients.\n");
	  if (badCproblem>0){
	    printf("%d issues with bad clients.\n", badCproblem);
	    printf("see the log above if it was type, message or both.\n");
	  }
	  ERRORresults+=badCproblem;
	      
	    
	  close(bobsMother);

	  printf("SUMMARY Tested:%d Dropped:%d OK:%d ERROR:%d BAD:%d",noClients,dropped, OKresults,ERRORresults,badCproblem);

	  double errorRatio = 100.0;
	  if (ERRORresults>0){
	    errorRatio = (double)(ERRORresults)/(double)(noClients-dropped);
	    //    printf("Calcs ratio = %8.8g \n", errorRatio);
	  } else {
	    errorRatio = 0;
	  }
	  
	  if(errorRatio>0.1) {
	    printf(" ErrorRatio is to high (%g > 0.1) \n",errorRatio);
	  } else {
	    printf(" ErrorRatio is fine (%g < 0.1) \n",errorRatio);
	  }
	  

	  if(fptr!=NULL)
	    fprintf(fptr,"Tested:%d Dropped:%d OK:%d ERROR:%d ErrorRatio:%g BAD:%d\n",noClients,dropped, OKresults,ERRORresults, errorRatio,badCproblem);

	  if(fptr!=NULL)
	    fclose(fptr);
	    
	  
	  
	  if(ERRORresults == 0){
	    printf("SUMMARY: PASSED!\n");
	  } else {
	    printf("SUMMARY: FAILED!\n");
	  }
}
