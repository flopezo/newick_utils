#include <stdio.h>
#include <string.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

int main(int argc, char *argv[])
{
	const char * dummy_doc_start = "<dummy>";
	const char * dummy_doc_stop = "</dummy>";

	char *svg_snippet = argv[1];
	xmlDocPtr doc;
	size_t snippet_length = strlen(svg_snippet);
	int doc_length;
	char *doc_str;

	printf ("snippet: %s\n", svg_snippet);

	/* wrap snippet into dummy document */
	int start_length = strlen(dummy_doc_start);
	int stop_length = strlen(dummy_doc_stop);
	doc_length = start_length + snippet_length + stop_length;
	doc_str = malloc(doc_length * sizeof(char));
	if (NULL == doc_str) {
		perror(NULL);
		return EXIT_FAILURE;
	}
	strcpy(doc_str, dummy_doc_start);
	strcpy(doc_str + start_length, svg_snippet);
	strcpy(doc_str + start_length + snippet_length, dummy_doc_stop);
	
	printf ("dummy doc: %s\n", doc_str);

	/* parse SVG from string */
	doc = xmlParseMemory(doc_str, doc_length);
	if (NULL == doc) {
		fprintf(stderr, "Failed to parse document\n");
		return EXIT_FAILURE;
	}
	free(doc_str);
	xmlFreeDoc(doc);

	return EXIT_SUCCESS;
}