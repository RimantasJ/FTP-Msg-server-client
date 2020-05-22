#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>

#include <pthread.h>

void *read_func(void *value){
	char recv_buff[256];

	int *ns = (int *) value;

	while (strcmp(recv_buff, "END") != 0)
	{
		if(recv(*ns, recv_buff, sizeof(recv_buff), 0) <= 0){
			perror("Nutruko rysys su serveriu");
			break;
		}
		printf("%s\n", recv_buff);
	}

	return NULL;
}

void *write_func(void *value)
{
	char send_buff[256];

	int *ns = (int *)value;

	while (strcmp(send_buff, "END\0") != 0)
	{
		fgets(send_buff, sizeof(send_buff), stdin);
		int len = strlen(send_buff);
		send_buff[len - 1] = '\0';
		if (send(*ns, send_buff, sizeof(send_buff), 0) == -1)
		{
			perror("Nutruko rysys su serveriu");
			break;
		}
	}

	return NULL;
}

int main() {

	pthread_t read_th;
	pthread_t write_th;

	int cmd_soc = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(13645);		  //PORT!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	server_address.sin_addr.s_addr = INADDR_ANY;  //No specific IP !!!!!!!!!!!!!!!!!!!!!!!!!!!!

	int connection_status = connect(cmd_soc, (struct sockaddr *)&server_address, sizeof(server_address));

	//Catch
	if(connection_status == -1) {
		printf("Conn went sideways!\n");
	}

	pthread_create(&read_th, NULL, read_func, &cmd_soc);
	pthread_create(&write_th, NULL, write_func, &cmd_soc);

	pthread_join(write_th, NULL);
	pthread_join(read_th, NULL);

	
	close(cmd_soc);

	

	return 0;
}
