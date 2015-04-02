
#include <stdio.h>
#include "mupdf/pdf.h"

#ifdef _MSC_VER
#define main main_utf8
#endif

static pdf_obj *
pdf_lookup_inherited_page_item(pdf_document *doc, pdf_obj *node, const char *key)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *node2 = node;
	pdf_obj *val;

	/* fz_var(node); Not required as node passed in */

	fz_try(ctx)
	{
		do
		{
			val = pdf_dict_gets(node, key);
			if (val)
				break;
			if (pdf_mark_obj(node))
				fz_throw(ctx, FZ_ERROR_GENERIC, "cycle in page tree (parents)");
			node = pdf_dict_gets(node, "Parent");
		}
		while (node);
	}
	fz_always(ctx)
	{
		do
		{
			pdf_unmark_obj(node2);
			if (node2 == node)
				break;
			node2 = pdf_dict_gets(node2, "Parent");
		}
		while (node2);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return val;
}

#define RESOLVE(obj) \
    do { \
        if (obj && pdf_is_indirect(obj))        \
        {\
            obj = pdf_resolve_indirect(obj); \
        } \
    } while (0)

void pdf_obj_to_json(pdf_document *doc, pdf_obj *obj)
{
    float f;
    int i, count;
    char *s, *s2;
    RESOLVE(obj);
    if( obj==NULL || pdf_is_null(obj)) {
        printf("null");
        // do nothing!
    } else if( pdf_is_array(obj)) {
        printf("[");
        count = pdf_array_len(obj);
        for(i=0; i<count; i++ ) {
            if( i!=0 ) {
                printf(",");
            }
            pdf_obj_to_json(doc,pdf_array_get(obj,i));
        }
        printf("]");
    }
    else if( pdf_is_number(obj)) {
        f = pdf_to_real(obj);
        printf("%g",f);
    }
    else if( pdf_is_dict(obj)) {
        printf("{");
        count = pdf_dict_len(obj);
        for(i=0; i<count; i++ ) {
            if( i!=0 ) {
                printf(",\n");
            }
            pdf_obj_to_json(doc,pdf_dict_get_key(obj,i));
            printf(": ");
            pdf_obj_to_json(doc,pdf_dict_get_val(obj,i));
        }
        printf("}");
    }
    else if( pdf_is_bool(obj)) {
        i = pdf_to_bool(obj);
        printf(i ? "true" : "false");
    }
    else if( pdf_is_string(obj)) {
        s = pdf_to_utf8(doc, obj);
        printf("\"");
        for(s2 = s; *s2; s2++) {
            if( *s2 == '\\' || *s2 == '"' ) {
                printf("\\%c",*s2);
            }
            else {
                printf("%c",*s2);
            }
        }
        printf("\"");
        fz_free(doc->ctx, s);
    }
    else if( pdf_is_name(obj)) {
        s = pdf_to_name(obj);
        printf("\"%s\"", s);
    }
    else {
        printf("\"unknown object\"");
    }
}

void printf_thing_in_page_object(pdf_document *doc, pdf_obj *pageobj, char *thing, int *need_comma)
{
    pdf_obj *obj;
    obj = pdf_lookup_inherited_page_item(doc, pageobj, thing);
    if( obj ) {
        if( *need_comma ) {
            printf(",");
        }
        printf("\"%s\": ", thing);
        pdf_obj_to_json(doc,obj);
        *need_comma = 1;
    }
}


void process_page(pdf_document *doc, int number)
{
    fz_context *ctx = doc->ctx;
    pdf_obj *pageobj, *pageref, *obj, *box;
    int need_comma = 0;

    if (doc->file_reading_linearly) {
        pageref = pdf_progressive_advance(doc, number);
        if (pageref == NULL)
            fz_throw(doc->ctx, FZ_ERROR_TRYLATER, "page %d not available yet", number);
    }
    else {
        pageref = pdf_lookup_page_obj(doc, number);
    }

    pageobj = pdf_resolve_indirect(pageref);

    /*
    obj = pdf_dict_gets(pageobj, "UserUnit");
    if (pdf_is_real(obj))
        userunit = pdf_to_real(obj);
    else
        userunit = 1;
    */

    printf("{");
    printf_thing_in_page_object(doc, pageobj, "ArtBox", &need_comma);
    printf_thing_in_page_object(doc, pageobj, "BleedBox", &need_comma);
    printf_thing_in_page_object(doc, pageobj, "CropBox", &need_comma);
    printf_thing_in_page_object(doc, pageobj, "MediaBox", &need_comma);
    printf_thing_in_page_object(doc, pageobj, "Rotate", &need_comma);
    printf_thing_in_page_object(doc, pageobj, "TrimBox", &need_comma);
    printf("}");
}


int main(int argc, char **argv)
{
    int i, count;
    pdf_obj *obj;
	char *filename = argc >= 2 ? argv[1] : "";
	pdf_document *doc;

	// Create a context to hold the exception stack and various caches.

	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);

	// Register document handlers for the default file types we support.

	fz_register_document_handlers(ctx);

	// Open the PDF, XPS or CBZ document.

	doc = pdf_open_document(ctx, filename);

    printf("{");

    printf("\"Info\": ");
    obj = pdf_trailer(doc);
    if( obj ) {
        obj = pdf_dict_gets(obj, "Info");
    }
    pdf_obj_to_json(doc,obj);
    printf(",\n");

    printf("\"PageLabels\": ");
    obj = pdf_trailer(doc);
    if( obj ) {
        obj = pdf_dict_gets(obj, "Root");
    }
    if( obj ) {
        obj = pdf_dict_gets(obj, "PageLabels");
    }
    pdf_obj_to_json(doc,obj);
    printf(",\n");

    count = pdf_count_pages(doc);
    printf("\"Pages\": [");
    for (i=0; i<doc->page_count; i++ ) {
        if(i!=0) {
            printf(",\n");
        }
        process_page(doc, i);
    }
    printf("]\n");
    printf("}\n");

	return 0;
}

#ifdef _MSC_VER
int wmain(int argc, wchar_t *wargv[])
{
	char **argv = fz_argv_from_wargv(argc, wargv);
	int ret = main(argc, argv);
	fz_free_argv(argc, argv);
	return ret;
}
#endif