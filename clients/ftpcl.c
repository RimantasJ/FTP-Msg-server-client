#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <regex.h>
#include <poll.h>

#include <pthread.h>

/* 
TODO:
1) Error handling
1.1) All functions must return status codes
1.2) Create enum for error states
*/

#define LINESIZE 256

void *read_func(void *value)
{
	char buff[LINESIZE];
	int main_th_pipe[2];
	int cmd_soc;
	int err_code;
	regex_t ret_code_regex;
	regex_t quit_regex;

	main_th_pipe[0] = *((int *)value);

	read(main_th_pipe[0], buff, LINESIZE);
	sscanf(buff, "%d\n", &main_th_pipe[1]);
	printf("R_th: %s\n", buff);

	read(main_th_pipe[0], buff, LINESIZE);
	sscanf(buff, "%d\n", &cmd_soc);
	printf("R_th: %s\n", buff);

	regcomp(&ret_code_regex, "^Server:[[:space:]][[:digit:]][[:digit:]][[:digit:]][[:space:]]", 0);
	regcomp(&quit_regex, "^Server:[[:space:]]221[[:space:]]", 0); // 221 is code for server closing command connection

	while (regexec(&quit_regex, buff, 0, NULL, 0) != 0)
	{
		if (recv(cmd_soc, buff, sizeof(buff), 0) <= 0)
		{
			perror("Error reading form server!");
			break;
		}
		printf("%s\n", buff);

		if (regexec(&ret_code_regex, buff, 0, NULL, 0) == 0)
		{
			printf("Return found!\n");
			write(main_th_pipe[1], buff, LINESIZE);
		}	
	}

	write(main_th_pipe[1], buff, LINESIZE);

	return NULL;
}

void *write_func(void *value)
{
	char buff[LINESIZE];
	int main_th_pipe[2];
	int cmd_soc;
	int err_code;
	regex_t cmd_code_regex;
	regex_t quit_regex;
	
	main_th_pipe[0] = *((int *)value);

	read(main_th_pipe[0], buff, LINESIZE);
	sscanf(buff, "%d\n", &main_th_pipe[1]);
	printf("W_th: %s\n", buff);

	read(main_th_pipe[0], buff, LINESIZE);
	sscanf(buff, "%d\n", &cmd_soc);
	printf("W_th: %s\n", buff);

	regcomp(&cmd_code_regex, "^[[:upper:]][[:upper:]][[:upper:]][[:upper:]][[:space:]]", 0);
	regcomp(&quit_regex, "^QUIT", 0);

	while (regexec(&quit_regex, buff, 0, NULL, 0) != 0)
	{
		fgets(buff, sizeof(buff), stdin);
		int len = strlen(buff);
		buff[len - 1] = '\0';
		if (send(cmd_soc, buff, sizeof(buff), 0) <= 0)
		{
			perror("Nutruko rysys su serveriu");
			break;
		}

		printf("buff: %s\n", buff);
		if (regexec(&cmd_code_regex, buff, 0, NULL, 0) == 0)
		{
			printf("Command found!\n");
			write(main_th_pipe[1], buff, LINESIZE);
		}
	}
	printf("Quit was found!!!\n");

	return NULL;
}

