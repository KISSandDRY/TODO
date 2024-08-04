#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define NAME "TODO"
#define VERSION "0.0.9v"

#define FLAG_IDENTIFIER "-"
#define TODO_FILE_NAME "TODO"

#define NEW_TODO(todos, index) \
    if(!todos) todos = malloc(sizeof(todo_t)); \
    else todos = realloc(todos, sizeof(todo_t)*(no_todos+1)); \
    todos[index].priority = 0; \
    todos[index].title = NULL; \
    todos[index].description = NULL;


#define SHOW_ERROR \
        offset = find_nth_occurrence(str, '\n', no_lines-1); \
        printf("%ld | ", no_lines); \
        print_until_symbol(str+offset+1, '\n'); \
        printf("\n"); \
        offset = buf-str-offset+get_long_length(no_lines)+2; \
        while(offset-->0) printf(" "); \
        printf("^\n");

#define SKIP_SPACE_COUNT_LINE \
        while(isspace(*buf)) { \
            if(*buf == '\n') ++no_lines; \
            ++buf; \
        } 

#define FLAG_DESCRIPTION_LEN 128

typedef enum FLAG_TYPES {
    SHOW_HELP,
    SHOW_VERSION,
    SET_PRIORITY_FILTER, UNSET_PRIORITY_FILTER,
    SET_TITLE_NAME_FILTER, UNSET_TITLE_NAME_FILTER,
    SET_PRIORITY_LEVEL 
} flag_action_t;

typedef struct {
    flag_action_t type;
    const char* flag_short_str;
    const char* flag_long_str;
    const char description[FLAG_DESCRIPTION_LEN];
} flag_t;

static const flag_t FLAGS[] = {
    {SHOW_HELP,               FLAG_IDENTIFIER "h",   FLAG_IDENTIFIER FLAG_IDENTIFIER "help", "Shows help info."},
    {SHOW_VERSION,            FLAG_IDENTIFIER "v",   FLAG_IDENTIFIER FLAG_IDENTIFIER "version", "Shows version."},
    {SET_PRIORITY_FILTER,     FLAG_IDENTIFIER "p=1", FLAG_IDENTIFIER FLAG_IDENTIFIER "priority=1", "Sets priority filter to true."},
    {UNSET_PRIORITY_FILTER,   FLAG_IDENTIFIER "p=0", FLAG_IDENTIFIER FLAG_IDENTIFIER "priority=0", "Sets priority filter to false."},
    {SET_TITLE_NAME_FILTER,   FLAG_IDENTIFIER "t=1", FLAG_IDENTIFIER FLAG_IDENTIFIER "title=1", "Sets title name filter to true."},
    {UNSET_TITLE_NAME_FILTER, FLAG_IDENTIFIER "t=0", FLAG_IDENTIFIER FLAG_IDENTIFIER "title=0", "Sets title name filter to false."},
    {SET_PRIORITY_LEVEL,      FLAG_IDENTIFIER "l",   FLAG_IDENTIFIER FLAG_IDENTIFIER "level", "Sets title name filter to false."},
};

enum FILTERS {
    NONE_F,
    PRIORITY_F = ((uint8_t)1 << 0),
    TITLE_NAME_F = ((uint8_t)1 << 1)
};

typedef struct {
    char* todo_file_name;
    uint8_t filters;
    long priority_level;
} config_t;

static config_t CONFIG = {
    .todo_file_name = TODO_FILE_NAME,
    .filters = PRIORITY_F,
    .priority_level = 0
};

typedef struct {
    char* title;
    char* description;
    long priority;
} todo_t;

char* strip(const char *str) {
    if(!str) return NULL;

    size_t begin = 0, end = strlen(str);

    while (begin < end && isspace(str[begin])) begin++;
    while (end > begin && isspace(str[end - 1])) end--;

    size_t new_length = end - begin;
    if(!new_length) return NULL;

    char *trimmed_str = malloc(new_length + 1);

    if (!trimmed_str) {
        printf("Error: allocating memory to trimmed str.\n");

        return NULL;
    }

    strncpy(trimmed_str, str + begin, new_length);
    trimmed_str[new_length] = 0;

    return trimmed_str;
}

long get_long_length(long num) {
    if (!num) return 1; 

    long length = 0;

    if (num < 0)  num = -num;

    while (num) { num /= 10; length++; }

    return length;
}

char* read_file(const char *file_name) {
    if(!file_name) return NULL;

    FILE *file = fopen(file_name, "rb");
    if (!file) {
        printf("Error: '%s' file or directory does not exist.\n", file_name);

        return 0;
    }

    if (fseek(file, 0, SEEK_END)) {
        printf("Error: seeking to end of file.\n");

        fclose(file);
        return 0;
    }

    long file_size = ftell(file);

    if (file_size == -1) {
        printf("Error: getting file size.\n");

        fclose(file);
        return 0;
    }

    rewind(file);

    char *buffer = (char*)malloc(file_size + 1);
    if (!buffer) {
        printf("Error: allocating memory.\n");

        fclose(file);
        return 0;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        printf("Error: reading file.\n");

        free(buffer);
        fclose(file);
        return 0;
    }

    buffer[file_size] = 0;

    fclose(file);

    return buffer;
}

