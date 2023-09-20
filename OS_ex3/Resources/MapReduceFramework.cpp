//
// Created by Hillel on 09 May 2023.
//

#include <atomic>
#include <iostream>
#include <algorithm>
#include "MapReduceFramework.h"
#include "Barrier.cpp"
#include <vector>  //std::vector

struct JobContext{
  JobState* job_state;
  bool is_main_thread{false};
  Barrier* barrier{};
  std::atomic<uint64_t>* main_atomic_counter;
  std::atomic<int>* input_vec_atomic_counter;
  std::atomic<int>* intermediate_vec_atomic_counter;
  std::atomic<int>* map_finished_counter;
  std::atomic<int>* max_for_shuffle_counter;
  std::atomic<int>* max_for_reduce;
  std::atomic<int>* reduce_finished_counter;
  int shuffle_finished_counter{0};
  pthread_mutex_t *wait_for_job_mutex{};
  pthread_mutex_t *job_state_mutex{};
  pthread_mutex_t *output_vec_mutex{};
  int my_input_vec_index{};
  const InputVec& inputVec;
  IntermediateVec intermediateVec;
  IntermediateVec shuffled_intermediateVec;
  std::vector<IntermediateVec>* shuffled_vectors;
  OutputVec& outputVec;
  pthread_t pthread{};
  const MapReduceClient& client;
  int num_of_threads;
  int * max_for_map;
  int flag {0};
  JobContext** contexts;
  pthread_t* threads;

  JobContext(std::atomic<int>* input_vec_atomic_counter,
             std::atomic<int>* intermediate_vec_atomic_counter,
             std::atomic<int>* map_finished_counter,
             std::atomic<int>* reduce_finished_counter,
             std::atomic<int>* max_for_shuffle_counter,
             std::atomic<int>* max_for_reduce,
             std::atomic<uint64_t>* atomic_counter_64,
             const InputVec& input_vec,
             OutputVec& outputVec, const MapReduceClient& map_reduce_client,
             int num_of_threads, JobContext** contexts,
             std::vector<IntermediateVec>* shuffled_vectors) :
      main_atomic_counter(atomic_counter_64),
      input_vec_atomic_counter(input_vec_atomic_counter),
      intermediate_vec_atomic_counter(intermediate_vec_atomic_counter),
      map_finished_counter(map_finished_counter),
      max_for_shuffle_counter(max_for_shuffle_counter),
      max_for_reduce(max_for_reduce),
      reduce_finished_counter(reduce_finished_counter),
      inputVec(input_vec),
      shuffled_vectors(shuffled_vectors),
      outputVec(outputVec),
      client(map_reduce_client),
      num_of_threads(num_of_threads),
      contexts(contexts)
  {}
};



/*
 * function to be passed to each thread to start running on
 */
void* map_reduce_for_thread(void* job_context);

bool comeprator_k2(const std::pair<K2*, V2*>& p1, const std::pair<K2*, V2*>&
        p2);

bool k2_equals (K2 *first, K2 *second);
void shuffle (JobContext *jc);
K2 *get_current_largest_element (JobContext *jc);
IntermediateVec get_shuffle_vector (JobContext *jc, K2 *key);
void reduce (JobContext *jc);


JobHandle startMapReduceJob(const MapReduceClient& client,
                            const InputVec& inputVec, OutputVec& outputVec,
                            int multiThreadLevel){
  auto* input_atomic_counter = new (std::nothrow) std::atomic<int>(0);
  auto* intermediate_atomic_counter = new (std::nothrow) std::atomic<int>(0);
  auto* map_finished_counter = new (std::nothrow) std::atomic<int>(0);
  auto * reduce_finished_counter = new (std::nothrow) std::atomic<int>(0);
  auto * main_atomic_counter = new (std::nothrow) std::atomic<uint64_t>(0);
  auto * max_for_shuffle_counter = new (std::nothrow) std::atomic<int>(0);
  auto* max_for_reduce_int = new (std::nothrow) std::atomic<int>(0);
  auto * wait_for_job_mutex = new (std::nothrow) pthread_mutex_t
          (PTHREAD_MUTEX_INITIALIZER);
  auto * job_state_mutex = new (std::nothrow) pthread_mutex_t
          (PTHREAD_MUTEX_INITIALIZER);
  auto* output_vec_mutex = new (std::nothrow) pthread_mutex_t
          (PTHREAD_MUTEX_INITIALIZER);
  auto* barrier = new (std::nothrow) Barrier(multiThreadLevel);
  auto* shuffled_vectors = new (std::nothrow) std::vector<IntermediateVec>;
  auto* threads = new (std::nothrow) pthread_t[multiThreadLevel];
  auto contexts = new (std::nothrow) JobContext* [multiThreadLevel];
  auto * job_c = new (std::nothrow) JobState {UNDEFINED_STAGE, 0};
  auto * max_map = new (std::nothrow) int;
  *max_map = 0;
  if (!(input_atomic_counter && intermediate_atomic_counter &&
  map_finished_counter && reduce_finished_counter && main_atomic_counter &&
  max_for_shuffle_counter && max_for_reduce_int && barrier &&
  shuffled_vectors && threads && contexts && job_c && max_map)){
      std::cout << "system error: failed to allocate memory." << std::endl;
      exit(1);
  }
  if(!(wait_for_job_mutex &&
       wait_for_job_mutex && job_state_mutex)){
    std::cout << "system error: failed to allocate memory for mutex." << std::endl;
    exit(1);
  }
  for(int i = 0; i < multiThreadLevel; ++i){
    auto* new_context = new (std::nothrow) JobContext(input_atomic_counter,
           intermediate_atomic_counter, map_finished_counter,
           reduce_finished_counter, max_for_shuffle_counter,
           max_for_reduce_int,
                   main_atomic_counter,
                   inputVec, outputVec,
               client, multiThreadLevel, contexts,
                           shuffled_vectors);
    if (!new_context){
        std::cout << "system error: failed to allocate memory for"
                     " a context." << std::endl;
        exit(1);
    }
    new_context->wait_for_job_mutex = wait_for_job_mutex;
    new_context->job_state_mutex = job_state_mutex;
    new_context->output_vec_mutex = output_vec_mutex;
    new_context->barrier = barrier;
    new_context->pthread = threads[i];
    new_context->job_state = job_c;
    new_context->max_for_reduce = max_for_reduce_int;
    new_context->threads = threads;
    new_context->max_for_map = max_map;
    contexts[i] = new_context;
      if(i == 0){
          new_context->is_main_thread = true;
        }
      if(pthread_create(threads + i, nullptr,
                     map_reduce_for_thread, new_context) != 0){
          std::cout << "system error: failed to create a thread." << std::endl;
          exit(1);
      }
    }
    JobHandle res = static_cast<JobHandle>(contexts[0]);
    return res;

}

