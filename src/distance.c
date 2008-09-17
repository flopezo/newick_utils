/* distance: print distances between nodes, in various ways. */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "tree.h"
#include "parser.h"
#include "nodemap.h"
#include "hash.h"
#include "list.h"
#include "lca.h"
#include "simple_node_pos.h"
#include "rnode.h"
#include "redge.h"
#include "node_pos_alloc.h"
#include "common.h"

enum {FROM_ROOT, FROM_LCA, MATRIX, FROM_PARENT};
enum {HORIZONTAL, VERTICAL};
enum {SQUARE, TRIANGLE};
enum {ALL_NODES, ALL_LABELS, ALL_LEAF_LABELS, ARGV_LABELS, ALL_INNER_NODES,
	ALL_LEAVES};

struct parameters {
	int distance_type;
	int selection;
	struct llist *labels;
	char separator;
	int show_header;
	int list_orientation;
	int matrix_shape;
};

void help(char *argv[])
{
	printf (
"Prints distances between nodes, following tree branches, in various ways.\n"
"\n"
"Synopsis\n"
"--------\n"
"\n"
"%s [-hmnst] <tree file|-> [label]*\n"
"\n"
"Input\n"
"-----\n"
"\n"
"First argument is the name of a file containing one or more trees, or '-'\n"
"(in which case trees are read from standard input). Any subsequent arguments\n"
"are node labels.\n"
"\n"
"Output\n"
"------\n"
"\n"
"Output is controlled by the mode and the selection. The mode specifies how\n"
"to compute distances, and the selection specifies which nodes to compute\n"
"distances for. There are four modes:\n"
"    o root mode (default): print distances from the tree's root to all\n"
"      nodes in the selection\n"
"    o last common ancestor mode: print distances from the LCA of all nodes\n"
"      in the selection to all nodes in the selection\n"
"    o parent mode: print distances from each node in the selection\n"
"      to its parent (0 for root the root node)\n"
"    o matrix mode: print distances from each node in the selection\n"
"      to each node in the selection\n"
"Valid selections are:\n"
"    o all labeled leaves (default): selection is formed by all labeled\n"
"      leaf nodes, in the order in which they appear in the Newick string\n"
"      (= Newick order)\n"
"    o command line arguments: labels are passed on the command line,\n"
"      and the selection is formed by the corresponding nodes, in the\n"
"      same order.\n"
"    o all labeled nodes: selection is formed by all labeled nodes, in\n"
"      Newick order (see Option -s)\n"
"    o all nodes: selection is formed by all nodes, in Newick order\n"
"      (see Option -s)\n"
"\n"
"Options\n"
"-------\n"
"\n"
"    -h: print this message and exit \n"
"    -m <mode>: selects mode (see Output). Mode is determined by the first\n"
"        letter of the argument: 'r' for root mode (default), 'l' for LCA,\n"
"        'p' for parent, and 'm' for matrix. Thus, '-mm', '-m matrix',\n"
"        and '-m mat' all select matrix mode.\n"
"    -n: prints labels (or '' if empty) in addition to distances.\n"
"    -s <selection>, where selection is determined by the first letter of\n"
"        the argument: 'a' for all nodes, 'l' for labeled nodes,\n"
"        'i' for inner nodes, 'f' for leaves.\n"
"        E.g. '-s a' and '-s all' both select all nodes.\n"
"    -t: tab-separated - prints values on one line, separated by tabs.\n"
"        Ignored in matrix mode.\n"
"\n"
"Assumptions and Limitations\n"
"---------------------------\n"
"\n"
"Labels passed as arguments are assumed to exist in the tree. Behaviour is\n"
"undefined if a label is not found.\n"
"\n"
"Examples\n"
"--------\n"
"\n"
"$ %s data/catarrhini	# all (labeled) leaves, from root\n"
"\n"
"# Pongo and Homo, from their LCA, with labels, single line\n"
"$ %s -m lca -t -n data/catarrhini Homo Pongo \n"
"\n"
"# All labeled nodes, matrix form, with labels\n"
"$ %s -mm -n -sl data/catarrhini\n"
"\n"
"# All nodes, distance to parent\n"
"$ %s -m p -s a data/catarrhini\n",
	argv[0],
	argv[0],
	argv[0],
	argv[0],
	argv[0]);
}

