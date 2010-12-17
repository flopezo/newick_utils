/* 

Copyright (c) 2010 Thomas Junier and Evgeny Zdobnov, University of Geneva
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
* Neither the name of the University of Geneva nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <libguile.h>

#include "rnode.h"
#include "link.h"
#include "list.h"
#include "tree.h"
#include "parser.h"
#include "to_newick.h"
#include "tree_editor_rnode_data.h"
#include "common.h"

struct rnode *current_node;

enum order { POST_ORDER, PRE_ORDER };

struct parameters {
	char *scheme_expr;	// a (test action) pair
	int show_tree;
	int order;
	int stop_clade_at_first_match;
};

void help(char *argv[])
{
	printf (
"Performs actions on nodes that match some condition\n"
"\n"
"Synopsis\n"
"--------\n"
"\n"
"%s [-hnor] <newick trees filename|-> <address> <action>\n"
"\n"
"Input\n"
"-----\n"
"\n"
"First argument is the name of a file that contains Newick trees, or '-' (in\n"
"which case trees are read from standard input).\n"
"\n"
"Second argument is a node address, in the form of a logical expression (see\n"
"Addresses below).\n"
"\n"
"Third argument is a code that specifies an action to perform on nodes\n"
"which match the address (see Actions).\n" 
"\n"
"Output\n"
"------\n"
"\n"
"By default, prints the input tree, which may have been modified. However,\n"
"the 's' action (see Actions, below) causes matching subtrees to be\n"
"printed out.\n"
"\n"
"This program is analogous to pattern-oriented, stream processing UNIX\n"
"utilities like sed(1) and awk(1), but instead of working on lines (like\n"
"sed) or records (like awk), %s works on tree nodes.\n"
"\n"
"The program traverses the tree in Newick order, evaluating the address\n"
"expression for each node in turn. If (and only if) the address matches, the\n"
"action is performed.\n"
"\n"
"Addresses\n"
"---------\n"
"\n"
"The address expression involves node properties such as depth, bootstrap\n"
"support, whether or not a node is a leaf, etc. These are represented by\n"
"single-letter codes, to make expressions short. For example:\n"
"\n"
"				       i\n"
"\n"
"matches internal nodes, while\n"
"\n"
"				     b > 75\n"
"\n"
"matches nodes whose label has a numerical value of 75 or more (if the label\n"
"is numeric). The usual logical and relational operators are available, so\n"
"\n"
"				   i & b > 75\n"
"\n"
"could be used to match internal nodes with a bootstrap support value\n"
"greater than 75.\n"
"\n"
"The functions are:\n"
"    a numeric    number of ancestors of node	\n"
"    b numeric    node's support value (or zero)\n"
"    d numeric    node's depth (distance to root)\n"
"    c numeric    node's number of children\n"
"    D numeric    node's number of descendants\n"
"    i boolean    true iff node is strictly internal (i.e., not root!)\n"
"    l boolean    true iff node is a leaf\n"
"    r boolean    true iff node is the root\n"
"\n"
"The operators are:\n"
"    ==  equality\n"
"    !=  inequality\n"
"    <   greater than\n"
"    >   lesser than\n"
"    >=  greater than or equal to\n"
"    <=  lesser than or equal to\n"
"    !   logical negation\n"
"    &   logical and\n"
"    |   logical or\n"
"\n"
"The operator precedence is: negation, relationals, and, or; i.e. \n"
"\n"
"				 1 == d & !i | l\n"
"\n"
"is equivalent to\n"
"\n"
"			       ((1 == d) & (!i)) | l\n"
"\n"
"Parentheses can be used for overriding precedence, or for clarity.\n"
"\n"
"Actions\n"
"-------\n"
"\n"
"Actions are performed on nodes that match the address. They are:\n"
"    s   (Subtree) print subtree rooted at matching node\n"
"    o   (splice Out) splice out node, and attach children to parent, \n"
"	   preserving branch lengths. This is useful for \"opening\" poorly\n"
"          supported nodes.\n"
"    d   Delete node\n"
"    l   Print node's label\n"   
"\n"
"\n"
"Options\n"
"-------\n"
"\n"
"    -h: print this help text, and exit\n"
"    -n: do not print the (possibly modified) tree at the end of the run \n"
"        (modeled after sed -n)\n"
"    -r: visit tree in preorder (starting at root, and visiting a node\n"
"        before any of its descendants). Default is post-order (ends at root\n"
"        and visits a node after all its descendats).\n"
"    -o: stop processing a clade after the first match - that is, if a node\n"
"        matches, its descendants are not processed.\n"
"        Note: this option will automatically set -r, as it makes no\n"
"        sense in post-order.\n"
"\n"
"Bugs\n"
"----\n"
"\n"
"Although there are no known bugs in this program, it is to be considered\n"
"more experimental than the others.\n"
"\n"
"Examples\n"
"--------\n"
"\n"
"# \"open\" all nodes with bootstrap support <= 10 (assuming support is coded\n"
"# in internal node labels)\n"
"\n"
"$ %s data/HRV.bs.nw 'i & b <= 10' o \n"
"\n"
"# \"open\" all nodes with bootstrap support < 750, then discard leaves that\n"
"# are directly attached to the ingroup's root. This effectively keeps only\n"
"# leaves that are part of well-supported clades.\n"
"\n"
"$ %s data/big.rn.nw 'i & b < 750' o | %s - 'l & a == 2' d\n"
"\n"
"# get all clades with at least one ancestor, 980 or better support. Do not\n"
"# print subtrees of matching clades, even if they match (option -o)\n"
"\n"
"$ %s data/big.rn.nw -n -o 'a >= 1 & b >= 980' s\n",
	argv[0],
	argv[0],
	argv[0],
	argv[0],
	argv[0],
	argv[0]
	);
}

struct parameters get_params(int argc, char *argv[])
{
	struct parameters params;

	params.show_tree = true;
	params.order = POST_ORDER;
	params.stop_clade_at_first_match = false;

	int opt_char;
	while ((opt_char = getopt(argc, argv, "hnor")) != -1) {
		switch (opt_char) {
		case 'h':
			help(argv);
			exit(EXIT_SUCCESS);
		case 'n':
			params.show_tree = false;
			break;
		case 'o':
			params.stop_clade_at_first_match = true;
			params.order = PRE_ORDER;
			break;
		case 'r':
			params.order = PRE_ORDER;
			break;
		default:
			fprintf (stderr, "Unknown option '-%c'\n", opt_char);
			exit (EXIT_FAILURE);
		}
	}

	/* check arguments */
	if (2 == (argc - optind))	{
		if (0 != strcmp("-", argv[optind])) {
			FILE *fin = fopen(argv[optind], "r");
			extern FILE *nwsin;
			if (NULL == fin) {
				perror(NULL);
				exit(EXIT_FAILURE);
			}
			nwsin = fin;
		}
		params.scheme_expr = argv[optind+1];
	} else {
		fprintf(stderr, "Usage: %s [-hnro] <filename|-> <address> <operation>\n",
				argv[0]);
		exit(EXIT_FAILURE);
	}

	return params;
}

