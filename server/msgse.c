#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#define PORT "13645"

struct pfd_name
{
    int fd;          
    int has_name; // 0 - no, 1 - yes  
    char name[256];
};

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int get_listener_socket(void)
{
    int listener; // Listening socket descriptor
    int yes = 1;  // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;        // Can be IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    // Socket type is stream
    hints.ai_flags = AI_PASSIVE;        // Address suitable for bind() socket that will accept() connection

    // If getaddrinfo returns 0, we have the address
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0)
    {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next)
    {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0)
        {
            continue;
        }

        // Allow resue of addresses on this socket
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(listener);
            continue;
        }

        break;
    }

    // Did not get bound
    if (p == NULL)
    {
        return -1;
    }

    freeaddrinfo(ai); // All done with this

    // Listen
    if (listen(listener, 10) == -1)
    {
        return -1;
    }

    return listener;
}

// Add a new file descriptor to the set
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size, struct pfd_name *pfd_names[])
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size)
    {
        *fd_size *= 2;

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
        *pfd_names = realloc(*pfd_names, sizeof(**pfd_names) * (*fd_size));
    }

    (*pfd_names)[*fd_count].has_name = 0; // New fd does not have a name

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

// Delete a file descriptor form the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count, struct pfd_name pfd_names[])
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count - 1];
    pfd_names[i] = pfd_names[*fd_count - 1];

    (*fd_count)--;
}

void broadcast(struct pollfd *pfds, int fd_count, char* name, char* message) {

    for (int i = 1; i < fd_count; i++)
    {
        char mess[256];
        strcpy(mess, name);
        strcat(mess, ": ");
        strcat(mess, message);
        if (send(pfds[i].fd, mess, sizeof(mess), 0) == -1)
        {
            perror("send");
        }
    }
}

int main(void)
{
    int listener;

    int newfd;
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;

    char buf[256];

    char remoteIP[INET6_ADDRSTRLEN];

    int fd_count = 0;
    int fd_size = 5;
    struct pollfd *pfds = malloc(sizeof *pfds * fd_size);
    struct pfd_name *pfd_names = malloc(sizeof *pfd_names * fd_size);

    // Get a listening socket
    listener = get_listener_socket();

    if (listener == -1)
    {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    // Add the listener to set
    pfds[0].fd = listener;
    pfds[0].events = POLLIN; // Report ready to read on incoming connection

    fd_count = 1;

    // Main loop
    for (;;)
    {
        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count == -1)
        {
            perror("poll");
            exit(1);
        }

        // Run through the existing connections looking for data to read
        for (int i = 0; i < fd_count; i++)
        {
            // Check if someone's ready to read
            if (pfds[i].revents & POLLIN)
            {
                // New connection to listener
                if (pfds[i].fd == listener)
                {
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

                    if (newfd == -1)
                    {
                        perror("accept");
                    }
                    else
                    {
                        add_to_pfds(&pfds, newfd, &fd_count, &fd_size, &pfd_names);
                        printf("Serveris: naujas vartotojas ant socket'o %d\n", newfd);

                        if (send(newfd, "Serveris: ATSIUSKVARDA", strlen("Serveris: ATSIUSKVARDA"), 0) == -1)
                        {
                            perror("Serveris: klaida siunciant vardo prasyma");
                        }
                    }
                }
                // Messenge from client
                else
                {
                    int nbytes = recv(pfds[i].fd, buf, sizeof buf, 0);
                    int sender_fd = pfds[i].fd;

                    // Got error or connection closed by client
                    if (nbytes <= 0)
                    {
                        if (nbytes == 0)
                        {
                            printf("Serveris: %s atsijungė\n", pfd_names[i].name);

                            strcpy(buf, pfd_names[i].name);
                            strcat(buf, " atsijunge");
                            
                            broadcast(pfds, fd_count, "Serveris", buf);
                        }
                        else
                        {
                            perror("recv");
                        }

                        close(pfds[i].fd);
                        del_from_pfds(pfds, i, &fd_count, pfd_names);
                    }
                    // Client sends his name
                    else if(pfd_names[i].has_name == 0){
                        printf("Socket'as %d: %s\n", pfds[i].fd, buf);
                        strcpy(pfd_names[i].name, buf);
                        pfd_names[i].has_name = 1;

                        if (send(pfds[i].fd, "Serveris: VARDASOK", nbytes, 0) == -1)
                        {
                            perror("Serveris: klaida siunciant vardo patvirtinima");
                        }  
                    }
                    // Client sends a normal messenge
                    else
                    {
                        printf("%s: %s\n", pfd_names[i].name, buf);

                        broadcast(pfds, fd_count, pfd_names[i].name, buf);
                    }
                } // END handle data from client
            }     // END got ready-to-read from poll()
        }         // END looping through file descriptors
    }             // END for(;;)--and you thought it would never end!

    return 0;
}