int get_selection()
{
	switch (tolower(optarg[0])) {
	case 'a':
		return ALL_NODES;
	case 'f':
		return ALL_LEAVES;
	case 'i':
		return ALL_INNER_NODES;
	case 'l':
		return ALL_LABELS;
	default:
		fprintf (stderr, 
		"ERROR: unknown selection '%s'\nvalid values: a(ll nodes), l(abeled nodes)", optarg);
		exit(EXIT_FAILURE);
	}
}

/* Returns the distance type (root, LCA, or matrix) based on the first characer
 * of 'optarg' */

int get_distance_type()
{
	switch (tolower(optarg[0])) {
	case 'l': /* lca, l, etc */
		return FROM_LCA;
	case 'm': /* matrix, m, etc */ 
		return MATRIX;
	case 'r': /* root, r, etc - default anyway */ 
		return FROM_ROOT;
	case 'p':
		return FROM_PARENT;
	default:
		fprintf (stderr, 
			"ERROR: unknown distance method '%s'\nvalid values: l(ca), m(atrix), r(oot), p(arent)", optarg);
		exit(EXIT_FAILURE);
	}
	/* should never get here */
	return -1;
}

struct parameters get_params(int argc, char *argv[])
{

	struct parameters params;

	params.distance_type = FROM_ROOT;
	params.selection = ALL_LEAF_LABELS;
	params.separator = '\n';
	params.show_header = FALSE;
	params.list_orientation = VERTICAL;
	params.matrix_shape = SQUARE;

	int opt_char;
	while ((opt_char = getopt(argc, argv, "hm:ns:t")) != -1) {
		switch (opt_char) {
		case 'h':
			help(argv);
			exit(EXIT_SUCCESS);
		case 'm':
			params.distance_type = get_distance_type();
			break;
		case 'n':
			params.show_header = TRUE;
			break;
		case 's':
			params.selection = get_selection();
			break;
		case 't':
			params.list_orientation = HORIZONTAL;
			break;
		default:
			fprintf (stderr, "Unknown option '-%c'\n", opt_char);
			exit(EXIT_FAILURE);
			break; /* ok, not very useful... but I might later decide to ignore the bad option rather than fail. */
		}
	}

	/* check arguments */
	if ((argc - optind) >= 1)	{
		if (0 != strcmp("-", argv[optind])) {
			FILE *fin = fopen(argv[optind], "r");
			extern FILE *nwsin;
			if (NULL == fin) {
				perror(NULL);
				exit(EXIT_FAILURE);
			}
			nwsin = fin;
		}
		struct llist *lbl_list = create_llist();
		optind++;	/* optind is now index of 1st label */
		for (; optind < argc; optind++) {
			append_element(lbl_list, argv[optind]);
		}
		params.labels = lbl_list;
		if (0 != lbl_list->count)
			params.selection = ARGV_LABELS;
	} else {
		fprintf(stderr, "Usage: %s [-ahimnt] <filename|-> [label+]\n",
				argv[0]);
		exit(EXIT_FAILURE);
	}

	return params;
}

