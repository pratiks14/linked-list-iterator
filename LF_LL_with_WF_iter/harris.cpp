#include "harris.h"
using namespace std;


void ReportDelete(Node *victim, int tid, atomic<SnapCollector*> PSC)
{
  Report *rep = new Report();
  rep->action = 2;    // code for delete
  rep->node = victim;
  rep->nextReport = NULL;
  if(PSC.load()->IsActive())
    PSC.load()->addReport(rep, tid);
}

void ReportInsert(Node *new_node, int tid, atomic<SnapCollector*> PSC)
{
  Report *rep = new Report();
  rep->action = 1;    // code for insert
  rep->node = new_node;
  rep->nextReport = NULL;
  if(PSC.load()->IsActive())
    PSC.load()->addReport(rep, tid);
}

/*
 * The five following functions handle the low-order mark bit that indicates
 * whether a node is logically deleted (1) or not (0).
 *  - is_marked_ref returns whether it is marked, 
 *  - (un)set_marked changes the mark,
 *  - get_(un)marked_ref sets the mark before returning the node.]
 *     
 * 
 *  NOTE: These are inline functions.
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


Node* harris_search(Node *head, long int key, Node **left_node, atomic<SnapCollector*> PSC, int tid) 
{
  Node *left_node_next = head;
  Node *right_node;
	
  do
    {
      Node *t = head;   // head
      Node *t_next = head->next.load(); // t and t_next are two iterator of our linked list
		
      /* 1. Find left_node and right_node */
      do 
      {
          if (!is_marked_ref((long) t_next)) // if not marked, it means that t is not marked for delete
          {
              *left_node = t;
              left_node_next = t_next;
          }
          else
          {
            ReportDelete(*left_node,tid, PSC.load());    // Report Delete
          }
          t = (Node *) get_unmarked_ref((long) t_next); // because marked ref is unaccessible

          if (t->next.load() == NULL)     // if t is the tail, then break
          {
              break;
          }
          t_next = t->next.load();
      }while (is_marked_ref((long) t_next) || (t->key < key));
    
      right_node = t;
  
      /* 2. Check that nodes are adjacent, if yes then we can return else we will have to help to physically remove it. */
      if (left_node_next == right_node) 
      {
          if (right_node->next.load()!=NULL && is_marked_ref((long)right_node->next.load()))     // if right node gets marked, restart
          {
              continue;
          }
          else
          {
              return right_node;
          }
      }

      
      /* 3. Remove one or more marked nodes (helping phase) */
      if(atomic_compare_exchange_strong(&(*left_node)->next, &left_node_next, right_node)) 
      {
          //report delete
          if (!(right_node->next && is_marked_ref((long)right_node->next.load())))
          {
              return right_node;
          }
      } 
    }while (1);
}


/*
 * harris_find returns whether there is a node in the list owning value val.
 */
long int harris_find(Node *head, long int key, atomic<SnapCollector*> PSC, int tid) 
{
  Node *right_node, *left_node;
  left_node = head;
	
  right_node = harris_search(head, key, &left_node, PSC.load(), tid);
    
  if ((right_node->next == NULL) || right_node->key != key)
  {
    return 0;
  }
  else 
  {
      //report insert
      ReportInsert(right_node, tid, PSC.load());
      return right_node->key;
  }
}


/*
 * harris_find inserts a new node with the given value val in the list
 * (if the value was absent) or does nothing (if the value is already present).
 */
int harris_insert(Node *head, long int key, atomic<SnapCollector*> PSC, int tid) 
{
  Node *newnode = NULL, *right_node, *left_node;
  left_node = head;
	
  do 
    {
      right_node = harris_search(head, key, &left_node, PSC.load(), tid);
      if (right_node->key == key)
      {
        ReportInsert(right_node, tid, PSC.load());    // is atomic getting affected because of load (DOUBT)
        return 0;
      }

      if (newnode == NULL)
      {
        newnode = new Node(key, right_node);
      }
      else
      {
        newnode->next = right_node;
      }

      if (atomic_compare_exchange_strong(&left_node->next, &right_node, newnode))
      {
        ReportInsert(newnode, tid, PSC.load());    // is atomic getting affected because of load (DOUBT)
        return 1;
      }
    }while(1);
}

int harris_delete(Node *head, long int key, atomic<SnapCollector*> PSC, int tid)
{
	Node *right_node, *right_node_next, *left_node;
	left_node = head;
	
	do
  {
		right_node = harris_search(head, key, &left_node, PSC.load(), tid);
		
    if (right_node->key != key)
			return 0;

		right_node_next = right_node->next;
		
    if (!is_marked_ref((long) right_node_next))
		{	
      if (atomic_compare_exchange_strong(&right_node->next, &right_node_next, (Node*)(get_marked_ref((long) right_node_next))))
				break;
    }
	} while(1);
	
  ReportDelete(right_node, tid, PSC.load());    // is atomic getting affected because of load (DOUBT). Check whether rightnode has to be reported or what

  if (!atomic_compare_exchange_strong(&left_node->next, &right_node, right_node_next))
  {
    right_node = harris_search(head, right_node->key, &left_node, PSC.load(), tid);
  }
		
	
  return 1;
}

// remove this, just to check the compilation of harris.cpp
/*
int main()
{
    atomic<SnapCollector*> psc = new SnapCollector(1);
    cout<<"Enter Your Choices below (ONLY LOCK FREE LINKED LIST program). \n\n\n";
    int choice;
    Node *head_loc = set_new();    // head holds the address of the head sentinel node
    long int input;

    while(1)
    {
        cout<<"Press '1' for inserting, '2' for deleting, '3' for printing and '4' for ending the program.\n";
        cin>>choice;

        if(choice == 1) // insert
        {
          cout<<"Enter the value you want to Insert : ";
          cin>>input;
          cout<<"\n";
          harris_insert(head_loc,input,NULL,0);
        }
        else if(choice == 2)  // delete
        {
          cout<<"Enter the value you want to Delete : ";
          cin>>input;
          cout<<"\n";
          harris_delete(head_loc, input, NULL, 0);
        }
        else if(choice == 3)  // print LL
        {
          Node *temp_head = head_loc;
           while(temp_head)
          {
            cout<<temp_head->key<<" -> ";
            temp_head = temp_head->next;
          }
          cout<<"\n\n\n";
        }
        else  // exit
        {
            break;
        }
    }
    return 0;
}
*/