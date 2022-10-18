#include<iostream>
#include<atomic>
#include<limits.h>
using namespace std;


class Node 
{
  public:
  long int key;
  atomic<Node*> next;
};

Node *new_node(long int key, Node *next);
Node** set_new();
void set_delete(Node** set);
int set_size(Node **set);