#include "a-memory-library/aml_pool.h"
#include "a-memory-library/aml_buffer.h"
#include "the-io-library/io.h"
#include "the-io-library/io_in.h"
#include "the-io-library/io_out.h"

io_out_t *open_sorted_file(const char *filename) {
    io_out_options_t opts;
    io_out_options_init(&opts);
    io_out_options_format(&opts, io_prefix());
    io_out_options_buffer_size(&opts, 32*1024*1024);

    io_out_ext_options_t ext_opts;
    io_out_ext_options_init(&ext_opts);
    io_out_ext_options_compare(&ext_opts, io_compare_uint32_t, NULL);
    io_out_ext_options_use_extra_thread(&ext_opts);

    return io_out_ext_init(filename, &opts, &ext_opts);
}

static io_in_t *open_interactions_text(const char *filename) {
    io_in_options_t opts;
    io_in_options_init(&opts);
    io_in_options_format(&opts, io_delimiter('\n'));

    return io_in_init(filename, &opts);
}

void transform_text(const char *filename, const char *output) {
    io_in_t *in = open_interactions_text(filename);
    io_out_t *out = open_sorted_file(output);

    io_record_t *r;
    if((r=io_in_advance(in))== NULL) {
        io_out_destroy(out);
        return;
    }

    aml_pool_t *pool = aml_pool_init(16384);
    aml_buffer_t *bh = aml_buffer_init(1024);
    int user_field = -1;
    int item_field = -1;
    int timestamp_field = -1;
    int event_type_field = -1;
    int max_field = 0;

    size_t num_fields = 0;
    if(r->length > 0 && r->record[r->length-1] == '\r')
        r->record[r->length-1] = 0;
    char **fields = aml_pool_split(pool, &num_fields, ',', r->record);
    for( size_t i=0; i<num_fields; i++ ) {
        if(!strcasecmp(fields[i], "ITEM_ID"))
            item_field = i;
        else if(!strcasecmp(fields[i], "USER_ID"))
            user_field = i;
        else if(!strcasecmp(fields[i], "EVENT_TYPE"))
            event_type_field = i;
        else if(!strcasecmp(fields[i], "TIMESTAMP"))
            timestamp_field = i;
        else
            continue;
        max_field = i+1;
    }
    if(item_field == -1)
        printf( "ERROR: ITEM_ID must be present in input!\n");
    if(user_field == -1)
        printf( "ERROR: USER_ID must be present in input!\n");
    if(timestamp_field == -1)
        printf( "ERROR: TIMESTAMP must be present in input!\n");

    while((r=io_in_advance(in)) != NULL) {
        aml_pool_clear(pool);
        if(r->length > 0 && r->record[r->length-1] == '\r')
            r->record[r->length-1] = 0;
        fields = aml_pool_split(pool, &num_fields, ',', r->record);
        if(num_fields < max_field) {
            printf( "WARN (num_fields: %zu < %d): %s\n", num_fields, max_field, r->record );
            continue;
        }
        char *event_type = (char *)"";
        if(event_type_field != -1)
            event_type = fields[event_type_field];
        char *user = fields[user_field];
        char *item = fields[item_field];
        uint32_t timestamp = 0;
        if(sscanf(fields[timestamp_field], "%u", &timestamp) != 1) {
            printf( "WARN (timestamp: %s): %s\n", fields[timestamp_field], r->record );
            continue;
        }

        aml_buffer_set(bh, &timestamp, sizeof(timestamp));
        aml_buffer_append(bh, user, strlen(user)+1);
        aml_buffer_append(bh, item, strlen(item)+1);
        aml_buffer_append(bh, event_type, strlen(event_type)+1);
        io_out_write_record(out, aml_buffer_data(bh), aml_buffer_length(bh));
    }
    aml_pool_destroy(pool);
    aml_buffer_destroy(bh);
    io_in_destroy(in);
    io_out_destroy(out);
}

int main( int argc, char *argv[]) {
    transform_text(argv[1], argv[2]);
    return 0;
}

