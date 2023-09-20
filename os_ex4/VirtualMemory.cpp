//
// Created by gilad on 6/9/2023.
//

#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#define SWAPPED_OUT 0
#define ROOT_TABLE 0

bool invalid_address (uint64_t address);
uint64_t gets_physical_address (uint64_t addresses_parts[]);
bool is_frame_empty(int frame_number);
int
page_not_found (uint64_t *const addresses_parts, uint64_t *const frame_path, int level, int *max_used_frame, uint64_t new_page_virtual_address);
void fill_frame_with_root_value(uint64_t frame);
/**
 * Initialize the virtual memory.
 */
void VMinitialize ()
{
  // clear frame 0
  word_t init_value = SWAPPED_OUT;
  for (int i = 0; i < PAGE_SIZE; i++)
    {
      PMwrite (ROOT_TABLE + i, init_value);
    }
}

/**
 * split a virtual address into it's sub-tables parts + offset.
 * puts the parts in the addresses variable.
 * (As in the pdf, into - [101][0001][0110] + offset).
 * This function (and only this hopefully) should contain the bit-wise operations.
 */
uint64_t *split_virtual_address (uint64_t address, uint64_t addresses[])
{
  uint64_t bit_mask = (1LL << OFFSET_WIDTH) - 1;
  uint64_t offset = address & bit_mask;
  addresses[TABLES_DEPTH] = offset;
  int address_width = OFFSET_WIDTH;
  bit_mask = ((1LL << address_width) - 1) << OFFSET_WIDTH;
  for (int i = 0; i < TABLES_DEPTH; ++i)
    {
      addresses[TABLES_DEPTH - i - 1] = (address & bit_mask) >> (address_width * (i+1));
      bit_mask = bit_mask << address_width;
      // todo: perhaps the last table needs different handle
    }
  return addresses; // todo: should this be a void function?
}

/**
 * return true if the address in invalid.
 * cases: a) the address is bigger than VIRTUAL_MEMORY_SIZE.
 *        b) ?
 */
bool invalid_address (uint64_t address)
{
  return address >= VIRTUAL_MEMORY_SIZE;  // start counting from 0
}

int calculate_vm_page(uint64_t virtual_address){
  return virtual_address >> OFFSET_WIDTH;
}

/**
 * Input: the splitted address + offset.
 * returns the fitted frameIndex.
 * Including page fault handling.
 */
uint64_t gets_physical_address (uint64_t addresses_parts[], uint64_t virtual_address)
{
    int cur_frame = 0;
    uint64_t frame_path[TABLES_DEPTH];
    int max_used_frame = 0;
    for (int depth = 0; depth < TABLES_DEPTH; ++depth) // ?? don't go over root
    {
        word_t tester = -1;
        PMread(cur_frame * PAGE_SIZE + addresses_parts[depth], &tester);
        //todo: check indexes agein!!
        if(!(tester))
        {
         int old_frame = cur_frame;
         cur_frame = page_not_found (addresses_parts, frame_path, depth, &max_used_frame, virtual_address);
         if(cur_frame > max_used_frame){max_used_frame = cur_frame;}
         PMwrite (old_frame * PAGE_SIZE + addresses_parts[depth], cur_frame);
         frame_path[depth] = cur_frame; //todo: chek if frame_path[depth+1]
         //todo: make sure implementation of page_not_found is good for here
         if(depth == TABLES_DEPTH -1){
             // got to last depth: restore from disk
             int vm_page = calculate_vm_page (virtual_address);
             PMrestore(cur_frame, vm_page);
         }
        }
        else
        {
            cur_frame = tester;
            frame_path[depth] = cur_frame;
        }
    }
    // successes! read address from last table

  return cur_frame * PAGE_SIZE + addresses_parts[TABLES_DEPTH];
}

/**
 * Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread (uint64_t virtualAddress, word_t *value)
{
  if(invalid_address(virtualAddress)){
    return 0;
  }
  uint64_t addresses[TABLES_DEPTH + 1] = {0}; // + 1 for offset
  split_virtual_address (virtualAddress, addresses);
  uint64_t frame = gets_physical_address(addresses, virtualAddress);
  PMread (frame, value);
  return 1;
}

/**
 * Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite (uint64_t virtualAddress, word_t value)
{
  if(invalid_address(virtualAddress)){
      return 0;
    }
  uint64_t addresses[TABLES_DEPTH + 1] = {0}; // + 1 for offset
  split_virtual_address (virtualAddress, addresses);
  uint64_t physical_address = gets_physical_address(addresses, virtualAddress);
  PMwrite(physical_address, value);
  return 1;
}

/**
 * Checks if frame is empty.
 * if empty - return true
 * if not - return false (can be used as a boolean func)
 */
bool is_frame_empty(int frame_number)
{
    uint64_t physical_address_frame_base = frame_number * PAGE_SIZE;
    for (int i = 0; i < PAGE_SIZE; i++)
    {
        word_t tester = -1;
        PMread(physical_address_frame_base + i, &tester);
        if(tester != SWAPPED_OUT){
            return false;
        }
    }
    return true;
}


