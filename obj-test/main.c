#include <obj/obj.h>
#include <obj-math/math.h>
//#include <obj-ui/ui.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define randnum(min, max) \
    ((rand() % (int)(((max) + 1) - (min))) + (min))

int main() {
    class_init();
    Vec2 v2 = vec2(1,2);
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

    String json = call(v2, to_json);
    printf("json = %s\n", json->buffer);

    Vec2 v2_f = from_json(Vec2, json);

    String json_2 = call(v2_f, to_json);
    printf("json_2 = %s\n", json_2->buffer);

    List list = new(List);
    for (int ii = 0; ii < 70000; ii++) {
        for (int i = 0; i < 1; i++) {
            String test = string("test!");
            call(list, push, (Base)test);
        }
        /*for (int i = 0; i < 1000; i++) {
            int rem = randnum(0, list->list.count - 1);
            Base item = call(list, object_at, rem);
            if (!item) {
                printf("shouldnt happen\n");
                exit(1);
            }
            call(list, remove, item);
        }*/
    }
    printf("list block count: %d\n", list->list.block_count);
    return 0;
}