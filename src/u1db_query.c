/*
 * Copyright 2012 Canonical Ltd.
 *
 * This file is part of u1db.
 *
 * u1db is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3
 * as published by the Free Software Foundation.
 *
 * u1db is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with u1db.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "u1db/u1db_internal.h"
#include <sqlite3.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <json/json.h>

#define NO_GLOB 0
#define IS_GLOB 1
#define ENDS_IN_GLOB 2

#define EXPRESSION 1
#define INTEGER 2

#define OPS (sizeof(OPERATIONS) / sizeof (struct operation))
#define MAX_INT_STR_LEN 21
#ifndef max
    #define max(a, b) (((a) > (b)) ? (a) : (b))
#endif


typedef struct string_list_item_
{
    char *data;
    struct string_list_item_ *next;
} string_list_item;

typedef struct string_list_
{
    string_list_item *head;
    string_list_item *tail;
    string_list_item *pos;
} string_list;

/*
static void
print_list(string_list *list)
{
    string_list_item *item = NULL;
    printf("[");
    for (item = list->head; item != NULL; item = item->next)
        printf("'%s',", item->data);
    printf("]\n");
}
*/

static int
init_list(string_list **list)
{
    *list = (string_list *)calloc(1, sizeof(string_list));
    if (*list == NULL)
        return U1DB_NOMEM;
    return U1DB_OK;
}

static void
destroy_list(string_list *list)
{
    string_list_item *item = NULL;
    string_list_item *previous = NULL;
    if (list == NULL)
        return;
    item = list->head;
    while (item != NULL)
    {
        previous = item;
        item = item->next;
        if (previous->data != NULL)
            free(previous->data);
        free(previous);
    }
    list->head = NULL;
    list->tail = NULL;
    free(list);
    list = NULL;
}

static int
append(string_list *list, const char *data)
{
    string_list_item *new_item = NULL;
    new_item = (string_list_item *)calloc(1, sizeof(string_list_item));
    if (new_item == NULL)
        return U1DB_NOMEM;
    new_item->data = strdup(data);
    if (new_item->data == NULL)
        return U1DB_NOMEM;
    if (list->head == NULL)
    {
        list->head = new_item;
        list->pos = new_item;
    }
    if (list->tail != NULL)
    {
        list->tail->next = new_item;
    }
    list->tail = new_item;
    return U1DB_OK;
}

static int
appendn(string_list *list, const char *data, int size)
{
    string_list_item *new_item = NULL;
    new_item = (string_list_item *)calloc(1, sizeof(string_list_item));
    if (new_item == NULL)
        return U1DB_NOMEM;
    new_item->data = strndup(data, size);
    if (new_item->data == NULL)
        return U1DB_NOMEM;
    if (list->head == NULL)
    {
        list->head = new_item;
        list->pos = new_item;
    }
    if (list->tail != NULL)
    {
        list->tail->next = new_item;
    }
    list->tail = new_item;
    return U1DB_OK;
}

typedef struct parse_tree_
{
    char *data;
    string_list *field_path;
    int arg_type;
    void *op;
    int arity;
    int number_of_children;
    struct parse_tree_ *first_child;
    struct parse_tree_ *last_child;
    struct parse_tree_ *next_sibling;
    const int *value_types;
} parse_tree;

/*
static void
print_tree(parse_tree *tree)
{
    parse_tree *sub;
    if (tree->data)
        printf("data: '%s'\n", tree->data);
    if (tree->field_path->head) {
        printf("field_path: ");
        print_list(tree->field_path);
    }
    if (tree->arg_type)
        printf("arg_type: %d\n", tree->arg_type);
    if (tree->arity)
        printf("arity: %d\n", tree->arity);
    if (tree->number_of_children)
        printf("number_of_children: %d\n", tree->number_of_children);
    printf("(");
    for (sub = tree->first_child; sub != NULL; sub = sub->next_sibling)
        print_tree(sub);
    printf(")\n");
}
*/

static int
init_parse_tree(parse_tree **result)
{
    int status = U1DB_OK;
    *result = (parse_tree *)calloc(1, sizeof(parse_tree));
    if (*result == NULL)
        return U1DB_NOMEM;
    (*result)->number_of_children = 0;
    status = init_list(&(*result)->field_path);
    return status;
}

static void
destroy_parse_tree(parse_tree *t)
{
    parse_tree *subtree = NULL;
    parse_tree *previous = NULL;

    if (t == NULL)
        return;
    if (t->field_path != NULL)
        destroy_list(t->field_path);
    if (t->data != NULL)
        free(t->data);
    subtree = t->first_child;
    while (subtree != NULL)
    {
        previous = subtree;
        subtree = subtree->next_sibling;
        destroy_parse_tree(previous);
    }
    free(t);
}

static int
append_node(parse_tree *t, parse_tree *node)
{
    if (t->first_child == NULL)
        t->first_child = node;
    if (t->last_child != NULL)
        t->last_child->next_sibling = node;
    t->last_child = node;
    (t->number_of_children)++;
    return U1DB_OK;
}

static int parse_op(string_list *tokens, char *term, parse_tree *result);
static int parse_term(string_list *tokens, parse_tree *result);
typedef int(*op_function)(parse_tree *, json_object *, string_list *);

static int op_lower(parse_tree *tree, json_object *obj, string_list *result);
static int op_number(parse_tree *tree, json_object *obj, string_list *result);
static int op_split_words(
    parse_tree *tree, json_object *obj, string_list *result);
static int op_bool(parse_tree *tree, json_object *obj, string_list *result);
static int op_combine(parse_tree *tree, json_object *obj, string_list *result);

static const int JUST_EXPRESSION[1] = {EXPRESSION};
static const int EXPRESSION_INTEGER[2] = {EXPRESSION, INTEGER};

struct operation
{
    void *function;
    char *name;
    int value_type;
    int arity;  /* negative means any N*(-arity) values are OK */
    const int *value_types;
} OPERATIONS[] = {
    { op_lower, "lower", json_type_string, 1, JUST_EXPRESSION },
    { op_number, "number", json_type_int, 2, EXPRESSION_INTEGER },
    { op_split_words, "split_words", json_type_string, 1, JUST_EXPRESSION },
    { op_bool, "bool", json_type_boolean, 1, JUST_EXPRESSION },
    { op_combine, "combine", json_type_string, -1, JUST_EXPRESSION },
};

