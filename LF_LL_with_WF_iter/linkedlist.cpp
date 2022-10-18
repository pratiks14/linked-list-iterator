#include "linkedlist.h"

Node* new_node(long int k, Node *next)
{
  Node *node = new Node();
  node->key = k;
  node->next.store(next);

  return node;
}

Node **set_new()
{
  Node** set = new Node*();
  Node *min, *max;

  max = new_node(LONG_MAX	, NULL);
  min = new_node(LONG_MIN	, max);
  set = &min;

  return set;
}

void set_delete(Node **set)
{
  Node *node, *next;

  node = *set;
  while (node != NULL) {
    next = node->next;
    free(node);
    node = next;
  }
  free(set);
}

int set_size(Node **set)
{
  int size = 0;
  Node *node;

  /* We have at least 2 elements, not counting sentinels */
  node = (*set)->next;
  while (node->next != NULL) {
    size++;
    node = node->next;
  }

  return size;
}
