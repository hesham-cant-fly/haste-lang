#include "haste.h"
#include "my_termcolor.h"

static const char *declaration_name(struct haste_ast_node *node)
{
	assert(node_is_declaration(node) and "Has to be a declaration.");

	switch (node->kind) {
	case ND_VAR_DECL: {
		struct haste_ast_var_decl *var = (void*)node;
		return var->name.ident;
	} break;
	default:
		unreachable();
	}
}

Error hoist(struct Allocator allocator, const source_file_id src)
{
	struct haste_ast_node *root = get_source_file_ast(src);
	leach (struct haste_ast_node, current, root) {
		if (not node_is_declaration(current)) continue;

		const char *name = declaration_name(current);
		
		if (hmget(sources.items[src].declarations, name)) {
			f_report_at_token(src, ANSI_CODE_RED "Error", current->start, "a redifinition of this global.");
			return ERROR;
		}

		hmput(allocator, sources.items[src].declarations, (struct haste_declaration){ .key = name, current });
	}

	return OK;
}