static int
extract_value(json_object *val, int value_type, string_list *values)
{
    int status = U1DB_OK;
    int i, integer_value, boolean_value, length;
    char string_value[MAX_INT_STR_LEN];
    if (json_object_is_type(val, json_type_string) && value_type ==
            json_type_string) {
        status = append(values, json_object_get_string(val));
        goto finish;
    }
    if (json_object_is_type(val, json_type_int) && value_type ==
            json_type_int) {
        integer_value = json_object_get_int(val);
        snprintf(string_value, MAX_INT_STR_LEN, "%d", integer_value);
        status = append(values, string_value);
        goto finish;
    }
    if (json_object_is_type(val, json_type_boolean) &&
            value_type == json_type_boolean) {
        boolean_value = json_object_get_boolean(val);
        if (boolean_value) {
            status = append(values, "1");
        } else {
            status = append(values, "0");
        }
        goto finish;
    }
    if (json_object_is_type(val, json_type_array)) {
        length = json_object_array_length(val);
        for (i = 0; i < length; i++) {
            status = extract_value(
                json_object_array_get_idx(val, i), value_type, values);
            if (status != U1DB_OK)
                goto finish;
        }
    }
finish:
    return status;
}

static int
extract_field_values(json_object *obj, const string_list_item *field,
                     int value_type, string_list *values)
{
    json_object *val = NULL;
    json_object *array_item = NULL;
    int i, length;
    int status = U1DB_OK;

    if (!json_object_is_type(obj, json_type_object)) {
        goto finish;
    }
    val = json_object_object_get(obj, field->data);
    if (val == NULL)
        goto finish;
    if (field->next != NULL) {
        if (json_object_is_type(val, json_type_array)) {
            length = json_object_array_length(val);
            for (i = 0; i < length; i++) {
                array_item = json_object_array_get_idx(val, i);
                status = extract_field_values(
                    array_item, field->next, value_type, values);
                if (status != U1DB_OK)
                    goto finish;
            }
            goto finish;
        }
        if (json_object_is_type(val, json_type_object)) {
            status = extract_field_values(val, field->next, value_type, values);
            goto finish;
        }
        goto finish;
    }
    status = extract_value(val, value_type, values);
finish:
    return status;
}


static int
split(string_list *result, char *string, char splitter)
{
    int status = U1DB_OK;
    char *result_ptr = NULL, *split_point = NULL;
    result_ptr = string;
    while (result_ptr != NULL) {
        split_point = strchr(result_ptr, splitter);
        if (split_point != NULL) {
            *split_point = '\0';
            split_point++;
        }
        status = append(result, result_ptr);
        if (status != U1DB_OK)
            return status;
        result_ptr = split_point;
    }
    return status;
}

static int
list_index(string_list *list, char *data)
{
    int i = 0;
    string_list_item *item = NULL;
    for (item = list->head; item != NULL; item = item->next)
    {
        if (strcmp(item->data, data) == 0)
        {
            return i;
        }
        i++;
    }
    return -1;
}

static int
get_values(parse_tree *tree, json_object *obj, string_list *values)
{
    int status = U1DB_OK;
    if (tree->op) {
        status = ((op_function)tree->op)(tree, obj, values);
    } else {
        status = extract_field_values(
            obj, tree->field_path->head, json_type_string, values);
    }
    return status;
}

static int
op_lower(parse_tree *tree, json_object *obj, string_list *result)
{
    string_list *values = NULL;
    string_list_item *item = NULL;
    char *new_value = NULL, *value = NULL;
    int i;
    int status = U1DB_OK;

    status = init_list(&values);
    if (status != U1DB_OK)
        return status;
    status = get_values(tree->first_child, obj, values);
    if (status != U1DB_OK)
        goto finish;
    for (item = values->head; item != NULL; item = item->next)
    {
        value = item->data;
        i = 0;
        new_value = (char *)calloc(strlen(value) + 1, 1);
        if (new_value != NULL)
        {
            while (value[i] != '\0')
            {
                // TODO: unicode hahaha
                new_value[i] = tolower(value[i]);
                i++;
            }
            new_value[i] = '\0';
        }
        status = append(result, new_value);
    }
finish:
    if (new_value != NULL)
        free(new_value);
    if (values != NULL)
        destroy_list(values);
    return status;
}

static int
op_number(parse_tree *tree, json_object *obj, string_list *result)
{
    string_list *values = NULL;
    string_list_item *item = NULL;
    char *p = NULL, *new_value = NULL, *value = NULL, *number = NULL;
    parse_tree *node = NULL;
    int count, zeroes, value_size, isnumber;
    int status = U1DB_OK;

    node = tree->first_child;
    status = init_list(&values);
    if (status != U1DB_OK)
        return status;
    status = extract_field_values(
        obj, node->field_path->head, json_type_int, values);
    if (status != U1DB_OK)
        goto finish;
    node = node->next_sibling;
    number = node->data;
    for (p = number; *p; p++) {
        if (isdigit(*p) == 0) {
            status = U1DB_INVALID_VALUE_FOR_INDEX;
            goto finish;
        }
    }
    zeroes = atoi(number);
    for (item = values->head; item != NULL; item = item->next)
    {
        value = item->data;
        isnumber = 1;
        for (p = value; *p; p++) {
            if (isdigit(*p) == 0) {
                isnumber = 0;
                break;
            }
        }
        if (isnumber == 0) {
            continue;
        }
        value_size = max(strlen(value), zeroes) + 1;
        new_value = (char *)calloc(value_size, 1);
        if (new_value == NULL)
        {
            status = U1DB_NOMEM;
            goto finish;
        }
        count = snprintf(new_value, value_size, "%0*d", zeroes, atoi(value));
        if (count != (value_size - 1)) {
            // Most likely encoding issues.
            status = U1DB_INVALID_PARAMETER;
            free(new_value);
            goto finish;
        }
        if ((status = append(result, new_value)) != U1DB_OK)
        {
            free(new_value);
            goto finish;
        }
        free(new_value);
    }
finish:
    if (values != NULL)
        destroy_list(values);
    return status;
}

static int
op_combine(parse_tree *tree, json_object *obj, string_list *result)
{
    parse_tree *node = NULL;
    int status = U1DB_OK;

    node = tree->first_child;
    for (node = tree->first_child; node != NULL; node = node->next_sibling) {
        status = get_values(node, obj, result);
        if (status != U1DB_OK)
            return status;
    }
    return status;
}