void* map_reduce_for_thread(void* job_context){
    JobContext* jc = static_cast<JobContext*>(job_context);

    // Stage 1: MAP
    int max = int(jc->inputVec.size());
    *jc->max_for_map = max;
    jc->job_state->stage = MAP_STAGE;
    while (*(jc->input_vec_atomic_counter) < max) {
        jc->my_input_vec_index = jc->input_vec_atomic_counter->fetch_add(1);
        if(jc->my_input_vec_index >= max){
          break;
        }
        jc->client.map(jc->inputVec[jc->my_input_vec_index]
                         .first,
                     jc->inputVec[jc->my_input_vec_index].second,
                     (void *) job_context);
        jc->map_finished_counter->fetch_add(1);
    }

    // STAGE 1.5: Sort
    std::sort(jc->intermediateVec.begin(), jc->intermediateVec.end(),
              comeprator_k2);
    jc->barrier->barrier();

  // STAGE 2: Shuffle:
        if (jc->is_main_thread){
          jc->job_state->stage = SHUFFLE_STAGE;
            shuffle (jc);
//            std::cerr << "FOR TESTING: Shuffled" <<std::endl;
          jc->job_state->stage = REDUCE_STAGE;
        }
    jc->barrier->barrier();

        // STAGE 3: reduce:
    reduce(jc);
        return nullptr;
}

bool comeprator_k2(const std::pair<K2*, V2*>& p1, const std::pair<K2*, V2*>& p2) {
    return (*p1.first) < (*p2.first);
}

void reduce (JobContext *jc)
{;
  size_t max = jc->shuffled_vectors->size();
  *jc->max_for_reduce = (int)max;

  while (*(jc->intermediate_vec_atomic_counter) < max) {
      IntermediateVec* inter_vector;
      int x = jc->intermediate_vec_atomic_counter->fetch_add(1);
        if(x >= max){
          return;
        }
      inter_vector = &jc->shuffled_vectors->at(x);
      // reduce
      jc->client.reduce(inter_vector, (void *) jc);
      jc->reduce_finished_counter->fetch_add((int)inter_vector->size());
    }
}

void shuffle (JobContext *jc)
{
  auto& shuffled_vectors = jc->shuffled_vectors;
  while(true){
      K2* cur_largest_element = get_current_largest_element(jc);
      if(cur_largest_element == nullptr) break;  // all vectors are empty. :)
      // get a vector with the cur_largest key
      IntermediateVec shuffled_vector = get_shuffle_vector (jc, cur_largest_element);
      shuffled_vectors->push_back (shuffled_vector);
      jc->shuffle_finished_counter++;
    }
}

/*
 * returns a vector with all the pairs with the key, and remove them from the
 * intermediate vector
 */
IntermediateVec get_shuffle_vector (JobContext *jc, K2 *key)
{
  IntermediateVec shuffled_vector;
  JobContext** contexts = jc->contexts;
  for (int i = 0; i < jc->num_of_threads; ++i)
    {
      IntermediateVec* cur_vec;
      cur_vec = &contexts[i]->intermediateVec;
      while(!(cur_vec)->empty() && k2_equals (cur_vec->back().first, key)){
          shuffled_vector.push_back(cur_vec->back());
          cur_vec->pop_back();
        }
    }
  return shuffled_vector;
}

/**
 * returns the current highest element in all intermediate vectors
 */
