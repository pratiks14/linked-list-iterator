#include<iostream>
#include<atomic>
#include "iterator.h"
using namespace std;

atomic<SnapCollector*> PSC = NULL;
int total_number_of_threads;
Node *head = NULL;  // head of the lock free linked list

SnapCollector* AcquireSnapCollector()
{
  SnapCollector* SC= PSC;
  if(SC!=NULL && SC->IsActive())
  {
    return SC;
  }
  SnapCollector* new_SC = new SnapCollector(total_number_of_threads);
  atomic_compare_exchange_strong(&PSC, &SC, new_SC);
  new_SC = PSC;
  return new_SC;
}

void CollectSnapshot(SnapCollector* SC, int tid)
{
  Node* curr = head;
  while(SC->IsActive())
  {
    if(is_marked_ref((long)curr))
    {
      SC->AddNode(curr, tid);
    }
    if(curr->next.load() == NULL) // curr is the last
    {
      SC->BlockFurtherNodes();
      SC->Deactivate();
    }
    curr = curr->next;
  }
  SC->BlockFurtherReports();
}

// #################################
// DISCUSS
// #################################
void ReconstructUsingReports(SnapCollector* SC)
{

}

SnapCollector* TakeSnapshot(int tid)
{
  SnapCollector* SC= AcquireSnapCollector();
  CollectSnapshot(SC, tid);
  ReconstructUsingReports(SC);
}

int main()
{
  cout<<"Enter Number of threads \n";
  cin>>total_number_of_threads;
  
  return 0;
}