static int
op_split_words(parse_tree *tree, json_object *obj, string_list *result)
{
    string_list_item *item = NULL;
    string_list *values = NULL;
    char *intermediate = NULL, *intermediate_ptr = NULL;
    char *space_chr = NULL;
    int status = U1DB_OK;

    status = init_list(&values);
    if (status != U1DB_OK)
        return status;
    status = get_values(tree->first_child, obj, values);
    if (status != U1DB_OK)
        goto finish;
    for (item = values->head; item != NULL; item = item->next)
    {
        intermediate = strdup(item->data);
        if (intermediate == NULL)
            return U1DB_NOMEM;
        intermediate_ptr = intermediate;
        while (intermediate_ptr != NULL) {
            space_chr = strchr(intermediate_ptr, ' ');
            if (space_chr != NULL) {
                *space_chr = '\0';
                space_chr++;
            }
            if (list_index(result, intermediate_ptr) == -1)
            {
                if ((status = append(result, intermediate_ptr)) != U1DB_OK)
                {
                    return status;
                }
            }
            intermediate_ptr = space_chr;
        }
        free(intermediate);
    }
finish:
    if (values != NULL)
        destroy_list(values);
    return status;
}

static int
op_bool(parse_tree *tree, json_object *obj, string_list *result)
{
    string_list_item *item = NULL;
    string_list *values = NULL;
    int status = U1DB_OK;

    status = init_list(&values);
    if (status != U1DB_OK)
        return status;
    status = get_values(tree->first_child, obj, values);
    if (status != U1DB_OK)
        goto finish;
    //just return all the strings which have been filtered and converted from
    //booleans by extract_field_values.

    status = extract_field_values(
        obj, tree->first_child->field_path->head, json_type_boolean, values);
    if (status != U1DB_OK)
        goto finish;
    for (item = values->head; item != NULL; item = item->next)
    {
        if ((status = append(result, item->data)) != U1DB_OK)
        {
            return status;
        }
    }
finish:
    if (values != NULL)
        destroy_list(values);
    return status;
}


static int
lookup_index_fields(u1database *db, u1query *query)
{
    int status, offset;
    char *field = NULL;
    sqlite3_stmt *statement = NULL;

    status = sqlite3_prepare_v2(db->sql_handle,
        "SELECT offset, field FROM index_definitions"
        " WHERE name = ?"
        " ORDER BY offset DESC",
        -1, &statement, NULL);
    if (status != SQLITE_OK) { goto finish; }
    status = sqlite3_bind_text(statement, 1, query->index_name, -1,
                               SQLITE_TRANSIENT);
    if (status != SQLITE_OK) { goto finish; }
    status = sqlite3_step(statement);
    if (status == SQLITE_DONE) {
        status = U1DB_INDEX_DOES_NOT_EXIST;
    }
    while (status == SQLITE_ROW) {
        offset = sqlite3_column_int(statement, 0);
        field = (char*)sqlite3_column_text(statement, 1);
        if (query->fields == NULL) {
            query->num_fields = offset + 1;
            query->fields = (char**)calloc(query->num_fields, sizeof(char*));
            if (query->fields == NULL) {
                status = U1DB_NOMEM;
                goto finish;
            }
        }
        if (offset >= query->num_fields) {
            status = U1DB_INVALID_PARAMETER; // TODO: better error code
            goto finish;
        }
        query->fields[offset] = strdup(field);
        if (query->fields[offset] == NULL) {
            status = U1DB_NOMEM;
            goto finish;
        }
        status = sqlite3_step(statement);
    }
    if (status == SQLITE_DONE) {
        status = U1DB_OK;
    }
finish:
    sqlite3_finalize(statement);
    return status;
}


int
u1db_query_init(u1database *db, const char *index_name, u1query **query)
{
    int status;
    if (db == NULL || index_name == NULL || query == NULL) {
        return U1DB_INVALID_PARAMETER;
    }
    *query = (u1query*)calloc(1, sizeof(u1query));
    if (*query == NULL) {
        return U1DB_NOMEM;
    }
    // Should we be copying this instead?
    (*query)->index_name = index_name;
    status = lookup_index_fields(db, *query);
    if (status != U1DB_OK) {
        u1db_free_query(query);
    }
    return status;
}


void
u1db_free_query(u1query **query)
{
    int i;
    u1query *q = NULL;
    if (query == NULL || *query == NULL) {
        return;
    }
    q = *query;
    if (q->fields != NULL) {
        for (i = 0; i < q->num_fields; ++i) {
            if (q->fields[i] != NULL) {
                free(q->fields[i]);
                q->fields[i] = NULL;
            }
        }
        free(q->fields);
        q->fields = NULL;
    }
    free(*query);
    *query = NULL;
}


int
u1db_simple_lookup1(u1database *db, const char *index_name,
                    const char *val0, void *context, u1db_doc_callback cb)
{
    int status = U1DB_OK;
    u1query *query = NULL;

    if (db == NULL || index_name == NULL || val0 == NULL || cb == NULL) {
        return U1DB_INVALID_PARAMETER;
    }
    status = u1db_query_init(db, index_name, &query);
    if (status != U1DB_OK) { goto finish; }
    status = u1db_get_from_index(db, query, context, cb, 1, val0);
finish:
    u1db_free_query(&query);
    return status;
}