int look_for_empty_table (int this_table, int cur_depth, int *max_used_frame,
                      uint64_t *const frame_path, uint64_t &max_dist, uint64_t &page_to_evict,
                      uint64_t p, uint64_t page_to_restore, uint64_t& frame_to_evict,
                      uint64_t& evict_father, uint64_t cur_father)
{
  // TNAI ATZIRA:
  if (cur_depth > TABLES_DEPTH)
    {
      return 0;
    }
    if(cur_depth == TABLES_DEPTH){
      uint64_t abs = page_to_restore - p > 0 ? page_to_restore - p : p - page_to_restore;
      uint64_t cyclic_distance = NUM_PAGES - abs > 0 ? NUM_PAGES - abs : abs;
      if(cyclic_distance > max_dist){
        page_to_evict = p;
        max_dist = cyclic_distance;
        frame_to_evict = this_table;
        evict_father = cur_father;
      }
    }
  word_t just_read= -1;
  for (int index_in_table = 0; index_in_table < PAGE_SIZE; ++index_in_table)
  {

    PMread (this_table * PAGE_SIZE + index_in_table, &just_read);
    // dead end - continue
    if (just_read == ROOT_TABLE)
    { continue; }

    // update max used frame
    if (*max_used_frame < just_read)
    {
      *max_used_frame = just_read;
    }
    // if found empty table - return it, and update the frame
    // referencing to it
    if ((cur_depth < TABLES_DEPTH - 1) && is_frame_empty (just_read))
    {
      // make sure not to delete an empty frame that is in the path - it isn't empty!
      bool flag = true;
      for(int k = 0; k < TABLES_DEPTH; ++k){
        if(frame_path[k] == (uint64_t) just_read){
          flag = false;
          break;
        }
      }
      // frame not in path
      if(flag){
        int frame_to_return = just_read;
        PMwrite (this_table * PAGE_SIZE + index_in_table, ROOT_TABLE);
        return frame_to_return;
      }
    }
    uint64_t update_p = (p << OFFSET_WIDTH)+ index_in_table;
    // found a number of non-empty table - look into it:
    int recursion_res = look_for_empty_table (just_read, cur_depth + 1,
                                              max_used_frame, frame_path,
                                              max_dist, page_to_evict, update_p,
                                              page_to_restore, frame_to_evict,
                                              evict_father, this_table * PAGE_SIZE + index_in_table);
    if(recursion_res){ // found table
      return recursion_res;
    }
    // couldn't find in this table-entry, try next entry in the for-loop
  }

  // if didn't find an empty table
  return 0;
}



/**
 * If a page is not found, this function will be called.
 * return: index in RAM of new frame to be used.
 */
int
page_not_found (uint64_t *const addresses_parts, uint64_t *const frame_path,
                int level, int *max_used_frame, uint64_t new_page_virtual_address)
{

    //uint64_t traveling_path[TABLES_DEPTH]{0};
    int cur_table = 0;
    uint64_t max_dist = 0;
    uint64_t page_to_evict = 0;
    uint64_t cur_page = 0;
    uint64_t frame_to_evict = 0;
  uint64_t evict_father = 0;

  uint64_t page_to_restore = calculate_vm_page (new_page_virtual_address);
    int option_1 = look_for_empty_table (cur_table, 0, max_used_frame, frame_path,
                                         max_dist, page_to_evict, cur_page,
                                         page_to_restore, frame_to_evict,
                                         evict_father, 0);
    if(option_1){
      return option_1;
    }

    // check if we have an unused frame
    if(*max_used_frame + 1 < NUM_FRAMES){
        if(level != TABLES_DEPTH){
            fill_frame_with_root_value(*max_used_frame + 1);
        }
        return (*max_used_frame) + 1;
    }

    // swap out a page

    // root the father
  PMwrite (evict_father, ROOT_TABLE);

  // evict
  PMevict (frame_to_evict, page_to_evict);
    if(level != TABLES_DEPTH){
        fill_frame_with_root_value(frame_to_evict);
    }
  if (level >= TABLES_DEPTH)
    {
      PMrestore (frame_to_evict, page_to_restore);
    }
  return frame_to_evict;
}

void fill_frame_with_root_value(uint64_t frame) {
    uint64_t physical_address_frame_base = frame * PAGE_SIZE;
    for (int i = 0; i < PAGE_SIZE; i++) {
        PMwrite(physical_address_frame_base + i, ROOT_TABLE);
    }
}


//
// assume we get to the depth d = TABLES_DEPTH. => we're at page.
// let's call the page p as in the pdf.
// dist_between_pages = |new_page - p| (new page - the page we want to add)
// dist = min(NUM_PAGES - dist_between_pages, dist_between_pages)
// that's cyclic because of this min.
