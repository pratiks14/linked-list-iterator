#include <iostream>
#include <atomic>
#include <algorithm>
#include <vector>
#include "harris.h"
using namespace std;

atomic<SnapCollector *> PSC = NULL;
int total_number_of_threads;
Node *head = NULL; // head of the lock free linked list

SnapCollector *AcquireSnapCollector()
{
  SnapCollector *SC = PSC;
  if (SC != NULL && SC->IsActive())
  {
    return SC;
  }
  SnapCollector *new_SC = new SnapCollector(total_number_of_threads);
  atomic_compare_exchange_strong(&PSC, &SC, new_SC);
  new_SC = PSC;
  return new_SC;
}

void CollectSnapshot(SnapCollector *SC, int tid)
{
  Node *curr = head;
  while (SC->IsActive())
  {
    if(!is_marked_ref((long)curr))
    {
      SC->AddNode(curr, tid);
    }
    if (curr->next.load() == NULL) // curr is the last
    {
      cout<<"\n LAST NODE THAT WAS ASKED TO BE ADDED IN THE SNAPSHOT WAS "<< curr->key << "\n";
      cout<<"And the Snapshot before blocking is as follows: \n";
      CollectorNode *temp = SC->head_sentinal;
      while(temp)
      {
        if(temp->node)
        {
          cout<< temp->node->key << " -> ";
        }
        else
        {
          cout<<"COLLECTOR SENTINAL -> ";
        }
        temp = temp->next_CollectorNode.load();
      }
      cout<<"\n\n";

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
bool comparator(const Report &lhs, const Report &rhs)
{
  if (lhs.node->key != rhs.node->key)
    return lhs.node->key < rhs.node->key;
  else if (lhs.node != rhs.node)
    return lhs.node < rhs.node;
  else
    return lhs.action > rhs.action; // delete report will have higher pref
}

void ReconstructUsingReports(SnapCollector *SC)
{
  
  cout<<"And the Snapshot before RECONSTRUCTION is as follows: \n";
      CollectorNode *temp = SC->head_sentinal;
      while(temp)
      {
        if(temp->node)
        {
          cout<< temp->node->key << " -> ";
        }
        else
        {
          cout<<"COLLECTOR SENTINAL -> ";
        }
        temp = temp->next_CollectorNode.load();
      }
      cout<<"\n\n";
  
  int total_report_cnt = 0;
  vector<Report> arr_of_all_reports;

  for (int i = 0; i < total_number_of_threads; i++)
  {
    Report *curr_head = SC->heads_of_reports[i];
    curr_head = curr_head->nextReport; // ignore the dummy report
    while (curr_head)
    {
      total_report_cnt++;
      arr_of_all_reports.push_back(*curr_head);
      curr_head = curr_head->nextReport;
    }
  }

  sort(arr_of_all_reports.begin(), arr_of_all_reports.end(), &comparator);

  int report_number = 0;
  Node *work_in_progess_node = NULL;
  Node *iterator_original_LL = head;
  CollectorNode *prev_collected_LL = SC->head_sentinal, *iter_collected_LL = SC->head_sentinal->next_CollectorNode, *temp_collected_LL;

  while ((report_number < total_report_cnt))
  {
    work_in_progess_node = arr_of_all_reports[report_number].node;

    // if delete report, ignore untill this node's report
    if (arr_of_all_reports[report_number].action == 2)
    {
      while (arr_of_all_reports[report_number].node == work_in_progess_node)
      {
        report_number++;
      }

      // check whether the node is present in the collected node set
      while (iter_collected_LL && iter_collected_LL->node != work_in_progess_node && iter_collected_LL->node->key < work_in_progess_node->key)
      {
        if (is_marked_ref((long)(iter_collected_LL->next_CollectorNode.load())))
        {
          // help
          temp_collected_LL = (CollectorNode *)(get_unmarked_ref((long)(iter_collected_LL->next_CollectorNode.load())));
          atomic_compare_exchange_strong(&prev_collected_LL->next_CollectorNode, &iter_collected_LL, temp_collected_LL);
          iter_collected_LL = temp_collected_LL;
        }
        else
        {
          prev_collected_LL = iter_collected_LL;
          iter_collected_LL = iter_collected_LL->next_CollectorNode;
        }
      }
      // if found then remove it
      if (iter_collected_LL->node == work_in_progess_node) // please check whether the request will be ==
      {
        temp_collected_LL = (CollectorNode *)(get_unmarked_ref((long)(iter_collected_LL->next_CollectorNode.load())));
        // logical removal
        iter_collected_LL->next_CollectorNode = (CollectorNode *)(get_marked_ref((long)(iter_collected_LL->next_CollectorNode.load())));
        // physical removal
        atomic_compare_exchange_strong(&prev_collected_LL->next_CollectorNode, &iter_collected_LL, temp_collected_LL);
      }
    }
    // else insert report, then ignore untill this node's report
    else if (arr_of_all_reports[report_number].action == 1)
    {
      while (arr_of_all_reports[report_number].node == work_in_progess_node)
      {
        report_number++;
      }
      // check whether the node is present in the collected node set
      while (iter_collected_LL && iter_collected_LL->node != work_in_progess_node && iter_collected_LL->node->key < work_in_progess_node->key)
      {
        if (is_marked_ref((long)(iter_collected_LL->next_CollectorNode.load())))
        {
          // help
          temp_collected_LL = (CollectorNode *)(get_unmarked_ref((long)(iter_collected_LL->next_CollectorNode.load())));
          atomic_compare_exchange_strong(&prev_collected_LL->next_CollectorNode, &iter_collected_LL, temp_collected_LL);
          iter_collected_LL = temp_collected_LL;
        }
        else
        {
          prev_collected_LL = iter_collected_LL;
          iter_collected_LL = iter_collected_LL->next_CollectorNode;
        }
      }
      // if no then insert it
      if (iter_collected_LL->node == work_in_progess_node)
      {
        CollectorNode *new_node = new CollectorNode(work_in_progess_node);
        new_node->next_CollectorNode.store(prev_collected_LL->next_CollectorNode.load());
        atomic_compare_exchange_strong(&prev_collected_LL->next_CollectorNode, &iter_collected_LL, new_node);
      }
    }
  }
}

SnapCollector *TakeSnapshot(int tid)
{
  SnapCollector *SC = AcquireSnapCollector();
  CollectSnapshot(SC, tid);
  ReconstructUsingReports(SC);
  return SC;
}

int main()
{
  cout << "Try 18 \n\n";
  cout << "Enter Number of threads \n";
  cin >> total_number_of_threads;

  cout << "Enter Your Choices below (ONLY LOCK FREE LINKED LIST program). \n\n\n";
  int choice;
  head = set_new(); // head holds the address of the head sentinel node
  long int input;

  PSC = new SnapCollector(total_number_of_threads);

  while (1)
  {
    cout << "Press '1' for inserting, '2' for deleting, '3' for printing, '4' for snapshot and '5' for ending the program.\n";
    cin >> choice;

    if (choice == 1) // insert
    {
      cout << "Enter the value you want to Insert : ";
      cin >> input;
      cout << "\n";
      harris_insert(head, input, PSC.load(), 0);
    }
    else if (choice == 2) // delete
    {
      cout << "Enter the value you want to Delete : ";
      cin >> input;
      cout << "\n";
      harris_delete(head, input, PSC.load(), 0);
    }
    else if (choice == 3) // print LL
    {
      Node *temp_head = head;
      while (temp_head)
      {
        cout << temp_head->key << " -> ";
        temp_head = temp_head->next;
      }
      cout << "\n\n\n";
    }
    else if (choice == 4) // print LL
    {
      // take the snapshot
      SnapCollector *SC = TakeSnapshot(1);

      // print the snapshot
      CollectorNode *temp_head = SC->head_sentinal->next_CollectorNode;
      cout << "\n -Infinity -> ";
      while (temp_head->node)
      {
        if (temp_head->node->key != LONG_MAX) // if not dummy nodes that were used for blocking nodess
        {
          cout << temp_head->node->key << " -> ";
        }
        temp_head = temp_head->next_CollectorNode;
      }
      cout << " +Infinity \n\n\n";
    }
    else // exit
    {
      break;
    }
  }
  return 0;
}