bool ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false; 
    
    size_t str_len = strlen(str), suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;

    return !strncmp(str + str_len - suffix_len, suffix, suffix_len);
}

bool starts_with(const char* str, const char* prefix) {
    size_t prefix_len = strlen(prefix);

    return !strncmp(str, prefix, prefix_len);
}

int find_nth_occurrence(const char *str, char ch, int n) {
    int count = 0, index = -1; 

    for (int i = 0; str[i]; i++) {
        if (str[i] != ch) continue;

        count++;
        if (count == n) {
            index = i;
            break;
        }
    }

    return index;
}

size_t is_num(const char* str) {
    const char* buf = str;

    while(*buf) {
        if(*buf < 0x30 || *buf > 0x39) {
            if(isspace(*buf)) break;
            
            return 0;
        }   

        ++buf;
    }

    return buf-str;
}

void print_until_symbol(const char *str, char symbol) {
    const char *pos = strchr(str, symbol);

    if (pos) {
        printf("%.*s", (int)(pos-str), str);

        return;
    }

    printf("%s", str);
}

todo_t* get_todos(const char* str) {
    if(!str) return NULL;

    size_t no_todos = 0;
    todo_t* todos = NULL;

    long offset = 0;
    size_t no_lines = 1, token_len = 0;
    const char* buf = str;

    while(*buf) {
        SKIP_SPACE_COUNT_LINE;

        offset = 0;
        token_len = 0;

        if(starts_with(buf, "TODO") && *buf) {
            NEW_TODO(todos, no_todos);

            buf += strlen("TODO"); 

            SKIP_SPACE_COUNT_LINE;

            if(*buf != ':') {
                SHOW_ERROR;

                printf("Error: expected ':'.\n");
                exit(1);
            }

            ++buf;
            
            SKIP_SPACE_COUNT_LINE;

            token_len = is_num(buf);
            if(!token_len) {
                SHOW_ERROR;

                printf("Error: priority is invalid or does not provided.\n");
                exit(1);
            }            

            char* pr_str = malloc(sizeof(char) * (token_len + 1));
            memcpy(pr_str, buf, token_len);
            pr_str[token_len] = 0;

            char *endptr = NULL;

            errno = 0;  
            long priority = strtol(pr_str, &endptr, 10);

            if (errno) {
                SHOW_ERROR;

                printf("Error: converting priority to number.\n");
                exit(1);
            }

            if (*endptr) {
                SHOW_ERROR;

                printf("Error: trailing characters after priority: %s.\n", endptr);
                exit(1);
            }

            if(priority <= 0) {
                SHOW_ERROR;

                printf("Error: priority is less or equal to 0.\n");
                exit(1);
            }

            todos[no_todos].priority = priority;

            free(pr_str);

            buf += token_len;

            SKIP_SPACE_COUNT_LINE;

            if(*buf != '"') {
                SHOW_ERROR;

                printf("Error: expected starting '\"'.\n");
                exit(1);
            }

            ++buf;

            size_t title_len = 0;
            while(*buf && *buf != '"') {
                if(*buf == '\n') ++no_lines;

                ++title_len; 
                ++buf;
            }

            if(!title_len) {
                SHOW_ERROR;

                printf("Error: title is empty.\n");
                exit(1);
            }

            if(*buf != '"') {
                SHOW_ERROR;

                printf("Error: expected ending '\"'.\n");
                exit(1);
            }

            ++buf; 

            todos[no_todos].title = malloc(sizeof(char)*(title_len+1));
            memcpy(todos[no_todos].title, buf-title_len-1, title_len);
            todos[no_todos].title[title_len] = 0;

            SKIP_SPACE_COUNT_LINE;

            if(!starts_with(buf, "{{")) {
                SHOW_ERROR;

                printf("Error: expected '{{'.\n");
                exit(1);
            }
            
            buf += strlen("{{");

            size_t description_len = 0;
            while(*buf && !starts_with(buf, "}}")) {
                if(*buf == '\n') ++no_lines;
                ++description_len; 
                ++buf;
            }

            if(!starts_with(buf, "}}")) {
                SHOW_ERROR;

                printf("Error: expected ending '}}'.\n");
                exit(1);
            }
            
            buf += strlen("}}");

            if(description_len) {
                todos[no_todos].description = malloc(sizeof(char)*(description_len+1));
                memcpy(todos[no_todos].description, buf-description_len-1, description_len);
                todos[no_todos].description[description_len] = 0;

                char* trimmed_description = strip(todos[no_todos].description);

                free(todos[no_todos].description);

                todos[no_todos].description = trimmed_description;
            }

            ++no_todos;

            SKIP_SPACE_COUNT_LINE;

            continue;
        } 

        SHOW_ERROR;

        printf("Error: expected todo start statement.\n");
        exit(1);
    }

    NEW_TODO(todos, no_todos);

    return todos;
}


