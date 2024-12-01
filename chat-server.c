#include "http-server.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

/**
 * GNEREAL STRUCTURE OF THE FILE ->
 * 
 * HTTP constant codes
 * 
 * get_time() -- function to easily get time string
 * hext_to_byte(char) -- for url decoding purposes
 * url_decode(char, char) -- for url decoding purposes
 * 
 * Struct objects:
 *      Reaction
 *      Chat
 *      ChatList
 * 
 * Constructors for each struct object type
 * 
 * Chat server methods:
 *      uint8_t add_chat(char* username, char* message)
 *      uint8_t add_reaction(char* username, char* message, char* id)
 *      unit8_t reset()
 * 
 * Handler methods:
 *      Error handling: 404
 *      handle_root()
 *      handle_chat()
 *      handle_path()
 *      handle_react()
 *      handle_reset()
 *      handle_response()
 * 
 * main()
 */


/**
 * HTTP Codes and Errors
 * 
 * 404: NOT FOUND
 * 200: OK
 * 500: INTERNAL SERVER
 */
char const HTTP_404_NOT_FOUND[] = "HTTP/1.1 404 Not found\r\nContent-Type: text/plain\r\n\r\n";
char const HTTP_200_OK[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
char const HTTP_500_INTERNAL_SERVER[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\n";

uint32_t chat_id = 0;

/**
 * time module to get the current time
 * 
 * @return "YYY-MM-DD HH:MM"
 */
char* get_time(){
    time_t t = time(NULL);

    //converts to local time
    struct tm tm = *localtime(&t);

    // "YYYY-MM-DD HH:MM:SS" requires 19 characters and 1 for null terminator
    static char time_str[20];
    
    //formats the time into the timr_str string
    snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d", 
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);

    //returns a string in format "20XX-MM-DD HH:MM:SS"
    return time_str;
}


/**
 * Code for url decoding
 */
uint8_t hex_to_byte(char c) {
	if ('0' <= c && c <= '9') return c - '0';
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	
    return -1;
}

void url_decode(char *src, char *dest){
	int i = 0;
	char *dest_ptr = dest;

	while (*(src+i) != 0){
		if(*(src+i)=='%'){
			uint8_t a = hex_to_byte(*(src+i+1));
		    uint8_t b = hex_to_byte(*(src+i+2));
			
            *dest_ptr=(unsigned char)((a<< 4)| b);
			dest_ptr++;
			
            i +=2; 
		} 
        else {
			*dest_ptr = *(src+i);
			dest_ptr++;
		}	
		i++;
	}
	*dest_ptr = 0;
}


/**
 * Objects:
 * 
 * Reaction
 * Chat
 * ChatList
 */
struct Reaction{
    char *user;
    char *message;
};
typedef struct Reaction Reaction;

struct Chat{
    uint32_t id;
    char *user;
    char *message;
    char *timestamp;
    uint32_t num_reactions;
    Reaction *reactions;
};
typedef struct Chat Chat;

struct ChatList{
    int size;
    int capacity;
    struct Chat *chat;
};
typedef struct ChatList ChatList;


/**
 * Constructors for:
 * 
 * Reaction
 * Chat
 * ChatList
 */
Reaction *new_reaction(char *username, char* message){
    Reaction *newReaction = malloc(sizeof(Reaction));

    newReaction->user = malloc(16);
    strncpy(newReaction->user, username, 15);
    newReaction->user[15] = '\0';

    newReaction->message = malloc(16);
    strncpy(newReaction->message, message, 15);
    newReaction->message[15] = '\0';

    return newReaction;    
}

Chat *new_chat(char *username, char *message) {
    Chat *newChat = malloc(sizeof(Chat));

    newChat->id = chat_id;

    newChat->user = malloc(16);
    strncpy(newChat->user, username, 15);
    newChat->user[15] = '\0';

    newChat->message = malloc(256);
    strncpy(newChat->message, message, 255);
    newChat->message[255] = '\0';

    //allocate memory for the timestamp and copy the value
    char *current_time = get_time();
    newChat->timestamp = malloc(strlen(current_time) + 1);
    strcpy(newChat->timestamp, current_time);

    newChat->num_reactions = 0;
    newChat->reactions = malloc(100 * sizeof(Reaction));

    return newChat;
}


ChatList *new_chat_list(){
    ChatList *newChatList = malloc(sizeof(ChatList));

    newChatList->chat = malloc(5 * sizeof(Chat));

    newChatList->size = 0;
    newChatList->capacity = 5;

    return newChatList;
}


/**
 * Chat server methods:
 *   uint8_t add_chat(char* username, char* message)
 *   uint8_t add_reaction(char* username, char* message, char* id)
 */
ChatList *chatList = NULL;

uint8_t add_chat(char *username, char *message){
    if(!chatList){
        chatList = new_chat_list();
    }

    //max capacity reached
    if(chatList->size >= 100000){
        return 0;
    }

    //dynamically allocate memory if size >= capacity
    if(chatList->size >= chatList->capacity){
        int new_capacity = chatList->capacity * 2;

        //making sure it does not exceed 100k capacity limit
        if(new_capacity > 100000){
            new_capacity = 100000;
        }

        Chat *new_array = realloc(chatList->chat, sizeof(Chat) * new_capacity);

        chatList->chat = new_array;
        chatList->capacity = new_capacity;
    }

    //adding and updating fields once new chat gets added
    Chat *newChat = new_chat(username, message);

    chatList->chat[chatList->size] = *newChat;
    chatList->size++;
    chat_id++;

    free(newChat);

    return 1;
}

uint8_t add_reaction(int id, char *username, char *message) {
    //check if the specific chat already has 100 reactions
    if (chatList->chat[id].num_reactions >= 100) {
        return 0;
    }

    Reaction *newReaction = new_reaction(username, message);

    //add the new reaction to the specific chat
    chatList->chat[id].reactions[chatList->chat[id].num_reactions] = *newReaction;
    chatList->chat[id].num_reactions++;

    free(newReaction);

    return 1;
}



/**
 * Handler methods for the following errors and paths:
 * 
 * Error:
 *      404
 *      200
 *      500
 * 
 * respond_with_chat(int, char*) -- prints out all the chats
 * 
 * Paths:
 *      /        --> root path
 *      /chat
 * 
 *      
 *      string_separater(char*, char*)
 *      /post
 *
 * 
 *      /react
 *      /reset
 */
void handle_404(int client_socket, char *path) {
    printf("SERVER LOG: Got requests for unrecognized path: \"%s\"\r\n", path);

    char response_buff[BUFFER_SIZE];
    
    //use snprintf to ensure buffer size is respected
    snprintf(response_buff, BUFFER_SIZE, "Error 404: \r\nUnrecognized path \"%s\"\r\n", path);

    //send response back to client
    write(client_socket, HTTP_404_NOT_FOUND, strlen(HTTP_404_NOT_FOUND));
    write(client_socket, response_buff, strlen(response_buff));
}

void respond_with_chats(int client_socket, char *path) {
    char reaction_user_space[21] = "                    "; //20 spaces + null terminator

    //iterate through all chats
    for (int i = 0; i < chat_id; i++) {
        char padded_username[16]; //fixed size for right-aligned username
        snprintf(padded_username, sizeof(padded_username), "%15s", chatList->chat[i].user);

        //write chat details
        char message[BUFFER_SIZE];
        snprintf(message, BUFFER_SIZE, "[#%u %s] %s : %s\n",
                 chatList->chat[i].id + 1,
                 chatList->chat[i].timestamp,
                 padded_username,
                 chatList->chat[i].message);
        write(client_socket, message, strlen(message));

        //write reactions if present
        for (int j = 0; j < chatList->chat[i].num_reactions; j++) {
            char reaction_line[BUFFER_SIZE];
            snprintf(reaction_line, BUFFER_SIZE, "%*s(%s) %s\n",
                     18 - (int)strlen(chatList->chat[i].reactions[j].user),
                     reaction_user_space,
                     chatList->chat[i].reactions[j].user,
                     chatList->chat[i].reactions[j].message);
            write(client_socket, reaction_line, strlen(reaction_line));
        }
    }
}

void handle_root(int client_socket, char *path){
    write(client_socket, HTTP_200_OK, strlen(HTTP_200_OK));

    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), 
        "Different requests you can make:\n"
        "/chats                                           -- for all chats\n"
        "/post?user=<username>&message=<message>          -- to post a chat\n"
        "/react?id=<id>&user=<username>&message=<message> -- to add a reaction\n"
        "/reset                                           -- to reset everything\n");

    write(client_socket, message, strlen(message));
}

