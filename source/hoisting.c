#include "haste.h"

static struct token declaration_name(struct haste_ast_node *node)
{
	assert(node_is_declaration(node) and "Has to be a declaration.");

	switch (node->kind) {
	case ND_VAR_DECL:
		return node->variable.name;
	default:
		unreachable();
	}
}

Error hoist(struct Allocator allocator,
            struct Allocator arena_allocator,
            const source_file_id src)
{
	struct haste_ast_node *root = get_source_file_ast(src);
	leach (struct haste_ast_node, current, root) {
		assert(node_is_declaration(current) and "Has to be a declaration.");

		struct token name = declaration_name(current);
		char *id = nclone_string(arena_allocator, name.start, name.len);
		
		if (hmget(sources.items[src].declarations, id)) {
			// TODO: report an error
			f_report_at_token(src, "Error", current->start, "a redifinition of this global.");
			return ERROR;
		}

		hmput(allocator, sources.items[src].declarations, (struct haste_declaration){ .key = id, current });
	}

	return OK;
}
