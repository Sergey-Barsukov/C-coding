#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define BUDDY_ALIGMENT  4096
#define SLAB_ORDER_MAX  10

#define get_slab_size(order) ((1UL << (order)) * BUDDY_ALIGMENT)

typedef struct obj_head {
  struct obj_head *next_obj;
}obj_head_t;

typedef struct slab_head {
  obj_head_t *free_obj_ptr;
  size_t free_obj_num;
  struct slab_head *next_slab;
  struct slab_head *prev_slab;
}slab_head_t;

/**
 * Эти две функции вы должны использовать для аллокации
 * и освобождения памяти в этом задании. Считайте, что
 * внутри они используют buddy аллокатор с размером
 * страницы равным 4096 байтам.
 **/

/**
 * Аллоцирует участок размером 4096 * 2^order байт,
 * выровненный на границу 4096 * 2^order байт. order
 * должен быть в интервале [0; 10] (обе границы
 * включительно), т. е. вы не можете аллоцировать больше
 * 4Mb за раз.
 **/
void *alloc_slab(int order)
{
  size_t alloc_size = get_slab_size(order);
  void *ret_mem = aligned_alloc(alloc_size, alloc_size);

  return ret_mem;
}

/**
 * Освобождает участок ранее аллоцированный с помощью
 * функции alloc_slab.
 **/
void free_slab(void *slab)
{
  free(slab);
}


/**
 * Эта структура представляет аллокатор, вы можете менять
 * ее как вам удобно. Приведенные в ней поля и комментарии
 * просто дают общую идею того, что вам может понадобится
 * сохранить в этой структуре.
 **/
struct cache {
    slab_head_t *slabs_free; /* список пустых SLAB-ов для поддержки cache_shrink */
    slab_head_t *slabs_part; /* список частично занятых SLAB-ов */
    slab_head_t *slabs_full; /* список заполненых SLAB-ов */

    size_t object_size; /* размер аллоцируемого объекта */
    int slab_order; /* используемый размер SLAB-а */
    size_t slab_objects; /* количество объектов в одном SLAB-е */ 
};

static int _optimize_slab_order(int order, size_t obj_mem)
{
  int tmp_order = order;
  size_t slab_head_size = sizeof(slab_head_t);
  size_t slab_size, prev_unused_mem, unused_mem;

  slab_size = get_slab_size(tmp_order) - slab_head_size;
  prev_unused_mem = slab_size % obj_mem;

  while (tmp_order < SLAB_ORDER_MAX && 0 != prev_unused_mem) {
    tmp_order++;
    slab_size = get_slab_size(tmp_order) - slab_head_size;
    unused_mem = slab_size % obj_mem;

    if (prev_unused_mem > unused_mem) {
      order = tmp_order;
      prev_unused_mem = unused_mem;
    }
  }

  return order;
}

static int _get_slab_order(size_t object_size)
{
  int order = 0;
  size_t obj_mem = object_size + sizeof(obj_head_t);

  size_t slab_size = BUDDY_ALIGMENT;
  size_t mem_req = sizeof(slab_head_t) + obj_mem;
  while (slab_size < mem_req) {
    order++;
    if (order > SLAB_ORDER_MAX) {
      printf("Slab order is out of Limit\n");
      return -1;
    }
    slab_size = get_slab_size(order);
  }

  order = _optimize_slab_order(order, obj_mem);

  return order;
}

/**
 * Функция инициализации будет вызвана перед тем, как
 * использовать это кеширующий аллокатор для аллокации.
 * Параметры:
 *  - cache - структура, которую вы должны инициализировать
 *  - object_size - размер объектов, которые должен
 *    аллоцировать этот кеширующий аллокатор 
 **/
void cache_setup(struct cache *cache, size_t object_size)
{
  int order = 0;

  cache->slabs_free = NULL;
  cache->slabs_part = NULL;
  cache->slabs_full = NULL;
  cache->slab_objects = 0;

  order = _get_slab_order(object_size);
  if (order < 0) {
    cache->object_size = 0;
    order = 0;
  } else {
    cache->object_size = object_size;
  }

  cache->slab_order = order;
}

static void _add_to_slab_list(slab_head_t *slab_head, slab_head_t **slab_list)
{
  if (NULL == *slab_list) {
    /* create 1st item */
    *slab_list = slab_head;
    slab_head->next_slab = slab_head;
    slab_head->prev_slab = slab_head;
  } else {
    /* add to doubly linked list */
    slab_head->prev_slab = (*slab_list)->prev_slab;
    (*slab_list)->prev_slab = slab_head;
    slab_head->prev_slab->next_slab = slab_head;
    slab_head->next_slab = *slab_list;
    *slab_list = slab_head;
  }
}

