//
// Created by gilad on 4/4/2023.
//

#ifndef _THREAD_H_
#define _THREAD_H_
#include <iostream>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include "utils.h"
#include "uthreads.h"

class Thread {
 public:
  Thread (int tid, thread_entry_point entry_point);

    Thread (int tid, bool thread_0) :
            tid (tid), state (READY), thread_quantum (0), thread_0(thread_0),
            sleep_until_this_quantum(0)
    {
        // initializes env to use the right stack, and to run from the function
        // 'entry_point', when we'll use siglongjmp to jump into the thread.
        sigsetjmp(this->env, 1);
        if (tid == 0){
          return;
        }
        sigemptyset (&this->env->__saved_mask);
        std::cout << "TESTING PRINT: finish with Thread 0 constructor." <<
        std::endl;
    }

  ~ Thread () = default;
  ThreadState get_state () const
  { return state; }
  void set_state (ThreadState new_state)
  { state = new_state; }
  int get_id () const
  { return tid; }
  sigjmp_buf env;
  void increment_t_quantum(){thread_quantum ++;}
  int get_t_t_quantum() const {return thread_quantum;}
  void set_sleep_until_this_quantum(int quantum){
    this->sleep_until_this_quantum = quantum;
  }
  int get_sleep_until_this_quantum(){
    return this->sleep_until_this_quantum;
  }
  bool is_sleep () const
  {
    return sleep;
  }
  void set_sleep (bool is_sleep)
  {
    this->sleep = is_sleep;
  }

 private:
  int tid;
  ThreadState state;
  int thread_quantum;
  char stack[STACK_SIZE];
  bool thread_0;
  int sleep_until_this_quantum;
  bool sleep;
};

#endif //_THREAD_H_
