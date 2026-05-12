#include "haste.h"
#include "my_termcolor.h"

static const char *declaration_name(struct intern_table *table, struct haste_ast_node *node)
{
	assert(node_is_declaration(node) and "Has to be a declaration.");

	switch (node->kind) {
	case ND_VAR_DECL:
		return intern_token(table, node->variable.name);
	default:
		unreachable();
	}
}

Error hoist(struct Allocator allocator,
			struct intern_table *table,
            const source_file_id src)
{
	struct haste_ast_node *root = get_source_file_ast(src);
	leach (struct haste_ast_node, current, root) {
		assert(node_is_declaration(current) and "Has to be a declaration.");

		const char *name = declaration_name(table, current);
		
		if (hmget(sources.items[src].declarations, name)) {
			f_report_at_token(src, ANSI_CODE_RED "Error", current->start, "a redifinition of this global.");
			return ERROR;
		}

		hmput(allocator, sources.items[src].declarations, (struct haste_declaration){ .key = name, current });
	}

	return OK;
}
