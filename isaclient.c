#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <err.h>
#include <pwd.h>
#include <ctype.h>
#include <stdbool.h>

#define BUFFER 1024 // buffer length
#define HLP_MSG "\nUsage: ./isaclient -H <host> -p <port> <command> \nboards\nboard add<name>\nboard delete<name>\nboard list<name>\nitem add<name><content>\nitem delete<name><id>\nitem update<name><id><content>\n"
#define CMD_ERR "Incorrect command!\n"
#define NO_ID -1

#define STR_LEN_INC 8

#define STR_ERROR 1
#define STR_SUCCESS 0

typedef struct
{
    char *str;
    int length;
    int allocSize;
} string;

void handleArguments(int argc, char *argv[]);
char *handleCommands(int argc, char *argv[]);
char *createRequest(char type[], char url[], char name[], char host[], int id, char content[]);
void nameCheck(char name[]);
void numCheck(char argv[]);
void getContent(char buffer[], string *content);

int strInit(string *s);
void strFree(string *s);
int *string_concat(string *s1, const char *s2);
void strClear(string *s);
int strAddChar(string *s1, char c);

int main(int argc, char *argv[])
{
    int sock, i;
    socklen_t len;
    struct sockaddr_in local, server;
    struct hostent *servent; // a pointer to the server addresses
    char buffer[BUFFER];
    char request[BUFFER];

    if (argc > 10 || argc < 2)
        errx(1, "%s", HLP_MSG);
    else
    {
        handleArguments(argc, argv);
        sprintf(request, handleCommands(argc, argv));
    }

    memset(&server, 0, sizeof(server)); // erase the server structure
    memset(&local, 0, sizeof(local));   // erase the local address structure

    server.sin_family = AF_INET;

    // make DNS resolution of the first parameter using gethostbyname()
    if ((servent = gethostbyname(argv[2])) == NULL) // check the first parameter
        errx(1, "gethostbyname() failed\n");

    // copy the first parameter to the server.sin_addr structure
    memcpy(&server.sin_addr, servent->h_addr, servent->h_length);

    server.sin_port = htons(atoi(argv[4])); // server port (network byte order)

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) //create a client socket
        err(1, "socket() failed\n");

    // connect to the remote server
    // client port and IP address are assigned automatically by the operating system

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == -1)
        err(1, "connect() failed");

    // obtain the local IP address and port using getsockname()
    len = sizeof(local);
    if (getsockname(sock, (struct sockaddr *)&local, &len) == -1)
        err(1, "getsockname() failed");

    // send http request to server
    strcpy(buffer, request);

    i = write(sock, buffer, strlen(request));
    if (i == -1)
    {
        err(1, "initial write() failed");
    }

    if ((i = read(sock, buffer, BUFFER)) == -1)
    { // read an initial string
        err(1, "initial read() failed");
    }
    else
    {
        string content;
        string headers;
        strInit(&content);
        strInit(&headers);

        sprintf(buffer, "%.*s", i, buffer);

        getContent(buffer, &content);

        char tmp[BUFFER];
        memcpy(tmp, &buffer[0], strlen(buffer) - content.length - 2);
        tmp[strlen(buffer) - content.length - 2] = '\0';
        string_concat(&headers, tmp);

        //printf("%.*s", i, buffer);
        fprintf(stderr, "%.*s", headers.length, headers.str);
        fprintf(stdout, "%.*s", content.length, content.str);

        strFree(&content);
        strFree(&headers);
    }

    close(sock);
    return 0;
}

void getContent(char buffer[], string *content)
{
    int cl = 0;
    bool isCl = false;
    char c;

    string word;
    strInit(&word);

    for (int i = 0; i <= strlen(buffer); i++)
    {
        c = buffer[i];

        if (c == ' ' || c == '\0' || c == '\n')
        {
            if (isCl)
            {
                cl = atoi(word.str);
                isCl = false;
            }
            else
            {
                if (strcmp(word.str, "Content-Length:") == 0)
                {
                    isCl = true;
                }
            }
            strClear(&word);
        }
        else
        {
            strAddChar(&word, c);
        }
    }

    if (cl != 0)
    {
        char cntnt[BUFFER];
        strClear(content);

        int poz = strlen(buffer) - cl - 2;
        memcpy(cntnt, &buffer[poz], cl);
        cntnt[cl] = '\0';

        string_concat(content, cntnt);
    }

    strFree(&word);
}

void handleArguments(int argc, char *argv[])
{
    if (strcmp(argv[1], "-h") == 0)
    {
        printf("%s", HLP_MSG);
        exit(0);
    }
}

