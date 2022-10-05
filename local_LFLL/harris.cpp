#include "harris.h"

RETRY_STATS_VARS;           // this is a group of "thread local" variables in latency.h, which i think haven't been used.

/*
 * The five following functions handle the low-order mark bit that indicates
 * whether a node is logically deleted (1) or not (0).
 *  - is_marked_ref returns whether it is marked, 
 *  - (un)set_marked changes the mark,
 *  - get_(un)marked_ref sets the mark before returning the node.]
 *     
 * 
 *     NOTE: These are inline functions.
 */
inline int is_marked_ref(long i) 
{
  /* return (int) (i & (LONG_MIN+1)); */
  return (int) (i & 0x1L);
}

inline long unset_mark(long i)
{
  /* i &= LONG_MAX-1; */
  i &= ~0x1L;
  return i;
}

inline long set_mark(long i) 
{
  /* i = unset_mark(i); */
  /* i += 1; */
  i |= 0x1L;
  return i;
}

inline long get_unmarked_ref(long w) 
{
  /* return unset_mark(w); */
  return w & ~0x1L;
}

inline long get_marked_ref(long w) 
{
  /* return set_mark(w); */
  return w | 0x1L;
}


/*
 1. harris_search looks for value val, it
   - returns right_node owning val (if present) or its immediately higher value present in the list (otherwise) and 
   - sets the "left_node" to the node owning the value immediately lower than val. 
   - Encountered nodes that are marked as logically deleted are physically removed  (helping) from the list, yet not garbage collected.
 
 2. The right node must be the immediate successor of the left node. This condition requires the search operation 
    to remove marked nodes from the list so that the left and right nodes are adjacent.
 */


/*
  - intset_t is just the structure with node pointer for head with cache aligned properties.
  - skey_t is just defined as "typedef intptr_t skey_t;", we have already seen intptr_t in harris.h
  - node_t is just the typecasted node struct, to avoid again and again writing "struct node"
*/

node_t* harris_search(intset_t *set, skey_t key, node_t **left_node) 
{
  node_t *left_node_next;
  node_t *right_node;
  left_node_next = set->head;
	
  do
    {
      PARSE_TRY();      // ignore. In latency.h, this is nothing since the condition is false there.
      node_t *t = set->head;
      node_t *t_next = set->head->next; // t and t_next are two iterator of our linked list
		
      /* 1. Find left_node and right_node */
      do 
      {
          if (!is_marked_ref((long) t_next)) 
          {
              (*left_node) = t;
              left_node_next = t_next;
          }
          t = (node_t *) get_unmarked_ref((long) t_next); // because marked ref is unaccessible

          if (!t->next)     // if t is the tail, then break
          {
              break;
          }
          t_next = t->next;
      }while (is_marked_ref((long) t_next) || (t->key < key));
    
      right_node = t;
  
      /* 2. Check that nodes are adjacent */
      if (left_node_next == right_node) 
      {
          if (right_node->next && is_marked_ref((long) right_node->next))
          {
              continue;
          }
          else
          {
              return right_node;
          }
      }
  
      CLEANUP_TRY();    // ignore.
      
      /* 3. Remove one or more marked nodes (helping phase) */
      if (ATOMIC_CAS_MB(&(*left_node)->next, left_node_next, right_node)) 
      {
          #if GC == 1         // this is something realted to garbage collection it seems, will have to search where is it!
          node_t* cur = left_node_next;
          do 
          {
              node_t* free = cur;
              cur = (node_t*) get_unmarked_ref((long) cur->next);
              ssmem_free(alloc, (void*) free);
          }while (cur != right_node);
          #endif

          if (!(right_node->next && is_marked_ref((long) right_node->next)))
          {
              return right_node;
          }
      } 
    }while (1);
}


/*
 * harris_find returns whether there is a node in the list owning value val.
 */
sval_t harris_find(intset_t *set, skey_t key) 
{
  node_t *right_node, *left_node;
  left_node = set->head;
	
  right_node = harris_search(set, key, &left_node);
  if ((right_node->next == NULL) || right_node->key != key)
    {
      return 0;
    }
  else 
    {
      return right_node->val;
    }
}


/*
 * harris_find inserts a new node with the given value val in the list
 * (if the value was absent) or does nothing (if the value is already present).
 */
int harris_insert(intset_t *set, skey_t key, sval_t val) 
{
  node_t *newnode = NULL, *right_node, *left_node;
  left_node = set->head;
	
  do 
    {
      UPDATE_TRY();
      right_node = harris_search(set, key, &left_node);
      if (right_node->key == key)
	{
#if GC == 1
	  if (unlikely(newnode != NULL))
	    {
	      ssmem_free(alloc, (void*) newnode);
	    }
#endif
	  return 0;
	}

      if (likely(newnode == NULL))
	{
	  newnode = new_node(key, val, right_node, 0);
	}
      else
	{
	  newnode->next = right_node;
	}
#ifdef __tile__
    MEM_BARRIER;
#endif
      if (ATOMIC_CAS_MB(&left_node->next, right_node, newnode))
	return 1;
    } 
  while(1);
}



int main()
{
    
    



    return 0;
}