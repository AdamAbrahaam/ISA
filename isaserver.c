#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <err.h>
#include <ctype.h>
#include <stdbool.h>

#define BUFFER 1024 // buffer for incoming messages
#define MAX_NAME 20
#define QUEUE 1 // queue length for  waiting connections
#define USG_MSG "Usage:  ./isaserver [-p , -h] <port>\n"
#define HLP_MSG "HELP:  ./isaserver [-p , -h] <port>\n"
#define RQ_OK 200
#define RQ_CREATED 201
#define RQ_NOT_FOUND 404
#define RQ_EXISTS 409
#define RQ_CL 400

#define STR_LEN_INC 8

#define STR_ERROR 1
#define STR_SUCCESS 0

typedef struct
{
    char type[7];
    char url[20];
    bool ct;
    int cl;
} tRqst;

tRqst rqst;

typedef struct tElem
{
    struct tElem *nPtr;
    struct tElem *pPtr;
    char data[BUFFER];
} * tElemPtr;

typedef struct tBoard
{
    char name[MAX_NAME];
    tElemPtr First;
    tElemPtr Last;
    struct tBoard *nPtr;
    struct tBoard *pPtr;
} * tBoardPtr;

typedef struct
{
    tBoardPtr First;
} tList;

typedef struct
{
    char *str;
    int length;
    int allocSize;
} string;

void initList(tList *L);
int newBoard(tList *L, char name[]);
int deleteBoard(tList *L, char name[]);
int deletePost(tList *L, char name[], int id);
tBoardPtr findByName(tList *L, char name[]);
tElemPtr findById(tBoardPtr B, int id);
int newPost(tList *L, char name[], char content[]);
int getBoards(tList *L, string *str);
int getPosts(tList *L, char name[], string *str);
int changePost(tList *L, char name[], int id, char content[]);
void createResponse(tList *L, string *response, char buffer[]);
void disposeList(tList *L);
void disposeBoard(tBoardPtr B);
bool isBoards(char url[]);

int strInit(string *s);
void strFree(string *s);
int *string_concat(string *s1, const char *s2);
void strClear(string *s);
int strAddChar(string *s1, char c);

void handleError(char *errorMessage);
void handleHelp();
void handleArguments(int argc, char *argv[]);
bool isNumber(char argv[]);
void processRequest(char msg[]);
void processLine(string *line);

int main(int argc, char *argv[])
{
    int fd;
    int newsock;
    int len, msg_size, i;
    struct sockaddr_in server; // the server configuration (socket info)
    struct sockaddr_in from;   // configuration of an incoming client (socket info)
    char buffer[BUFFER];
    tList boardList;
    string response;

    initList(&boardList);
    strInit(&response);

    // Test for correct number of arguments
    if (argc >= 4)
    {
        handleError(USG_MSG);
    }
    else
    {
        handleArguments(argc, argv);
    }

    // create a server socket
    // AF_INET = IPv4 protocol
    // SOCK_STREAM = TCP
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        err(1, "socket(): could not create the socket");

    server.sin_family = AF_INET;            // initialize server's sockaddr_in structure
    server.sin_addr.s_addr = INADDR_ANY;    // wait on every network interface, see <netinet/in.h>
    server.sin_port = htons(atoi(argv[2])); // set the port where server is waiting

    if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) //bind the socket to the port
        err(1, "bind() failed");

    if (listen(fd, QUEUE) != 0) //set a queue for incoming connections
        err(1, "listen() failed");

    // accept new connection from an incoming client
    // parameter "from" retreives information of the client socket
    // newsock is a file descriptor to a new socket where incoming connection is processed
    len = sizeof(from);
    while (1)
    { // wait for incoming connections (iterative server)
        if ((newsock = accept(fd, (struct sockaddr *)&from, (socklen_t *)&len)) == -1)
            err(1, "accept failed");

        // process incoming messages from the client using "newsock" socket
        // until the client stops sending data (CRLF)

        while ((msg_size = read(newsock, buffer, BUFFER)) > 0)
        { // read the message

            char newBuffer[msg_size];
            sprintf(newBuffer, "%.*s", msg_size, buffer);

            processRequest(newBuffer);

            createResponse(&boardList, &response, newBuffer);

            char rspns[BUFFER];
            sprintf(rspns, response.str);
            msg_size = strlen(rspns);

            i = write(newsock, rspns, msg_size); // send a converted message to the client
            if (i == -1)                         // check if data was successfully sent out
                err(1, "write() failed.");
            else if (i != msg_size)
                err(1, "write(): buffer written partially");

            strClear(&response);
        }

        // no more data from the client -> close the client and wait for a new one
        close(newsock); // close the new socket
    }
    // close the server
    close(fd); // close an original server socket

    strFree(&response);
    disposeList(&boardList);
    return 0;
}