void handle_chat(int client_socket, char *path){
    write(client_socket, HTTP_200_OK, strlen(HTTP_200_OK));
    if(chatList == NULL){
        write(client_socket, "No chats available\n", strlen("No chats available\n"));
        return;
    }

    respond_with_chats(client_socket, path);
}


char* separate(char* key, char* path, int limit, int *error_flag) {
    char *key_pointer = strstr(path, key);
    if (!key_pointer) return NULL;

    key_pointer += strlen(key);

    //allocate memory for the extracted value
    char *value = malloc(limit + 1); //include space for null terminator
    if (!value) return NULL;

    int i = 0;
    while (i < limit && key_pointer[i] != '&' && key_pointer[i] != '\0') {
        value[i] = key_pointer[i];
        i++;
    }

    if (key_pointer[i] != '&' && key_pointer[i] != '\0') {
        //if the string exceeds the limit
        *error_flag = 1;
        free(value);
        return NULL;
    }

    value[i] = '\0'; //null-terminate the extracted string
    return value;
}



void handle_post(int client_socket, char *path) {
    write(client_socket, HTTP_200_OK, strlen(HTTP_200_OK));

    int error_flag = 0;

    //separate username and message with validation
    char *username = separate("user=", path, 15, &error_flag);
    char *message = separate("message=", path, 255, &error_flag);

    if (error_flag) {
        write(client_socket, "Error: Username or message exceeds allowed length\n",
              strlen("Error: Username or message exceeds allowed length\n"));
        free(username);
        free(message);
        return;
    }

    if (!username || !message) {
        write(client_socket, "Error: Invalid username or message\n",
              strlen("Error: Invalid username or message\n"));
        free(username);
        free(message);
        return;
    }

    //add the chat and print all chats
    if (add_chat(username, message) == 0) {
        write(client_socket, "Error: Maximum number of chats reached\n",
              strlen("Error: Maximum number of chats reached\n"));
    }

    free(username);
    free(message);

    respond_with_chats(client_socket, path);
}



