/* Copyright 2023 <Afanasiuc Daniel> */
#include <stdlib.h>
#include <string.h>

#include "server.h"

unsigned int hash_function_key(void *a) {
    unsigned char *puchar_a = (unsigned char *)a;
    unsigned int hash = 5381;
    int c;

    while ((c = *puchar_a++))
        hash = ((hash << 5u) + hash) + c;

    return hash;
}

server_memory *init_server_memory()
{
	server_memory *server = (server_memory *)malloc(sizeof(server_memory));
	DIE(!server, "Could not allocate server memory!");
    server->server_id = 0;
	server->size = 0;

	server->buckets = (linked_list_t **)malloc(HMAX * sizeof(linked_list_t *));
	DIE(!server->buckets, "Could not allocate server buckets!");
	for (int i = 0; i < HMAX; ++i) {
	    server->buckets[i] = ll_create(sizeof(info));
	}

	return server;
}

int server_has_key(server_memory *server, char *key)
{
	if (!server)
	    return -1;

	int hash = hash_function_key(key) % HMAX;

	ll_node_t *curr = server->buckets[hash]->head;
	while (curr) {
	    if (strcmp(key, ((info *)curr->data)->key) == 0)
	        return 1;
	    curr = curr->next;
	}

	return 0;
}

void server_store(server_memory *server, char *key, char *value) {
	if (!server)
	    return;

	int hash = hash_function_key(key) % HMAX;

	if (server_has_key(server, key)) {  // if the key exists
	    free(server_retrieve(server, key));

	    ll_node_t *curr = server->buckets[hash]->head;
	    while (curr) {
    	    if (strcmp(key, ((info *)curr->data)->key) == 0)
    	        break;
    	    curr = curr->next;
	    }

		int value_size = strlen(value);
	    ((info *)curr->data)->value = malloc((value_size + 1) * sizeof(char));
		DIE(!((info *)curr->data)->value, "Could not allocate a value!");
	    memcpy(((info *)curr->data)->value, value, value_size + 1);

	} else {  // if the key does not exist
	    info information;
		int key_size = strlen(key);
	    information.key = malloc((key_size + 1) * sizeof(char));
		DIE(!information.key, "Could not allocate a key!");
	    memcpy(information.key, key, key_size + 1);
		int value_size = strlen(value);
	    information.value = malloc((value_size + 1) * sizeof(char));
		DIE(!information.value, "Could not allocate a value!");
	    memcpy(information.value, value, value_size + 1);
	    ll_add_nth_node(server->buckets[hash], 0, &information);
	    server->size++;
	}
}

char *server_retrieve(server_memory *server, char *key) {
	if (!server || !server_has_key(server, key))
	    return NULL;

	int hash = hash_function_key(key) % HMAX;

	ll_node_t *curr = server->buckets[hash]->head;
	while (curr) {
	    if (strcmp(key, ((info *)curr->data)->key) == 0)
	        return ((info *)curr->data)->value;
	    curr = curr->next;
	}

	return NULL;
}

void server_remove(server_memory *server, char *key) {
	if (!server || !server_has_key(server, key))
	    return;

	int hash = hash_function_key(key) % HMAX;
	int cnt = 0;

	ll_node_t *curr = server->buckets[hash]->head;
    while (curr) {
	    if (strcmp(key, ((info *)curr->data)->key) == 0)
	        break;
	    cnt++;
	    curr = curr->next;
    }

    curr = ll_remove_nth_node(server->buckets[hash], cnt);
	free(((info *)curr->data)->key);
	free(((info *)curr->data)->value);
    free(curr->data);
    free(curr);
    server->size--;
}

void free_server_memory(server_memory *server) {
	for (int i = 0; i < HMAX; ++i) {
        ll_node_t *curr = server->buckets[i]->head;
        while (curr) {
            free(((info *)curr->data)->key);
			free(((info *)curr->data)->value);
    	    curr = curr->next;
        }
        ll_free(&server->buckets[i]);
    }
    free(server->buckets);
}

char **server_retrieve_all_keys(server_memory *server)
{
    if (server->size == 0)
        return NULL;
    char **keys = (char **)malloc(sizeof(char *) * server->size);
    int key_index = 0;
    DIE(!keys, "Could not allocate all keys to retrieve!");
    for (int k = 0; k < HMAX; ++k) {
        // if there are elements at k hash
        ll_node_t *curr = server->buckets[k]->head;
        while (curr) {
            keys[key_index++] = ((info *)curr->data)->key;
            curr = curr->next;
        }
    }

    return keys;
}

linked_list_t *ll_create(unsigned int data_size)
{
    linked_list_t* ll;

    ll = malloc(sizeof(*ll));
	DIE(!ll, "Could not allocate list!");
    ll->head = NULL;
    ll->data_size = data_size;
    ll->size = 0;

    return ll;
}

void ll_add_nth_node(linked_list_t* list, unsigned int n, const void* new_data)
{
    ll_node_t *prev, *curr;
    ll_node_t* new_node;

    if (!list) {
        return;
    }

    if (n > list->size) {
        n = list->size;
    }

    curr = list->head;
    prev = NULL;
    while (n > 0) {
        prev = curr;
        curr = curr->next;
        --n;
    }

    new_node = malloc(sizeof(*new_node));
	DIE(!new_node, "Could not allocate list node!");
    new_node->data = malloc(list->data_size);
    memcpy(new_node->data, new_data, list->data_size);

    new_node->next = curr;
    if (prev == NULL) {
        list->head = new_node;
    } else {
        prev->next = new_node;
    }

    list->size++;
}

ll_node_t *ll_remove_nth_node(linked_list_t* list, unsigned int n)
{
    ll_node_t *prev, *curr;

    if (!list || !list->head) {
        return NULL;
    }

    if (n > list->size - 1) {
        n = list->size - 1;
    }

    curr = list->head;
    prev = NULL;
    while (n > 0) {
        prev = curr;
        curr = curr->next;
        --n;
    }

    if (prev == NULL) {
        list->head = curr->next;
    } else {
        prev->next = curr->next;
    }

    list->size--;

    return curr;
}

unsigned int ll_get_size(linked_list_t* list)
{
     if (!list) {
        return -1;
    }

    return list->size;
}

void ll_free(linked_list_t** pp_list)
{
    ll_node_t* currNode;

    if (!pp_list || !*pp_list) {
        return;
    }

    while (ll_get_size(*pp_list) > 0) {
        currNode = ll_remove_nth_node(*pp_list, 0);
        free(currNode->data);
        currNode->data = NULL;
        free(currNode);
        currNode = NULL;
    }

    free(*pp_list);
    *pp_list = NULL;
}