void *data_func(void *value)
{
	char buff[LINESIZE];
	char cmd_code[LINESIZE];
	char file_name[LINESIZE];
	int main_th_pipe[0];
	char ip[LINESIZE];
	int port;
	int data_soc;
	int err_code;
	int fd;

	main_th_pipe[0] = *((int *)value);

	read(main_th_pipe[0], ip, LINESIZE);
	printf("Data ip: %s", ip);

	read(main_th_pipe[0], buff, LINESIZE);
	sscanf(buff, "%d\n", &port);
	printf("Data port: %d", port);

	data_soc = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in data_addr;
	data_addr.sin_family = AF_INET;
	data_addr.sin_addr.s_addr = inet_addr(ip);
	data_addr.sin_port = htons(port);

	int connection_status = connect(data_soc, (struct sockaddr *)&data_addr, sizeof(data_addr));

	while (strcmp(cmd_code, "QUIT") != 0)
	{
		read(main_th_pipe[0], cmd_code, LINESIZE);
		read(main_th_pipe[0], file_name, LINESIZE);

		if (strcmp(cmd_code, "STOR") == 0)
		{
			if (access(file_name, F_OK) == -1)
			{
				perror("File not found\n");
				break;
			}

			fd = open(file_name, O_RDONLY);
			for (ssize_t read_len = 1; read_len > 0;)
			{
				read_len = read(fd, buff, LINESIZE);
				if (read_len > 0)
				{
					write(data_soc, buff, read_len);
				}
			}
			close(fd);
		}
		else if (strcmp(cmd_code, "RETR") == 0)
		{
			fd = open(file_name, O_CREAT | O_WRONLY);
			int count;
			ioctl(data_soc, FIONREAD, &count);
			for (ssize_t read_len = 1; count > 0;)
			{
				read_len = read(data_soc, buff, LINESIZE);
				write(fd, buff, read_len);
				ioctl(data_soc, FIONREAD, &count);
			}
			close(fd);
		}
		else if (strcmp(cmd_code, "QUIT") == 0){
			break;
		}
		else
		{
			perror("Data thread command unindentified!");
		}
		
	}

	close(data_soc);

	return NULL;
}

int connect_command_socket(int* cmd_soc, char* ip, int port)
{
	*cmd_soc = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in cmd_addr;
	cmd_addr.sin_family = AF_INET;
	cmd_addr.sin_addr.s_addr = inet_addr(ip);
	cmd_addr.sin_port = htons(port);
	
	int connection_status = connect(*cmd_soc, (struct sockaddr *)&cmd_addr, sizeof(cmd_addr));

	return connection_status;
}

// Takes 227 return message to mess, returns IP to ip, and PORT to port 
int get_ip_and_port(char* mess, char* ip, int* port)
{
	char *buff, *h1, *h2, *h3, *h4, *p1, *p2;
	int int_p1, int_p2;

	buff = strtok(mess, "(");
	h1 = strtok(NULL, ",");
	h2 = strtok(NULL, ",");
	h3 = strtok(NULL, ",");
	h4 = strtok(NULL, ",");
	p1 = strtok(NULL, ",");
	p2 = strtok(NULL, ")");
	sscanf(p1, "%d", &int_p1);
	sscanf(p2, "%d", &int_p2);
	*port = int_p1 * 256 + int_p2;

	strncpy(ip, "", 32);
	strcat(ip, h1);
	strcat(ip, ".");
	strcat(ip, h2);
	strcat(ip, ".");
	strcat(ip, h3);
	strcat(ip, ".");
	strcat(ip, h4);

	return 0;
}

// Takes IP and PORT from stdin for command soc
int get_ip_and_port_from_stdin(char* ip, int* port)
{
	char buff[16];

	printf("IP: ");
	fgets(ip, 32, stdin);
	printf("Port: ");
	fgets(buff, 16, stdin);
	sscanf(buff, "%d", port);

	return 0;
}

// Takes return message to ret_mess, returns return code to ret_code
int get_return_code(char* ret_mess, int* ret_code)
{
	char code_str[3];

	strncpy(code_str, ret_mess, 3);
	sscanf(code_str, "%d", ret_code);

	return 0;
}

int get_1st_word(char* mess, char* word)
{
	char temp1[LINESIZE];
	char *temp2;

	strcpy(temp1, mess);
	temp2 = strtok(temp1, " ");
	strcpy(word, temp2);

	return 0;
}

int get_2nd_word(char* mess, char* word)
{
	char temp1[LINESIZE];
	char *temp2;

	strcpy(temp1, mess);
	strtok(temp1, " ");
	temp2 = strtok(NULL, " ");
	strcpy(word, temp2);

	return 0;
}