int
u1db_get_from_index_list(u1database *db, u1query *query, void *context,
                         u1db_doc_callback cb, int n_values,
                         const char **values)
{
    int status = U1DB_OK;
    sqlite3_stmt *statement = NULL;
    char *query_str = NULL;
    int i, bind_arg;
    int wildcard[20] = {0};

    if (db == NULL || query == NULL || cb == NULL || n_values < 0)
    {
        return U1DB_INVALID_PARAMETER;
    }
    if (query->num_fields != n_values) {
        return U1DB_INVALID_VALUE_FOR_INDEX;
    }
    if (n_values > 20) {
        return U1DB_NOT_IMPLEMENTED;
    }
    status = u1db__format_query(
        query->num_fields, values, &query_str, wildcard);
    if (status != U1DB_OK) { goto finish; }
    status = sqlite3_prepare_v2(db->sql_handle, query_str, -1,
                                &statement, NULL);
    if (status != SQLITE_OK) { goto finish; }
    // Bind all of the 'field_name' parameters. sqlite_bind starts at 1
    bind_arg = 1;
    for (i = 0; i < query->num_fields; ++i) {
        status = sqlite3_bind_text(statement, bind_arg, query->fields[i], -1,
                                   SQLITE_TRANSIENT);
        bind_arg++;
        if (status != SQLITE_OK) { goto finish; }
        if (wildcard[i] == NO_GLOB) {
            // Not a wildcard, so add the argument
            status = sqlite3_bind_text(statement, bind_arg, values[i], -1,
                                       SQLITE_TRANSIENT);
            bind_arg++;
        } else if (wildcard[i] == ENDS_IN_GLOB) {
            status = sqlite3_bind_text(statement, bind_arg, values[i], -1,
                                       SQLITE_TRANSIENT);
            bind_arg++;
        }
        if (status != SQLITE_OK) { goto finish; }
    }
    status = sqlite3_step(statement);
    while (status == SQLITE_ROW) {
        status = u1db__process_doc(db, statement, NULL, 1, 0, context, cb);
        if (status != U1DB_OK) { goto finish; }
        status = sqlite3_step(statement);
    }
    if (status == SQLITE_DONE) {
        status = U1DB_OK;
    }
finish:
    sqlite3_finalize(statement);
    if (query_str != NULL) {
        free(query_str);
    }
    return status;
}

int
u1db_get_range_from_index(u1database *db, u1query *query,
                          void *context, u1db_doc_callback cb,
                          int n_values, const char **start_values,
                          const char **end_values)
{
    int i, bind_arg, status = U1DB_OK;
    char *query_str = NULL;
    sqlite3_stmt *statement = NULL;
    char *stripped = NULL;
    int start_wildcard[20] = {0};
    int end_wildcard[20] = {0};

    if (db == NULL || query == NULL || cb == NULL || n_values < 0) {
        return U1DB_INVALID_PARAMETER;
    }
    if (n_values != query->num_fields) {
        return U1DB_INVALID_VALUE_FOR_INDEX;
    }
    status = u1db__format_range_query(
        query->num_fields, start_values, end_values, &query_str,
        start_wildcard, end_wildcard);
    if (status != U1DB_OK) { goto finish; }
    status = sqlite3_prepare_v2(db->sql_handle, query_str, -1,
                                &statement, NULL);
    if (status != SQLITE_OK) { goto finish; }
    // Bind all of the 'field_name' parameters. sqlite_bind starts at 1
    bind_arg = 1;
    for (i = 0; i < query->num_fields; ++i) {
        status = sqlite3_bind_text(
            statement, bind_arg, query->fields[i], -1, SQLITE_TRANSIENT);
        if (status != SQLITE_OK) { goto finish; }
        bind_arg++;
        if (start_values != NULL) {
            if (start_wildcard[i] == NO_GLOB) {
                status = sqlite3_bind_text(
                    statement, bind_arg, start_values[i], -1,
                    SQLITE_TRANSIENT);
                bind_arg++;
            } else if (start_wildcard[i] == ENDS_IN_GLOB) {
                if (stripped != NULL)
                    free(stripped);
                stripped = strdup(start_values[i]);
                if (stripped == NULL) {
                    status = U1DB_NOMEM;
                    goto finish;
                }
                stripped[strlen(stripped) - 1] = '\0';
                status = sqlite3_bind_text(
                    statement, bind_arg, stripped, -1, SQLITE_TRANSIENT);
                bind_arg++;
            }
           if (status != SQLITE_OK) { goto finish; }
        }
        if (end_values != NULL) {
            if (end_wildcard[i] == NO_GLOB) {
                status = sqlite3_bind_text(
                    statement, bind_arg, end_values[i], -1,
                    SQLITE_TRANSIENT);
                bind_arg++;
            } else if (end_wildcard[i] == ENDS_IN_GLOB) {
                if (stripped != NULL)
                    free(stripped);
                stripped = strdup(end_values[i]);
                if (stripped == NULL) {
                    status = U1DB_NOMEM;
                    goto finish;
                }
                stripped[strlen(stripped) - 1] = '\0';
                status = sqlite3_bind_text(
                    statement, bind_arg, stripped, -1, SQLITE_TRANSIENT);
                bind_arg++;
                status = sqlite3_bind_text(
                    statement, bind_arg, end_values[i], -1, SQLITE_TRANSIENT);
                bind_arg++;
            }
            if (status != SQLITE_OK) { goto finish; }
        }
    }
    status = sqlite3_step(statement);
    while (status == SQLITE_ROW) {
        status = u1db__process_doc(db, statement, NULL, 1, 0, context, cb);
        if (status != U1DB_OK) { goto finish; }
        status = sqlite3_step(statement);
    }
    if (status == SQLITE_DONE) {
        status = U1DB_OK;
    }
finish:
    sqlite3_finalize(statement);
    if (query_str != NULL) {
        free(query_str);
    }
    if (stripped != NULL)
        free(stripped);
    return status;
}


int
u1db_get_from_index(u1database *db, u1query *query, void *context,
                    u1db_doc_callback cb, int n_values, ...)
{
    int i, status = U1DB_OK;
    va_list argp;
    const char **values = NULL;

    values = (const char **)calloc(n_values, sizeof(char*));
    if (values == NULL) {
        status = U1DB_NOMEM;
        goto finish;
    }
    va_start(argp, n_values);
    for (i = 0; i < n_values; ++i) {
        values[i] = va_arg(argp, char *);
    }
    status = u1db_get_from_index_list(
        db, query, context, cb, n_values, values);
finish:
    if (values != NULL)
        free(values);
    va_end(argp);
    return status;
}


