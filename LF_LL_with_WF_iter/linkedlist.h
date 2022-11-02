#include<iostream>
#include<atomic>
#include<limits.h>
using namespace std;


class Node 
{
  public:
  long int key;
  atomic<Node*> next;

  Node(long int k, Node *next)
  {
    this->key = k;
    this->next.store(next);
  }
};

//Node *new_node(long int key, Node *next);
Node *set_new();
void set_delete(Node **set);
int set_size(Node **set);