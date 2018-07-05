#include <obj/obj.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define randnum(min, max) \
    ((rand() % (int)(((max) + 1) - (min))) + (min))

int main() {
    class_init();
    List test_list = new(List);
    Boolean meta = prop_meta(test_list, "min_block_size", "test", Boolean);
    print(test_list, "list meta = %p", meta);

    char *test_bytes = "leasure. leasure leasure leasure leasure leasure leasure leasure leasure leasure leasure";
    Data data = new(Data);
    data->bytes = (uint8 *)test_bytes;
    data->length = strlen(test_bytes);
    String data_str = call(data, to_string);

    printf("data string: %s\n", data_str->buffer);

    Data leasure_data = class_call(Data, from_string, data_str);
    
    printf("leasure_data: %d %d %d\n", leasure_data->bytes[0], leasure_data->bytes[1], leasure_data->bytes[2]);

    String test_str = call(leasure_data, to_string);

    printf("test string: %s\n", test_str->buffer);

    printf("leasure: %s\n", leasure_data->bytes);

    List containing_meta = props_with_meta("test", List);

    List strings_list = list_of(List, Int16, string("65537"), string("32"));
    Int16 str;
    each(strings_list, str) {
        print(str, "str = %p", str);
    }
    return 0;
}