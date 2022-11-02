#include<iostream>
#include<atomic>
//#include "harris.h"
#include "linkedlist.h"
using namespace std;



class Report
{
    public:
      Node* node;
      int action;                              //1 for insert or 2 for Delete or 3 for DUMMY
      Report* nextReport;
      bool read_report = false;

    Report operator = (Report const &obj)
    {
         Report res;
         res.node = obj.node;
         res.action = obj.action;
         res.nextReport = obj.nextReport;
         res.read_report = obj.read_report;
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
        this->node = n;
        this->next_CollectorNode.store(NULL);
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
        CollectorNode* tempNode1 = new CollectorNode(NULL);
        CollectorNode* tempNode2 = new CollectorNode(NULL);
        tempNode1->next_CollectorNode = tempNode2;
        this->head_sentinal = tempNode1;
        this->tail_sentinal = tempNode2;
        
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
        
        if(temp_tail_non_atomic == head_sentinal) // first node to be added
        {
          //cas(temp_tail->next, sentinelNode, collectNode);
          atomic_compare_exchange_strong(&temp_tail_non_atomic->next_CollectorNode, &tail_sentinal, collectNode);
          
          //cas(tail,temp_tail,temp_tail->next)
          atomic_compare_exchange_strong(&tail, &temp_tail_non_atomic, temp_tail_non_atomic->next_CollectorNode);
          return tail;
        }
        else if(temp_tail_non_atomic->node->key > collectNode->node->key)  // if tail's key is larger than the new node being added to snapshot LL
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
      return this->active;
    }
        
    
    void BlockFurtherNodes()
    {
      Node *largest_val_node = new Node(LONG_MAX, NULL);
      CollectorNode *dummynode = new CollectorNode(largest_val_node);
      dummynode->next_CollectorNode.store(tail_sentinal);
      
      CollectorNode *temp_tail = tail;
      //######################################
              //RECHECK-REDISCUSS-2
      //######################################
      //while(!atomic_compare_exchange_strong(&tail->next, &temp_tail, dummynode));
      //while(!atomic_compare_exchange_strong(&tail, &temp_tail, dummynode));

      // while(!atomic_compare_exchange_strong(&temp_tail->next_CollectorNode, &tail_sentinal, dummynode))
      // {
      //   temp_tail = tail;
      // }

      if(temp_tail->node->key != LONG_MAX)// if tail is not dummy node then proceed
            temp_tail->next_CollectorNode.store(dummynode);   // discussed in a meeting
      tail.store(temp_tail);
      
      /*while(!atomic_compare_exchange_strong(&tail, &temp_tail, temp_tail->next_CollectorNode)) // THIS IS NOT NEEDED IF THE ABOVE CAS SUCCEEDED
      {
        temp_tail = tail;
      }*/
    }

    void Deactivate()
    {
      this->active = false;
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

        // if this changes then recheck the "recons using repo" logic in main function
        // since will only run limited numbner of times, therefore WF
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