int
u1db_get_index_keys(u1database *db, char *index_name,
                    void *context, u1db_key_callback cb)
{
    int status = U1DB_OK;
    int num_fields = 0;
    int bind_arg, i;
    const char **key = NULL;
    string_list *field_names = NULL;
    string_list_item *field_name = NULL;
    char *query_str = NULL;
    sqlite3_stmt *statement;

    status = init_list(&field_names);
    if (status != U1DB_OK)
        goto finish;
    status = sqlite3_prepare_v2(
        db->sql_handle,
        "SELECT field FROM index_definitions WHERE name = ? ORDER BY "
        "offset;",
        -1, &statement, NULL);
    if (status != SQLITE_OK) {
        goto finish;
    }
    status = sqlite3_bind_text(
        statement, 1, index_name, -1, SQLITE_TRANSIENT);
    if (status != SQLITE_OK) {
        goto finish;
    }
    status = sqlite3_step(statement);
    if (status == SQLITE_DONE) {
        status = U1DB_INDEX_DOES_NOT_EXIST;
        goto finish;
    }
    while (status == SQLITE_ROW) {
        num_fields++;
        status = append(field_names, (char*)sqlite3_column_text(statement, 0));
        if (status != U1DB_OK)
            goto finish;
        status = sqlite3_step(statement);
    }
    if (status != SQLITE_DONE) {
        goto finish;
    }
    sqlite3_finalize(statement);
    status = u1db__format_index_keys_query(num_fields, &query_str);

    if (status != U1DB_OK) {
        goto finish;
    }
    status = sqlite3_prepare_v2(
        db->sql_handle, query_str, -1, &statement, NULL);
    if (status != SQLITE_OK) {
        goto finish;
    }
    bind_arg = 1;
    for (field_name = field_names->head; field_name != NULL; field_name =
             field_name->next) {
        status = sqlite3_bind_text(
            statement, bind_arg++, field_name->data, -1, SQLITE_TRANSIENT);
        if (status != SQLITE_OK)
            goto finish;
    }
    status = sqlite3_step(statement);
    key = (const char**)calloc(num_fields, sizeof(char*));
    if (key == NULL) {
        status = U1DB_NOMEM;
        goto finish;
    }
    while (status == SQLITE_ROW) {
        for (i = 0; i < num_fields; ++i) {
            key[i]  = (const char*)sqlite3_column_text(statement, i);
        }
        if ((status = cb(context, num_fields, key)) != U1DB_OK) {
            goto finish;
        }
        status = sqlite3_step(statement);
    }
    if (status == SQLITE_DONE) {
        status = U1DB_OK;
    }
finish:
    if (key != NULL) {
        free(key);
    }
    if (query_str != NULL) {
        free(query_str);
    }
    destroy_list(field_names);
    sqlite3_finalize(statement);
    return status;
}

static void
add_to_buf(char **buf, int *buf_size, const char *fmt, ...)
{
    int count;
    va_list argp;
    va_start(argp, fmt);
    count = vsnprintf(*buf, *buf_size, fmt, argp);
    va_end(argp);
    *buf += count;
    *buf_size -= count;
}


int
u1db__format_query(int n_fields, const char **values, char **buf,
                   int *wildcard)
{
    int status = U1DB_OK;
    int buf_size, i;
    char *cur = NULL;
    const char *val = NULL;
    int have_wildcard = 0;

    if (n_fields < 1) {
        return U1DB_INVALID_PARAMETER;
    }
    // 81 for 1 doc, 166 for 2, 251 for 3
    buf_size = (1 + n_fields) * 100;
    // The first field is treated specially
    cur = (char*)calloc(buf_size, 1);
    if (cur == NULL) {
        return U1DB_NOMEM;
    }
    *buf = cur;
    add_to_buf(
        &cur, &buf_size,
        "SELECT doc.doc_id, doc.doc_rev, doc.content FROM document_fields d0");
    for (i = 1; i < n_fields; ++i) {
        add_to_buf(&cur, &buf_size, ", document_fields d%d", i);
    }
    add_to_buf(
        &cur, &buf_size, " INNER JOIN document doc ON doc.doc_id = d0.doc_id");
    add_to_buf(&cur, &buf_size, " WHERE d0.field_name = ?");
    for (i = 0; i < n_fields; ++i) {
        if (i != 0) {
            add_to_buf(&cur, &buf_size,
                " AND d0.doc_id = d%d.doc_id"
                " AND d%d.field_name = ?",
                i, i);
        }
        val = values[i];
        if (val == NULL) {
            status = U1DB_INVALID_VALUE_FOR_INDEX;
            goto finish;
        }
        if (val[0] == '*') {
            wildcard[i] = IS_GLOB;
            have_wildcard = 1;
            add_to_buf(&cur, &buf_size, " AND d%d.value NOT NULL", i);
        } else if (val[0] != '\0' && val[strlen(val)-1] == '*') {
            // glob
            wildcard[i] = ENDS_IN_GLOB;
            if (have_wildcard) {
                //globs not allowed after another wildcard
                status = U1DB_INVALID_GLOBBING;
                goto finish;
            }
            have_wildcard = 1;
            add_to_buf(&cur, &buf_size, " AND d%d.value GLOB ?", i);
        } else {
            wildcard[i] = NO_GLOB;
            if (have_wildcard) {
                // Can't have a non-wildcard after a wildcard
                status = U1DB_INVALID_GLOBBING;
                goto finish;
            }
            add_to_buf(&cur, &buf_size, " AND d%d.value = ?", i);
        }
    }
    add_to_buf(&cur, &buf_size, " ORDER BY ");
    for (i = 0; i < n_fields; ++i) {
        if (i != 0) {
            add_to_buf(&cur, &buf_size, ", ");
        }
        add_to_buf(&cur, &buf_size, "d%d.value", i);
    }
finish:
    if (status != U1DB_OK && *buf != NULL) {
        free(*buf);
        *buf = NULL;
    }
    return status;
}