/* A helper for parse_order_traversal() - gets the number of descendants of a
 * node. This function is NOT recursive, the node data of all descendants must
 * have been set already - which is why we're calling it from
 * parse_order_traversal().
 * */

int get_nb_descendants(struct rnode *node)
{
	struct list_elem *e;
	struct rnode *kid;
	struct rnode_data *rndata;
	int descendants = 0;

	for (e = node->children->head; NULL != e; e = e->next) {
		kid = e->data;
		rndata = kid->data;
		descendants += rndata->nb_descendants;
		descendants += 1;	/* kid itself (no pun intended :-) ) */
	}

	return descendants;
}

/* This allocates the rnode_data structure for each node, and fills it with
 * "top-down" data,  i.e. data for which the parent's value needs to be known
 * (such as depth and number of ancestors). Some values do not depend on the
 * parent nor on the children, I also set them here (this is arbitrary, I
 * could set them in parse_order_traversal() below as well). */

void reverse_parse_order_traversal(struct rooted_tree *tree)
{
	struct list_elem *el;
	struct llist *rev_nodes = llist_reverse(tree->nodes_in_order);
	if (NULL == rev_nodes) { perror(NULL), exit(EXIT_FAILURE); }
	struct rnode *node;
	struct rnode_data *rndata;

	el = rev_nodes->head;	/* root */
	node = (struct rnode *) el->data;
	rndata = malloc(sizeof(struct rnode_data));
	if (NULL == rndata) { perror(NULL); exit (EXIT_FAILURE); }
	rndata->nb_ancestors = 0;
	rndata->depth = 0;
	rndata->stop_mark = false;
	node->data = rndata;

	/* WARNING: don't forget to set values for the root's data, above. The
	 * following loop starts at the first non-root node! */

	for (el = rev_nodes->head->next; NULL != el; el = el -> next) {
		node = (struct rnode *) el->data;
		struct rnode_data *parent_data = node->parent->data;
		rndata = malloc(sizeof(struct rnode_data));
		if (NULL == rndata) { perror(NULL); exit (EXIT_FAILURE); }
		rndata->nb_ancestors = parent_data->nb_ancestors + 1;
		rndata->depth = parent_data->depth +
			atof(node->edge_length_as_string);

		rndata->stop_mark = false;
		node->data = rndata;
	}

	destroy_llist(rev_nodes);
}

