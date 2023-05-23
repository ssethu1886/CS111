#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process
{
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  TAILQ_ENTRY(process) pointers;

  /* Additional fields here */
  u32 remaining_time;
  u32 scheduled_flag;
  /* End of "Additional fields here" */
};

TAILQ_HEAD(process_list, process);

u32 next_int(const char **data, const char *data_end)
{
  u32 current = 0;
  bool started = false;
  while (*data != data_end)
  {
    char c = **data;

    if (c < 0x30 || c > 0x39)
    {
      if (started)
      {
        return current;
      }
    }
    else
    {
      if (!started)
      {
        current = (c - 0x30);
        started = true;
      }
      else
      {
        current *= 10;
        current += (c - 0x30);
      }
    }

    ++(*data);
  }

  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

u32 next_int_from_c_str(const char *data)
{
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++]))
  {
    if (c < 0x30 || c > 0x39)
    {
      exit(EINVAL);
    }
    if (!started)
    {
      current = (c - 0x30);
      started = true;
    }
    else
    {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}

void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    int err = errno;
    perror("open");
    exit(err);
  }

  struct stat st;
  if (fstat(fd, &st) == -1)
  {
    int err = errno;
    perror("stat");
    exit(err);
  }

  u32 size = st.st_size;
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED)
  {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  const char *data_end = data_start + size;
  const char *data = data_start;

  *process_size = next_int(&data, data_end);

  *process_data = calloc(sizeof(struct process), *process_size);
  if (*process_data == NULL)
  {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  for (u32 i = 0; i < *process_size; ++i)
  {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
  }

  munmap((void *)data, size);
  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    return EINVAL;
  }
  struct process *data;
  u32 size;
  init_processes(argv[1], &data, &size);

  u32 quantum_length = next_int_from_c_str(argv[2]);

  struct process_list list;
  TAILQ_INIT(&list);

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;

  /* Your code here */
  if ( quantum_length <= 0) return EINVAL;
  struct process *p, *p1;
  for (u32 i = 0; i < size; i++) {
     p = &data[i];
     p->remaining_time = p->burst_time;
     p->scheduled_flag = 0;
  }
   
  u32 current_time = 0;
  bool if_finished = false;
  u32 p_ct = 0; //process count
  u32 q_ct = 0; //quantum length count
  struct process *phead, *active_process=NULL;
  u32 start_process=0;
  

  
  while (!if_finished) {
     if (start_process == 1) start_process = 2;

     for (u32 i = 0; i < size; i++) {
        p1 = &data[i];
        if (p1->arrival_time == current_time) {
           p_ct++;
           struct process *new_process = malloc(sizeof(struct process));
           new_process->pid = p1->pid;
           new_process->arrival_time = p1->arrival_time;
           new_process->burst_time = p1->burst_time;
           new_process->remaining_time = p1->remaining_time;
           new_process->scheduled_flag = p1->scheduled_flag;
           TAILQ_INSERT_TAIL(&list, new_process, pointers);
        }
     }

     if((q_ct == 0) &&  !TAILQ_EMPTY(&list)   && start_process  == 0) {
         phead = TAILQ_FIRST(&list);
         if (phead->arrival_time == current_time) {
            active_process = malloc(sizeof(struct process));
            active_process->pid = phead->pid;
            active_process->arrival_time = phead->arrival_time;
            active_process->burst_time = phead->burst_time;
            active_process->remaining_time = phead->remaining_time;
            active_process->scheduled_flag = phead->scheduled_flag;
            if (active_process->scheduled_flag == 0) {
             active_process->scheduled_flag = 1;
             total_response_time = total_response_time + (current_time - active_process->arrival_time);
            }
            TAILQ_REMOVE(&list, phead, pointers);
            free(phead);
            start_process = 1;
        }
        
     }
     if ( start_process  == 2 && active_process != NULL) {
       q_ct++;
       active_process->remaining_time = active_process->remaining_time - 1; 
       if (active_process->remaining_time == 0) {
         total_waiting_time = total_waiting_time + (current_time - active_process->arrival_time - active_process->burst_time);
         free(active_process);
         active_process = NULL;
        
       
        if (q_ct <= quantum_length)  {
           q_ct = 0;
        }
     
      } else if (active_process->remaining_time > 0) {
         
         if (q_ct == quantum_length) {
           q_ct = 0;
           struct process *readd_process = malloc(sizeof(struct process));
           readd_process->pid = active_process->pid;
           readd_process->arrival_time = active_process->arrival_time;
           readd_process->burst_time = active_process->burst_time;
           readd_process->remaining_time = active_process->remaining_time;
           readd_process->scheduled_flag = active_process->scheduled_flag;
           TAILQ_INSERT_TAIL(&list, readd_process, pointers);
           free(active_process);
           active_process = NULL;
         } 
      } 
      if((q_ct == 0) &&  !TAILQ_EMPTY(&list))    {
         phead = TAILQ_FIRST(&list);
         active_process = malloc(sizeof(struct process));
         active_process->pid = phead->pid;
         active_process->arrival_time = phead->arrival_time;
         active_process->burst_time = phead->burst_time;
         active_process->remaining_time = phead->remaining_time;
         active_process->scheduled_flag = phead->scheduled_flag;
         if (active_process->scheduled_flag == 0) {
             active_process->scheduled_flag = 1;
             total_response_time = total_response_time + (current_time - active_process->arrival_time);
         }
         TAILQ_REMOVE(&list, phead, pointers);
         free(phead);
        
      }
    }
    if (p_ct == size && TAILQ_EMPTY(&list) && active_process == NULL)  if_finished = true;
    if( p_ct  <  size && TAILQ_EMPTY(&list) && active_process == NULL) start_process = 0;
    current_time++;
    
  }

  /* End of "Your code here" */

  printf("Average waiting time: %.2f\n", (float)total_waiting_time / (float)size);
  printf("Average response time: %.2f\n", (float)total_response_time / (float)size);

  free(data);
  return 0;
}
