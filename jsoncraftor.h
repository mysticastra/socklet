// Copyright 2025 mysticastra

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef JSONPARSER_H
#define JSONPARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Define the mapping between JSON key and struct member
typedef struct JsonMap {
    const char* json_key;     // JSON key name
    void* struct_member;      // Pointer to struct member
    char type;               // 'i' for int, 's' for string, 'b' for bool, 'd' for double, 'o' for object, 'a' for array
    size_t size;            // Size for strings/arrays, or number of mappings for objects
    bool required;          // Whether this field is required
    struct JsonMap* nested; // For nested objects or array items
} JsonMap;

// Skip whitespace and specific character
static bool skip_char(const char** ptr, char c) {
    while (**ptr && (**ptr == ' ' || **ptr == '\n' || **ptr == '\t' || **ptr == '\r')) (*ptr)++;
    if (**ptr == c) {
        (*ptr)++;
        return true;
    }
    return false;
}

// Check if we've reached the end of input
static bool is_end(const char* ptr) {
    while (*ptr && (*ptr == ' ' || *ptr == '\n' || *ptr == '\t' || *ptr == '\r')) ptr++;
    return *ptr == '\0';
}

// Parse a single value based on type
static bool parse_value(const char** ptr, JsonMap* map, char** error, bool* found) {
    while (**ptr && (**ptr == ' ' || **ptr == '\n' || **ptr == '\t' || **ptr == '\r')) (*ptr)++;
    
    switch (map->type) {
        case 'i': {
            char* endptr;
            long val = strtol(*ptr, &endptr, 10);
            if (*ptr == endptr) {
                *error = "Invalid integer value";
                return false;
            }
            *(int*)map->struct_member = (int)val;
            *ptr = endptr;
            *found = true;
            return true;
        }
        case 's': {
            if (**ptr != '"') {
                *error = "Expected string value";
                return false;
            }
            (*ptr)++;
            char* str_start = (char*)*ptr;
            while (**ptr && **ptr != '"') (*ptr)++;
            if (**ptr != '"') {
                *error = "Unterminated string";
                return false;
            }
            size_t len = *ptr - str_start;
            if (len >= map->size) {
                *error = "String too long";
                return false;
            }
            strncpy(map->struct_member, str_start, len);
            ((char*)map->struct_member)[len] = '\0';
            (*ptr)++;
            *found = true;
            return true;
        }
        case 'b': {
            if (strncmp(*ptr, "true", 4) == 0) {
                *(bool*)map->struct_member = true;
                *ptr += 4;
                *found = true;
                return true;
            } else if (strncmp(*ptr, "false", 5) == 0) {
                *(bool*)map->struct_member = false;
                *ptr += 5;
                *found = true;
                return true;
            }
            *error = "Invalid boolean value";
            return false;
        }
        case 'd': {
            char* endptr;
            double val = strtod(*ptr, &endptr);
            if (*ptr == endptr) {
                *error = "Invalid double value";
                return false;
            }
            *(double*)map->struct_member = val;
            *ptr = endptr;
            *found = true;
            return true;
        }
        case 'o': {
            if (**ptr != '{') {
                *error = "Expected object";
                return false;
            }
            (*ptr)++;
            
            bool* nested_found = calloc(map->size, sizeof(bool));
            
            while (**ptr) {
                if (skip_char(ptr, '}')) break;
                
                if (**ptr != '"') {
                    *error = "Expected property name";
                    free(nested_found);
                    return false;
                }
                (*ptr)++;
                
                char* key_start = (char*)*ptr;
                while (**ptr && **ptr != '"') (*ptr)++;
                size_t key_len = *ptr - key_start;
                (*ptr)++;
                
                if (!skip_char(ptr, ':')) {
                    *error = "Expected ':'";
                    free(nested_found);
                    return false;
                }
                
                // Find matching nested mapping
                JsonMap* nested_map = NULL;
                int nested_index = -1;
                for (size_t i = 0; i < map->size; i++) {
                    if (strncmp(map->nested[i].json_key, key_start, key_len) == 0 && 
                        strlen(map->nested[i].json_key) == key_len) {
                        nested_map = &map->nested[i];
                        nested_index = i;
                        break;
                    }
                }
                
                if (nested_map) {
                    bool nested_field_found = false;
                    if (!parse_value(ptr, nested_map, error, &nested_field_found)) {
                        free(nested_found);
                        return false;
                    }
                    if (nested_field_found) {
                        nested_found[nested_index] = true;
                    }
                } else {
                    // Skip unknown property
                    int depth = 1;
                    while (**ptr && depth > 0) {
                        if (**ptr == '{' || **ptr == '[') depth++;
                        if (**ptr == '}' || **ptr == ']') depth--;
                        (*ptr)++;
                    }
                }
                
                skip_char(ptr, ',');
            }
            
            // Check required fields in nested object
            for (size_t i = 0; i < map->size; i++) {
                if (map->nested[i].required && !nested_found[i]) {
                    *error = "Missing required field in nested object";
                    free(nested_found);
                    return false;
                }
            }
            
            free(nested_found);
            *found = true;
            return true;
        }
        case 'a': {
            if (**ptr != '[') {
                *error = "Expected array";
                return false;
            }
            (*ptr)++;
            
            // Get array pointer and sizes
            void* array = map->struct_member;
            size_t array_len = map->size;  // Number of elements in array
            JsonMap* item_map = map->nested;
            size_t item_size = (item_map->type == 's') ? item_map->size : sizeof(int);  // Size of each element
            size_t count = 0;
            
            while (**ptr) {
                if (skip_char(ptr, ']')) break;
                
                if (count >= array_len) {
                    *error = "Array too long";
                    return false;
                }
                
                // Calculate pointer to current array element
                void* item_ptr;
                if (item_map->type == 's') {
                    // For string arrays, calculate offset using item_size
                    item_ptr = (char*)array + (count * item_size);
                } else {
                    // For integer arrays, use sizeof(int)
                    item_ptr = (int*)array + count;
                }
                
                JsonMap current_item = *item_map;
                current_item.struct_member = item_ptr;
                
                bool item_found = false;
                if (!parse_value(ptr, &current_item, error, &item_found)) {
                    return false;
                }
                
                count++;
                skip_char(ptr, ',');
            }
            
            if (count < array_len) {
                *error = "Array too short";
                return false;
            }
            
            *found = true;
            return true;
        }
        default:
            *error = "Unknown type";
            return false;
    }
}