void handle_react(int client_socket, char *path) {
    if (chatList == NULL) {
        write(client_socket, "Error: No chats available to add reactions to.\n",
              strlen("Error: No chats available to add reactions to.\n"));
        return;
    }

    int error_flag = 0;

    // Separate id, username, and message with validation
    char *id_str = separate("id=", path, 7, &error_flag);
    char *username = separate("user=", path, 15, &error_flag);
    char *message = separate("message=", path, 15, &error_flag);

    if (error_flag) {
        write(client_socket, HTTP_500_INTERNAL_SERVER, strlen(HTTP_500_INTERNAL_SERVER)); // Add the status line
        write(client_socket, "Error: Username or reaction exceeds allowed length\n",
              strlen("Error: Username or reaction exceeds allowed length\n"));
        free(id_str);
        free(username);
        free(message);
        return;
    }

    if (!id_str || !username || !message) {
        write(client_socket, HTTP_500_INTERNAL_SERVER, strlen(HTTP_500_INTERNAL_SERVER)); // Add the status line
        write(client_socket, "Error: Invalid id, username, or message\n",
              strlen("Error: Invalid id, username, or message\n"));
        free(id_str);
        free(username);
        free(message);
        return;
    }

    // Check if id is valid
    int id = atoi(id_str) - 1;
    free(id_str); // No longer needed
    if (id < 0 || id >= chatList->size) {
        write(client_socket, HTTP_500_INTERNAL_SERVER, strlen(HTTP_500_INTERNAL_SERVER)); // Add the status line
        write(client_socket, "Error: Chat with the specified ID does not exist.\n",
              strlen("Error: Chat with the specified ID does not exist.\n"));
        free(username);
        free(message);
        return;
    }

    // Add the reaction
    if (add_reaction(id, username, message) == 0) {
        write(client_socket, HTTP_500_INTERNAL_SERVER, strlen(HTTP_500_INTERNAL_SERVER)); // Add the status line
        write(client_socket, "Error: Maximum number of reactions on this chat reached\n",
              strlen("Error: Maximum number of reactions on this chat reached\n"));
        free(username);
        free(message);
        return;
    }
    else{
        write(client_socket, HTTP_200_OK, strlen(HTTP_200_OK));
    }

    free(username);
    free(message);

    respond_with_chats(client_socket, path);
}




