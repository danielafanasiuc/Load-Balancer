/* Copyright 2023 <Afanasiuc Daniel> */
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#include "load_balancer.h"

unsigned int hash_function_servers(void *a) {
    unsigned int uint_a = *((unsigned int *)a);

    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = (uint_a >> 16u) ^ uint_a;
    return uint_a;
}

load_balancer *init_load_balancer() {
    load_balancer *balancer = (load_balancer *)malloc(sizeof(load_balancer));
    DIE(!balancer, "Could not allocate load balancer!");
    balancer->servers = NULL;
    balancer->hash_ring = NULL;
    balancer->server_count = 0;

    return balancer;
}

void hash_ring_add_server(load_balancer *main, int server_id)
{
    unsigned int tag, hashes[3];
    tag = server_id;
    hashes[0] = hash_function_servers(&tag) % MAX_HASH;
    tag = 1e5 + server_id;
    hashes[1] = hash_function_servers(&tag) % MAX_HASH;
    tag = 2 * 1e5 + server_id;
    hashes[2] = hash_function_servers(&tag) % MAX_HASH;
    for (int i = 0; i < 3 - 1; ++i) {
        for (int j = i; j < 3; ++j) {
            if (hashes[i] > hashes[j]) {
                unsigned int aux_hash = hashes[i];
                hashes[i] = hashes[j];
                hashes[j] = aux_hash;
            }
        }
    }

    int server_index = REPLIC_SIZE * (main->server_count - 1);

    main->hash_ring[server_index].server_hash = hashes[0];
    main->hash_ring[server_index].server_index = main->server_count - 1;
    main->hash_ring[server_index + 1].server_hash = hashes[1];
    main->hash_ring[server_index + 1].server_index = main->server_count - 1;
    main->hash_ring[server_index + 2].server_hash = hashes[2];
    main->hash_ring[server_index + 2].server_index = main->server_count - 1;
}

void sort_replica(load_balancer *main, int replica)
{
    // sort the servers on the hash ring
    int max_server = REPLIC_SIZE * main->server_count;
    for (int i = 0; i < (max_server - 1) - REPLIC_SIZE + replica; ++i) {
        for (int j = i + 1; j < (max_server) - REPLIC_SIZE + replica; ++j) {
            if (main->hash_ring[i].server_hash >
                main->hash_ring[j].server_hash) {
                hash_ring_info aux = main->hash_ring[i];
                main->hash_ring[i] = main->hash_ring[j];
                main->hash_ring[j] = aux;
            } else if (main->hash_ring[i].server_hash ==
                      main->hash_ring[j].server_hash) {
                if (main->servers[main->hash_ring[i].server_index].server_id >
                    main->servers[main->hash_ring[j].server_index].server_id) {
                    hash_ring_info aux = main->hash_ring[i];
                    main->hash_ring[i] = main->hash_ring[j];
                    main->hash_ring[j] = aux;
                }
            }
        }
    }
}

void get_server_indexes(load_balancer *main, int server_id,
                        int *index, int count)
{
    int cnt = 0;
    for (int i = 0; i < REPLIC_SIZE * main->server_count; ++i) {
        if  (main->servers[main->hash_ring[i].server_index].server_id ==
             server_id) {
            *index = i;
            cnt++;
        }
        if (cnt == count)
            return;
    }
}

