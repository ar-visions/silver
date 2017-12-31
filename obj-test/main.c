#include <obj/obj.h>
#include <obj-math/math.h>
//#include <obj-ui/ui.h>

int main() {
    class_init();
    Test t = new(Test);
    Vec2 v2 = vec2(1,2);
    t->prop1 = 10;
    t->prop2 = 20;
    v2->test = t;
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

    list_push(v2->list, t);

    String json = call(v2, to_json);
    printf("json = %s\n", json->buffer);

    Vec2 v2_f = from_json(Vec2, json);

    String json_2 = call(v2_f, to_json);
    printf("json_2 = %s\n", json_2->buffer);
    return 0;
}