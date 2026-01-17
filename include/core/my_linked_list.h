#ifndef MY_LINKED_LIST_H__
#define MY_LINKED_LIST_H__

#define linked_list_each(T__, arr__, current__, next__)	\
		for (T__ *current__ = (arr__),*next__ = (current__)->next; \
			 (current__) != NULL; \
		     (current__) = (next__), (next__) = ((current__) == NULL) ? NULL : (current__)->next)

#endif // !MY_LINKED_LIST_H__
