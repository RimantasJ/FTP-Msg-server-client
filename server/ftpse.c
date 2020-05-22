#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/ioctl.h>
#include <dirent.h>

#include <netinet/in.h>

#define LINESIZE 256

int format_227(char *buff, char* ip, int port) 
{
	char data[256], *h1, *h2, *h3, *h4, p1[8], p2[8];
	int s1, s2, s3;

	port += 1;

	strcpy(buff, ip);
	h1 = strtok(buff, ".");
	h2 = strtok(NULL, ".");
	h3 = strtok(NULL, ".");
	h4 = strtok(NULL, " ");

	sscanf(h4, "%d", &s3);
	sprintf(h4, "%d", s3);
	s1 = port / 256;
	s2 = port % 256;
	sprintf(p1, "%d", s1);
	sprintf(p2, "%d", s2);

	strncpy(data, "(", 32);
	strcat(data, h1);
	strcat(data, ",");
	strcat(data, h2);
	strcat(data, ",");
	strcat(data, h3);
	strcat(data, ",");
	strcat(data, h4);
	strcat(data, ",");
	strcat(data, p1);
	strcat(data, ",");
	strcat(data, p2);
	strcat(data, ")");

	strncpy(buff, data, 32);
}

int get_command(char *cmd_mess, char *cmd)
{
	strncpy(cmd, cmd_mess, 4);

	return 0;
}

int get_file_list(char* list)
{
	DIR *d;
	struct dirent *dir;
	d = opendir(".");
	strncpy(list, "", 256);

	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			strcat(list, dir->d_name);
			strcat(list, "\n");
		}
		closedir(d);
	}

	return 0;
}

int get_2nd_word(char *mess, char *word)
{
	char temp1[256];
	char *temp2;

	strcpy(temp1, mess);
	strtok(temp1, " ");
	temp2 = strtok(NULL, " ");
	strcpy(word, temp2);

	return 0;
}

