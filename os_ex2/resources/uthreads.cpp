//
// Created by gilad on 4/2/2023.
//

#include <iostream>
#include <deque>
#include <sys/time.h>
#include <signal.h>
#include <vector>
#include <unordered_map>
#include "Thread.h"
#define FAILURE_EXIT_MINUS_ONE (-1)
#define SUCCESS_EXIT 0

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address (address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
            : "=g" (ret)
            : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


#endif

// ################ GLOBAL VARIABLES ##################### //
std::deque<Thread*> ready_threads_list;
Thread *all_threads_list[MAX_THREAD_NUM]{nullptr};
std::unordered_map <int, std::vector<Thread*>> wake_up_quantum_to_sleeping_threads;
int total_quantum = 0;
int running_tid = -1;
int time_for_timer;

struct itimerval timer;
sigset_t blocked_signalarm = {0};


//int for_testing_clock_ticks;

// ############### END OF GLOBAL VARIABLES ################ //


// ################ NON LIBRARY FUNCTIONS ############### //
int _init_timer();
void move_to_next_thread();
void block_signals();
void unblock_signals();
void reset_timer();

void remove_from_ready (int tid);
bool invalid_quantum_usecs (int usecs);
void wake_threads (int quantum);
void handle_new_quantum ();
Thread *run_new_thread ();
void create_sa_handler ();
void scheduler(int sig){
  move_to_next_thread();
}

Thread::Thread(int tid, thread_entry_point entry_point) :
        tid (tid), state (READY), thread_quantum (0), sleep_until_this_quantum(0)
{
    // initializes env to use the right stack, and to run from the function
    // 'entry_point', when we'll use siglongjmp to jump into the thread.
    address_t sp = (address_t) this->stack + STACK_SIZE - sizeof (address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(this->env, 1);
    (this->env->__jmpbuf)[JB_SP] = translate_address (sp);
    (this->env->__jmpbuf)[JB_PC] = translate_address (pc);
    sigemptyset (&this->env->__saved_mask);
    thread_0 = false;
}

void move_to_next_thread(){
  // keep cur entry point for cur thread
  int sig_ret_val = sigsetjmp(all_threads_list[running_tid]->env, 1);
  if (sig_ret_val == 1){
        return;
    }
    // update quantum
  handle_new_quantum ();

  // move to next thread
  Thread *entering = run_new_thread ();
  // jump around and have lost of fun
    reset_timer();
    siglongjmp(entering->env, 1);
}
Thread *run_new_thread ()
{
  Thread* entering = ready_threads_list.front();
  entering->set_state(RUNNING);
  entering->increment_t_quantum();
  running_tid = entering->get_id();
  return entering;
}
void handle_new_quantum ()
{
  wake_threads(total_quantum);
  total_quantum ++;
  Thread* exiting = ready_threads_list.front();
  ready_threads_list.pop_front();
  if(exiting->get_state() == RUNNING){
      exiting->set_state(READY);
    }
  if(exiting->get_state() == READY && exiting->is_sleep() == false){
      ready_threads_list.push_back(exiting);
    }
}
void wake_threads (int quantum)
{
  // if there are no threads to wake - return
  if(wake_up_quantum_to_sleeping_threads.count (quantum) == 0){
      return;
  }
  std::vector<Thread*> threads_to_wake =
          wake_up_quantum_to_sleeping_threads[quantum];
  for(auto thread : threads_to_wake){
      (*thread).set_sleep (false);
      if((*thread).get_state() != BLOCKED){
        ready_threads_list.push_back ((thread));
      }
  }
}


//// signal clocking functions ////
/**
 * init the blocked_signalarm to contain SIGVATALARM
 */
void init_blocked_signalarm(){
  sigemptyset (&blocked_signalarm);
  sigaddset (&blocked_signalarm, SIGVTALRM);
}

/**
 * blocks signals. will used in any of the library funcs
 */
void block_signals(){
if (sigprocmask (SIG_BLOCK, &blocked_signalarm, nullptr) == -1){
std::cerr << "system error: sigprocmask error" << std::endl;
exit(1);
}
}
/**
 * unblocks the signals. will used in the end of library funcs
 */
void unblock_signals(){
  if (sigprocmask (SIG_UNBLOCK, &blocked_signalarm, nullptr) == -1){
      std::cerr << "system error: sigprocmask error" << std::endl;
      exit(1);
    }
}


// ################ END OF NON LIBRARY FUNCTIONS ############### //

// ################ LIBRARY FUNCTIONS ############### //

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init (int quantum_usecs)
{
    time_for_timer = quantum_usecs;
    if(invalid_quantum_usecs(quantum_usecs)){
        return FAILURE_EXIT_MINUS_ONE;
    }
  init_blocked_signalarm();
  // Install scheduler as the signal handler for SIGVTALRM.
  create_sa_handler ();
  // create main thread
  auto main_thread = new Thread(0, true);
  all_threads_list[0] = main_thread;
  ready_threads_list.push_back(main_thread);
  main_thread->set_state(RUNNING);
  total_quantum = 1;
  main_thread->increment_t_quantum();
  running_tid = 0;

    return _init_timer();
}
void create_sa_handler ()
{
  struct sigaction sa = {0};
  sa.sa_handler = &scheduler;
  if (sigaction(SIGVTALRM, &sa, NULL) < 0)
    {
      std::cerr<<"system error: sigaction error.\n";
      exit(1);
    }
}
bool invalid_quantum_usecs (int usecs)
{
  if (usecs < 1)
    {
      std::cerr<<"thread library error: quantum must positive.\n";
      return true;
    }
    return false;
}

int _init_timer() {
    block_signals();
    // now can init timer every time we start.
    int quantum_usecs = time_for_timer;
    timer.it_value.tv_sec = quantum_usecs / 1000000;
    timer.it_value.tv_usec = quantum_usecs % 1000000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
    {
        std::cerr<<"system error: setitimer error.\n";
        exit(1);
    }
    unblock_signals();
    return SUCCESS_EXIT;
}

void reset_timer(){
  if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
    {
      std::cerr << "system error: setitimer error.\n";
      exit(1);
    }
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn (thread_entry_point entry_point)
{
  block_signals();
  if(entry_point == nullptr) {
      std::cerr<<"thread library error: spawn with null entry point.\n";
      return FAILURE_EXIT_MINUS_ONE;
  }
  int new_id = -1;
  for (int i = 0; i < MAX_THREAD_NUM; ++i)
    { if (all_threads_list[i] == nullptr) {
      new_id = i;
      break;}
    }
  if (new_id < 0){
    std::cerr<<"thread library error: to many threads.\n";
    return FAILURE_EXIT_MINUS_ONE;
  }
  Thread* new_thread = new (std::nothrow) Thread(new_id, entry_point);
  if (new_thread == nullptr){
      std::cerr<<"system error: didn't manage to allocate new thread.\n";
      exit(1);
    }
  all_threads_list[new_id] = new_thread;
  ready_threads_list.push_back (new_thread);
  unblock_signals();
  return new_id;
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){
  block_signals();
  if(tid < 0 || tid > MAX_THREAD_NUM){
      std::cerr<<
               "thread library error: tried to block non exiting TID.\n";
      unblock_signals();
      return FAILURE_EXIT_MINUS_ONE;
    }
    if(all_threads_list[tid] == nullptr)
    {
      std::cerr<<
      "thread library error: tried to terminate non exiting TID\n";
        unblock_signals();
        return FAILURE_EXIT_MINUS_ONE;
    }
    if (tid == 0)
    {
      for (auto & i : all_threads_list)
      {
        delete i;
        i = nullptr;
      }
      exit (0);
    }
   if((all_threads_list[tid])->get_state() == RUNNING){
     // delete terminated thread
     delete all_threads_list[tid];
     all_threads_list[tid] = nullptr;
       total_quantum ++;
       ready_threads_list.pop_front();
       Thread* entering = run_new_thread();
      unblock_signals();
       siglongjmp(entering->env, 1);
   }
   if(all_threads_list[tid]->get_state() == READY)
   {
       remove_from_ready (tid);
   }
  delete all_threads_list[tid];
  all_threads_list[tid] = nullptr;
  unblock_signals();
  return EXIT_SUCCESS;
}

/**
* @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
*
* If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
* main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
* BLOCKED state has no effect and is not considered an error.
*
* @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid){
  block_signals();
  if(tid < 0 || tid > MAX_THREAD_NUM){
      std::cerr<<
               "thread library error: tried to block non exiting TID.\n";
      unblock_signals();
      return FAILURE_EXIT_MINUS_ONE;
  }
  if(all_threads_list[tid] == nullptr)
  {
    std::cerr<<
          "thread library error: tried to block non exiting TID.\n";
    unblock_signals();
    return FAILURE_EXIT_MINUS_ONE;
  }
  if (tid == 0)
  {
    std::cerr<<
          "thread library error: tried to block Main thread.\n";
    unblock_signals();
    return FAILURE_EXIT_MINUS_ONE;
  }
  if (tid == running_tid){
    all_threads_list[tid]->set_state (BLOCKED);
    unblock_signals();
    move_to_next_thread();
  }
  else if((all_threads_list[tid])->get_state() == RUNNING){
    (all_threads_list[tid])->set_state(BLOCKED);
    remove_from_ready(tid);
    unblock_signals();
    return EXIT_SUCCESS;
  }
  return EXIT_SUCCESS;
}

void remove_from_ready (int tid)
{
    for (auto thread = ready_threads_list.begin();
                    thread != ready_threads_list.end();){
      if ((*thread)->get_id() == tid){
        ready_threads_list.erase (thread);
          return;
      }
      else{
        thread++;
      }
    }
}

/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not
 * considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){
  block_signals();
  if(tid < 0 || tid > MAX_THREAD_NUM){
      std::cerr<<
               "thread library error: tried to block non exiting TID.\n";
      unblock_signals();
      return FAILURE_EXIT_MINUS_ONE;
    }
  if(all_threads_list[tid] == nullptr)
  {
    std::cerr<<
        "thread library error: tried to resume a non exiting TID.\n";
    return FAILURE_EXIT_MINUS_ONE;
  }
  if ((all_threads_list[tid])->get_state() == BLOCKED)
  {
    (all_threads_list[tid])->set_state (READY);
    ready_threads_list.push_back(all_threads_list[tid]);
  }
  unblock_signals();
  return EXIT_SUCCESS;
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid(){
  return running_tid;  // the calling thread must be the running one
}

/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums(){
    return total_quantum;
}

/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid){
    block_signals();
  if(tid < 0 || tid > MAX_THREAD_NUM){
      std::cerr<<
         "thread library error: tried to get_quantums of non exiting TID.\n";
      unblock_signals();
      return FAILURE_EXIT_MINUS_ONE;
    }
  if(all_threads_list[tid] == nullptr)
    {
      std::cerr<<
          "thread library error: tried to get_quantums of non exiting TID.\n";
      return FAILURE_EXIT_MINUS_ONE;
    }
    unblock_signals();
  return all_threads_list[tid]->get_t_t_quantum();
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums){
  block_signals();
    if(invalid_quantum_usecs (num_quantums)){
      return FAILURE_EXIT_MINUS_ONE;
    }
    if (running_tid == 0)
      {
        std::cerr << "thread library error: tried sleeped main thread"
                  << std::endl;
      }
  Thread* cur_thread = all_threads_list[running_tid];
  int wake_quantum = total_quantum + num_quantums;
  cur_thread->set_sleep (true);
  cur_thread->set_sleep_until_this_quantum(wake_quantum);
  wake_up_quantum_to_sleeping_threads[wake_quantum].push_back (cur_thread);
  unblock_signals();
  move_to_next_thread();
    return SUCCESS_EXIT;
}