void redistribute_objects(load_balancer *main, int server_id, int replica)
{
    int index;
    get_server_indexes(main, server_id, &index, replica);

    // retrieve all keys from the next server for all 3 replicas
    // if replica is last in the array, then the next server is the first one
    if (index ==
        (REPLIC_SIZE * main->server_count - 1) - REPLIC_SIZE + replica) {
        server_memory *next = &main->servers[main->hash_ring[0].server_index];
        server_memory *curr =
        &main->servers[main->hash_ring[index].server_index];
        char **keys = server_retrieve_all_keys(next);
        int key_count = main->servers[main->hash_ring[0].server_index].size;

        // get the interval in which the next server's keys are stored
        // on the newly added server
        unsigned int lower_hash_bound = main->hash_ring[index - 1].server_hash;
        unsigned int upper_hash_bound = main->hash_ring[index].server_hash;

        for (int i = 0; i < key_count; ++i) {
            unsigned int curr_key_hash = hash_function_key(keys[i]) % MAX_HASH;
            // if in bounds, put the key in the new server
            // and delete it from the old one
            if (curr_key_hash >= lower_hash_bound &&
                curr_key_hash < upper_hash_bound) {
                char *value = server_retrieve(next, keys[i]);

                server_store(curr, keys[i], value);
                server_remove(next, keys[i]);
            }
        }
        free(keys);
    } else {  // if it isn't on last pos, simply get the next server
        char **keys = server_retrieve_all_keys
        (&main->servers[main->hash_ring[index + 1].server_index]);
        int key_count =
        main->servers[main->hash_ring[index + 1].server_index].size;

        // if replica is first in the array,
        // then the previous server is the last one in the array
        if (index == 0) {
            // get the interval in which we store
            // next server's keys on the newly added server
            unsigned int lower_hash_bound =
            main->hash_ring
            [(REPLIC_SIZE * main->server_count - 1) - REPLIC_SIZE + replica]
            .server_hash;
            unsigned int upper_hash_bound = main->hash_ring[0].server_hash;

            for (int i = 0; i < key_count; ++i) {
                unsigned int curr_key_hash =
                hash_function_key(keys[i]) % MAX_HASH;
                // if in bounds, put the key in the new server,
                // and delete it from the old one
                if (curr_key_hash >= lower_hash_bound ||
                    curr_key_hash < upper_hash_bound) {
                    char *value = server_retrieve
                    (&main->servers[main->hash_ring[index + 1].server_index],
                    keys[i]);

                    server_store
                    (&main->servers[main->hash_ring[index].server_index],
                    keys[i], value);
                    server_remove
                    (&main->servers[main->hash_ring[index + 1].server_index],
                    keys[i]);
                }
            }
        } else {
            // if it isn't on first pos, simply get the
            // hash of the previous server
            // get the interval in which we store next server's
            // keys on the newly added server
            unsigned int lower_hash_bound =
            main->hash_ring[index - 1].server_hash;
            unsigned int upper_hash_bound =
            main->hash_ring[index].server_hash;

            for (int i = 0; i < key_count; ++i) {
                unsigned int curr_key_hash =
                hash_function_key(keys[i]) % MAX_HASH;
                // if in bounds, put the key in the new server,
                // and delete it from the old one
                if (curr_key_hash >= lower_hash_bound &&
                curr_key_hash < upper_hash_bound) {
                    char *value = server_retrieve
                    (&main->servers[main->hash_ring[index + 1].server_index],
                    keys[i]);

                    server_store
                    (&main->servers[main->hash_ring[index].server_index],
                    keys[i], value);
                    server_remove
                    (&main->servers[main->hash_ring[index + 1].server_index],
                    keys[i]);
                }
            }
        }
        free(keys);
    }
}

void loader_add_server(load_balancer *main, int server_id) {
    if (main->server_count == 0) {  // if there aren't any servers
        // allocate the server array
        main->servers = (server_memory *)malloc(sizeof(server_memory));
        DIE(!main->servers, "Could not allocate server!");
        // add the server to the server array
        server_memory *new_server = init_server_memory();
        main->servers[0] = *new_server;
        main->servers[0].server_id = server_id;
        free(new_server);
        // allocate the hash ring
        main->hash_ring =
        (hash_ring_info *)malloc(sizeof(hash_ring_info) * REPLIC_SIZE);
        DIE(!main->hash_ring, "Could not allocate hash ring!");

        main->server_count++;

        // add server on hash ring along with the replicas
        // and sort the hash ring
        hash_ring_add_server(main, server_id);

        return;
    }
    // if there are servers
    main->server_count++;

    // reallocate the server array
    server_memory *server_array_tmp =
    (server_memory *)realloc(main->servers,
    main->server_count * sizeof(server_memory));
    DIE(!server_array_tmp, "Could not reallocate server!");
    main->servers = server_array_tmp;
    // add the server to the server array
    server_memory *new_server = init_server_memory();
    main->servers[main->server_count - 1] = *new_server;
    main->servers[main->server_count - 1].server_id = server_id;
    free(new_server);
    // reallocate the hash ring
    hash_ring_info *hash_ring_tmp = (hash_ring_info *)realloc
    (main->hash_ring,
    main->server_count * sizeof(hash_ring_info) * REPLIC_SIZE);
    DIE(!hash_ring_tmp, "Could not reallocate hash ring!");
    main->hash_ring = hash_ring_tmp;

    // add server on hash ring along with the replicas and sort the hash ring
    hash_ring_add_server(main, server_id);

    for (int i = 1; i <= 3; ++i) {
        sort_replica(main, i);
        redistribute_objects(main, server_id, i);
    }
}