/* This fills bottom-up data. Note that it relies on rnode_data being already
 * allocated, which is done in reverse_parse_order_traversal(). Data that does
 * not depend on order is also filled in here. */

void parse_order_traversal(struct rooted_tree *tree)
{
	struct list_elem *el;
	struct rnode *node;
	struct rnode_data *rndata;

	for (el = tree->nodes_in_order->head; NULL != el; el = el -> next) {
		node = (struct rnode *) el->data;
		rndata = (struct rnode_data *) node->data;
		rndata->support = atof(node->label);	
		rndata->nb_descendants = get_nb_descendants(node);
	}
}

/* Sets the value of the predefined variables (i, l, a, etc), according to the
 * node passed as argument. This is normally the current node, while visiting
 * the tree. Using variables rather than functions makes for shorter Scheme
 * expression can be shorter, because function calls can be replaced by
 * variables, e.g. (< 2 a) instead of (< 2 (a)) to check that the current node
 * has fewer than two ancestors. On the command line, this is handy. */

void set_predefined_variables(struct rnode *node)
{
	/* b: returns node label, as a bootstrap support value */
	if (is_leaf(node))
		scm_c_define("b", SCM_BOOL_F);
	else {
		SCM label = scm_from_locale_string(node->label);
		SCM support_value = scm_string_to_number(label, SCM_UNDEFINED);
		scm_c_define("b", support_value);
	}

	/* i: true IFF node is inner (not leaf, not root) */
	if (is_inner_node(node))
		scm_c_define("i", SCM_BOOL_T);
	else
		scm_c_define("i", SCM_BOOL_F);

	/* l: true IFF node is a leaf */
	if (is_leaf(node))
		scm_c_define("l", SCM_BOOL_T);
	else
		scm_c_define("l", SCM_BOOL_F);
}

/* 'address' and 'action' are Scheme expressions. The tree is visited, and
 * 'address' is evaluated at each node (though some may be skipped). If
 * 'address' is true, 'action' is perfomed. */

void process_tree(struct rooted_tree *tree, SCM address,
		SCM action, struct parameters params)
{
	struct llist *nodes;
	struct list_elem *el;

	/* these two traversals fill the node data. */
	// TODO: would gain time by only performing those traversals that are
	// needed
	reverse_parse_order_traversal(tree);
	parse_order_traversal(tree);

	if (POST_ORDER == params.order)
		nodes = tree->nodes_in_order;
	else if (PRE_ORDER == params.order) {
		nodes = llist_reverse(tree->nodes_in_order);
		if (NULL == nodes) { perror(NULL); exit(EXIT_FAILURE); }
	}
	else 
		assert(0);	 /* programmer error... */


	/* Main loop: Iterate over all nodes */
	for (el = nodes->head; NULL != el; el = el -> next) {
		current_node = (struct rnode *) el->data;

		/* Check for stop mark in parent (see option -o) */
		if (! is_root(current_node)) { 	/* root has no parent... */
			if (((struct rnode_data *)
				current_node->parent->data)->stop_mark) {
				/* Stop-mark the current node and continue */ 
				((struct rnode_data *)
					current_node->data)->stop_mark = true;
				continue;
			}
		} 

		set_predefined_variables(current_node);
		SCM is_match = scm_primitive_eval(address);
		if (! scm_is_false(is_match))
			scm_primitive_eval(action);
	}

	/* If order is PRE_ORDER, the list of nodes is an inverted copy, so we
	 * need to free it. */
	if (PRE_ORDER == params.order)
		destroy_llist(nodes);
}

/* Makes C functions available to Scheme */

SCM scm_dump_subclade()
{
	dump_newick(current_node);
}

static void register_C_functions()
{
	// TODO: later
	// scm_c_define_gsubr("l?", 0, 0, 0, scm_is_leaf);
	scm_c_define_gsubr("s", 0, 0, 0, scm_dump_subclade);
	scm_c_define_gsubr("dump-subclade", 0, 0, 0, scm_dump_subclade);
}

static void inner_main(void *closure, int argc, char* argv[])
{
	struct parameters params = get_params(argc, argv);
	struct rooted_tree *tree;

	scm_c_eval_string("(define & and)");	/* short alias */

	SCM expr_scm = scm_from_locale_string(params.scheme_expr);
	SCM in_port = scm_open_input_string(expr_scm);
	SCM expr = scm_read(in_port);
	SCM address = scm_car(expr);
	SCM action = scm_cadr(expr);

	register_C_functions();

	while (NULL != (tree = parse_tree())) {
		process_tree(tree, address, action, params);
		if (params.show_tree) {
			dump_newick(tree->root);
		}
		destroy_tree(tree, FREE_NODE_DATA);
	}
}

int main(int argc, char* argv[])
{
       scm_boot_guile (argc, argv, inner_main, 0);
       return 0; /* never reached */
}
