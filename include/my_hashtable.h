#ifndef MY_HASHTABLE_H_
# define MY_HASHTABLE_H_

#include <stddef.h>
#include "my_allocator.h"

/* #define MY_HASHTABLE_IMPL */
/* Usage:
  struct persons {
    size_t len; // mandatory
    struct person {
      char *key; // mandatory AND must be a string
	  // the rest are values
      int age;
    } *items;
  };

  struct persons persons = {0};

  hmput(allocator, persons, (struct person){ "hesham", 21 });
  hmput(allocator, persons, (struct person){ "othmane", 22 });
  hmput(allocator, persons, (strcut person){ "something", 67 });

  for (size_t i=0; i < persons.len; i += 1) {
    struct person person = persons.items[i];
	printf("%s: %d\n", person.key, person.age);
  }
 */

# ifndef MY_HASHTABLE_CAPACITY
#  define MY_HASHTABLE_CAPACITY 10
# endif // MY_HASHTABLE_CAPACITY

# ifndef HM_EMPTY
#  define HM_EMPTY ((size_t)-1)
# endif // HM_EMPTY

typedef size_t hmindex_t;
struct hash_table_meta {
    hmindex_t index_cap;
    hmindex_t *index;
    hmindex_t *dist;
};

#define hmsize(hm_) (_hmsize((hm_).items, sizeof(*(hm_).items)))
#define hmsizeraw(hm_, size_) (_hmsize(items, size_))
#define hmmeta(hm_) (_hm_meta((hm_).items))
#define hmmetaraw(items_) (_hm_meta(items_))
#define hmcap(hm_) (hmmeta(hm_))->index_cap
#define hmcapraw(items_) (hmmetaraw(items_))->index_cap
#define hmput(allocator_, hm_, ...) \
	_hm_put_impl( \
		(allocator_), \
		(void**)&((hm_).items), \
		&(hm_).len, \
		sizeof(*(hm_).items), \
		&(__VA_ARGS__))
#define hmget(map_, key_) _hm_get_impl((map_).items, sizeof(*(map_).items), (key_))
#define hmdel(map_, key_) _hm_del_impl((map_).items, &(map_).len, sizeof(*(map_).items), (key_))
#define hmfree(allocator_, map_) _hm_free((allocator_), (map_).items, sizeof(*(map_).items))
#define hmfreeraw(allocator_, items_, item_size_) _hm_free((allocator_), items_, item_size_)

/* implementation */
size_t _hash_str(const char *str);
size_t _hmsize(void *items, size_t item_size);
struct hash_table_meta *_hm_meta(void *items);
void *_hm_init_impl(struct Allocator allocator, size_t item_size, hmindex_t cap);
void _hm_free(struct Allocator allocator, void *items, size_t item_size);
void _hm_resize(
    struct Allocator allocator,
    void **items_ptr,
    size_t *len,
    size_t item_size);
void _hm_put_impl(
	struct Allocator allocator,
	void **items_ptr,
	size_t *len,
	size_t item_size,
	void *item);
void *_hm_get_impl(void *items, size_t item_size, const char *key);
void _hm_del_impl(void *items, size_t *len, size_t item_size, const char *key);

# ifdef MY_HASHTABLE_IMPL
size_t _hash_str(const char *str)
{
	size_t h = 1469598103934665603ULL;
	while (*str != '\0') {
		h ^= (unsigned char)*str++;
		h *= 1099511628211ULL;
	}
	return h;
}

size_t _hmsize(void *items, size_t item_size)
{
	struct hash_table_meta *meta = hmmetaraw(items);
	size_t cap = meta->index_cap;
    return
		cap * item_size + // items
		cap * sizeof(hmindex_t) + // index
		cap * sizeof(hmindex_t) + // dist
		sizeof(struct hash_table_meta);
}

struct hash_table_meta *_hm_meta(void *items)
{
    return (struct hash_table_meta *)((char *)items - sizeof(struct hash_table_meta));
}

void *_hm_init_impl(struct Allocator allocator, size_t item_size, hmindex_t cap)
{
    size_t total =
        cap * sizeof(hmindex_t) + // index
        cap * sizeof(hmindex_t) + // dist
		sizeof(struct hash_table_meta) +
        cap * item_size;          // items

    void *mem = alloc(allocator, total);

	hmindex_t *index = mem;
	hmindex_t *dist = index + cap;

    for (size_t i = 0; i < cap; i++) {
        index[i] = HM_EMPTY;
        dist[i]  = 0;
    }

    struct hash_table_meta *meta = (void*)(dist + cap);
    meta->index_cap = cap;
    meta->index = index;
    meta->dist  = dist;

    return (void *)(meta + 1);
}

void _hm_free(struct Allocator allocator, void *items, size_t item_size)
{
    if (!items) return;

    struct hash_table_meta *meta = _hm_meta(items);
	// get the begining
	hmindex_t *ptr = (void*)meta;
	ptr -= meta->index_cap * 2;
    xdestroy(allocator, hmsizeraw(items, item_size), ptr);
}