void handleError(char *errorMessage)
{
    fprintf(stderr, "%s", errorMessage);
    exit(1);
}

void handleHelp()
{
    printf(HLP_MSG);
    exit(0);
}

void handleArguments(int argc, char *argv[])
{
    switch (argc)
    {
    case 2:
        if (strcmp(argv[1], "-h") == 0)
        {
            handleHelp();
        }
        else
        {
            handleError(USG_MSG);
        }
        break;

    case 3:
        if (strcmp(argv[1], "-p") == 0)
        {
            if (strcmp(argv[2], "-h") == 0)
            {
                handleHelp();
            }
            else if (!isNumber(argv[2]))
            {
                handleError("Port must be a number!\n");
            }
        }
        else if (strcmp(argv[1], "-h") == 0)
        {
            handleHelp();
        }
        else
        {
            handleError(USG_MSG);
        }
        break;
    default:
        handleError(USG_MSG);
        break;
    }
}

bool isNumber(char argv[])
{
    if (argv[0] == '-')
        handleError("Port can not be negative!\n");

    for (int i = 0; argv[i] != 0; i++)
    {
        if (!isdigit(argv[i]))
            return false;
    }
    return true;
}

void processRequest(char msg[])
{
    char c;

    string line;
    strInit(&line);

    for (int i = 0; i < strlen(msg); i++)
    {
        c = msg[i];

        if (c == '\n')
        {
            processLine(&line);
            strClear(&line);
        }
        else
        {
            strAddChar(&line, c);
        }
    }

    strFree(&line);
}

