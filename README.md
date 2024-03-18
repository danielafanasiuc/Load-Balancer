# Afanasiuc Daniel 312CA

# SERVER_STORE

* When adding a key to a server, it is checked if the key already exists on the hashtable, and if it does, the old value of the key is free'd and the new value is stored at the respective key.
* If the key does not exist, a deep copy of the the key and value is created and a node with the data being the key and the value is added in the list of the keys' hash.

# SERVER_REMOVE

* In the list of the keys hash, the key is searched and when found, its node is removed and the the node along with the data is free'd.

# LOADER_ADD_SERVER

* When adding a new server, its memory is added in array 'servers' and the replic's hash and the index of the replic's server is stored in hash_ring.
* Upon adding the server, the array of servers and the hash_ring is reallocated and the server's hashes are added on the last positions.
* Each replic is sorted and the objects in the first server with the bigger hash are redistributed if they are within bounds. The 2 special cases that are considered are if the newly added server is on the last position and thus the next server is the first one in the array (because the array is circular) and if the new server is on the first position and thus the lower bound is the hash of the server with the biggest hash in the array.

# LOADER_REMOVE_SERVER

* When removing a server, a deep copy of the keys and values is made and the server is removed from the servers array along with the replicas from the hash ring.
* After deleting the replicas from the hash ring, every index bigger than the index of the removed server is decremented and the keys get redistributed to the remaining servers.

#LOADER_STORE

* When storing a key, binary searched is performed on the sorted array of hashes to find the server in which the key is stored and if the hash at mid position is bigger than the key hash, it is searched if the server hash on position mid - 1 is smaller than the key hash. If so, the key is stored in the hashes' server.
* If mid - 1 is smaller than 0, then high gets the value of mid, so low == high and it is checked if the hash at the position 0 is bigger than the key hash. If so, store the key in the server.
* If binary search was performed without finding the server, the hash at position high is checked (when high == low)
* If no server was found, the hash of the key is bigger than all the server's hashes, so the key gets stored on the first server.

#LOADER_RETRIEVE

* Binary search is performed to find the server containing the key, in order to retrieve its value. (same as in LOADER_STORE).
* If no server was found after binary search, the hash of the key is bigger than all the server's hashes, so the value of the key is retrieved from the first server and if the key does not exist on the first server, the key does not exist on any of the servers.
