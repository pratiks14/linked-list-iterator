#include<iostream>
#include<atomic>
#include "harris.h"
using namespace std;



class Report
{
    public:
      Node* node;
      int action;                              //1 for insert or 2 for Delete or 3 for DUMMY
      Report* nextReport;

    Report operator = (Report const &obj)
    {
         Report res;
         res.node = obj.node;
         res.action = obj.action;
         res.nextReport = obj.nextReport;
         return res;
    }
};

class CollectorNode
{
    public:
      Node* node;
      //atomic<CollectorNode> *next;  // pointer to an atomic obj
      atomic<CollectorNode*> next_CollectorNode;  // atomic pointer to obj
      //CollectorNode* next_CollectorNode;  //chng1

      CollectorNode(Node* n)
      {
        node = n;
        next_CollectorNode.store(NULL);
      }
};

 
class SnapCollector
{
    public:
      bool active = true;     //indicates if the snap collect field is currently active
      CollectorNode *head_sentinal, *tail_sentinal;     // end of the list node (make head->next = tail)
      atomic<CollectorNode*> tail;

    int number_of_threads;
    //Report *report;           // array of atomic reports
    atomic<Report*> *heads_of_reports;  // array of atomic report heads

    SnapCollector(int total_threads)
    {
        CollectorNode* tempNode(NULL);
        tail_sentinal = tempNode;
        tempNode->next_CollectorNode = tail_sentinal;
        head_sentinal  = tail_sentinal;

        tail.store(head_sentinal);  // initially tail is at the head.
        //report = new Report[total_threads];
        heads_of_reports = new atomic<Report*>[total_threads];
        for(int i=0;i<total_threads;i++)
        {
          //report[i].node=NULL;
          //report[i].action=0;
          //report[i].nextReport=NULL;
          heads_of_reports[i].store(NULL);
        }
    }
    

    CollectorNode* AddNode(Node* node, int tid)
    {
        CollectorNode* collectNode = new CollectorNode(node);
        collectNode->next_CollectorNode.store(tail_sentinal);
        //atomic<CollectorNode*> temp_tail_atomic;
        CollectorNode* temp_tail_non_atomic;  // BECAUSE of 'CAS' def, we have introduced 2 copies of tails. One it atomic \
                                                 and other is non-atomic 
        temp_tail_non_atomic = tail;


        if(tail.load()->next_CollectorNode != tail_sentinal)              // go to the tail
        {
          //atomic_compare_exchange_strong(&tail, &temp_tail_non_atomic , temp_tail_non_atomic->next_CollectorNode);
          atomic_compare_exchange_strong(&tail, &temp_tail_non_atomic , temp_tail_non_atomic->next_CollectorNode);
          temp_tail_non_atomic = temp_tail_non_atomic->next_CollectorNode;
        }
        //temp_tail_atomic.store(temp_tail_non_atomic->next);
        
        if(temp_tail_non_atomic->node->key > collectNode->node->key)
        {
          return tail;
        } 
        else
        {
          //######################################
              //RECHECK-REDISCUSS-1
          //######################################

          //cas(temp_tail->next, sentinelNode, collectNode);
          atomic_compare_exchange_strong(&temp_tail_non_atomic->next_CollectorNode, &tail_sentinal, collectNode);
          
          //cas(tail,temp_tail,temp_tail->next)
          atomic_compare_exchange_strong(&tail, &temp_tail_non_atomic, temp_tail_non_atomic->next_CollectorNode);
          return tail;
        }      
    }  

    void addReport(Report * report, int tid)
    {
      Report *temp = heads_of_reports[tid];
      if(temp == NULL || temp->action != 3)   // first node OR not blocked from adding further reports 
      {
        report->nextReport = temp;
        atomic_compare_exchange_strong(&heads_of_reports[tid], &temp, report);
      }
    }
        
        
    bool IsActive()
    {
      return active;
    }
        
    
    void BlockFurtherNodes()
    {
      Node* largest_val_node;
      largest_val_node->key=INT_MAX;
      largest_val_node->next=NULL;
      CollectorNode* dummynode = new CollectorNode(largest_val_node);
      dummynode->next_CollectorNode=tail_sentinal;
      
      CollectorNode* temp_tail;
      temp_tail = tail;
      //######################################
              //RECHECK-REDISCUSS-2
      //######################################
      //while(!atomic_compare_exchange_strong(&tail->nex, &temp_tail, dummynode));
      //while(!atomic_compare_exchange_strong(&tail, &temp_tail, dummynode));

      while(!atomic_compare_exchange_strong(&temp_tail->next_CollectorNode, &tail_sentinal, dummynode));
      while(!atomic_compare_exchange_strong(&tail, &temp_tail, temp_tail->next_CollectorNode)) // THIS IS NOT NEEDED IF THE ABOVE CAS SUCCEEDED
      {
        temp_tail = tail;
      }
    }

    void Deactivate()
    {
      active = false;
    }

    void BlockFurtherReports()
    {
      for(int i=0;i<number_of_threads;i++)
      {
        Report *dummy_report = new Report();
        dummy_report->action=3;
        dummy_report->node=NULL;
        dummy_report->nextReport=heads_of_reports[i];
        Report *temp = heads_of_reports[i];

        while(!atomic_compare_exchange_strong(&heads_of_reports[i], &temp, dummy_report))
        {
          temp = heads_of_reports[i];
        }
        dummy_report->nextReport=temp;
      }
    }

    void ReadPointers();
    void ReadReports();
};