#include <fstream>
#include <iostream>
#include <stdint.h>
#include <vector>
#include <map>
#include <string>
#include <list>
#include <set>

typedef std::map< int64_t, std::vector<int64_t> > index_t;
typedef std::pair< int64_t, std::vector<int64_t> > index_elem_t;

const size_t max_parts = 2;

class IndexBuilder
{

public:

    IndexBuilder(const char *input_file_name);
    ~IndexBuilder();

    void build_index(const char *output_file_name);
    void echo_index_file(const char *file_name);
 
private:

    std::set<int64_t> unique_word_ids;

    std::ifstream input_file;
    std::ifstream part_files[max_parts];

    size_t input_file_size;
    size_t size_threshold;
    size_t n_parts;

    size_t get_file_size(const char *file_name);
    index_t build_index_part(size_t part_i);
    void dump_index_part_to_file(index_t &index_part, size_t part_i);
    void append_doc_ids(int64_t target_word_id, 
                        std::list<int64_t> &doc_ids);

};