struct llist *get_selected_nodes (struct rooted_tree *tree, 
		int selection)
{
	struct llist *result;
	struct list_elem *el;
	struct rnode *node;

	switch (selection) {
		case ALL_NODES:
			result = shallow_copy(tree->nodes_in_order);
			break;
		case ALL_LABELS:
			result = create_llist();
			for (el = tree->nodes_in_order->head; NULL != el;
				el = el->next) {
				node = el->data;
				if (0 != strcmp(node->label, ""))
					append_element(result, node);
			}
			break;
		case ALL_LEAF_LABELS:
			result = create_llist();
			for (el = tree->nodes_in_order->head; NULL != el;
				el = el->next) {
				node = el->data;
				if (is_leaf(node) &&
				   (0 != strcmp(node->label, "")))
					append_element(result, node);
			}
			break;
		case ALL_LEAVES:
			result = create_llist();
			for (el = tree->nodes_in_order->head; NULL != el;
				el = el->next) {
				node = el->data;
				if (is_leaf(node))
					append_element(result, node);
			}
			break;
		case ALL_INNER_NODES:
			result = create_llist();
			for (el = tree->nodes_in_order->head; NULL != el;
				el = el->next) {
				node = el->data;
				if (is_inner_node(node))
					append_element(result, node);
			}
			break;
		default:
			fprintf (stderr, "ERROR: no selection code '%d'\n",
					selection);
			exit (EXIT_FAILURE);
	}

	return result;
}

/* Returns the distance from node 'ancestor' to node 'descendant', where
 * 'descendant' is a decendant of 'ancestor'. Node positions must have been
 * assigned in simple_node_pos structures. */

double distance_to_descendant(struct rnode *ancestor, struct rnode *descendant)
{
	if (is_root(descendant))
		return 0.0;

	if (NULL == ancestor) {
		ancestor = descendant->parent_edge->parent_node;
	}

	double ancestor_depth =
		((struct simple_node_pos *) ancestor->data)->depth;
	double descendant_depth =
		((struct simple_node_pos *) descendant->data)->depth;
	
	return descendant_depth - ancestor_depth;
}

void print_distance_list (struct rnode *origin,
	struct llist *selected_nodes, int orientation, int header)
{
	struct list_elem *el;
	struct rnode *node;
	double distance;
	if (VERTICAL == orientation) {
		for (el = selected_nodes->head; NULL != el; el = el->next) {
			node = el->data;
			distance = distance_to_descendant(origin, node);
			if (header) { printf("%s\t", node->label); }
			printf( "%g\n", distance);
		}
	} else if (HORIZONTAL == orientation) {
		if (header) {
			for (el=selected_nodes->head; NULL!=el; el=el->next) {
				node = el->data;
				if (el != selected_nodes->head)
					putchar ('\t');
				printf( "%s", node->label);
			}
			putchar('\n');
		}
		for (el = selected_nodes->head; NULL != el; el = el->next) {
			node = el->data;
			distance = distance_to_descendant(origin, node);
			if (el != selected_nodes->head) { putchar ('\t'); }
			printf( "%g", distance);
		}
		putchar('\n');
	} else {
		fprintf (stderr, "ERROR: unknown orientation (%d)\n", 
				orientation);
		exit(EXIT_FAILURE);
	}
}

/* NOTE: this function could be made more efficient. First, the matrix is
 * symmetric, yet each cell is computed. It is trivial to halve the job.
 * Second, for every pait of labels, the LCA is computed from scratch. But
 * there is a better way, akin to dynamic programming (heck, maybe it *is*
 * dynamic programming): one can fill a table of LCAs in the following way:
 *
 * 1. for each inner node, in parse order:
 * 	1.1. store its leftmost and rightmost descendants's labels (leftmost
 * 	descendant is leftmost descendant of its left child, rightmost
 * 	descendant is rightmost descendant of its right child)
 * 	1.2. in the matrix, find the cell (L,R), where L is the leftmost
 * 	descendants's label and R the rightmost descendant's. Set this cell's
 * 	value to the current inner node - it is the LCA of L and R.
 * 	1.3 now find all cells "below" this one, i.e. that are to the left or
 * 	below this one in the upper-right triangle (or to the right and above
 * 	in the lower-left triangle): these cells are set to the same value,
 * 	UNLESS they are already set.
 *
 * Of course this only works if the matrix is ordered in parse order too.
 * Moreover it only works when seeking the LCA of two leaves, but if one of the
 * descendants whose LCA we seek is an inner node, one can always fetch its
 * leftmost (or rightmost) descendant from the tree, and use this to get the
 * LCA.
 */