static slab_head_t *_remove_from_slab_list(slab_head_t *slab_head)
{
  if (slab_head->next_slab == slab_head->prev_slab) {
    slab_head = NULL;
  } else {
    slab_head->next_slab->prev_slab = slab_head->prev_slab;
    slab_head->prev_slab->next_slab = slab_head->next_slab;
    slab_head = slab_head->next_slab;
  }

  return slab_head;
}

static void _release_cache_list(slab_head_t **slab_list, int cache_order)
{
  slab_head_t *tmp_slab = NULL;
  uintptr_t *slab_mem = NULL;
  size_t slab_mem_mask = ~(get_slab_size(cache_order) - 1);

  while (NULL != *slab_list) {
    tmp_slab = *slab_list;
    *slab_list = _remove_from_slab_list(tmp_slab);
    slab_mem = (uintptr_t *)((uintptr_t)tmp_slab & slab_mem_mask);
    free_slab(slab_mem);
  }
}

/**
 * Функция освобождения будет вызвана когда работа с
 * аллокатором будет закончена. Она должна освободить
 * всю память занятую аллокатором. Проверяющая система
 * будет считать ошибкой, если не вся память будет
 * освбождена.
 **/
void cache_release(struct cache *cache)
{
  _release_cache_list(&(cache->slabs_free), cache->slab_order);
  _release_cache_list(&(cache->slabs_part), cache->slab_order);
  _release_cache_list(&(cache->slabs_full), cache->slab_order);
}

inline static void _replace_slab_list(slab_head_t **slab_head, slab_head_t **dest_slab_list)
{
  slab_head_t *tmp_slab = *slab_head;

  *slab_head = _remove_from_slab_list(*slab_head);
  _add_to_slab_list(tmp_slab, dest_slab_list);
}

static slab_head_t *_get_slab_head_ptr(void *slab_mem, struct cache *cache)
{
  slab_head_t *slab_head = (slab_head_t *)(
                              (uintptr_t)(slab_mem)
                              + get_slab_size(cache->slab_order)
                              - sizeof(slab_head_t)
                           );

  return slab_head;
}

slab_head_t *_generate_slab(void *slab_mem, struct cache *cache)
{
  size_t obj_head_size = sizeof(obj_head_t);
  size_t obj_full_size = cache->object_size + obj_head_size;
  size_t obj_num = get_slab_size(cache->slab_order) / obj_full_size;

  cache->slab_objects = obj_num;

  /* generate obj chain */
  obj_head_t *obj_mem = (obj_head_t *)slab_mem;
  for (size_t i = 0; i < obj_num - 1; i++) {
    obj_mem->next_obj = (obj_head_t *)((uintptr_t)obj_mem + obj_full_size);
    obj_mem = obj_mem->next_obj;
  }
  obj_mem->next_obj = NULL;
 
  /* create slab head */
  slab_head_t *slab_head = _get_slab_head_ptr(slab_mem, cache);
  slab_head->free_obj_ptr = (obj_head_t *)(slab_mem);
  slab_head->free_obj_num = obj_num;
  
  return slab_head;
}

static void *_alloc_obj_from_slab(slab_head_t *slab_head)
{
  void *ret_mem = slab_head->free_obj_ptr;
  slab_head->free_obj_ptr = slab_head->free_obj_ptr->next_obj;
  slab_head->free_obj_num -= 1;

  return (void *)((uintptr_t)ret_mem + sizeof(obj_head_t));
}

/**
 * Функция аллокации памяти из кеширующего аллокатора.
 * Должна возвращать указатель на участок памяти размера
 * как минимум object_size байт (см cache_setup).
 * Гарантируется, что cache указывает на корректный
 * инициализированный аллокатор.
 **/
void *cache_alloc(struct cache *cache)
{
  void *ret_mem = NULL;

  if (NULL != cache->slabs_part) {
    ret_mem = _alloc_obj_from_slab(cache->slabs_part);
    if (0 == cache->slabs_part->free_obj_num) {
      _replace_slab_list(&(cache->slabs_part), &(cache->slabs_full));
    }
  } else if (NULL != cache->slabs_free) {
    ret_mem = _alloc_obj_from_slab(cache->slabs_free);
    _replace_slab_list(&(cache->slabs_free), &(cache->slabs_part));
  } else {
    /* generate new slab */
    void *new_slab_mem = alloc_slab(cache->slab_order);
    slab_head_t *new_slab = _generate_slab(new_slab_mem, cache);
    ret_mem = _alloc_obj_from_slab(new_slab);
    _add_to_slab_list(new_slab, &(cache->slabs_part));
  }

  return ret_mem;
}


/**
 * Функция освобождения памяти назад в кеширующий аллокатор.
 * Гарантируется, что ptr - указатель ранее возвращенный из
 * cache_alloc.
 **/
