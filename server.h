/* Copyright 2023 <Afanasiuc Daniel> */
#ifndef SERVER_H_
#define SERVER_H_
#include "utils.h"

#define HMAX 1000

struct ll_node_t {
    void* data;
    struct ll_node_t* next;
};
typedef struct ll_node_t ll_node_t;

struct linked_list_t {
    ll_node_t* head;
    unsigned int data_size;
    unsigned int size;
};
typedef struct linked_list_t linked_list_t;

linked_list_t *ll_create(unsigned int data_size);
void ll_add_nth_node(linked_list_t* list, unsigned int n, const void* new_data);
ll_node_t *ll_remove_nth_node(linked_list_t* list, unsigned int n);
unsigned int ll_get_size(linked_list_t* list);
void ll_free(linked_list_t** pp_list);

struct info {
	char *key;
	char *value;
};
typedef struct info info;;

struct server_memory {
    int server_id;
	linked_list_t **buckets;
	unsigned int size;
};
typedef struct server_memory server_memory;

unsigned int hash_function_key(void *a);

int server_has_key(server_memory *server, char *key);

/** init_server_memory() -  Initializes the memory for a new server struct.
 * 							Make sure to check what is returned by malloc using DIE.
 * 							Use the linked list implementation from the lab.
 *
 * Return: pointer to the allocated server_memory struct.
 */
server_memory *init_server_memory();

/** free_server_memory() - Free the memory used by the server.
 * 						   Make sure to also free the pointer to the server struct.
 * 						   You can use the server_remove() function for this.
 *
 * @arg1: Server to free
 */
void free_server_memory(server_memory *server);

/**
 * server_store() - Stores a key-value pair to the server.
 *
 * @arg1: Server which performs the task.
 * @arg2: Key represented as a string.
 * @arg3: Value represented as a string.
 */
void server_store(server_memory *server, char *key, char *value);

/**
 * server_remove() - Removes a key-pair value from the server.
 *					 Make sure to free the memory of everything that is related to the entry removed.
 *
 * @arg1: Server which performs the task.
 * @arg2: Key represented as a string.
 */
void server_remove(server_memory *server, char *key);

/**
 * server_retrieve() - Gets the value associated with the key.
 * @arg1: Server which performs the task.
 * @arg2: Key represented as a string.
 *
 * Return: String value associated with the key
 *         or NULL (in case the key does not exist).
 */
char *server_retrieve(server_memory *server, char *key);

char **server_retrieve_all_keys(server_memory *server);

#endif /* SERVER_H_ */
