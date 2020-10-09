/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// JSON parser
// Adapted from jsmnSpark at https://github.com/pkourany/JSMNSpark
// Merged changes from original https://github.com/zserge/jsmn
//
// Rob Dobson 2017-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdlib.h>

#define JSMNR_PARENT_LINKS 1
#define JSMNR_STRICT 1

/**
 * JSON type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT = 1,
    JSMN_ARRAY = 2,
    JSMN_STRING = 3,
    JSMN_PRIMITIVE = 4
} jsmntype_t;

typedef enum {
    /* Not enough tokens were provided */
    JSMN_ERROR_NOMEM = -1,
    /* Invalid character inside JSON string */
    JSMN_ERROR_INVAL = -2,
    /* The string is not a full JSON packet, more bytes expected */
    JSMN_ERROR_PART = -3,
    /* Everything was fine */
    JSMN_SUCCESS = 0
} jsmnerr_t;

/**
 * JSON token description.
 * @param		type	type (object, array, string etc.)
 * @param		start	start position in JSON data string
 * @param		end		end position in JSON data string
 */
typedef struct jsmntok_t {
    jsmntype_t type;
    int start;
    int end;
    int size;
#ifdef JSMNR_PARENT_LINKS
    int parent;
#endif
} jsmntok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string
 */
typedef struct {
    unsigned int pos; /* offset in the JSON string */
    unsigned int toknext; /* next token to allocate */
    int toksuper; /* superior token node, e.g parent object or array */
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens
 */
void jsmn_init(jsmn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each describing
 * a single JSON object.
 * Returns count of parsed objects.
 */
int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
                jsmntok_t *tokens, unsigned int num_tokens);


void jsmn_logLongStr(const char* headerMsg, const char* toLog, bool infoLevel);
