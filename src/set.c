#include <stdlib.h>
#include <stdbool.h>

#include "set.h"

static const int DEFAULT_SET_SIZE = 100;

set_t* create_set()
{
	return create_hash(DEFAULT_SET_SIZE);
}

int set_cardinal(set_t *s) { return s->count; }

int set_add(set_t *s, char *key)
{
	if (hash_set(s, key, &MEMBER))
		return set_cardinal(s);
	else
		return SET_ADD_ERROR;
}

bool set_has_element(set_t *s, const char *key)
{
	if (NULL == hash_get(s, key))
		return false;
	else
		return true;
}

void destroy_set(set_t *s) { destroy_hash(s); }
