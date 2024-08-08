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

#define FLAG_DESCRIPTION_LEN 128

typedef enum FLAG_TYPES {
    SHOW_HELP,
    SHOW_VERSION,
    SET_PRIORITY_FILTER, UNSET_PRIORITY_FILTER,
    SET_TITLE_NAME_FILTER, UNSET_TITLE_NAME_FILTER,
    SET_PRIORITY_LEVEL,
    ADD_TODO,
    REMOVE_TODO,
} flag_action_t;

typedef struct {
    flag_action_t type;
    const char* flag_short_str;
    const char* flag_long_str;
    const char description[FLAG_DESCRIPTION_LEN];
} flag_t;

static const flag_t FLAGS[] = {
    {SHOW_HELP,               FLAG_IDENTIFIER "h",   FLAG_IDENTIFIER FLAG_IDENTIFIER "help",       "Shows help info."},
    {SHOW_VERSION,            FLAG_IDENTIFIER "v",   FLAG_IDENTIFIER FLAG_IDENTIFIER "version",    "Shows version."},
    {SET_PRIORITY_FILTER,     FLAG_IDENTIFIER "p=1", FLAG_IDENTIFIER FLAG_IDENTIFIER "priority=1", "Sets priority filter to true."},
    {UNSET_PRIORITY_FILTER,   FLAG_IDENTIFIER "p=0", FLAG_IDENTIFIER FLAG_IDENTIFIER "priority=0", "Sets priority filter to false."},
    {SET_TITLE_NAME_FILTER,   FLAG_IDENTIFIER "t=1", FLAG_IDENTIFIER FLAG_IDENTIFIER "title=1",    "Sets title name filter to true."},
    {UNSET_TITLE_NAME_FILTER, FLAG_IDENTIFIER "t=0", FLAG_IDENTIFIER FLAG_IDENTIFIER "title=0",    "Sets title name filter to false."},
    {SET_PRIORITY_LEVEL,      FLAG_IDENTIFIER "l",   FLAG_IDENTIFIER FLAG_IDENTIFIER "level",      "Sets level filter. Usage: -l <number>"},
    {ADD_TODO,                FLAG_IDENTIFIER "a",   FLAG_IDENTIFIER FLAG_IDENTIFIER "add",        "Adds new todo to the " TODO_FILE_NAME " file. Usage: -a <priority> <todo_title>"},
    {REMOVE_TODO,             FLAG_IDENTIFIER "r",   FLAG_IDENTIFIER FLAG_IDENTIFIER "remove",     "Removes todo from the " TODO_FILE_NAME " file. Usage: -r <title_name>"}
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

static char* strip(const char *str) {
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

static int get_number_length(int num) {
    if (!num) return 1; 

    int length = 0;

    if (num < 0)  num = -num;

    while (num) { num /= 10; length++; }

    return length;
}

static char* read_file(const char *file_name) {
    if(!file_name) return NULL;

    FILE* file = fopen(file_name, "rb");
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

    char* buffer = malloc(file_size + 1);
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

static bool starts_with(const char* str, const char* prefix) {
    size_t prefix_len = strlen(prefix);

    return !strncmp(str, prefix, prefix_len);
}

static int find_nth_occurrence(const char *str, char ch, int n) {
    if(!n) return -1;

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

static int is_num(const char* str) {
    const char* buf = str;

    while(*buf) {
        if(!isdigit(*buf)) {
            if(!isspace(*buf) && isalpha(*buf)) return 0; 
            break;
        }

        ++buf;
    }

    return buf-str;
}

static void print_until_symbol(const char *str, char symbol) {
    const char *pos = strchr(str, symbol);

    if (pos) {
        printf("%.*s", (int)(pos-str), str);

        return;
    }

    printf("%s", str);
}

static inline void show_error(const char* str, const char* processed_str, int line_number) {
    int offset = find_nth_occurrence(str, '\n', line_number - 1);

    printf("%d | ", line_number); 
    print_until_symbol(str + offset + 1, '\n'); 
    printf("\n"); 

    offset = processed_str - str - offset + get_number_length(line_number) + 2; 
    while(offset-->0) printf(" "); 
    printf("^\n");
}

static inline void new_todo(todo_t** todos, int* todo_index) {
    if(!todos) return;

    if(!*todos) *todos = malloc(sizeof(todo_t)); 
    else *todos = realloc(*todos, sizeof(todo_t) * (*todo_index + 1));

    memset(*todos+*todo_index, 0, sizeof(todo_t));
}

static inline void skip_space(const char** str, int* line_number) {
    if(!str) return;

    while(isspace(**str) && **str) { 
        if(**str == '\n') *line_number += 1;

        *str += 1;
    } 
}

static void clear_todos(todo_t* todos) {
    if(!todos) return;

    for(int i = 0; todos[i].priority; i++) {
        free(todos[i].title);
        free(todos[i].description);
    }

    free(todos);
}

static todo_t* parse_todos(const char* str) {
    if(!str) return NULL;

    int todo_index = 0;
    todo_t* todos = NULL;

    int line_number = 1, token_len = 0;
    const char* buf = str;

    while(*buf) {
        skip_space(&buf, &line_number);

        token_len = 0;

        if(starts_with(buf, "TODO") && *buf) {
            new_todo(&todos, &todo_index);

            buf += strlen("TODO"); 

            skip_space(&buf, &line_number);

            if(*buf != ':') {
                show_error(str, buf, line_number);

                printf("Error: expected ':'.\n");
                exit(1);
            }

            ++buf;
            
            skip_space(&buf, &line_number);

            token_len = is_num(buf);
            if(!token_len) {
                show_error(str, buf, line_number);

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
                show_error(str, buf, line_number);

                printf("Error: converting priority to number.\n");
                exit(1);
            }

            if (*endptr) {
                show_error(str, buf, line_number);

                printf("Error: trailing characters after priority: %s.\n", endptr);
                exit(1);
            }

            if(priority <= 0) {
                show_error(str, buf, line_number);

                printf("Error: priority is less or equal to 0.\n");
                exit(1);
            }

            todos[todo_index].priority = priority;

            free(pr_str);

            buf += token_len;

            skip_space(&buf, &line_number);

            if(*buf != '"') {
                show_error(str, buf, line_number);

                printf("Error: expected starting '\"'.\n");
                exit(1);
            }

            ++buf;

            size_t title_len = 0;
            while(*buf && *buf != '"') {
                if(*buf == '\n') ++line_number;

                ++title_len; 
                ++buf;
            }

            if(!title_len) {
                show_error(str, buf, line_number);

                printf("Error: title is empty.\n");
                exit(1);
            }

            if(*buf != '"') {
                show_error(str, buf, line_number);

                printf("Error: expected ending '\"'.\n");
                exit(1);
            }

            ++buf; 

            todos[todo_index].title = malloc(sizeof(char)*(title_len+1));
            memcpy(todos[todo_index].title, buf-title_len-1, title_len);
            todos[todo_index].title[title_len] = 0;

            skip_space(&buf, &line_number);

            if(!starts_with(buf, "{{")) {
                show_error(str, buf, line_number);

                printf("Error: expected '{{'.\n");
                exit(1);
            }
            
            buf += strlen("{{");

            size_t description_len = 0;
            while(*buf && !starts_with(buf, "}}")) {
                if(*buf == '\n') ++line_number;
                ++description_len; 
                ++buf;
            }

            if(!starts_with(buf, "}}")) {
                show_error(str, buf, line_number);

                printf("Error: expected ending '}}'.\n");
                exit(1);
            }
            
            buf += strlen("}}");

            if(description_len) {
                todos[todo_index].description = malloc(sizeof(char)*(description_len+1));
                memcpy(todos[todo_index].description, buf-description_len-2, description_len);
                todos[todo_index].description[description_len] = 0;

                char* trimmed_description = strip(todos[todo_index].description);

                free(todos[todo_index].description);

                todos[todo_index].description = trimmed_description;
            }

            ++todo_index;

            skip_space(&buf, &line_number);

            continue;
        } 

        show_error(str, buf, line_number);

        printf("Error: expected '%s' statement.\n", "TODO");
        exit(1);
    }

    new_todo(&todos, &todo_index);

    return todos;
}

static void sort_todos_by_title_name(todo_t* todos) {
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

static void sort_todos_by_priority(todo_t* todos) {
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

static todo_t* get_todos() {
    char* file_content = read_file(CONFIG.todo_file_name);
    if(!file_content) return NULL;

    todo_t* todos = parse_todos(file_content);

    free(file_content);
    return todos;
}

static void print_todos(todo_t* todos) {
    if(!todos) return;

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

static void show_version() {
    printf("%s version(%s)\n", NAME, VERSION);
}

static void show_help() {
    printf("%s version(%s)\n", NAME, VERSION);

    printf("Flags:\n");
    for(size_t i = 0; i < sizeof(FLAGS) / sizeof(FLAGS[0]); i++) {
        printf("%4s or %-12s - %-64s\n", 
                FLAGS[i].flag_short_str, 
                FLAGS[i].flag_long_str, 
                FLAGS[i].description); 
    }
}

static bool does_file_exist(const char* file_name) {
    FILE* file = fopen(file_name, "r");

    return file ? true : false;
}

static void replace_escape_sequences(char* str) {
    char *src = str;
    char *dst = str;

    while (*src) {
        if (*src == '\\' && *(src + 1)) {
            switch (*(src + 1)) {
                case 'n':
                    *dst = '\n';
                    src++;
                    break;

                case 't':
                    *dst = '\t';
                    src++;
                    break;

                case '\\':
                    *dst = '\\';
                    src++;
                    break;

                case '"':
                    *dst = '"';
                    src++;
                    break;

                case 'r':
                    *dst = '\r';
                    src++;
                    break;

                default:
                    *dst = *src;
                    break;
            }
        } else {
            *dst = *src;
        }

        src++;
        dst++;
    }

    *dst = 0; 
}

static void add_todo(todo_t* todo) {
    if(!todo) return;

    if(does_file_exist(CONFIG.todo_file_name)) {
        todo_t* todos = get_todos();    

        bool is_found = false;

        for(int i = 0; todos[i].priority; i++) {
            if(!strcmp(todo->title, todos[i].title)) {
                is_found = true;
                break;
            }    
        }

        if(is_found) {
            printf("Error: todo with title '%s' is already in todos.\n", todo->title);

            clear_todos(todos);
            exit(1);
        }

        clear_todos(todos);
    }

    FILE* todo_file = fopen(CONFIG.todo_file_name, "a+");  

    if(todo->description) {
        fprintf(todo_file, "%s:%ld \"%s\" {{\n  %s\n}}\n", "TODO", todo->priority, todo->title, todo->description);
    } else {
        fprintf(todo_file, "%s:%ld \"%s\" {{}}\n", "TODO", todo->priority, todo->title);
    }

    fclose(todo_file);
}

static void remove_substring(char* str, const char* sub) {
    char* pos = strstr(str, sub); 

    if (pos != NULL) {
        size_t len = strlen(sub);
        memmove(pos, pos + len, strlen(pos + len) + 1); 
    }
}

int find_substring_backwards(const char* start_pos, const char* str, const char* sub) {
    size_t sub_len = strlen(sub);

    if (start_pos < str || sub_len > (size_t)(start_pos - str + 1)) return -1;

    for (const char *pos = start_pos; pos >= str; pos--) {
        if (!strncmp(pos, sub, sub_len)) {
            return pos - str;  
        }
    }

    return -1;
}

static int find_strict_substring(const char *str, const char *substr) {
    int substr_len = strlen(substr);
    int str_len = strlen(str);
    
    for (int i = 0; i <= str_len - substr_len; i++) {
        if (strncmp(&str[i], substr, substr_len) == 0) {
            if (i + substr_len == str_len || !isalnum((unsigned char)str[i + substr_len])) {
                return i;
            }
        }
    }

    return -1; 
}

static void remove_todo(const char* title_name) {
    if(!title_name) return;

    todo_t* todos = get_todos();    
    if(!todos) {
        exit(1);
    }

    bool is_found = false;
    for(int i = 0; todos[i].priority; i++) {
        if(!strcmp(title_name, todos[i].title)) {
            is_found = true;
            break;
        }    
    }

    if(!is_found) {
        printf("Error: todo with title '%s' is not found.\n", title_name);

        clear_todos(todos);
        exit(1);
    }

    char* file_content = read_file(CONFIG.todo_file_name);

    int start = find_substring_backwards(file_content + find_strict_substring(file_content, title_name), file_content, "TODO"), 
        len = 0;

    while(!starts_with(file_content+start+len, "}}") && *(file_content+start+len)) len++;
    len+=2;

    if(*(file_content+start+len) == '\n') ++len;

    char* remove_str = malloc(sizeof(char) * (len+1));
    strncpy(remove_str, file_content+start, len);
    remove_str[len] = 0;

    remove_substring(file_content, remove_str);

    FILE* todo_file = fopen(CONFIG.todo_file_name, "w");
    fprintf(todo_file, "%s", file_content);

    free(file_content);
    clear_todos(todos);
}

static int execute_flag(flag_action_t action, char* argv[]) {
    char* flag = *argv;
    int offset = 0; 

    ++argv;

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
            if(!*argv) {
                printf("Error: flag '%s' requires number value.\n", flag);

                exit(1);
            }

            if(!is_num(*argv)) {
                printf("Error: '%s' is invalid priority.\n", *argv);

                exit(1);
            }
            
            ++offset;

            CONFIG.priority_level = atoi(*argv);
            break;


        case ADD_TODO:
            if(!*argv) {
                printf("Error: flag '%s' requires 2 necessary values. Description is optional.\nUsage: -a <priority(number)> <title_name(string)> [description]", flag);

                exit(1);
            }

            todo_t todo = {0};

            if(!is_num(*argv)) {
                printf("Error: '%s' is invalid priority.\n", *argv);

                exit(1);
            }
            
            todo.priority = atoi(*argv);

            ++argv;

            if(!*argv) {
                printf("Error: flag '%s' requires title_name for todo.\n", flag);

                exit(1);
            }

            todo.title = malloc(sizeof(char) * (strlen(*argv)+1));
            strncpy(todo.title, *argv, strlen(*argv) + 1);

            replace_escape_sequences(todo.title);

            ++argv;

            if(*argv) {
                todo.description = malloc(sizeof(char) * (strlen(*argv)+1));
                strncpy(todo.description, *argv, strlen(*argv) + 1);

                replace_escape_sequences(todo.description);
            }

            add_todo(&todo);

            exit(0);
            
            
        case REMOVE_TODO:
            if(!*argv) {
                printf("Error: flag '%s' requires todo title name.\n", flag);

                exit(1);
            }

            remove_todo(*argv);

            exit(0);

        default:
            printf("Error: '%d' is undefined flag action.\n", action);
            exit(1);
    }

    return offset;
}


static void check_flags(int argc, char* argv[]) {
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

    todo_t* todos = get_todos();

    print_todos(todos);

    clear_todos(todos);

    return 0;
}
