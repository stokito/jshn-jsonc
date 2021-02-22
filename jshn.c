/*
 * Copyright (C) 2011-2013 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <json.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

struct lh_table *env_vars = NULL;
static const char *var_prefix = "";
static int var_prefix_len = 0;

static int add_json_element(const char *key, json_object *obj);

static void load_vars_from_envs();

static int add_json_object(json_object *obj)
{
	int ret = 0;

	json_object_object_foreach(obj, key, val) {
		ret = add_json_element(key, val);
		if (ret)
			break;
	}
	return ret;
}

static int add_json_array(struct array_list *a)
{
	char seq[12];
	int i, len;
	int ret;

	for (i = 0, len = array_list_length(a); i < len; i++) {
		snprintf(seq, sizeof(seq), "%d", i);
		ret = add_json_element(seq, array_list_get_idx(a, i));
		if (ret)
			return ret;
	}

	return 0;
}

static void add_json_string(const char *str)
{
	char *ptr = (char *) str;
	int len;
	char *c;

	while ((c = strchr(ptr, '\'')) != NULL) {
		len = c - ptr;
		if (len > 0)
			fwrite(ptr, len, 1, stdout);
		ptr = c + 1;
		c = "'\\''";
		fwrite(c, strlen(c), 1, stdout);
	}
	len = strlen(ptr);
	if (len > 0)
		fwrite(ptr, len, 1, stdout);
}

static void write_key_string(const char *key)
{
	char k;
	while ((k = *key)) {
		k = isalnum(k) ? k : '_';
		putc(k, stdout);
		key++;
	}
}

static int add_json_element(const char *key, json_object *obj)
{
	char *type;

	switch (json_object_get_type(obj)) {
	case json_type_object:
		type = "object";
		break;
	case json_type_array:
		type = "array";
		break;
	case json_type_string:
		type = "string";
		break;
	case json_type_boolean:
		type = "boolean";
		break;
	case json_type_int:
		type = "int";
		break;
	case json_type_double:
		type = "double";
		break;
	case json_type_null:
		type = "null";
		break;
	default:
		return -1;
	}

	fprintf(stdout, "json_add_%s '", type);
	write_key_string(key);

	switch (json_object_get_type(obj)) {
	case json_type_object:
		fprintf(stdout, "';\n");
		add_json_object(obj);
		fprintf(stdout, "json_close_object;\n");
		break;
	case json_type_array:
		fprintf(stdout, "';\n");
		add_json_array(json_object_get_array(obj));
		fprintf(stdout, "json_close_array;\n");
		break;
	case json_type_string:
		fprintf(stdout, "' '");
		add_json_string(json_object_get_string(obj));
		fprintf(stdout, "';\n");
		break;
	case json_type_boolean:
		fprintf(stdout, "' %d;\n", json_object_get_boolean(obj));
		break;
	case json_type_int:
		fprintf(stdout, "' %"PRId64";\n", json_object_get_int64(obj));
		break;
	case json_type_double:
		fprintf(stdout, "' %lf;\n", json_object_get_double(obj));
		break;
	case json_type_null:
		fprintf(stdout, "';\n");
		break;
	default:
		return -1;
	}

	return 0;
}

static int jshn_parse(const char *str)
{
	json_object *obj;

	obj = json_tokener_parse(str);
	if (!obj || json_object_get_type(obj) != json_type_object) {
		if (obj)
			json_object_put(obj);
		fprintf(stderr, "Failed to parse message data\n");
		return 1;
	}
	fprintf(stdout, "json_init;\n");
	add_json_object(obj);
	fflush(stdout);
	json_object_put(obj);

	return 0;
}

static char *getenv_val(const char *key)
{
	struct lh_entry *var = lh_table_lookup_entry(env_vars, key);
	return var ? (char *) var->v : NULL;
}

static char *get_keys(const char *prefix)
{
	char *keys;
	size_t len = var_prefix_len + strlen(prefix) + sizeof("K_") + 1;

	keys = alloca(len);
	snprintf(keys, len, "%sK_%s", var_prefix, prefix);
	return getenv_val(keys);
}

static void get_var(const char *prefix, const char **name, char **var, char **type)
{
	char *tmpname, *varname;
	size_t len = var_prefix_len + strlen(prefix) + 1 + strlen(*name) + 1 + sizeof("T_");

	tmpname = alloca(len);

	snprintf(tmpname, len, "%s%s_%s", var_prefix, prefix, *name);
	*var = getenv_val(tmpname);

	snprintf(tmpname, len, "%sT_%s_%s", var_prefix, prefix, *name);
	*type = getenv_val(tmpname);

	snprintf(tmpname, len, "%sN_%s_%s", var_prefix, prefix, *name);
	varname = getenv_val(tmpname);
	if (varname)
		*name = varname;
}

static json_object *jshn_add_objects(json_object *obj, const char *prefix, bool array);

static void jshn_add_object_var(json_object *obj, bool array, const char *prefix, const char *name)
{
	json_object *new;
	char *var, *type;

	get_var(prefix, &name, &var, &type);
	if (!var || !type)
		return;

	if (!strcmp(type, "array")) {
		new = json_object_new_array();
		jshn_add_objects(new, var, true);
	} else if (!strcmp(type, "object")) {
		new = json_object_new_object();
		jshn_add_objects(new, var, false);
	} else if (!strcmp(type, "string")) {
		new = json_object_new_string(var);
	} else if (!strcmp(type, "int")) {
		new = json_object_new_int64(atoll(var));
	} else if (!strcmp(type, "double")) {
		new = json_object_new_double(strtod(var, NULL));
	} else if (!strcmp(type, "boolean")) {
		new = json_object_new_boolean(!!atoi(var));
	} else if (!strcmp(type, "null")) {
		new = NULL;
	} else {
		return;
	}

	if (array)
		json_object_array_add(obj, new);
	else
		json_object_object_add(obj, name, new);
}

static json_object *jshn_add_objects(json_object *obj, const char *prefix, bool array)
{
	char *keys, *key, *brk;

	keys = get_keys(prefix);
	if (!keys || !obj)
		goto out;

	for (key = strtok_r(keys, " ", &brk); key;
	     key = strtok_r(NULL, " ", &brk)) {
		jshn_add_object_var(obj, array, prefix, key);
	}

out:
	return obj;
}

static int jshn_format(FILE *stream)
{
	json_object *obj;
	const char *output;
	char *blobmsg_output = NULL;
	int ret = -1;

	if (!(obj = json_object_new_object()))
		return -1;

	load_vars_from_envs();
	jshn_add_objects(obj, "J_V", false);
	if (!(output = json_object_to_json_string(obj)))
		goto out;

	fprintf(stream, "%s", output);
	free(blobmsg_output);
	ret = 0;

out:
	lh_table_free(env_vars);
	json_object_put(obj);
	return ret;
}

static int usage(const char *progname)
{
	fprintf(stderr, "Usage: %s -r <message>|-R <file>|-o <file>|-p <prefix>|-w\n", progname);
	return 2;
}

static void env_var_lh_entry_free(struct lh_entry *ent)
{
	free(lh_entry_k(ent));
}

/**
 * JSON variables are passed via environment variables.
 * But getenv() makes a linear search so for faster lookups we store envs into hash table.
 *
 * The function allocates `env_vars` variable and it should be freed afterwards.
 */
