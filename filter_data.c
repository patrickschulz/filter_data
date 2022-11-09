#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUFSIZE 1024

struct yvalue {
    union {
        double d;
        int i;
        const char* str;
    };
    enum {
        DOUBLE,
        INT,
        STRING
    } type;
};

struct xydatum {
    double x;
    struct yvalue y;
};

struct data {
    struct xydatum* data;
    size_t length;
    size_t capacity;
};

typedef int (*filter_func_1_arg)(struct xydatum*, void*);
typedef int (*filter_func_2_arg)(struct xydatum*, void*, void*);

struct filter {
    union {
        filter_func_1_arg func_1_arg;
        filter_func_2_arg func_2_arg;
    };
    enum {
        FILTER_1_ARG,
        FILTER_2_ARG
    } type;
    void* arg1;
    void* arg2;
};

struct filterlist {
    struct filter** filter;
    size_t size;
};

struct filterlist* create_filterlist(void)
{
    struct filterlist* list = malloc(sizeof(*list));
    list->filter = NULL;
    list->size = 0;
    return list;
}

void _destroy_filter(struct filter* filter)
{
    switch(filter->type)
    {
        case FILTER_1_ARG:
            free(filter->arg1);
            break;
        case FILTER_2_ARG:
            free(filter->arg1);
            free(filter->arg2);
            break;
    }
    free(filter);
}

void destroy_filterlist(struct filterlist* filterlist)
{
    for(size_t i = 0; i < filterlist->size; ++i)
    {
        _destroy_filter(filterlist->filter[i]);
    }
    free(filterlist->filter);
    free(filterlist);
}

int _apply_filter(struct xydatum* datum, struct filter* filter)
{
    switch(filter->type)
    {
        case FILTER_1_ARG:
            return filter->func_1_arg(datum, filter->arg1);
        case FILTER_2_ARG:
            return filter->func_2_arg(datum, filter->arg1, filter->arg2);
    }
    return 0;
}

static const char* _next_separator(const char* str, char separator, int* eol)
{
    const char* pos = str;
    while(1)
    {
        if((!*pos) || (*pos == '\n'))
        {
            *eol = 1;
            break;
        }
        if(*pos == separator)
        {
            break;
        }
        ++pos;
    }
    return pos;
}

struct data* read_data(const char* filename, unsigned int xindex, unsigned int yindex, char separator, struct filterlist* filterlist)
{
    FILE* file = fopen(filename, "r");
    if(!file)
    {
        fprintf(stderr, "filter_data: could not open file '%s'\n", filename);
        return NULL;
    }
    char buf[BUFSIZE];
    struct data* data = malloc(sizeof(*data));
    data->capacity = 1024;
    data->data = malloc(sizeof(*data->data) * data->capacity);
    data->length = 0;
    while(1) /* iterate lines */
    {
        const char* s = fgets(buf, BUFSIZE, file);
        if(!s)
        {
            break;
        }
        const char* str = buf;
        unsigned int index = 0;
        if(data->length == data->capacity)
        {
            data->capacity *= 2;
            data->data = realloc(data->data, sizeof(*data->data) * data->capacity);
        }
        struct xydatum* datum = data->data + data->length;
        int advance = 1;
        while(1) /* parse line */
        {
            int eol = 0;
            const char* pos = _next_separator(str, separator, &eol);
            if(index == xindex)
            {
                datum->x = atof(str);
            }
            if(index == yindex)
            {
                datum->y.d = atof(str);
                datum->y.type = DOUBLE;
            }
            if(eol)
            {
                for(size_t i = 0; i < filterlist->size; ++i)
                {
                    advance = advance && _apply_filter(datum, filterlist->filter[i]);
                }
                break;
            }
            str = pos + 1;
            ++index;
        }
        if(advance)
        {
            ++data->length;
        }
    }
    fclose(file);
    return data;
}

int _scale_x(struct xydatum* datum, void* factor)
{
    datum->x *= *((double*)factor);
    return 1;
}

int _scale_y(struct xydatum* datum, void* factor)
{
    datum->y.d *= *((double*)factor);
    return 1;
}