double ** fill_matrix (struct rooted_tree *tree, struct llist *selected_nodes)
{
	struct list_elem *h_el, *v_el;
	int i, j;
	int count = selected_nodes->count;
	
	double **lines = malloc(count * sizeof(double *));
	if (NULL == lines) { perror(NULL), exit (EXIT_FAILURE); }
	
	for (j = 0, v_el = selected_nodes->head; NULL != v_el;
		v_el = v_el->next, j++) {

		lines[j] = malloc(count * sizeof(double));
		if (NULL == lines[j]) { perror(NULL), exit (EXIT_FAILURE); }

		struct rnode *h_node, *v_node;
		v_node = v_el->data;

		for (i = 0, h_el = selected_nodes->head; NULL != h_el;
			h_el = h_el->next, i++) {

			h_node =  h_el->data;
			if (NULL == v_node || NULL == h_node) {
				lines[j][i] = -1;
				break;
			}
			struct rnode *lca = lca2(tree, h_node, v_node);
			lines[j][i] = 
				((struct simple_node_pos *) h_node->data)->depth
				+
				((struct simple_node_pos *) v_node->data)->depth
				-
				2 * ((struct simple_node_pos *) lca->data)->depth
				;
		}
	}	

	return lines;
}

void print_distance_matrix (struct rooted_tree *tree,
		struct llist *selected_nodes, int show_headers, int shape)
{
	double **matrix = fill_matrix(tree, selected_nodes);

	struct list_elem *h_el, *v_el;
	int i, j;
	
	for (j = 0, v_el = selected_nodes->head; NULL != v_el;
		v_el = v_el->next, j++) {

		if (show_headers) printf ("%s\t",
			((struct rnode *) v_el->data)->label);

		for (i = 0, h_el = selected_nodes->head; NULL != h_el;
			h_el = h_el->next , i++) {

			printf("%g", matrix[j][i]);
			if (h_el == selected_nodes->tail) 
				putchar('\n');
			else
				putchar('\t');
		}
	}	
}

int main(int argc, char *argv[])
{
	struct rooted_tree *tree;	
	struct parameters params;
	
	params = get_params(argc, argv);

	/* TODO: could take the switch out of the loop, since the distance type
	 * is fixed for the program's lifetime. OTOH the code is easier to
	 * understand this way, and it's unlikely the switch has a visible
	 * impact on performance. */

	while ((tree = parse_tree()) != NULL) {
		alloc_simple_node_pos(tree);
		set_node_depth_cb(tree,
				set_simple_node_pos_depth,
				get_simple_node_pos_depth);
		struct rnode *lca_node;
		struct llist *selected_nodes;

		if (ARGV_LABELS == params.selection) {
			selected_nodes = nodes_from_labels(tree,
					params.labels);
		} else {
			selected_nodes = get_selected_nodes(tree,
					params.selection);
		}
		switch (params.distance_type) {
		case FROM_ROOT:
			print_distance_list(tree->root, selected_nodes,
				params.list_orientation, params.show_header);
			break;
		case FROM_LCA:
			/* if no lbl given, use root as LCA */
			if (0 == params.labels->count)  {
				lca_node = tree->root;
			}
			else {
				lca_node = lca_from_nodes(tree, selected_nodes);
			}
			print_distance_list(lca_node, selected_nodes,
				params.list_orientation, params.show_header);
			break;
		case MATRIX:
			print_distance_matrix(tree, selected_nodes,
				params.show_header, params.matrix_shape);
			break;
		case FROM_PARENT:
			print_distance_list(NULL, selected_nodes,
				params.list_orientation, params.show_header);
			break;
		default:
			fprintf (stderr,
				"ERROR: invalid distance type '%d'.\n",
				params.distance_type);
			exit(EXIT_FAILURE);
		}

		destroy_tree(tree, FREE_NODE_DATA);
	}

	destroy_llist(params.labels);

	return 0;
}