void handle_reset(int client_socket, char *path) {
    if (chatList != NULL) {
        for (int i = 0; i < chatList->size; i++) {
            free(chatList->chat[i].reactions);
            free(chatList->chat[i].user);
            free(chatList->chat[i].message);
            free(chatList->chat[i].timestamp);
        }
        free(chatList->chat);
        free(chatList);
        chatList = NULL;
    }

    chat_id = 0;

    write(client_socket, HTTP_200_OK, strlen(HTTP_200_OK));
}


/**
 * Handles actions based on the path type:
 * 
 * /chats --> prints out all chats
 * /post --> posts the chat and prints out all chats
 * /react --> adds a new reaction in the given chat id
 * /reset --> removes everything and frees the memory
 * 
 */
/**
 * Handles actions based on the path type:
 * 
 * /chats --> prints out all chats
 * /post --> posts the chat and prints out all chats
 * /react --> adds a new reaction in the given chat id
 * /reset --> removes everything and frees the memory
 */
void handle_response(char *request, int client_socket) {
    char path[300];
    char path_decoded[300];

    printf("\nSERVER LOG: Received request: \"%s\"\n", request);

    //parse the path out of the request line
    if (sscanf(request, "GET %s", path) != 1) {
        printf("SERVER LOG: Invalid request line\n");
        handle_404(client_socket, "/invalid");
        return;
    }

    //decode the URL
    url_decode(path, path_decoded);
    printf("SERVER LOG: Decoded path: \"%s\"\n", path_decoded);

    //route the request to the appropriate handler
    if (strcmp(path_decoded, "/") == 0) {
        handle_root(client_socket, path_decoded);
    } 
    else if (strncmp(path_decoded, "/chats", strlen("/chats")) == 0) {
        printf("SERVER LOG: Handling /chats request\n");
        handle_chat(client_socket, path_decoded);
    } 
    else if (strncmp(path_decoded, "/post", strlen("/post")) == 0) {
        printf("SERVER LOG: Handling /post request\n");
        handle_post(client_socket, path_decoded);
    } 
    else if (strncmp(path_decoded, "/react", strlen("/react")) == 0) {
        printf("SERVER LOG: Handling /react request\n");
        handle_react(client_socket, path_decoded);
    } 
    else if (strncmp(path_decoded, "/reset", strlen("/reset")) == 0) {
        printf("SERVER LOG: Handling /reset request\n");
        handle_reset(client_socket, path_decoded);
    }
    else {
        printf("SERVER LOG: Unrecognized path: \"%s\"\n", path_decoded);
        handle_404(client_socket, path_decoded);
    }
}


int main(int argc, char* argv[]){
    int port = 0;
    if(argc >= 2){
        port = atoi(argv[1]);
    }

    start_server(&handle_response, port);
}