void cache_free(struct cache *cache, void *ptr)
{
  uintptr_t *slab_mem = (uintptr_t *)((uintptr_t)ptr & ~(get_slab_size(cache->slab_order) - 1));
  slab_head_t *slab_head = _get_slab_head_ptr(slab_mem, cache);
  obj_head_t *obj_head = (obj_head_t *)((uintptr_t)ptr - sizeof(obj_head_t));

  obj_head->next_obj = slab_head->free_obj_ptr;
  slab_head->free_obj_ptr = obj_head;
  slab_head->free_obj_num += 1;

  if (slab_head->free_obj_num > cache->slab_objects) {
    printf("ERROR: cache_free: extra frees!!\n");
    return;
  }

  if (slab_head->free_obj_num == cache->slab_objects) {
    /* part -> free */
    if (cache->slabs_part == slab_head) {
      _replace_slab_list(&(cache->slabs_part), &(cache->slabs_free));
    } else {
      _replace_slab_list(&slab_head, &(cache->slabs_free));
    }
  } else if (slab_head->free_obj_num == 1) {
    /* full -> part */
    if (cache->slabs_full == slab_head) {
      _replace_slab_list(&(cache->slabs_full), &(cache->slabs_free));
    } else {
      _replace_slab_list(&slab_head, &(cache->slabs_free));
    }
  }
}


/**
 * Функция должна освободить все SLAB, которые не содержат
 * занятых объектов. Если SLAB не использовался для аллокации
 * объектов (например, если вы выделяли с помощью alloc_slab
 * память для внутренних нужд вашего алгоритма), то освбождать
 *slab_head->free_obj_num == cache-> его не обязательно.
 **/
void cache_shrink(struct cache *cache)
{
  _release_cache_list(&(cache->slabs_free), cache->slab_order);
}

void test1()
{
  const int test_allocs_num = 1000;

  struct cache b80_cache;
  void *newMem[test_allocs_num];

  printf("====== TEST 1 ========\n");

  cache_setup(&b80_cache, 3000);

  for (int i = 0; i < test_allocs_num; i++) {
    newMem[i] = cache_alloc(&b80_cache);
  }

  cache_release(&b80_cache);

  /* DEBUG */

  if (NULL == b80_cache.slabs_free) {
    printf("FREE - OK\n");
  } else {
    printf("FREE: %p\n", b80_cache.slabs_free);
  }

  if (NULL == b80_cache.slabs_full) {
    printf("FULL - OK\n");
  } else {
    printf("FULL: %p\n", b80_cache.slabs_full);
  }

  if (NULL == b80_cache.slabs_part) {
    printf("PART - OK\n");
  } else {
    printf("PART: %p\n", b80_cache.slabs_part);
  }

}

void test2()
{
  const int test_allocs_num = 25;
  const int loops = 15;

  struct cache b80_cache;
  void *newMem[test_allocs_num * loops];

  printf("====== TEST 2 ========\n");
  cache_setup(&b80_cache, 3000);

  int last_id = 0;
  for (volatile int j = 0; j < loops; j++) {
    for (volatile int i = 0; i < test_allocs_num; i++) {
      last_id = j * test_allocs_num + i;
      printf("%d: [j]%d * [i]%d\n", last_id, j, i);
      newMem[last_id] = cache_alloc(&b80_cache);
    }
    printf("%d: [j]%d\n", last_id, j);
    cache_free(&b80_cache, newMem[last_id]);
  }

  cache_release(&b80_cache);

  /* DEBUG */

  if (NULL == b80_cache.slabs_free) {
    printf("FREE - OK\n");
  } else {
    printf("FREE: %p\n", b80_cache.slabs_free);
  }

  if (NULL == b80_cache.slabs_full) {
    printf("FULL - OK\n");
  } else {
    printf("FULL: %p\n", b80_cache.slabs_full);
  }

  if (NULL == b80_cache.slabs_part) {
    printf("PART - OK\n");
  } else {
    printf("PART: %p\n", b80_cache.slabs_part);
  }
}

void test3()
{
  const int test_allocs_num = 900;
  const int test_frees_num = 870;

  struct cache b80_cache;
  void *newMem[test_allocs_num];

  printf("====== TEST 3 ========\n");

  cache_setup(&b80_cache, 3000);

  for (int i = 0; i < test_allocs_num; i++) {
    newMem[i] = cache_alloc(&b80_cache);
  }

  for (int i = 0; i < test_frees_num; i++) {
    cache_free(&b80_cache, newMem[i]);
  }

  cache_shrink(&b80_cache);

  /* DEBUG */

  if (NULL == b80_cache.slabs_free) {
    printf("FREE - OK\n");
  } else {
    printf("FREE: %p\n", b80_cache.slabs_free);
  }

  if (NULL == b80_cache.slabs_full) {
    printf("FULL - OK\n");
  } else {
    printf("FULL: %p\n", b80_cache.slabs_full);
  }

  if (NULL == b80_cache.slabs_part) {
    printf("PART - OK\n");
  } else {
    printf("PART: %p\n", b80_cache.slabs_part);
  }

}

int main()
{
  test1();
  test2();
  test3();
  return 0;
}