int main()
{
	char *cmd_mess = malloc(LINESIZE);
	char *ret_mess = malloc(LINESIZE);
	char *buff = malloc(LINESIZE);

	char *cmd = malloc(sizeof(char) * 4);
	char *username = malloc(LINESIZE);
	char *ip = malloc(32);
	int port;
	char mode[8] = "";
	int logged_in = 0;
	int quit = 0;

	int cmd_soc = socket(AF_INET, SOCK_STREAM, 0);
	int data_soc = socket(AF_INET, SOCK_STREAM, 0);
	int fd;

	printf("Server starting...\n");
	printf("IP: ");
	fgets(ip, 32, stdin);
	printf("Port: ");
	fgets(buff, 8, stdin);
	sscanf(buff, "%d", &port);

	struct sockaddr_in cmd_address;
	cmd_address.sin_family = AF_INET;
	cmd_address.sin_addr.s_addr = inet_addr(ip);
	cmd_address.sin_port = htons(port);
	
	struct sockaddr_in data_address;
	data_address.sin_family = AF_INET;
	data_address.sin_addr.s_addr = inet_addr(ip);
	data_address.sin_port = htons(port + 1);
	
	bind(cmd_soc, (struct sockaddr *)&cmd_address, sizeof(cmd_address));
	listen(cmd_soc, 1);

	bind(data_soc, (struct sockaddr *)&data_address, sizeof(data_address));
	listen(data_soc, 1);

	printf("Waiting for command connection...\n");
	cmd_soc = accept(cmd_soc, NULL, NULL);
	if(cmd_soc > 0) {
		printf("Connected!\n");
	}
	else
	{
		printf("Connection failed\n");
		free(ip);
		free(buff);
		free(ret_mess);
		free(cmd_mess);
		return 0;
	}

	while(quit != 1){
		recv(cmd_soc, cmd_mess, LINESIZE, 0);
		printf("command: %s\n", cmd_mess);

		get_command(cmd_mess, cmd);

		if (strcmp(cmd, "PASV") == 0)
		{
			if (strcmp(mode, "PASV") != 0)
			{
				strcpy(mode, "PASV");

				format_227(buff, ip, port);
				strcpy(ret_mess, "227 Entering Passive Mode ");
				strcat(ret_mess, buff);
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}

				data_soc = accept(data_soc, NULL, NULL);

			}
			else if (strcmp(mode, "PASV") == 0) 
			{
				format_227(buff, ip, port);
				strcpy(ret_mess, "227 Entering Passive Mode ");
				strcat(ret_mess, buff);
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}
		}
		else if (strcmp(cmd, "USER") == 0)
		{	
			if (logged_in == 0)
			{
				get_2nd_word(cmd_mess, username);
				struct stat st = {0};
				if (stat(username, &st) == -1)
				{
					mkdir(username, 0777);
				}
				chdir(username);
				logged_in = 1;

				strcpy(ret_mess, "230 User logged in");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}
			else if (logged_in == 1)
			{
				get_2nd_word(cmd_mess, username);
				chdir("..");
				struct stat st = {0};
				if (stat(username, &st) == -1)
				{
					mkdir(username, 0777);
				}
				chdir(username);

				strcpy(ret_mess, "230 User logged in");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}
		}
		else if (strcmp(cmd, "LIST") == 0)
		{
			if (logged_in == 1) {
				get_file_list(buff);
				strcpy(ret_mess, "250 File list:");
				strcat(ret_mess, buff);
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}
		}
		else if (strcmp(cmd, "STOR") == 0)
		{
			if (logged_in == 1 && strcmp(mode, "PASV") == 0)
			{
				get_2nd_word(cmd_mess, buff);

				strcpy(ret_mess, "125 Starting file transfer");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}

				fd = open(buff, O_CREAT | O_WRONLY);
				ssize_t read_len = 1;
				int count;
				sleep(1);
				ioctl(data_soc, FIONREAD, &count);
				while (count > 0)
				{
					read_len = read(data_soc, buff, 256);
					write(fd, buff, read_len);
					ioctl(data_soc, FIONREAD, &count);
				}

				close(fd);

				strcpy(ret_mess, "226 file transfer complete");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}
			else if (logged_in == 0) {
				strcpy(ret_mess, "530 Not logged in");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}
			else if (strcmp(mode, "PASV") != 0) {
				strcpy(ret_mess, "400 Mode not selected");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}

		}
		else if (strcmp(cmd, "RETR") == 0)
		{
			if (logged_in == 1 && strcmp(mode, "PASV") == 0)
			{
				get_2nd_word(cmd_mess, buff);
				if (access(buff, F_OK) == -1)
				{
					strcpy(ret_mess, "550 file unavailable");
					if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
					{
						perror("Lost connection to client\n");
					}
					printf("return: %s\n", ret_mess);
					continue;
				}

				strcpy(ret_mess, "125 Starting file transfer");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}

				fd = open(buff, O_RDONLY);
				ssize_t read_len = 1;
				while (read_len > 0)
				{
					read_len = read(fd, buff, 256);
					if (read_len > 0)
					{
						write(data_soc, buff, read_len);
					}
				}
				close(fd);

				strcpy(ret_mess, "226 file transfer complete");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}
			else if (logged_in == 0)
			{
				strcpy(ret_mess, "530 Not logged in");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}
			else if (strcmp(mode, "PASV") != 0)
			{
				strcpy(ret_mess, "400 Mode not selected");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}
		}
		else if (strcmp(cmd, "DELE") == 0)
		{
			if (logged_in == 1 && strcmp(mode, "PASV") == 0)
			{
				get_2nd_word(cmd_mess, buff);
				if (access(buff, F_OK) == -1)
				{
					strcpy(ret_mess, "550 file unavailable");
					if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
					{
						perror("Lost connection to client\n");
					}
					printf("return: %s\n", ret_mess);
					continue;
				}
				if (remove(buff) == 0) 
				{
					strcpy(ret_mess, "250 File removed");
					if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
					{
						perror("Lost connection to client\n");
					}
					printf("return: %s\n", ret_mess);
				}
			}
			else if (logged_in == 0)
			{
				strcpy(ret_mess, "530 Not logged in");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}
			else if (strcmp(mode, "PASV") != 0)
			{
				strcpy(ret_mess, "400 Mode not selected");
				if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
				{
					perror("Lost connection to client\n");
				}
			}
		}
		else if (strcmp(cmd, "QUIT") == 0)
		{
			quit = 1;
			strcpy(ret_mess, "221 service closing");
			if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
			{
				perror("Lost connection to client\n");
			}
			break;
		}
		else {
			strcpy(ret_mess, "500 Command not recognized");

			if (send(cmd_soc, ret_mess, LINESIZE, 0) == -1)
			{
				perror("Lost connection to client\n");

			}
		}

		printf("return: %s\n", ret_mess);
		
	}

	
	close(cmd_soc);
	close(data_soc);

	free(ret_mess);
	free(cmd_mess);
	free(buff);
	free(cmd);
	free(username);
	free(ip);

	return 0;
}