void _hm_resize(
    struct Allocator allocator,
    void **items_ptr,
    size_t *len,
    size_t item_size)
{
    void *old_items = *items_ptr;
    struct hash_table_meta *old_meta = _hm_meta(old_items);

    size_t new_cap = old_meta->index_cap * 2;

    void *new_items = _hm_init_impl(allocator, item_size, new_cap);

    *items_ptr = new_items;
    *len = 0;

    for (size_t i = 0; i < old_meta->index_cap; i++) {
        if (old_meta->index[i] != HM_EMPTY) {
            size_t idx = old_meta->index[i];
            void *item = (char *)old_items + idx * item_size;

            _hm_put_impl(
                allocator,
                items_ptr,
                len,
                item_size,
                item);
        }
    }

	hmfreeraw(allocator, old_items, item_size);
}

void _hm_put_impl(
    struct Allocator allocator,
    void **items_ptr,
    size_t *len,
    size_t item_size,
    void *item) {
	const char *key = *((char **)item);
    if (*items_ptr == NULL) {
        *items_ptr = _hm_init_impl(allocator, item_size, MY_HASHTABLE_CAPACITY);
    }

	if (*items_ptr && *len >= (hmcapraw(*items_ptr) * 7) / 10) {
		_hm_resize(allocator, items_ptr, len, item_size);
	}

    struct hash_table_meta *meta = _hm_meta(*items_ptr);
    size_t index_cap = meta->index_cap;

    size_t h = _hash_str(key);
    size_t pos = h % index_cap;
    size_t dist = 0;

    size_t new_idx = (size_t)-1;

    while (1) {
        if (meta->index[pos] == HM_EMPTY) {
            if (new_idx == (size_t)-1) {
                new_idx = (*len);
                (*len)++;

                memcpy((char *)(*items_ptr) + new_idx * item_size,
                       item, item_size);
            }

            meta->index[pos] = new_idx;
            meta->dist[pos] = dist;
            return;
        }

        size_t cur_idx = meta->index[pos];
        void *cur_item = (char *)(*items_ptr) + cur_idx * item_size;
        char **cur_key = (char **)cur_item;

        /* key exists → replace */
        if (*cur_key == key ||
			strcmp(*cur_key, key) == 0) {
            memcpy(cur_item, item, item_size);
            return;
        }

        if (meta->dist[pos] < dist) {
            /* swap */
            size_t tmp_idx = meta->index[pos];
            size_t tmp_dist = meta->dist[pos];

            if (new_idx == (size_t)-1) {
                new_idx = (*len);
                (*len)++;

                memcpy((char *)(*items_ptr) + new_idx * item_size,
                       item, item_size);
            }

            meta->index[pos] = new_idx;
            meta->dist[pos] = dist;

            new_idx = tmp_idx;
            dist = tmp_dist;
        }

        pos = (pos + 1) % index_cap;
        dist++;
    }
}

void *_hm_get_impl(void *items, size_t item_size, const char *key)
{
    if (!items) return NULL;

    struct hash_table_meta *meta = _hm_meta(items);
    size_t cap = meta->index_cap;

    size_t h = _hash_str(key);
    size_t pos = h % cap;
    size_t dist = 0;

    while (1) {
        if (meta->index[pos] == HM_EMPTY)
            return NULL;

        if (meta->dist[pos] < dist)
            return NULL;

        size_t idx = meta->index[pos];
        void *item = (char *)items + idx * item_size;
        char **item_key = (char **)item;

        if (*item_key == key ||
			strcmp(*item_key, key) == 0)
            return item;

        pos = (pos + 1) % cap;
        dist++;
    }
}

void _hm_del_impl(void *items, size_t *len, size_t item_size, const char *key)
{
    if (items == NULL) return;

    struct hash_table_meta *meta = _hm_meta(items);
    size_t cap = meta->index_cap;

	size_t idx = 0;
    size_t h = _hash_str(key);
    size_t pos = h % cap;
    size_t dist = 0;

    while (1) {
        if (meta->index[pos] == HM_EMPTY)
            return;

        if (meta->dist[pos] < dist)
            return;

        idx = meta->index[pos];
        void *item = (char *)items + idx * item_size;
        char **item_key = (char **)item;

        if (*item_key == key ||
			strcmp(*item_key, key) == 0) {
            break;
        }

        pos = (pos + 1) % cap;
        dist++;
    }

	{
		size_t last = (*len) - 1;

		if (idx != last) {
			/* move last item into deleted slot */
			memcpy(
				(char *)items + idx * item_size,
				(char *)items + last * item_size,
				item_size
			);

			/* update hash table to point to new index */
			void *moved_item = (char *)items + idx * item_size;
			const char *moved_key = *((char **)moved_item);

			size_t h2 = _hash_str(moved_key);
			size_t pos2 = h2 % cap;
			size_t dist2 = 0;

			while (1) {
				size_t i2 = meta->index[pos2];

				if (i2 == last) {
					meta->index[pos2] = idx;
					break;
				}

				pos2 = (pos2 + 1) % cap;
				dist2++;
			}

            (void)dist2;
		}

		(*len)--;
	}

    /* backward shift */
    size_t cur = pos;
    size_t next = (cur + 1) % cap;

    while (meta->index[next] != HM_EMPTY && meta->dist[next] > 0) {
        meta->index[cur] = meta->index[next];
        meta->dist[cur]  = meta->dist[next] - 1;

        cur = next;
        next = (next + 1) % cap;
    }

    meta->index[cur] = HM_EMPTY;
    meta->dist[cur]  = 0;
}
# endif // MY_HASHTABLE_IMPL

#endif // !MY_HASHTABLE_H_
