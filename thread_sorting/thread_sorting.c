#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static enum constant {
  e_max_inputs = 10000,
  e_max_chars = 9,
  e_max_input_size = 60000,
}const_e;

static int get_num_of_threads()
{
  char buf[e_max_chars];
  int input_size = 0;

  printf("Input number of threads:\n");
  if (fgets(buf, e_max_chars, stdin) != NULL) {
    input_size = atoi(buf);
    if (input_size && input_size < e_max_inputs + 1) {
      /* OK */
      printf("OK: %d\n", input_size);
    } else {
      /* error input val */
      fprintf(stderr, "Invalid enter data: need integer val(0; 10000]\n");
      return 0;
    }
  }

  return input_size;
}

typedef struct sch_list {
  int val;
  int num;
} sch_list_t;

static int get_threads_timings(int input_size, sch_list_t *timing_list)
{
  char buf[e_max_input_size];
  char *sep_val_str = buf;

  printf("Input thread timings:\n");
  if (fgets(buf, e_max_input_size, stdin) != NULL) {
    for (int i = 0; i < input_size; i++) {
      sep_val_str = strtok(sep_val_str, " ");
      timing_list[i].val = atoi(sep_val_str);
      timing_list[i].num = i;
      sep_val_str = NULL;
      if (!timing_list[i].val) {
        fprintf(stderr, "Invalid enter data: need N integer timings\n");
        return -1;
      }
    }
  }

  return 0;
}

static int compare(const void *p1, const void *p2)
{
  sch_list_t a = *(const sch_list_t *)p1;
  sch_list_t b = *(const sch_list_t *)p2;

  return a.val - b.val;
}

int main()
{
  sch_list_t timing_list[e_max_input_size];

  int input_size = get_num_of_threads();
  assert(input_size);
  assert(!get_threads_timings(input_size, timing_list));
 
  printf("Sorted threads min->max:\n");
  qsort(timing_list, input_size, sizeof(timing_list[0]), compare);
  for (int i = 0; i < input_size; i++) {
    printf("%d ", timing_list[i].num);
  }
  printf("\n");

  return 0;
}