// Main parsing function
bool parse_json(const char* json, JsonMap* mappings, int map_count, char** error) {
    if (!json) {
        *error = "NULL input";
        return false;
    }

    const char* ptr = json;
    bool* found = calloc(map_count, sizeof(bool));
    
    // Skip initial whitespace and {
    if (!skip_char(&ptr, '{')) {
        *error = "Expected {";
        free(found);
        return false;
    }

    bool first_field = true;
    bool found_end = false;
    while (*ptr) {
        // Handle end of object
        if (skip_char(&ptr, '}')) {
            // Check for trailing content
            if (!is_end(ptr)) {
                *error = "Unexpected content after }";
                free(found);
                return false;
            }
            found_end = true;
            break;
        }

        // Check for premature end of input
        if (!*ptr) {
            *error = "Unexpected end of input";
            free(found);
            return false;
        }

        // Handle comma between fields
        if (!first_field) {
            if (!skip_char(&ptr, ',')) {
                *error = "Expected ,";
                free(found);
                return false;
            }
        }

        // Check for trailing comma
        if (skip_char(&ptr, '}')) {
            *error = "Trailing comma";
            free(found);
            return false;
        }

        if (*ptr != '"') {
            *error = "Expected property name";
            free(found);
            return false;
        }
        ptr++;
        
        // Get key
        char* key_start = (char*)ptr;
        while (*ptr && *ptr != '"') ptr++;
        if (!*ptr) {
            *error = "Unterminated string";
            free(found);
            return false;
        }
        size_t key_len = ptr - key_start;
        ptr++;
        
        if (!skip_char(&ptr, ':')) {
            *error = "Expected :";
            free(found);
            return false;
        }

        // Find matching mapping
        JsonMap* map = NULL;
        int map_index = -1;
        for (int i = 0; i < map_count; i++) {
            if (strncmp(mappings[i].json_key, key_start, key_len) == 0 && 
                strlen(mappings[i].json_key) == key_len) {
                map = &mappings[i];
                map_index = i;
                break;
            }
        }

        if (map) {
            bool field_found = false;
            if (!parse_value(&ptr, map, error, &field_found)) {
                free(found);
                return false;
            }
            if (field_found && map_index >= 0) {
                found[map_index] = true;
            }
        } else {
            // Skip unknown property
            int depth = 1;
            while (*ptr && depth > 0) {
                if (*ptr == '{' || *ptr == '[') depth++;
                if (*ptr == '}' || *ptr == ']') depth--;
                ptr++;
            }
        }
        
        first_field = false;
    }

    // Check if we found the closing brace
    if (!found_end) {
        *error = "Missing closing brace";
        free(found);
        return false;
    }

    // Check if all required fields were found
    for (int i = 0; i < map_count; i++) {
        if (mappings[i].required && !found[i]) {
            *error = "Missing required field";
            free(found);
            return false;
        }
    }

    free(found);
    return true;
}

#endif