void loader_remove_server(load_balancer *main, int server_id) {
    for (int i = 0; i < main->server_count; ++i) {
        // if the removed server has been found
        if (main->servers[i].server_id == server_id) {
            // getting all the keys
            char **keys = server_retrieve_all_keys(&main->servers[i]);
            // getting all the keys's values
            char **values =
            (char **)malloc(sizeof(char *) * main->servers[i].size);
            DIE(!values, "Could not allocate all values of a server's keys!");
            for (unsigned int v = 0; v < main->servers[i].size; ++v) {
                values[v] = server_retrieve(&main->servers[i], keys[v]);
            }
            // making a deep copy of the keys
            char **deep_keys =
            (char **)malloc(sizeof(char *) * main->servers[i].size);
            DIE(!deep_keys,
            "Could not make a deep copy of a removed server's keys!");
            for (unsigned int k = 0; k < main->servers[i].size; ++k) {
                deep_keys[k] = malloc(sizeof(char) * (strlen(keys[k]) + 1));
                DIE(!deep_keys[k],
                "Could not make a shallow copy!");
                memcpy(deep_keys[k], keys[k], strlen(keys[k]) + 1);
            }
            // making a deep copy of the values
            char **deep_values =
            (char **)malloc(sizeof(char *) * main->servers[i].size);
            DIE(!deep_values, "Could not make a deep copy!");
            for (unsigned int v = 0; v < main->servers[i].size; ++v) {
                deep_values[v] = malloc(sizeof(char) * (strlen(values[v]) + 1));
                DIE(!deep_values[v],
                "Could not make a deep copy!");
                memcpy(deep_values[v], values[v], strlen(values[v]) + 1);
            }
            free(keys);
            free(values);
            // getting the number of keys and values
            int key_val_count = main->servers[i].size;
            // removing the server from the servers array
            free_server_memory(&main->servers[i]);
            for (int j = i; j < main->server_count - 1; ++j) {
                main->servers[j] = main->servers[j + 1];
            }
            // deleting the replicas from the hash ring
            int hash_ring_size = REPLIC_SIZE * main->server_count;
            for (int sv = hash_ring_size - 1; sv >= 0; --sv) {
                // if a replica is found
                if (main->hash_ring[sv].server_index == i) {
                    for (int sv2 = sv; sv2 < hash_ring_size - 1; ++sv2) {
                        main->hash_ring[sv2] = main->hash_ring[sv2 + 1];
                    }
                    hash_ring_size--;
                }
            }
            main->server_count--;
            // reallocating the server array
            server_memory *server_array_tmp =
            (server_memory *)realloc(main->servers,
            main->server_count * sizeof(server_memory));
            DIE(!server_array_tmp, "Could not reallocate server!");
            main->servers = server_array_tmp;
            // reallocating the hash ring
            hash_ring_info *hash_ring_tmp =
            (hash_ring_info *)realloc(main->hash_ring,
            main->server_count * sizeof(hash_ring_info) * REPLIC_SIZE);
            DIE(!hash_ring_tmp, "Could not reallocate hash ring!");
            main->hash_ring = hash_ring_tmp;

            for (int sv = 0; sv < REPLIC_SIZE * main->server_count; ++sv) {
                // decrement every index bigger than 'i'
                if (main->hash_ring[sv].server_index > i)
                    main->hash_ring[sv].server_index--;
            }
            // redistribute all keys
            for (int it = 0; it < key_val_count; ++it) {
                int sv_id;
                loader_store(main, deep_keys[it], deep_values[it], &sv_id);
            }
            // delete the deep copies
            for (int key_val = 0; key_val < key_val_count; ++key_val) {
                free(deep_keys[key_val]);
                free(deep_values[key_val]);
            }
            free(deep_keys);
            free(deep_values);
            break;
        }
    }
}