int
u1db__format_range_query(int n_fields, const char **start_values,
                         const char **end_values, char **buf,
                         int *start_wildcard, int *end_wildcard)
{
    int status = U1DB_OK;
    int buf_size, i;
    char *cur = NULL;
    const char *val = NULL;
    int have_start_wildcard = 0;
    int have_end_wildcard = 0;

    if (n_fields < 1) {
        return U1DB_INVALID_PARAMETER;
    }
    // Had to up the buf_size to prevent segfaults. Probably add_to_buf needs
    // to be smarter about increasing storage when needed (i.e. do it *before*
    // writing to the buffer, rather than after.)
    buf_size = 100 + (1 + n_fields) * 100;
    // The first field is treated specially
    cur = (char*)calloc(buf_size, 1);
    if (cur == NULL) {
        return U1DB_NOMEM;
    }
    *buf = cur;
    add_to_buf(
        &cur, &buf_size,
        "SELECT doc.doc_id, doc.doc_rev, doc.content FROM document_fields d0");
    for (i = 1; i < n_fields; ++i) {
        add_to_buf(&cur, &buf_size, ", document_fields d%d", i);
    }
    add_to_buf(
        &cur, &buf_size, " INNER JOIN document doc ON doc.doc_id = d0.doc_id");
    add_to_buf(&cur, &buf_size, " WHERE d0.field_name = ?");
    for (i = 0; i < n_fields; ++i) {
        if (i != 0) {
            add_to_buf(&cur, &buf_size,
                " AND d0.doc_id = d%d.doc_id"
                " AND d%d.field_name = ?",
                i, i);
        }
        if (start_values != NULL) {
            val = start_values[i];
            if (val == NULL) {
                status = U1DB_INVALID_VALUE_FOR_INDEX;
                goto finish;
            }
            if (val[0] == '*') {
                start_wildcard[i] = IS_GLOB;
                have_start_wildcard= 1;
                add_to_buf(&cur, &buf_size, " and d%d.value not null", i);
            } else if (val[0] != '\0' && val[strlen(val)-1] == '*') {
                // glob
                start_wildcard[i] = ENDS_IN_GLOB;
                if (have_start_wildcard) {
                    //globs not allowed after another wildcard
                    status = U1DB_INVALID_GLOBBING;
                    goto finish;
                }
                have_start_wildcard = 1;
                add_to_buf(&cur, &buf_size, " and d%d.value >= ?", i);
            } else {
                start_wildcard[i] = NO_GLOB;
                if (have_start_wildcard) {
                    // can't have a non-wildcard after a wildcard
                    status = U1DB_INVALID_GLOBBING;
                    goto finish;
                }
                add_to_buf(&cur, &buf_size, " and d%d.value >= ?", i);
            }
        }
        if (end_values != NULL) {
            val = end_values[i];
            if (val == NULL) {
                status = U1DB_INVALID_VALUE_FOR_INDEX;
                goto finish;
            }
            if (val[0] == '*') {
                end_wildcard[i] = IS_GLOB;
                have_end_wildcard = 1;
                add_to_buf(&cur, &buf_size, " AND d%d.value NOT NULL", i);
            } else if (val[0] != '\0' && val[strlen(val)-1] == '*') {
                // glob
                end_wildcard[i] = ENDS_IN_GLOB;
                if (have_end_wildcard) {
                    //globs not allowed after another wildcard
                    status = U1DB_INVALID_GLOBBING;
                    goto finish;
                }
                have_end_wildcard = 1;
                add_to_buf(
                    &cur, &buf_size,
                    " AND (d%d.value < ? OR d%d.value GLOB ?)", i, i);
            } else {
                end_wildcard[i] = NO_GLOB;
                if (have_end_wildcard) {
                    // Can't have a non-wildcard after a wildcard
                    status = U1DB_INVALID_GLOBBING;
                    goto finish;
                }
                add_to_buf(&cur, &buf_size, " AND d%d.value <= ?", i);
            }
        }

    }
    add_to_buf(&cur, &buf_size, " ORDER BY ");
    for (i = 0; i < n_fields; ++i) {
        if (i != 0) {
            add_to_buf(&cur, &buf_size, ", ");
        }
        add_to_buf(&cur, &buf_size, "d%d.value", i);
    }
finish:
    if (status != U1DB_OK && *buf != NULL) {
        free(*buf);
        *buf = NULL;
    }
    return status;
}

int
u1db__format_index_keys_query(int n_fields, char **buf)
{
    int status = U1DB_OK;
    int buf_size, i;
    char *cur = NULL;

    if (n_fields < 1) {
        return U1DB_INVALID_PARAMETER;
    }
    // 81 for 1 doc, 166 for 2, 251 for 3
    buf_size = (1 + n_fields) * 100;
    // The first field is treated specially
    cur = (char*)calloc(buf_size, 1);
    if (cur == NULL) {
        return U1DB_NOMEM;
    }
    *buf = cur;
    add_to_buf(&cur, &buf_size, "SELECT ");
    for (i = 0; i < n_fields; ++i) {
        if (i != 0) {
            add_to_buf(&cur, &buf_size, ", ");
        }
        add_to_buf(&cur, &buf_size, " d%d.value", i);
    }
    add_to_buf(&cur, &buf_size, " FROM ");
    for (i = 0; i < n_fields; ++i) {
        if (i != 0) {
            add_to_buf(&cur, &buf_size, ", ");
        }
        add_to_buf(&cur, &buf_size, "document_fields d%d", i);
    }
    add_to_buf(&cur, &buf_size, " WHERE d0.field_name = ?");
    for (i = 0; i < n_fields; ++i) {
        if (i != 0) {
            add_to_buf(&cur, &buf_size,
                " AND d0.doc_id = d%d.doc_id"
                " AND d%d.field_name = ?",
                i, i);
        }
        add_to_buf(&cur, &buf_size, " AND d%d.value NOT NULL", i);
    }
    add_to_buf(&cur, &buf_size, " GROUP BY ");
    for (i = 0; i < n_fields; ++i) {
        if (i != 0) {
            add_to_buf(&cur, &buf_size, ", ");
        }
        add_to_buf(&cur, &buf_size, "d%d.value", i);
    }
    if (status != U1DB_OK && *buf != NULL) {
        free(*buf);
        *buf = NULL;
    }
    return status;
}

struct sqlcb_to_field_cb {
    void *user_context;
    int (*user_cb)(void *, const char*, parse_tree *tree);
};

static int
check_fieldname(const char *fieldname)
{
    if (fieldname[strlen(fieldname) - 1] == '.')
        return U1DB_INVALID_FIELD_SPECIFIER;
    return U1DB_OK;
}

static char *
get_token(string_list *tokens)
{
    char *token = NULL;
    if (tokens->pos != NULL) {
        token = tokens->pos->data;
        tokens->pos = tokens->pos->next;
    }
    return token;
}

static char *
peek_token(string_list *tokens)
{
    if (tokens->pos != NULL) {
        return tokens->pos->data;
    }
    return NULL;
}

static int
to_getter(parse_tree *node)
{
    int status = U1DB_OK;
    if (node->op == NULL) {
        status = check_fieldname(node->data);
        if (status != U1DB_OK)
            return status;
        status = split(node->field_path, node->data, '.');
    }
    return status;
}