void sort_todos_by_title_name(todo_t* todos) {
    int i, j, n = 0;

    while (todos[n].priority) n++;

    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n; j++) {
            if (tolower((unsigned char)todos[i].title[0]) > tolower((unsigned char)todos[j].title[0])) {
                todo_t temp = todos[i];
                todos[i] = todos[j];
                todos[j] = temp;
            }
        }
    }
}

void sort_todos_by_priority(todo_t* todos) {
    int i, j;

    for (i = 0; todos[i].priority; i++) {
        for (j = 0; todos[j + 1].priority; j++) {
            if (todos[j].priority > todos[j + 1].priority) {
                todo_t temp = todos[j];

                todos[j] = todos[j + 1];
                todos[j + 1] = temp;
            }
        }
    }
}

void print_todos(todo_t* todos) {
    bool todo_is_found = false;

    printf("TODOs:\n");

    if(!todos) {
        printf("No TODOs.\n");
        return;
    }

    if(CONFIG.filters & TITLE_NAME_F) {
        sort_todos_by_title_name(todos);
    }

    if(CONFIG.filters & PRIORITY_F) {
        sort_todos_by_priority(todos);
    }

    for(int i = 0; todos[i].priority; i++) {
        if(CONFIG.priority_level == 0 || todos[i].priority == CONFIG.priority_level) {
            todo_is_found = true;

            printf("-----------------------\n");

            printf("Title: %s\nPriority: %ld\nDescription:\n  %s\n", 
                    todos[i].title, todos[i].priority, 
                    todos[i].description ? todos[i].description : "Not added.");

            printf("-----------------------\n");
        }
    }

    if(!todo_is_found) {
        printf("No TODOs.\n");
    }
}

void show_version() {
    printf("%s version(%s)\n", NAME, VERSION);
}

void show_help() {
    printf("%s version(%s)\n", NAME, VERSION);

    printf("Flags:\n");
    for(size_t i = 0; i < sizeof(FLAGS)/sizeof(FLAGS[0]); i++) {
        printf("'%s'\tor\t'%s'\t-\t%s\n", FLAGS[i].flag_short_str, FLAGS[i].flag_long_str, FLAGS[i].description); 
    }
}

int execute_flag(flag_action_t action, char* argv[]) {
    bool is_value_empty = false;
    int offset = 0; 

    char* flag = *argv;
    argv++;

    if(!*argv) is_value_empty = true; 

    switch(action) {
        case SHOW_HELP:
            show_help();
            exit(0);

        case SHOW_VERSION:
            show_version();
            exit(0);

        case SET_PRIORITY_FILTER:
            CONFIG.filters |= PRIORITY_F;
            break;

        case UNSET_PRIORITY_FILTER:
            CONFIG.filters &= ~PRIORITY_F;
            break;

        case SET_TITLE_NAME_FILTER:
            CONFIG.filters |= TITLE_NAME_F;
            break;

        case UNSET_TITLE_NAME_FILTER:
            CONFIG.filters &= ~TITLE_NAME_F;
            break;
        
        case SET_PRIORITY_LEVEL:
            if(is_value_empty) {
                printf("Error: flag '%s' requires value.\n", flag);
                exit(1);
            }

            int number = is_num(*argv);
            if(!number) {
                printf("Error: '%s' is invalid priority.\n", *argv);
                exit(1);
            }
            
            number = atoi(*argv);

            ++offset;

            CONFIG.priority_level = number;

            return offset;

        default:
            printf("Error: undefined flag type.\n");
            exit(1);
    }

    return offset;
}

void check_flags(int argc, char* argv[]) {
    bool is_defined_flag = false;
    const int no_flags = sizeof(FLAGS) / sizeof(FLAGS[0]);

    for(int i = 0; argv[i] && i < argc; i++) {
        if(argv[i][0] != FLAG_IDENTIFIER[0]) {
            CONFIG.todo_file_name = argv[i];
            continue;
        }

        is_defined_flag = false;

        for(int j = 0; j < no_flags; j++) {
            if(starts_with(argv[i], FLAGS[j].flag_long_str) || starts_with(argv[i], FLAGS[j].flag_short_str)) {
                i += execute_flag(FLAGS[j].type, argv+i);
                is_defined_flag = true;
                break;
            } 
        } 

        if(!is_defined_flag) {
            printf("Error: flag '%s' is undefined.\n", argv[i]);
            exit(1);
        }
    }
}

int main(int argc, char* argv[]) {
    if(argc != 1) {
        check_flags(argc-1, argv+1);
    }

    FILE* todo_file = fopen(CONFIG.todo_file_name, "rb");
    if(!todo_file) { printf("Error: '%s' file does not exist.\n", CONFIG.todo_file_name); return 1; }
    fclose(todo_file);

    const char* file_content = read_file(CONFIG.todo_file_name);
    todo_t* todos = get_todos(file_content);

    print_todos(todos);

    return 0;
}
