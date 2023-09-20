#include <ctime>
#include <sys/time.h>
#include <iostream>
#include <cmath>
#include "osm.h"

int UNROLLING_FACTOR = 7;
double FROM_MICRO_TO_SEC_FACTOR = 1000000;
double FROM_SEC_TO_NANO_FACTOR = 1000000000;
bool valid_iterations (unsigned int iterations);
double
get_time_in_nano_sec (const timeval &start_time, const timeval &end_time);

/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time (unsigned int iterations)
{
  if (!valid_iterations (iterations))
    { return -1; }
  timeval start_time;
  timeval end_time;
  int loop_unrolled_iterations = ceil ((double) iterations / UNROLLING_FACTOR);
  // create variables for independent computations at the loop unrolling
  int x1 = 0, x2 = 0, x3 = 0, x4 = 0, x5 = 0, x6 = 0, x7 = 0;
  if (gettimeofday (&start_time, nullptr) == -1)
    { return -1; }
  for (int i = 0; i < loop_unrolled_iterations; ++i)
    {
      x1++;
      x2++;
      x3++;
      x4++;
      x5++;
      x6++;
      x7++;
    }
  if (gettimeofday (&end_time, nullptr) == -1)
    { return -1; }
  return get_time_in_nano_sec (start_time, end_time);
}
/**
 * An empty func for time counting purpose.
 */
void empty_func ()
{};

/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time (unsigned int iterations)
{
  if (!valid_iterations (iterations))
    { return -1; }
  timeval start_time;
  timeval end_time;
  int loop_unrolled_iterations = ceil ((double) iterations / UNROLLING_FACTOR);
  if (gettimeofday (&start_time, nullptr) == -1)
    { return -1; }
  for (int i = 0; i < loop_unrolled_iterations; ++i)
    {
      empty_func ();
      empty_func ();
      empty_func ();
      empty_func ();
      empty_func ();
      empty_func ();
      empty_func ();
    }
  if (gettimeofday (&end_time, nullptr) == -1)
    { return -1; }
  return get_time_in_nano_sec (start_time, end_time);
}

/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time (unsigned int iterations)
{
  if (!valid_iterations (iterations))
    { return -1; }
  timeval start_time;
  timeval end_time;
  int loop_unrolled_iterations = ceil ((double) iterations / UNROLLING_FACTOR);
  if (gettimeofday (&start_time, nullptr) == -1)
    { return -1; }
  for (int i = 0; i < loop_unrolled_iterations; ++i)
    {
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
    }
  if (gettimeofday (&end_time, nullptr) == -1)
    { return -1; }
  return get_time_in_nano_sec (start_time, end_time);
}

/**
 * Gets start and end timeval, and returns the time between them in nano
 * seconds.
 */
double
get_time_in_nano_sec (const timeval &start_time, const timeval &end_time)
{
  double time_in_sec = (end_time.tv_sec - start_time.tv_sec) +
                       ((end_time.tv_usec - start_time.tv_usec)
                        / FROM_MICRO_TO_SEC_FACTOR);
  return time_in_sec * FROM_SEC_TO_NANO_FACTOR;
}

/**
 * Validate the number of iterations.
 */
bool valid_iterations (unsigned int iterations)
{
  return iterations != 0;
}

