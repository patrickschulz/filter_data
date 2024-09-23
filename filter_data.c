#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 1024

struct yvalue {
    union {
        double d;
        int i;
        const char* str;
    };
    enum {
        REAL,
        INTEGER,
        STRING
    } type;
};

struct xydatum {
    double x;
    struct yvalue y;
    int deleted;
};

struct data {
    struct xydatum* data;
    size_t length;
    size_t capacity;
};

typedef int (*filter_func_0_arg)(struct xydatum*, size_t index);
typedef int (*filter_func_1_arg)(struct xydatum*, size_t index, void*);
typedef int (*filter_func_2_arg)(struct xydatum*, size_t index, void*, void*);

struct filter {
    union {
        filter_func_0_arg func_0_arg;
        filter_func_1_arg func_1_arg;
        filter_func_2_arg func_2_arg;
    };
    enum {
        FILTER_0_ARG,
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

static struct filterlist* create_filterlist(void)
{
    struct filterlist* list = malloc(sizeof(*list));
    list->filter = NULL;
    list->size = 0;
    return list;
}

static void _destroy_filter(struct filter* filter)
{
    switch(filter->type)
    {
        case FILTER_0_ARG:
            // nothing to do
            break;
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

static void destroy_filterlist(struct filterlist* filterlist)
{
    for(size_t i = 0; i < filterlist->size; ++i)
    {
        _destroy_filter(filterlist->filter[i]);
    }
    free(filterlist->filter);
    free(filterlist);
}

int _apply_filter(struct xydatum* datum, size_t index, struct filter* filter)
{
    switch(filter->type)
    {
        case FILTER_0_ARG:
            return filter->func_0_arg(datum, index);
        case FILTER_1_ARG:
            return filter->func_1_arg(datum, index, filter->arg1);
        case FILTER_2_ARG:
            return filter->func_2_arg(datum, index, filter->arg1, filter->arg2);
    }
    return 0;
}

static void _next_separator(const char* str, const char* separator, const char** startpos, const char** endpos, int* eol)
{
    const char* pos = str;
    while(1)
    {
        const char* seppos = separator;
        if((!*pos) || (*pos == '\n'))
        {
            *eol = 1;
            *startpos = pos; // used as terminator for string parsing
            break;
        }
        while(*pos && *seppos && (*pos == *seppos))
        {
            *startpos = pos;
            ++pos;
            ++seppos;
        }
        if(!*seppos)
        {
            if(!*pos)
            {
                *eol = 1;
            }
            break;
        }
        ++pos;
    }
    *endpos = pos - 1;
}

static double _str_to_number(const char* str, const char* endptr)
{
    char* tmp = malloc(endptr - str + 1);
    const char* ptr = str;
    char* tptr = tmp;
    while(ptr != endptr)
    {
        *tptr = *ptr;
        ++ptr;
        ++tptr;
    }
    *tptr = 0;
    double num = atof(tmp);
    free(tmp);
    return num;
}

static struct data* read_data(const char* filename, size_t skip, unsigned int xindex, unsigned int yindex, const char* separator, struct filterlist* filterlist)
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
    size_t numlines = 0;
    while(1) /* iterate lines */
    {
        const char* s = fgets(buf, BUFSIZE, file);
        ++numlines;
        if((numlines - 1) < skip)
        {
            continue;
        }
        if(!s)
        {
            break;
        }
        const char* str = buf;
        size_t index = 0;
        if(data->length == data->capacity)
        {
            data->capacity *= 2;
            data->data = realloc(data->data, sizeof(*data->data) * data->capacity);
        }
        struct xydatum* datum = data->data + data->length;
        datum->deleted = 0;
        int advance = 1;
        while(1) /* parse line */
        {
            int eol = 0;
            const char* startpos;
            const char* endpos;
            _next_separator(str, separator, &startpos, &endpos, &eol);
            if(index == xindex)
            {
                datum->x = _str_to_number(str, startpos);
            }
            if(index == yindex)
            {
                datum->y.d = _str_to_number(str, startpos);
                datum->y.type = REAL;
            }
            if(eol)
            {
                for(size_t i = 0; i < filterlist->size; ++i)
                {
                    advance = advance && _apply_filter(datum, index, filterlist->filter[i]);
                }
                break;
            }
            str = endpos + 1;
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

static int _scale_x(struct xydatum* datum, size_t index, void* factor)
{
    (void)index;
    datum->x *= *((double*)factor);
    return 1;
}

static int _scale_y(struct xydatum* datum, size_t index, void* factor)
{
    (void)index;
    datum->y.d *= *((double*)factor);
    return 1;
}

static int _x_min(struct xydatum* datum, size_t index, void* min)
{
    (void)index;
    if(datum->x < *((double*)min))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

static int _x_max(struct xydatum* datum, size_t index, void* min)
{
    (void)index;
    if(datum->x > *((double*)min))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

static int _shift_x(struct xydatum* datum, size_t index, void* shift)
{
    (void)index;
    datum->x += *((double*)shift);
    return 1;
}

static int _shift_y(struct xydatum* datum, size_t index, void* shift)
{
    (void)index;
    datum->y.d += *((double*)shift);
    return 1;
}

static int _y_is_integer(struct xydatum* datum, size_t index)
{
    (void)index;
    datum->y.i += (int) datum->y.d;
    datum->y.type = INTEGER;
    return 1;
}

static int _every_nth(struct xydatum* datum, size_t index, void* nthp)
{
    (void)index;
    (void)datum;
    int nth = *((int*)nthp);
    if((index % nth) == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static struct filter* _create_filter_0_arg(filter_func_0_arg func)
{
    struct filter* filter = malloc(sizeof(*filter));
    filter->func_0_arg = func;
    filter->type = FILTER_0_ARG;
    return filter;
}

static struct filter* _create_filter_1_arg(filter_func_1_arg func, void* arg)
{
    struct filter* filter = malloc(sizeof(*filter));
    filter->func_1_arg = func;
    filter->type = FILTER_1_ARG;
    filter->arg1 = arg;
    return filter;
}

static struct filter* _create_filter_2_arg(filter_func_2_arg func, void* arg1, void* arg2)
{
    struct filter* filter = malloc(sizeof(*filter));
    filter->func_2_arg = func;
    filter->type = FILTER_2_ARG;
    filter->arg1 = arg1;
    filter->arg2 = arg2;
    return filter;
}

static void _append_filter(struct filterlist* filterlist, struct filter* filter)
{
    filterlist->filter = realloc(filterlist->filter, (filterlist->size + 1) * sizeof(*filterlist->filter));
    filterlist->filter[filterlist->size] = filter;
    ++filterlist->size;
}

static void _usage(void)
{
    puts("Filter simulation data");
    puts("    <filename> (string)                  filename of data");
    puts("    <xindex> (number)                    index of x data");
    puts("    <yindex> (number)                    index if y data");
    puts("    --y-is-integer                       y values are integers, not real numbers");
    puts("    -s,--separator (default \",\")         input data separator");
    puts("    -S,--print-separator (default \" \")   output data separator");
    puts("    -k,--skip                            number of initial lines (header) to skip");
    puts("    -r,--remove-redundant-points         remove points that add lines to output data, but not numerically (e.g. 42.2 and 42.2 as x coordinates for consecutive points)");
    puts("    --sample                             take samples of the input data. Use with --sample-start and --sample-interval");
    puts("    --sample                             take samples of the input data. Use with --sample-start and --sample-interval");
    puts("    --sample-start                       start of sampling (x-coordinate)");
    puts("    --sample-interval                    interval of sampling (x-coordinate)");
    //puts("    -f,--filter                          filter data (remove redundant points)");
    //puts("    -n,--every-nth (default 1)           only keep every nth point");
    //puts("    --as-string                          don't do any numerical processing on y");
    //puts("    --digital                            interpret data as digital data, use with --threshold");
    //puts("    --threshold (default 0.0)            threshold for digital data");
    //puts("    --xstart (default -1e32)             minimum x datum");
    //puts("    --xend (default 1e32)                maximum x datum");
    //puts("    --xscale (default 1)                 factor for scaling x data");
    //puts("    --yscale (default 1)                 factor for scaling y data");
    //puts("    --y-is-multibit                      map binary data (e.g. 101) to decimal (e.g. 5) (implies --as-string)");
    //puts("    --ymin (default 0)                   minimum value for y map range");
    //puts("    --ymax (default 0)                   maximum value for y map range");
    puts("    --xprecision                         decimal digits for x data");
    //puts("    --yprecision (default 1e-3)          decimal digits for y data");
    //puts("    --xshift (default 0)                 shift x values");
    //puts("    --x-relative                         output relative x values");
}

static int _arg_is(const char* arg, const char* short_arg, const char* long_arg)
{
    if(short_arg && strcmp(arg, short_arg) == 0)
    {
        return 1;
    }
    if(long_arg && strcmp(arg, long_arg) == 0)
    {
        return 1;
    }
    return 0;
}

static int _has_arg(int argc, char** argv, const char* short_arg, const char* long_arg)
{
    for(int i = 1; i < argc; ++i)
    {
        if(_arg_is(argv[i], short_arg, long_arg))
        {
            return 1;
        }
    }
    return 0;
}

static size_t _get_skiplines(int argc, char** argv)
{
    for(int i = 1; i < argc; ++i)
    {
        if(_arg_is(argv[i], "-k", "--skip"))
        {
            if(i < argc - 1)
            {
                return atoi(argv[i + 1]);
            }
        }
    }
    return 0;
}

static int _get_xdecimals(int argc, char** argv)
{
    for(int i = 1; i < argc; ++i)
    {
        if(_arg_is(argv[i], NULL, "--xprecision"))
        {
            if(i < argc - 1)
            {
                return atoi(argv[i + 1]);
            }
        }
    }
    return 16;
}

static int _get_ydecimals(int argc, char** argv)
{
    for(int i = 1; i < argc; ++i)
    {
        if(_arg_is(argv[i], NULL, "--yprecision"))
        {
            if(i < argc - 1)
            {
                return atoi(argv[i + 1]);
            }
        }
    }
    return 16;
}

static double _get_sample_start(int argc, char** argv)
{
    for(int i = 1; i < argc; ++i)
    {
        if(_arg_is(argv[i], NULL, "--sample-start"))
        {
            if(i < argc - 1)
            {
                return atof(argv[i + 1]);
            }
        }
    }
    return 0.0;
}

static double _get_sample_interval(int argc, char** argv)
{
    for(int i = 1; i < argc; ++i)
    {
        if(_arg_is(argv[i], NULL, "--sample-interval"))
        {
            if(i < argc - 1)
            {
                return atof(argv[i + 1]);
            }
        }
    }
    return 0.0;
}

static char* _get_separator(int argc, char** argv, const char* default_sep)
{
    for(int i = 1; i < argc; ++i)
    {
        if(_arg_is(argv[i], "-s", "--separator"))
        {
            if(i < argc - 1)
            {
                return strdup(argv[i + 1]);
            }
        }
    }
    return strdup(default_sep);
}

static char* _get_print_separator(int argc, char** argv, const char* default_sep)
{
    for(int i = 1; i < argc; ++i)
    {
        if(_arg_is(argv[i], "-S", "--print-separator"))
        {
            if(i < argc - 1)
            {
                return strdup(argv[i + 1]);
            }
        }
    }
    return strdup(default_sep);
}

static int _is_equal(double x1, double x2, double precision)
{
    return fabs(x1 - x2) < precision;
}

static void _remove_redundant_points(struct data* data, int xdecimals, int ydecimals)
{
    // x-filter
    double xprecision = pow(10, -xdecimals);
    // find redundant points and mark the as deleted
    struct xydatum* lastdatum = data->data + 0;
    for(size_t i = 1; i < data->length; ++i)
    {
        struct xydatum* datum = data->data + i;
        if(_is_equal(datum->x, lastdatum->x, xprecision))
        {
            datum->deleted = 1;
        }
        else
        {
            lastdatum = datum;
        }
    }

    // y-filter
    double yprecision = pow(10, -ydecimals);
    // find redundant points and mark the as deleted
    lastdatum = data->data + 0;
    for(size_t i = 1; i < data->length; ++i)
    {
        struct xydatum* datum = data->data + i;
        int advance = 1;
        switch(lastdatum->y.type)
        {
            case REAL:
                advance = !_is_equal(datum->y.d, lastdatum->y.d, yprecision);
                break;
            case INTEGER:
                advance = !(datum->y.i == lastdatum->y.i);
                break;
            case STRING:
                advance = (strcmp(datum->y.str, lastdatum->y.str) != 0);
                break;
        }
        if(advance)
        {
            lastdatum = datum;
        }
        else
        {
            datum->deleted = 1;
        }
    }
}

static void _sample_data(struct data* data, double samplestart, double sampleinterval)
{
    // FIXME: assumes monotone data, implement check?
    size_t sampleindex = 0;
    for(size_t i = 0; i < data->length; ++i)
    {
        struct xydatum* datum = data->data + i;
        if(datum->x < samplestart)
        {
            datum->deleted = 1;
        }
        else
        {
            size_t index = (datum->x - samplestart) / sampleinterval;
            if(index > sampleindex)
            {
                sampleindex = index;
            }
            else
            {
                datum->deleted = 1;
            }
        }
    }
}

int main(int argc, char** argv)
{
    if((argc == 2) && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)))
    {
        _usage();
        return 0;
    }
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

    char* separator = _get_separator(argc, argv, ",");
    char* print_separator = _get_print_separator(argc, argv, " ");

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
        else if(strcmp(argv[i], "--yscale") == 0)
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
        else if(strcmp(argv[i], "--xmin") == 0)
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
        else if(strcmp(argv[i], "--xmax") == 0)
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
        else if(strcmp(argv[i], "--xshift") == 0)
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
        else if(strcmp(argv[i], "--yshift") == 0)
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
        else if(strcmp(argv[i], "--y-is-integer") == 0)
        {
            struct filter* filter = _create_filter_0_arg(_y_is_integer);
            _append_filter(filterlist, filter);
        }
        else if(strcmp(argv[i], "--every-nth") == 0)
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
            struct filter* filter = _create_filter_1_arg(_every_nth, arg);
            _append_filter(filterlist, filter);
            ++i;
        }
        /*
        else
        {
            fprintf(stderr, "unknown option: '%s'\n", argv[i]);
            return 1;
        }
        */
        ++i;
    }

    int xdecimals = _get_xdecimals(argc, argv);
    int ydecimals = _get_ydecimals(argc, argv);

    // read data
    size_t skip = _get_skiplines(argc, argv);
    struct data* data = read_data(filename, skip, xindex, yindex, separator, filterlist);
    // FIXME: move filter out of data read-in? efficiency?

    // sample data
    if(_has_arg(argc, argv, NULL, "--sample"))
    {
        double samplestart = _get_sample_start(argc, argv);
        double sampleinterval = _get_sample_interval(argc, argv);
        _sample_data(data, samplestart, sampleinterval);
    }

    // post-process data
    if(_has_arg(argc, argv, "-r", "--remove-redundant-points"))
    {
        _remove_redundant_points(data, xdecimals, ydecimals);
    }

    // print data
    for(size_t i = 0; i < data->length; ++i)
    {
        if(!(data->data + i)->deleted)
        {
            switch((data->data + i)->y.type)
            {
                case REAL:
                    printf("%.*f%s%.*f\n", xdecimals, (data->data + i)->x, print_separator, ydecimals, (data->data + i)->y.d);
                    break;
                case INTEGER:
                    printf("%.*f%s%d\n", xdecimals, (data->data + i)->x, print_separator, (data->data + i)->y.i);
                    break;
                case STRING:
                    printf("%.*f%s%s\n", xdecimals, (data->data + i)->x, print_separator, (data->data + i)->y.str);
                    break;
            }
        }
    }
    free(data->data);
    free(data);
    free(separator);
    free(print_separator);
    destroy_filterlist(filterlist);
    return 0;
}

