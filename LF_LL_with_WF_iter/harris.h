#include "linkedlist.h"
using namespace std;


/* ################################################################### *
 * HARRIS' LINKED LIST
 * ################################################################### */

inline int is_marked_ref(long i);
inline long unset_mark(long i);
inline long set_mark(long i);
inline long get_unmarked_ref(long w);
inline long get_marked_ref(long w);

Node* harris_search(Node **set, long int key, Node** left_node);
long int harris_find(Node **set, long int key);
int harris_insert(Node **set, long int key);
int harris_delete(Node **set, long int key);