static void load_vars_from_envs() {
	int i;
	extern char **environ;
	// count env vars
	for (i = 0; environ[i]; i++);

	env_vars = lh_kchar_table_new(i, &env_var_lh_entry_free);
	if (!env_vars) {
		fprintf(stderr, "%m\n");
		exit(EXIT_FAILURE);
	}
	for (i = 0; environ[i]; i++) {
		char *c;
		c = strchr(environ[i], '=');
		if (!c)
			continue;
		size_t key_length = c - environ[i];
		char *key = strndup(environ[i], key_length);
		// value comes after '=' separator
		char *val = c + 1;
		lh_table_insert(env_vars, key, val);
	}
}

static int jshn_parse_file(const char *path)
{
	struct stat sb;
	int ret = 0;
	char *fbuf;
	int fd;

	if ((fd = open(path, O_RDONLY)) == -1) {
		fprintf(stderr, "Error opening %s\n", path);
		return 3;
	}

	if (fstat(fd, &sb) == -1) {
		fprintf(stderr, "Error getting size of %s\n", path);
		close(fd);
		return 3;
	}

	if (!(fbuf = calloc(1, sb.st_size+1))) {
		fprintf(stderr, "Error allocating memory for %s\n", path);
		close(fd);
		return 3;
	}

	if (read(fd, fbuf, sb.st_size) != sb.st_size) {
		fprintf(stderr, "Error reading %s\n", path);
		free(fbuf);
		close(fd);
		return 3;
	}

	ret = jshn_parse(fbuf);
	free(fbuf);
	close(fd);

	return ret;
}

static int jshn_format_file(const char *path)
{
	FILE *fp = NULL;
	int ret = 0;

	fp = fopen(path, "w");
	if (!fp) {
		fprintf(stderr, "Error opening %s\n", path);
		return 3;
	}

	ret = jshn_format(fp);
	fclose(fp);

	return ret;
}

int main(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, "p:nir:R:o:w")) != -1) {
		switch(ch) {
		case 'p':
			var_prefix = optarg;
			var_prefix_len = strlen(var_prefix);
			break;
		case 'r':
			return jshn_parse(optarg);
		case 'R':
			return jshn_parse_file(optarg);
		case 'w':
			return jshn_format(stdout);
		case 'o':
			return jshn_format_file(optarg);
		case 'n':
		case 'i':
			// ignore
			break;
		default:
			// unknown param, show usage
			goto show_usage;
		}
	}

show_usage:
	return usage(argv[0]);
}