K2 *get_current_largest_element (JobContext *jc)
{
  JobContext** contexts = jc->contexts;
  K2* cur_largest_element = nullptr, *cur_candidate;
  IntermediateVec cur_vec;
  // loop over all intermediate and find the largest
  for (int i=0; i<jc->num_of_threads; i++){
      cur_vec = contexts[i]->intermediateVec;
      if((cur_vec).empty()) continue;
      cur_candidate = (cur_vec).back().first;
      if(cur_largest_element == nullptr) cur_largest_element = cur_candidate;
      else{
          if (*cur_largest_element < *cur_candidate){
              cur_largest_element = cur_candidate;
            }
        }
    }
  return cur_largest_element;
}

/**
 * check whether 2 K2 elements are equals
 */
bool k2_equals (K2 *first, K2 *second)
{
  return !(*first < *second) && !(*second < *first);
}

void emit2 (K2* key, V2* value, void* context){
    auto* cur_context = static_cast<JobContext*>(context);
    IntermediatePair med_pair = std::make_pair(key, value);
    cur_context->intermediateVec.push_back(med_pair);
    (*cur_context->max_for_shuffle_counter).fetch_add(1);
}


void emit3 (K3* key, V3* value, void* context){
    auto* cur_context = static_cast<JobContext*>(context);
    OutputPair out_pair = std::make_pair(key, value);
    if(pthread_mutex_lock(cur_context->output_vec_mutex)){
      std::cout << "system error: mutex fail." << std::endl;
      exit(1);
    }
    cur_context->outputVec.push_back(out_pair);
    if(pthread_mutex_unlock(cur_context->output_vec_mutex)){
      std::cout << "system error: mutex fail." << std::endl;
      exit(1);
    }
}

void getJobState(JobHandle job, JobState* state){
    if (job != NULL){
        JobContext* this_context = static_cast<JobContext*>(job);
        if(pthread_mutex_lock(this_context->job_state_mutex)){
          std::cout << "system error: mutex fail." << std::endl;
          exit(1);
        }
        stage_t cur_state = this_context->job_state->stage;
        float percentage = 0;
        if (cur_state == MAP_STAGE) {
            if (this_context->max_for_map == 0) {
                percentage = 0;
//                std::cerr<< "FOR TESTING: map max is 0: " << std::endl;
            }
            else
            {
                int temp = this_context->map_finished_counter->load();
                percentage =
                        ((float) temp / (float) *this_context->max_for_map) *
                        100;

            }
        }
        if (cur_state == SHUFFLE_STAGE)
        {
            percentage =
              ((float)this_context->shuffle_finished_counter /
                      (float)*this_context->max_for_shuffle_counter) * 100;
        }
        if (cur_state == REDUCE_STAGE)
        {
            int temp_reduce = this_context->reduce_finished_counter->load();
            percentage = ((float)temp_reduce /
                    (float)*this_context->max_for_shuffle_counter) *100;
        }
        *state = {cur_state, percentage};
        if(pthread_mutex_unlock(this_context->job_state_mutex)){
          std::cout << "system error: mutex failure." << std::endl;
          exit(1);
        }
    }
}

void waitForJob(JobHandle job){
    if (job != NULL) {
        JobContext *this_context = static_cast<JobContext *>(job);
        if(pthread_mutex_lock(this_context->wait_for_job_mutex)){
          std::cout << "system error: mutex failure." << std::endl;
          exit(1);
        }

        if (this_context->flag == false)
        {
          for (int i = 0; i < this_context->num_of_threads; i++){
              if(pthread_join(this_context->threads[i], nullptr)){
                std::cout << "system error: thread join failure." << std::endl;
                exit(1);
              }
//              std::cerr<< "FOR TESTING: finished waiting for thread: " << i <<
//              std::endl;
          }
          this_context->flag = true;
        }
        if(pthread_mutex_unlock(this_context->wait_for_job_mutex)){
          std::cout << "system error: mutex failure." << std::endl;
          exit(1);
        }
    }
}


void closeJobHandle(JobHandle job){
    waitForJob(job);
    JobContext *this_context = static_cast<JobContext *>(job);
    delete this_context->main_atomic_counter;
    delete this_context->input_vec_atomic_counter;
    delete this_context->intermediate_vec_atomic_counter;
    delete this_context->map_finished_counter;
    delete this_context->max_for_shuffle_counter;
    delete this_context->reduce_finished_counter;
    delete this_context->wait_for_job_mutex;
    delete this_context->output_vec_mutex;
    delete this_context->barrier;
    delete this_context->max_for_reduce;
    delete this_context->shuffled_vectors;
    if(pthread_mutex_destroy(this_context->job_state_mutex)){
      std::cout << "system error: failed to distroy mutex." << std::endl;
      exit(1);
    }
    delete this_context->job_state_mutex;
    delete this_context->job_state;
    int num_threads = this_context->num_of_threads;
    for(int i=0; i<num_threads; i++){
        if (this_context->contexts[i] == this_context) { continue;}
        delete this_context->contexts[i];
    }
    delete[] this_context->threads;
    delete[] this_context->contexts;
    delete this_context->max_for_map;
    delete this_context;
}