int _x_min(struct xydatum* datum, void* min)
{
    if(datum->x < *((double*)min))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

int _x_max(struct xydatum* datum, void* min)
{
    if(datum->x > *((double*)min))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

int _shift_x(struct xydatum* datum, void* shift)
{
    datum->x += *((double*)shift);
    return 1;
}

int _shift_y(struct xydatum* datum, void* shift)
{
    datum->y.d += *((double*)shift);
    return 1;
}

int _every_nth(struct xydatum* datum, void* nthp, void* countv)
{
    (void)datum;
    int nth = *((int*)nthp);
    if(nth == 0)
    {
        return 1;
    }
    int* countp = countv;
    int ret = 0;
    if(*countp == nth - 1)
    {
        ret = 1;
        *countp = 0;
    }
    else
    {
        ++(*countp);
    }
    return ret;
}

struct filter* _create_filter_1_arg(filter_func_1_arg func, void* arg)
{
    struct filter* filter = malloc(sizeof(*filter));
    filter->func_1_arg = func;
    filter->type = FILTER_1_ARG;
    filter->arg1 = arg;
    return filter;
}

struct filter* _create_filter_2_arg(filter_func_2_arg func, void* arg1, void* arg2)
{
    struct filter* filter = malloc(sizeof(*filter));
    filter->func_2_arg = func;
    filter->type = FILTER_2_ARG;
    filter->arg1 = arg1;
    filter->arg2 = arg2;
    return filter;
}

void _append_filter(struct filterlist* filterlist, struct filter* filter)
{
    filterlist->filter = realloc(filterlist->filter, (filterlist->size + 1) * sizeof(*filterlist->filter));
    filterlist->filter[filterlist->size] = filter;
    ++filterlist->size;
}

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        fputs("filter_data: no filename given\n", stderr);
        return 1;
    }
    if(argc < 3)
    {
        fputs("filter_data: no xindex given\n", stderr);
        return 1;
    }
    if(argc < 4)
    {
        fputs("filter_data: no yindex given\n", stderr);
        return 1;
    }
    const char* filename = argv[1];
    unsigned int xindex = atoi(argv[2]);
    unsigned int yindex = atoi(argv[3]);

    struct filterlist* filterlist = create_filterlist();

    char separator = ',';
    char* print_separator = strdup(",");

    int i = 4;
    while(i < argc)
    {
        if(strcmp(argv[i], "--xscale") == 0)
        {
            if(i + 1 >= argc)
            {
                fprintf(stderr, "%s\n", "--xscale: argument required");
                return 0;
            }
            double* arg = malloc(sizeof(*arg));
            *arg = atof(argv[i + 1]);
            struct filter* filter = _create_filter_1_arg(_scale_x, arg);
            _append_filter(filterlist, filter);
            ++i;
        }
        if(strcmp(argv[i], "--yscale") == 0)
        {
            if(i + 1 >= argc)
            {
                fprintf(stderr, "%s\n", "--yscale: argument required");
                return 0;
            }
            double* arg = malloc(sizeof(*arg));
            *arg = atof(argv[i + 1]);
            struct filter* filter = _create_filter_1_arg(_scale_y, arg);
            _append_filter(filterlist, filter);
            ++i;
        }
        if(strcmp(argv[i], "--xmin") == 0)
        {
            if(i + 1 >= argc)
            {
                fprintf(stderr, "%s\n", "--xmin: argument required");
                return 0;
            }
            double* arg = malloc(sizeof(*arg));
            *arg = atof(argv[i + 1]);
            struct filter* filter = _create_filter_1_arg(_x_min, arg);
            _append_filter(filterlist, filter);
            ++i;
        }
        if(strcmp(argv[i], "--xmax") == 0)
        {
            if(i + 1 >= argc)
            {
                fprintf(stderr, "%s\n", "--xmax: argument required");
                return 0;
            }
            double* arg = malloc(sizeof(*arg));
            *arg = atof(argv[i + 1]);
            struct filter* filter = _create_filter_1_arg(_x_max, arg);
            _append_filter(filterlist, filter);
            ++i;
        }
        if(strcmp(argv[i], "--xshift") == 0)
        {
            if(i + 1 >= argc)
            {
                fprintf(stderr, "%s\n", "--xshift: argument required");
                return 0;
            }
            double* arg = malloc(sizeof(*arg));
            *arg = atof(argv[i + 1]);
            struct filter* filter = _create_filter_1_arg(_shift_x, arg);
            _append_filter(filterlist, filter);
            ++i;
        }
        if(strcmp(argv[i], "--yshift") == 0)
        {
            if(i + 1 >= argc)
            {
                fprintf(stderr, "%s\n", "--yshift: argument required");
                return 0;
            }
            double* arg = malloc(sizeof(*arg));
            *arg = atof(argv[i + 1]);
            struct filter* filter = _create_filter_1_arg(_shift_y, arg);
            _append_filter(filterlist, filter);
            ++i;
        }
        if(strcmp(argv[i], "--every-nth") == 0)
        {
            if(i + 1 >= argc)
            {
                fprintf(stderr, "%s\n", "--every-nth: argument required");
                return 0;
            }
            int* arg = malloc(sizeof(*arg));
            *arg = atoi(argv[i + 1]);
            int* count = malloc(sizeof(*arg));
            *count = 0;
            struct filter* filter = _create_filter_2_arg(_every_nth, arg, count);
            _append_filter(filterlist, filter);
            ++i;
        }
        if(strcmp(argv[i], "--separator") == 0)
        {
            if(i + 1 >= argc)
            {
                fprintf(stderr, "%s\n", "--separator: argument required");
                return 0;
            }
            separator = argv[i + 1][0];
            ++i;
        }
        if(strcmp(argv[i], "--print-separator") == 0)
        {
            if(i + 1 >= argc)
            {
                fprintf(stderr, "%s\n", "--print-separator: argument required");
                return 0;
            }
            free(print_separator);
            print_separator = strdup(argv[i + 1]);
            ++i;
        }
        ++i;
    }

    struct data* data = read_data(filename, xindex, yindex, separator, filterlist);
    for(size_t i = 0; i < data->length; ++i)
    {
        printf("%f%s%f\n", (data->data + i)->x, print_separator, (data->data + i)->y.d);
    }
    free(data->data);
    free(data);
    free(print_separator);
    destroy_filterlist(filterlist);
    return 0;
}

/*
        -r,--reduce (default 0)              reduce number of points to this value (if > 0)
        -f,--filter                          filter data (remove redundant points)
        --as-string                          don't do any numerical processing on y
        --digital                            interpret data as digital data, use with --threshold
        --threshold (default 0.0)            threshold for digital data
        --y-is-multibit                      map binary data (e.g. 101) to decimal (e.g. 5) (implies --as-string)
        --ymin (default 0)                   minimum value for y map range 
        --ymax (default 0)                   maximum value for y map range 
        --xprecision (default 1e-3)          decimal digits for x data
        --yprecision (default 1e-3)          decimal digits for y data
        --x-relative                         output relative x values
*/
