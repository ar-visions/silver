
class Data {
    override void free(C);
    override String to_string(C);
    override C from_string(String);
    C with_size(uint);
    C with_bytes(uint8 *, uint);
    void get_vector(C, void **, size_t, uint *);
    uint8 *bytes;
    uint length;
};

#define data_vector(S,P,T,C) ((S ? call(S, get_vector, &(P), sizeof(T), &(C)) : NULL)