char *handleCommands(int argc, char *argv[])
{
    static char request[BUFFER];

    switch (argc)
    {
    // GET /boards
    case 6:
        if (strcmp(argv[5], "boards") == 0)
        {
            strcpy(request, createRequest("GET", "/boards", "", argv[2], NO_ID, ""));
        }
        else
        {
            fprintf(stderr, "%s", CMD_ERR);
            exit(1);
        }
        break;

    // POST /boards/name
    // DELETE /boards/name
    // GET /boards/name
    case 8:
        if (strcmp(argv[5], "board") == 0)
        {
            if (strcmp(argv[6], "add") == 0)
            {
                nameCheck(argv[7]);
                strcpy(request, createRequest("POST", "/boards", argv[7], argv[2], NO_ID, ""));
            }
            else if (strcmp(argv[6], "delete") == 0)
            {
                nameCheck(argv[7]);
                strcpy(request, createRequest("DELETE", "/boards", argv[7], argv[2], NO_ID, ""));
            }
            else if (strcmp(argv[6], "list") == 0)
            {
                nameCheck(argv[7]);
                strcpy(request, createRequest("GET", "/board", argv[7], argv[2], NO_ID, ""));
            }
            else
            {
                fprintf(stderr, "%s", CMD_ERR);
                exit(1);
            }
        }
        else
        {
            fprintf(stderr, "%s", CMD_ERR);
            exit(1);
        }
        break;

    // POST /board/name
    // DELETE /board/name/id
    case 9:
        if (strcmp(argv[5], "item") == 0)
        {
            if (strcmp(argv[6], "delete") == 0)
            {
                nameCheck(argv[7]);
                numCheck(argv[8]);
                strcpy(request, createRequest("DELETE", "/board", argv[7], argv[2], atoi(argv[8]), ""));
            }
            else if (strcmp(argv[6], "add") == 0)
            {
                nameCheck(argv[7]);

                strcpy(request, createRequest("POST", "/board", argv[7], argv[2], NO_ID, argv[8]));
            }
            else
            {
                fprintf(stderr, "%s", CMD_ERR);
                exit(1);
            }
        }
        else
        {
            fprintf(stderr, "%s", CMD_ERR);
            exit(1);
        }
        break;

    case 10:
        if (strcmp(argv[5], "item") == 0)
        {
            if (strcmp(argv[6], "update") == 0)
            {
                nameCheck(argv[7]);
                numCheck(argv[8]);

                strcpy(request, createRequest("PUT", "/board", argv[7], argv[2], atoi(argv[8]), argv[9]));
            }
            else
            {
                fprintf(stderr, "%s", CMD_ERR);
                exit(1);
            }
        }
        else
        {
            fprintf(stderr, "%s", CMD_ERR);
            exit(1);
        }
        break;

    default:
        fprintf(stderr, "%s", CMD_ERR);
        exit(1);
        break;
    }

    return request;
}

char *createRequest(char type[], char url[], char name[], char host[], int id, char content[])
{
    static char request[BUFFER];

    if (name[0] == '\0')
    {
        sprintf(request, "%s %s HTTP/1.1\r\nHost: %s\r\n", type, url, host);
    }
    else if (id != NO_ID)
    {
        sprintf(request, "%s %s/%s/%d HTTP/1.1\r\nHost: %s\r\n", type, url, name, id, host);
    }
    else
    {
        sprintf(request, "%s %s/%s HTTP/1.1\r\nHost: %s\r\n", type, url, name, host);
    }

    // concat content
    if (content[0] != '\0')
    {
        char ctHeaders[100] = "Content-Type: text/plain\r\n";
        sprintf(ctHeaders, "%sContent-Length: %ld\r\n\r\n", ctHeaders, strlen(content));
        strcat(request, ctHeaders);
        strcat(request, content);
    }
    return strcat(request, "\r\n");
}

void nameCheck(char name[])
{
    for (int i = 0; i < strlen(name); i++)
    {
        if (!((name[i] >= '0' && name[i] <= '9') || (name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= 'a' && name[i] <= 'z')))
        {
            fprintf(stderr, "%s", "Invalid name!");
            exit(1);
        }
    }
}

void numCheck(char argv[])
{
    if (argv[0] == '-')
    {
        fprintf(stderr, "%s", "ID can not be negative!");
        exit(1);
    }

    for (int i = 0; argv[i] != 0; i++)
    {
        if (!isdigit(argv[i]))
        {
            fprintf(stderr, "%s", "Incorrect ID!");
            exit(1);
        }
    }
}

int strInit(string *s)
{
    if ((s->str = (char *)malloc(STR_LEN_INC)) == NULL)
        return STR_ERROR;
    s->str[0] = '\0';
    s->length = 0;
    s->allocSize = STR_LEN_INC;
    return STR_SUCCESS;
}

void strFree(string *s)
{
    free(s->str);
}

void strClear(string *s)
{
    s->str[0] = '\0';
    s->length = 0;
}

int strAddChar(string *s1, char c)
{
    if (s1->length + 1 >= s1->allocSize)
    {
        if ((s1->str = (char *)realloc(s1->str, s1->length + STR_LEN_INC)) == NULL)
            return STR_ERROR;
        s1->allocSize = s1->length + STR_LEN_INC;
    }
    s1->str[s1->length] = c;
    s1->length++;
    s1->str[s1->length] = '\0';
    return STR_SUCCESS;
}

int *string_concat(string *s1, const char *s2)
{
    int length_const_char = strlen(s2);
    if ((s1->length + length_const_char + 1) >= s1->allocSize)
    {
        s1->str = (char *)realloc(s1->str, s1->length + length_const_char + 1);
    }
    strncpy(&s1->str[s1->length], s2, length_const_char);
    s1->str[s1->length + length_const_char] = '\0';
    s1->allocSize = s1->length + length_const_char;
    s1->length += length_const_char;
    return STR_SUCCESS;
}