static int
parse_op(string_list *tokens, char *term, parse_tree *result)
{
    char *sep = NULL;
    parse_tree *node = NULL;
    int status = U1DB_OK;
    int i, cyclic_arity;

    sep = get_token(tokens);
    if (sep == NULL || strcmp(sep, "(") != 0) {
        return U1DB_INVALID_TRANSFORMATION_FUNCTION;
    }
    for (i = 0; i < OPS; i++) {
        if (strcmp(OPERATIONS[i].name, term) == 0)
        {
            result->data = strdup(term);
            result->op = OPERATIONS[i].function;
            result->arity = OPERATIONS[i].arity;
            result->value_types = OPERATIONS[i].value_types;
            break;
        }
    }
    if (result->op == NULL) {
        status = U1DB_UNKNOWN_OPERATION;
        goto finish;
    }
    while (1) {
        init_parse_tree(&node);
        status = parse_term(tokens, node);
        if (status != U1DB_OK)
            if (node != NULL) {
                destroy_parse_tree(node);
                goto finish;
            }
        status = append_node(result, node);
        if (status != U1DB_OK) {
            destroy_parse_tree(node);
            goto finish;
        }
        sep = get_token(tokens);
        if (sep == NULL) {
            status = U1DB_INVALID_TRANSFORMATION_FUNCTION;
            goto finish;
        }
        if (strcmp(sep, ")") == 0) {
            break;
        }
        if (strcmp(sep, ",") != 0) {
            status = U1DB_INVALID_TRANSFORMATION_FUNCTION;
            goto finish;
        }
    }
    i = 0;
    cyclic_arity = abs(result->arity);
    for (node = result->first_child; node != NULL; node = node->next_sibling) {
        node->arg_type = result->value_types[i % cyclic_arity];
        if (node->arg_type == EXPRESSION) {
            status = to_getter(node);
            if (status != U1DB_OK)
                goto finish;
        }
        i++;
    }
finish:
    return status;
}

static int
parse_term(string_list *tokens, parse_tree *result)
{
    char *term = NULL;
    char *next = NULL;
    int status = U1DB_OK;

    term = get_token(tokens);
    if (term == NULL) {
        status = U1DB_INVALID_TRANSFORMATION_FUNCTION;
        goto finish;
    }
    if (strcmp(term, ",") == 0 || strcmp(term, "(") == 0 ||
            strcmp(term, ")") == 0) {
        status = U1DB_INVALID_TRANSFORMATION_FUNCTION;
        goto finish;
    }
    next = peek_token(tokens);
    if (next != NULL && strcmp(next, "(") == 0) {
        status = parse_op(tokens, term, result);
        goto finish;
    } else {
        result->data = strdup(term);
    }
finish:
    return status;
}

static int
make_tokens(const char *expression, string_list *tokens)
{
    int status = U1DB_OK;
    int size;
    int idx, start, end;
    char c;
    idx = 0;
    while (idx < strlen(expression) && expression[idx] == ' ')
        idx++;
    start = idx;
    end = strlen(expression) - 1;
    while (end && expression[end] == ' ')
        end--;
    end++;
    while (idx < end) {
        c = expression[idx];
        if (c == '(' || c == ',' || c == ')') {
            if (idx == start) {
                status = appendn(tokens, expression + idx, 1);
                if (status != U1DB_OK)
                    return status;
                idx++;
                start = idx;
            } else {
                while (start < idx && expression[start] == ' ')
                    start++;
                size = idx - start;
                while (size && expression[start + size - 1] == ' ')
                    size--;
                if (size > 0) {
                    status = appendn(tokens, expression + start, size);
                    if (status != U1DB_OK)
                        return status;
                }
                start = idx;
            }
        } else {
            idx++;
        }
    }
    size = end - start;
    if (size > 0) {
        status = appendn(tokens, expression + start, size);
    }
    return status;
}

static int
parse(const char *expression, parse_tree *result)
{
    int status = U1DB_OK;
    int open_parens = 0;
    string_list *tokens = NULL;

    status = init_list(&tokens);
    if (status != U1DB_OK)
        goto finish;
    status = make_tokens(expression, tokens);
    if (status != U1DB_OK) {
        goto finish;
    }
    status = parse_term(tokens, result);
    if (status != U1DB_OK) {
        goto finish;
    }
    if (peek_token(tokens) != NULL) {
        status = U1DB_INVALID_TRANSFORMATION_FUNCTION;
        goto finish;
    }
    status = to_getter(result);
    if (status != U1DB_OK) {
        goto finish;
    }
    if (status != U1DB_OK)
        goto finish;
    if (open_parens != 0)
        status = U1DB_INVALID_TRANSFORMATION_FUNCTION;
finish:
    if (tokens != NULL)
        destroy_list(tokens);
    return status;
}

// Thunk from the SQL interface, to a nicer single value interface
static int
sqlite_cb_to_field_cb(void *context, int n_cols, char **cols, char **rows)
{
    struct sqlcb_to_field_cb *ctx = NULL;
    parse_tree *tree = NULL;
    char *expression = NULL;
    int status = U1DB_OK;
    ctx = (struct sqlcb_to_field_cb*)context;
    if (n_cols != 1) {
        return 1; // Error
    }
    expression = cols[0];
    status = init_parse_tree(&tree);
    if (status != U1DB_OK)
        goto finish;
    status = parse(expression, tree);
    if (status != U1DB_OK)
        goto finish;
    status = ctx->user_cb(ctx->user_context, expression, tree);
finish:
    destroy_parse_tree(tree);
    return status;
}


// Iterate over the fields that are indexed, and invoke cb for each one
static int
iter_field_definitions(u1database *db, void *context,
                      int (*cb)(void *context, const char *expression,
                                parse_tree *tree))
{
    int status;
    struct sqlcb_to_field_cb ctx;

    ctx.user_context = context;
    ctx.user_cb = cb;
    status = sqlite3_exec(db->sql_handle,
        "SELECT field FROM index_definitions",
        sqlite_cb_to_field_cb, &ctx, NULL);
    return status;
}

struct evaluate_index_context {
    u1database *db;
    const char *doc_id;
    json_object *obj;
    const char *content;
};