void print_info()
{
	printf("FTP client. \nImplemented commands:\n");;
	printf("PASV\n");
	printf("USER <username>\n");
	printf("LIST\n");
	printf("STOR <filename>\n");
	printf("RETR <filename>\n");
	printf("DELE <filename>\n");
	printf("QUIT\n");
}

int main() 
{
	int cmd_soc;
	int data_soc;

	pthread_t read_th;
	pthread_t write_th;
	pthread_t data_th;

	int r_read_th_pipe[2]; 	// read from read_th
	int w_read_th_pipe[2];	// write to read_th
	int r_write_th_pipe[2]; // read form write_th
	int w_write_th_pipe[2]; // write to write_th
	int w_data_th_pipe[2];	// write to data_th

	char *cmd_mess = malloc(LINESIZE);
	char *ret_mess = malloc(LINESIZE);
	char *buff = malloc(LINESIZE);
	char *cmd_code = malloc(16);	/* ([A-Z][A-Z][A-Z][A-Z]\s) */
	int ret_code;					/* (\d{3}) */
	char *ip = malloc(32);			/* (\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}) */
	int port;						/* (\d{4,5}) */

	//int fd;
	int err_code;
	//int quit = 0;//!!!!!!!!!!!!!!!!!!!!!!!!!!

	print_info();

	get_ip_and_port_from_stdin(ip, &port);
	printf("Establishing command connection...\n");
	err_code = connect_command_socket(&cmd_soc, ip, port);
	if(err_code < 0){
		perror("Command connection failed! Exiting...");
		free(ip);
		free(buff);
		free(ret_mess);
		free(cmd_mess);
		return 0;
	}

	/* Creating the read thread with two pipes
	r_read_th_pipe is for reading FTP return codes from read_th
	w_read_th_pipe is only used to send two values to the read_th:
	1) writing end of r_read_th_pipe, for sending FTP return codes from read_th to main thread
	2) cmd_soc, from which read_th reads data
	*/
	err_code = pipe(r_read_th_pipe);
	if (err_code < 0)
	{
		perror("Pipe creation failed! Exiting...");
		free(ip);
		free(buff);
		free(ret_mess);
		free(cmd_mess);
		return 0;
	}
	err_code = pipe(w_read_th_pipe);
	if (err_code < 0)
	{
		perror("Pipe creation failed! Exiting...");
		free(ip);
		free(buff);
		free(ret_mess);
		free(cmd_mess);
		return 0;
	}
	
	pthread_create(&read_th, NULL, read_func, &w_read_th_pipe[0]);
	sprintf(buff, "%d", r_read_th_pipe[1]);
	write(w_read_th_pipe[1], buff, LINESIZE);
	sprintf(buff, "%d", cmd_soc);
	write(w_read_th_pipe[1], buff, LINESIZE);

	/* Creating the write thread with two pipes
	r_write_th_pipe is for reading FTP command codes from write_th
	w_write_th_pipe is only used to send two values to the write_th:
	1) writing end of r_write_th_pipe, for sending FTP command codes from write_th to main thread
	2) cmd_soc, to which write_th sends commands and messages
	*/
	err_code = pipe(r_write_th_pipe);
	if (err_code < 0)
	{
		perror("Pipe creation failed! Exiting...");
		free(ip);
		free(buff);
		free(ret_mess);
		free(cmd_mess);
		return 0;
	}
	err_code = pipe(w_write_th_pipe);
	if (err_code < 0)
	{
		perror("Pipe creation failed! Exiting...");
		free(ip);
		free(buff);
		free(ret_mess);
		free(cmd_mess);
		return 0;
	}

	pthread_create(&write_th, NULL, write_func, &w_write_th_pipe[0]);
	sprintf(buff, "%d", r_write_th_pipe[1]);
	write(w_write_th_pipe[1], buff, LINESIZE);
	sprintf(buff, "%d", cmd_soc);
	write(w_write_th_pipe[1], buff, LINESIZE);

		/* Creating the data thread with one pipe
	w_data_th_pipe is used to send instruction to the data thread about file transfering
	*/
	err_code = pipe(w_data_th_pipe);
	if (err_code < 0)
	{
		perror("Pipe creation failed! Exiting...");
		free(ip);
		free(buff);
		free(ret_mess);
		free(cmd_mess);
		return 0;
	}

	pthread_create(&write_th, NULL, write_func, &w_data_th_pipe[0]);

	struct pollfd *pfds = malloc(sizeof *pfds * 3);

	pfds[0].fd = r_read_th_pipe[0];
	pfds[0].events = POLLIN;

	pfds[1].fd = r_write_th_pipe[0];
	pfds[1].events = POLLIN;

	while(1){
		int poll_count = poll(pfds, 2, -1);
		if (poll_count == -1)
		{
			perror("Poll error");
		}

		// Read return from read_th
		if (pfds[0].revents & POLLIN) 
		{
			read(r_read_th_pipe[0], ret_mess, LINESIZE);
			printf("Return: %s\n", ret_mess);

			get_2nd_word(ret_mess, buff);
			sscanf(buff, "%d", &ret_code);

			switch (ret_code)
			{
			case 125:
			// Transfer strarting
				get_1st_word(cmd_mess, cmd_code);
				get_2nd_word(cmd_mess, buff);

				write(w_data_th_pipe[1], cmd_code, LINESIZE);
				write(w_data_th_pipe[1], buff, LINESIZE);

				break;
			case 221:
			// Closing control connection
			write(w_data_th_pipe[1], "QUIT", LINESIZE);
			break;

			case 226:
			// File transfer successful
				break;

			case 227:
			// Entering passive mode (h1,h2,h3,h4,p1,p2)
			#pragma region 227
				get_ip_and_port(ret_mess, ip, &port);
				connect_data_socket(&data_soc, ip, port);
				if (cmd_soc > 0)
				{
					printf("Data socket connected!\n");
				}
				else
				{
					printf("Data socket connection failed. Exiting\n");
					strncpy(cmd_mess, "QUIT", 32);
					if (send(cmd_soc, cmd_mess, LINESIZE, 0) == -1)
					{
						perror("Nutruko rysys su serveriu\n");
						break;
					}
					close(cmd_soc);
					free(ip);
					free(buff);
					free(ret_mess);
					free(cmd_mess);
					return 0;
				}
				break;
			#pragma endregion
			case 230:
			// User logged in
				break;

			case 250:
			// File list
				break;

			case 400:
			// Mode not selected
				printf("Use PASV to select passive mode\n");
				break;

			case 500:
			// Unknown command
				printf("Please refer to implemented commands");
				break;

			case 530:
			// Not logged in
				printf("Log in with USER <username> before managing data\n");
				break;

			case 550:
			// File not found
				printf("Use LIST to see your files in server\n");
				break;

			default:
				printf("Cannot recognize return value\n");
				break;
			}
		}
		// Read from write_th
		else if (pfds[1].revents & POLLIN) 
		{
			read(r_write_th_pipe[0], cmd_mess, LINESIZE);
			printf("Command: %s\n", cmd_mess);
		}
		else
		{
			perror("Something went wrong with poll()");
		}


	}
	
	pthread_join(read_th, NULL);
	pthread_join(write_th, NULL);
	pthread_join(data_th, NULL);
	close(r_read_th_pipe[0]);
	close(r_read_th_pipe[1]);
	close(w_read_th_pipe[0]);
	close(w_read_th_pipe[1]);
	close(r_write_th_pipe[0]);
	close(r_write_th_pipe[1]);
	close(w_write_th_pipe[0]);
	close(w_write_th_pipe[1]);
	close(w_data_th_pipe[0]);
	close(w_data_th_pipe[1]);
	close(cmd_soc);
	close(data_soc);
	free(cmd_mess);
	free(ret_mess);
	free(buff);
	free(cmd_code);
	free(ip);
	
	
	

	return 0;
}