void loader_store(load_balancer *main, char *key, char *value, int *server_id)
{
    if (main->server_count == 0)
        return;

    unsigned int key_hash = hash_function_key(key) % MAX_HASH;
    // performing binary search to find server in which to store key
    int low = 0;
    int high = REPLIC_SIZE * main->server_count - 1;
    int mid = (low + high) / 2;

    while (low < high) {
        if (main->hash_ring[mid].server_hash > key_hash) {
            // if mid is not first in the array
            if (mid - 1 >= 0) {
                if (main->hash_ring[mid - 1].server_hash < key_hash) {
                    *server_id =
                    main->servers[main->hash_ring[mid].server_index].server_id;
                    server_store
                    (&main->servers[main->hash_ring[mid].server_index],
                    key, value);
                    return;
                } else {
                    high = mid - 1;
                    mid = (low + high) / 2;
                }
            } else {  // if mid is first in array check first server in array
                high = mid;
                break;
            }
        } else {
            low = mid + 1;
            mid = (low + high) / 2;
        }
    }
    // checking the first or the last server in array
    if (low == high) {
        if (main->hash_ring[high].server_hash > key_hash) {
            *server_id =
            main->servers[main->hash_ring[high].server_index].server_id;
            server_store
            (&main->servers[main->hash_ring[high].server_index], key, value);
            return;
        }
    }

    // it gets here if the hash is bigger than all server hashes
    // (then it gets stored back on 1st server)
    *server_id = main->servers[main->hash_ring[0].server_index].server_id;
    server_store(&main->servers[main->hash_ring[0].server_index], key, value);
}

char *loader_retrieve(load_balancer *main, char *key, int *server_id)
{
    if (main->server_count == 0)
        return NULL;

    unsigned int key_hash = hash_function_key(key) % MAX_HASH;
    // performing binary search to retrieve value
    int low = 0;
    int high = REPLIC_SIZE * main->server_count - 1;
    int mid = (low + high) / 2;

    while (low < high) {
        if (main->hash_ring[mid].server_hash > key_hash) {
            // if mid is not first in the array
            if (mid - 1 >= 0) {
                if (main->hash_ring[mid - 1].server_hash < key_hash) {
                    *server_id =
                    main->servers[main->hash_ring[mid].server_index].server_id;
                    return server_retrieve
                    (&main->servers[main->hash_ring[mid].server_index], key);
                } else {
                    high = mid - 1;
                    mid = (low + high) / 2;
                }
            } else {  // if mid is first in array check first server in array
                high = mid;
                break;
            }
        } else {
            low = mid + 1;
            mid = (low + high) / 2;
        }
    }
    // checking the first or the last server in array
    if (low == high) {
        if (main->hash_ring[high].server_hash > key_hash) {
            *server_id =
            main->servers[main->hash_ring[high].server_index].server_id;
            return server_retrieve
            (&main->servers[main->hash_ring[high].server_index], key);
        }
    }

    // it gets here if the hash is bigger
    // than all server hashes (then the 1st server is checked)
    char *value =
    server_retrieve(&main->servers[main->hash_ring[0].server_index], key);
    if (value) {
        *server_id = main->servers[main->hash_ring[0].server_index].server_id;
        return value;
    }

    return NULL;
}

void free_load_balancer(load_balancer *main) {
    free(main->hash_ring);
    for (int i = 0; i < main->server_count; ++i) {
        free_server_memory(&main->servers[i]);
    }
    free(main->servers);
    free(main);
}