static int
add_to_document_fields(u1database *db, const char *doc_id,
                       const char *expression, const char *val)
{
    int status;
    sqlite3_stmt *statement = NULL;

    status = sqlite3_prepare_v2(db->sql_handle,
        "INSERT INTO document_fields (doc_id, field_name, value)"
        " VALUES (?, ?, ?)", -1,
        &statement, NULL);
    if (status != SQLITE_OK) {
        return status;
    }
    status = sqlite3_bind_text(statement, 1, doc_id, -1, SQLITE_TRANSIENT);
    if (status != SQLITE_OK) { goto finish; }
    status = sqlite3_bind_text(statement, 2, expression, -1, SQLITE_TRANSIENT);
    if (status != SQLITE_OK) { goto finish; }
    status = sqlite3_bind_text(statement, 3, val, -1, SQLITE_TRANSIENT);
    if (status != SQLITE_OK) { goto finish; }
    status = sqlite3_step(statement);
    if (status == SQLITE_DONE) {
        status = SQLITE_OK;
    }
finish:
    sqlite3_finalize(statement);
    return status;
}

static int
evaluate_index_and_insert_into_db(void *context, const char *expression,
                                  parse_tree *tree)
{
    struct evaluate_index_context *ctx;
    string_list *values = NULL;
    string_list_item *item = NULL;
    int status = U1DB_OK;

    ctx = (struct evaluate_index_context *)context;
    if (ctx->obj == NULL || !json_object_is_type(ctx->obj, json_type_object)) {
        return U1DB_INVALID_JSON;
    }
    status = init_list(&values);
    if (status != U1DB_OK)
        goto finish;
    status = get_values(tree, ctx->obj, values);
    if (status != U1DB_OK)
        goto finish;
    for (item = values->head; item != NULL; item = item->next)
    {
        if ((status = add_to_document_fields(ctx->db, ctx->doc_id, expression,
                        item->data)) != U1DB_OK)
            goto finish;
    }
finish:
    if (values != NULL) {
        destroy_list(values);
        values = NULL;
    }
    return status;
}

// Is this expression field already in the indexed list?
// We make an assumption that the number of new expressions is always small
// relative to what is already indexed (which should be reasonably accurate).
static int
is_present(u1database *db, const char *expression, int *present)
{
    sqlite3_stmt *statement = NULL;
    int status;

    status = sqlite3_prepare_v2(db->sql_handle,
        "SELECT 1 FROM index_definitions WHERE field = ? LIMIT 1", -1,
        &statement, NULL);
    if (status != SQLITE_OK) {
        return status;
    }
    status = sqlite3_bind_text(statement, 1, expression, -1, SQLITE_TRANSIENT);
    if (status != SQLITE_OK) { goto finish; }
    status = sqlite3_step(statement);
    if (status == SQLITE_DONE) {
        status = SQLITE_OK;
        *present = 0;
    } else if (status == SQLITE_ROW) {
        status = SQLITE_OK;
        *present = 1;
    }
finish:
    sqlite3_finalize(statement);
    return status;
}


int
u1db__find_unique_expressions(u1database *db,
                              int n_expressions, const char **expressions,
                              int *n_unique, const char ***unique_expressions)
{
    int i, status, present = 0;
    const char **tmp = NULL;

    tmp = (const char **)calloc(n_expressions, sizeof(char*));
    if (tmp == NULL) {
        return U1DB_NOMEM;
    }
    status = U1DB_OK;
    *n_unique = 0;
    for (i = 0; i < n_expressions; ++i) {
        if (expressions[i] == NULL) {
            status = U1DB_INVALID_PARAMETER;
            goto finish;
        }
        status = is_present(db, expressions[i], &present);
        if (status != SQLITE_OK) { goto finish; }
        if (!present) {
            tmp[*n_unique] = expressions[i];
            (*n_unique)++;
        }
    }
finish:
    if (status == U1DB_OK) {
        *unique_expressions = tmp;
    } else {
        free((void*)tmp);
    }
    return status;
}


int
u1db__update_indexes(u1database *db, const char *doc_id, const char *content)
{
    struct evaluate_index_context context;
    int status;

    if (content == NULL) {
        // No new fields to add to the database.
        return U1DB_OK;
    }
    context.db = db;
    context.doc_id = doc_id;
    context.content = content;
    context.obj = json_tokener_parse(content);
    if (context.obj == NULL
            || !json_object_is_type(context.obj, json_type_object))
    {
        return U1DB_INVALID_JSON;
    }
    status = iter_field_definitions(
        db, &context, evaluate_index_and_insert_into_db);
    json_object_put(context.obj);
    return status;
}


int
u1db__index_all_docs(u1database *db, int n_expressions,
                     const char **expressions)
{
    int status, i;
    sqlite3_stmt *statement = NULL;
    struct evaluate_index_context context = {0};
    parse_tree **trees;

    trees = (parse_tree**)calloc(n_expressions, sizeof(parse_tree*));
    for (i = 0; i < n_expressions; ++i) {
        init_parse_tree(&trees[i]);
        status = parse(expressions[i], trees[i]);
        if (status != U1DB_OK)
            goto finish;
    }
    status = sqlite3_prepare_v2(db->sql_handle,
        "SELECT doc_id, content FROM document", -1,
        &statement, NULL);
    if (status != SQLITE_OK) {
        return status;
    }
    context.db = db;
    status = sqlite3_step(statement);
    while (status == SQLITE_ROW) {
        if (context.obj != NULL) {
            json_object_put(context.obj);
            context.obj = NULL;
        }
        context.doc_id = (const char*)sqlite3_column_text(statement, 0);
        context.content = (const char*)sqlite3_column_text(statement, 1);
        if (context.content == NULL)
        {
            // This document is deleted so does not need to be indexed.
            status = sqlite3_step(statement);
            continue;
        }
        context.obj = json_tokener_parse(context.content);
        if (context.obj == NULL
                || !json_object_is_type(context.obj, json_type_object))
        {
            // Invalid JSON in the database, for now we just continue?
            // TODO: Raise an error here.
            status = sqlite3_step(statement);
            continue;
        }
        for (i = 0; i < n_expressions; ++i) {
            status = evaluate_index_and_insert_into_db(
                &context, expressions[i], trees[i]);
            if (status != U1DB_OK)
                goto finish;
        }
        status = sqlite3_step(statement);
    }
    if (status == SQLITE_DONE) {
        status = U1DB_OK;
    }
finish:
    for (i = 0; i < n_expressions; ++i)
        destroy_parse_tree(trees[i]);
    free(trees);
    if (context.obj != NULL) {
        json_object_put(context.obj);
        context.obj = NULL;
    }
    sqlite3_finalize(statement);
    return status;
}