void processLine(string *line)
{
    char c;
    bool isRqst = false;
    bool isCl = false;

    string word;
    strInit(&word);

    for (int i = 0; i <= line->length; i++)
    {
        c = line->str[i];

        if (c == ' ' || c == '\0')
        {
            if (isRqst)
            {
                sprintf(rqst.url, word.str);
                isRqst = false;
            }
            else if (isCl)
            {
                rqst.cl = atoi(word.str);
                isCl = false;
            }
            else
            {
                if ((strcmp(word.str, "GET") == 0) ||
                    (strcmp(word.str, "POST") == 0) ||
                    (strcmp(word.str, "PUT") == 0) ||
                    (strcmp(word.str, "DELETE") == 0))
                {
                    sprintf(rqst.type, word.str);
                    isRqst = true;
                }
                else if (strcmp(word.str, "Content-Type:") == 0)
                {
                    rqst.ct = true;
                }
                else if (strcmp(word.str, "Content-Length:") == 0)
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

    strFree(&word);
}

void createResponse(tList *L, string *response, char buffer[])
{
    int code = RQ_NOT_FOUND;
    string body;
    strInit(&body);

    if (strcmp(rqst.type, "POST") == 0)
    {
        // POST /boards/name
        if (isBoards(rqst.url))
        {
            char name[20];
            memcpy(name, &rqst.url[8], strlen(rqst.url) - 8);
            name[strlen(rqst.url) - 8] = '\0';

            code = newBoard(L, name);
        }
        // POST /board/name
        else
        {
            if (rqst.cl == 0)
            {
                code = RQ_CL;
            }
            else
            {
                char name[20];
                memcpy(name, &rqst.url[7], strlen(rqst.url) - 7);
                name[strlen(rqst.url) - 7] = '\0';

                char content[rqst.cl + 1];
                int poz = strlen(buffer) - rqst.cl - 2;
                memcpy(content, &buffer[poz], rqst.cl);
                content[rqst.cl] = '\0';

                code = newPost(L, name, content);
            }
        }
    }
    else if (strcmp(rqst.type, "GET") == 0)
    {

        // GET /boards
        if (isBoards(rqst.url))
        {
            code = getBoards(L, &body);
        }
        // GET /board/name
        else
        {
            char name[20];
            memcpy(name, &rqst.url[7], strlen(rqst.url) - 7);
            name[strlen(rqst.url) - 7] = '\0';

            code = getPosts(L, name, &body);
        }
    }
    else if (strcmp(rqst.type, "DELETE") == 0)
    {
        // DELETE /boards/name
        if (isBoards(rqst.url))
        {
            char name[20];
            memcpy(name, &rqst.url[8], strlen(rqst.url) - 8);
            name[strlen(rqst.url) - 8] = '\0';

            code = deleteBoard(L, name);
        }
        // DELETE /board/name/id
        else
        {
            char url[50];
            memcpy(url, &rqst.url[7], strlen(rqst.url) - 7);
            url[strlen(rqst.url) - 7] = '\0';

            char *idStart = strrchr(url, '/');

            char name[20];
            memcpy(name, &url[0], idStart - url);
            name[idStart - url] = '\0';

            char tmpId[10];
            memcpy(tmpId, &url[idStart - url + 1], strlen(url) - strlen(name));
            url[strlen(url) - strlen(name)] = '\0';
            int id = atoi(tmpId);

            code = deletePost(L, name, id);
        }
    }
    else if (strcmp(rqst.type, "PUT") == 0)
    {
        if (!isBoards(rqst.url))
        {
            char url[50];
            memcpy(url, &rqst.url[7], strlen(rqst.url) - 7);
            url[strlen(rqst.url) - 7] = '\0';

            char *idStart = strrchr(url, '/');

            char name[20];
            memcpy(name, &url[0], idStart - url);
            name[idStart - url] = '\0';

            char tmpId[10];
            memcpy(tmpId, &url[idStart - url + 1], strlen(url) - strlen(name));
            url[strlen(url) - strlen(name)] = '\0';
            int id = atoi(tmpId);

            char content[rqst.cl + 1];
            int poz = strlen(buffer) - rqst.cl - 2;
            memcpy(content, &buffer[poz], rqst.cl);
            content[rqst.cl] = '\0';

            code = changePost(L, name, id, content);
        }
    }
    else
    {
        code = RQ_NOT_FOUND;
    }

    char codeName[20];
    if (code == RQ_OK)
    {
        sprintf(codeName, "OK\r\n");
    }
    else if (code == RQ_NOT_FOUND)
    {
        sprintf(codeName, "Not Found\r\n");
    }
    else if (code == RQ_EXISTS)
    {
        sprintf(codeName, "Conflict\r\n");
    }
    else if (code == RQ_CREATED)
    {
        sprintf(codeName, "Created\r\n");
    }
    else if (code == RQ_CL)
    {
        sprintf(codeName, "Bad Request\r\n");
    }

    char rqHeader[100] = "HTTP/1.1 ";
    sprintf(rqHeader, "%s%d %s", rqHeader, code, codeName);
    string_concat(response, rqHeader);

    if (body.length != 0)
    {
        char ctHeaders[100] = "Content-Type: text/plain\r\n";
        sprintf(ctHeaders, "%sContent-Length: %d\r\n\r\n", ctHeaders, body.length);
        string_concat(response, ctHeaders);
        string_concat(response, body.str);
    }

    string_concat(response, "\r\n");

    strFree(&body);
}

bool isBoards(char url[])
{
    string str;
    strInit(&str);
    bool result;

    for (int i = 1; i <= strlen(url); i++)
    {
        if (url[i] == '/' || url[i] == '\0')
        {
            if (strcmp(str.str, "boards") == 0)
            {
                result = true;
                break;
            }
            else
            {
                result = false;
                break;
            }
        }
        else
        {
            strAddChar(&str, url[i]);
        }
    }

    strFree(&str);
    return result;
}

void initList(tList *L)
{
    L->First = NULL;
}

int newBoard(tList *L, char name[])
{
    if (strlen(name) > MAX_NAME)
    {
        disposeList(L);
        fprintf(stderr, "Name is too long!");
        exit(1);
    }
    else
    {
        tBoardPtr tmp = findByName(L, name);
        if (tmp != NULL)
        {
            return RQ_EXISTS;
        }
    }

    tBoardPtr newBoard = malloc(sizeof(struct tBoard));

    if (newBoard == NULL)
    {
        disposeList(L);
        fprintf(stderr, "Malloc error!");
        exit(1);
    }
    else
    {
        strcpy(newBoard->name, name);
        newBoard->First = NULL;
        newBoard->Last = NULL;

        if (L->First != NULL)
        {
            L->First->pPtr = newBoard;
        }
        newBoard->nPtr = L->First;
        newBoard->pPtr = NULL;

        L->First = newBoard;

        return RQ_CREATED;
    }
}

tBoardPtr findByName(tList *L, char name[])
{
    tBoardPtr tmp = L->First;

    while (tmp != NULL)
    {
        if (strcmp(tmp->name, name) == 0)
        {
            return tmp;
        }
        else
        {
            tmp = tmp->nPtr;
        }
    }

    return NULL;
}

tElemPtr findById(tBoardPtr B, int id)
{
    int tmpId = 1;
    tElemPtr tmp = B->First;

    while (tmpId != id)
    {
        tmp = tmp->nPtr;
        tmpId++;

        if (tmp == NULL)
        {
            return NULL;
        }
    }

    return tmp;
}

int newPost(tList *L, char name[], char content[])
{
    tBoardPtr tmp = findByName(L, name);

    if (tmp == NULL)
    {
        return RQ_NOT_FOUND;
    }

    tElemPtr newPost = malloc(sizeof(struct tElem));

    if (newPost == NULL)
    {
        disposeList(L);
        fprintf(stderr, "Malloc error!");
        exit(1);
    }
    else
    {
        strcpy(newPost->data, content);
        newPost->nPtr = NULL;

        if (tmp->First == NULL)
        {
            tmp->First = newPost;
            tmp->Last = newPost;
            newPost->pPtr = NULL;
        }
        else
        {
            newPost->pPtr = tmp->Last;
            tmp->Last->nPtr = newPost;
            tmp->Last = newPost;
        }

        return RQ_CREATED;
    }
}

int deleteBoard(tList *L, char name[])
{
    tBoardPtr tmp = findByName(L, name);

    if (tmp == NULL)
    {
        return RQ_NOT_FOUND;
    }

    if (tmp == L->First)
    {
        L->First = tmp->nPtr;
    }

    tBoardPtr next = tmp->nPtr;
    tBoardPtr prev = tmp->pPtr;

    if (tmp->pPtr != NULL)
    {
        tmp->pPtr->nPtr = next;
    }

    if (tmp->nPtr != NULL)
    {
        tmp->nPtr->pPtr = prev;
    }

    disposeBoard(tmp);

    return RQ_OK;
}

int getBoards(tList *L, string *str)
{
    strClear(str);
    tBoardPtr tmp = L->First;

    if (tmp == NULL)
    {
        return RQ_NOT_FOUND;
    }

    while (tmp != NULL)
    {
        string_concat(str, tmp->name);
        string_concat(str, "\n");
        tmp = tmp->nPtr;
    }

    return RQ_OK;
}

int getPosts(tList *L, char name[], string *str)
{
    tBoardPtr tmp = findByName(L, name);
    if (tmp == NULL)
    {
        return RQ_NOT_FOUND;
    }

    char tmpName[23];
    sprintf(tmpName, "[%s]\n", name);
    string_concat(str, tmpName);

    tElemPtr post = tmp->First;

    int id = 1;
    char cId[10];

    while (post != NULL)
    {
        sprintf(cId, "%d", id);
        string_concat(str, cId);
        string_concat(str, ". ");
        string_concat(str, post->data);
        string_concat(str, "\n");

        id++;
        post = post->nPtr;
    }

    return RQ_OK;
}

int changePost(tList *L, char name[], int id, char content[])
{
    tBoardPtr tmp = findByName(L, name);
    if (tmp == NULL)
    {
        return RQ_NOT_FOUND;
    }

    tElemPtr post = findById(tmp, id);
    if (post == NULL)
    {
        return RQ_NOT_FOUND;
    }

    sprintf(post->data, content);

    return RQ_OK;
}

int deletePost(tList *L, char name[], int id)
{
    tBoardPtr tmp = findByName(L, name);
    if (tmp == NULL)
    {
        return RQ_NOT_FOUND;
    }

    tElemPtr post = findById(tmp, id);
    if (post == NULL)
    {
        return RQ_NOT_FOUND;
    }

    if (post == tmp->First)
    {
        tmp->First = post->nPtr;
    }

    if (post == tmp->Last)
    {
        tmp->Last = post->pPtr;
    }

    tElemPtr next = post->nPtr;
    tElemPtr prev = post->pPtr;

    if (post->pPtr != NULL)
    {
        post->pPtr->nPtr = next;
    }

    if (post->nPtr != NULL)
    {
        post->nPtr->pPtr = prev;
    }

    free(post);

    return RQ_OK;
}

void disposeList(tList *L)
{
    tBoardPtr tmp;

    while (L->First != NULL)
    {
        tmp = L->First;

        if (tmp->First != NULL)
        {
            disposeBoard(tmp);
        }

        L->First = L->First->nPtr;
        free(tmp);
    }
}

void disposeBoard(tBoardPtr B)
{
    tElemPtr tmp;

    while (B->First != NULL)
    {
        tmp = B->First;

        B->First = B->First->nPtr;
        free(tmp);
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
