// Linux + C++
// Endian: little
// command line arguments: <input file name> <output_file_name>

#include "index.h"

// get file size in bytes -------------------------------------------
size_t IndexBuilder::get_file_size(const char *file_name)
{
    std::ifstream f(file_name, 
                    std::ifstream::ate | std::ifstream::binary);
    size_t result = f.tellg();
    f.close();
    return result;
}

// constructor ------------------------------------------------------
IndexBuilder::IndexBuilder(const char *input_file_name) 
    : input_file(input_file_name, std::ifstream::binary)
{
    input_file_size = get_file_size(input_file_name);
    size_threshold = input_file_size/max_parts + 1;

    n_parts = 0;
    unique_word_ids.clear();
}

// destructor -------------------------------------------------------
IndexBuilder::~IndexBuilder() 
{ 
    for (size_t part_i = 0; part_i < n_parts; part_i++)
    {
        std::string std_file_name = 
            std::string("ind_part") + std::to_string(part_i);
        part_files[part_i].close();
        remove(std_file_name.c_str()); // remove temporary files
    }
}

// main method ------------------------------------------------------
void IndexBuilder::build_index(const char *output_file_name)
{
    size_t part_i = 0;
    bool file_finished = false;
    int64_t zero = 0;

    std::ofstream output_file(output_file_name, std::ofstream::binary);

    // split input data into parts and store in temporary files
    while ( (part_i < max_parts) && (!file_finished) ) 
    {
        index_t index_part = build_index_part(part_i);
        if (index_part.size() == 0)
        {
            file_finished = true;
        }
        else
        {
            n_parts++;
            dump_index_part_to_file(index_part, part_i);
        }

        part_i++;
    }

    // open each temporary file for reading
    for (part_i = 0; part_i < n_parts; part_i++)
    {
        std::string std_file_name = 
            std::string("ind_part") + std::to_string(part_i);
        part_files[part_i].open(std_file_name);
    }

    // sort all unique WordIds
    std::list<int64_t> unique_word_ids_list;
    unique_word_ids_list.clear();
    for (std::set<int64_t>::iterator it = unique_word_ids.begin();
         it != unique_word_ids.end();
         it++)
    {
        unique_word_ids_list.push_back(*it);
    }
    unique_word_ids_list.sort();

    size_t n_unique_words = unique_word_ids.size();
    int64_t body_offset = (8+8)*(n_unique_words + 1);
    int64_t title_offset = 0;

    // iterate through each unique word
    for (std::list<int64_t>::iterator it = unique_word_ids_list.begin();
         it != unique_word_ids_list.end();
         it++)
    {
        int64_t word_id = *it;
     
        // get all documents containing current word
        std::list<int64_t> doc_ids;
        doc_ids.clear();
        append_doc_ids(word_id, doc_ids);
        doc_ids.sort();
        int32_t n = doc_ids.size();

        // write header for current word
        output_file.seekp(title_offset);
        output_file.write(
            reinterpret_cast<const char*>(&word_id), 8);
        output_file.write(
            reinterpret_cast<const char*>(&body_offset), 8);

        // write reversed index for current word
        output_file.seekp(body_offset);
        output_file.write(
            reinterpret_cast<const char*>(&n), 4); 
        for (std::list<int64_t>::iterator it_l = doc_ids.begin();
             it_l != doc_ids.end();
             it_l++)
        {
            output_file.write(
                reinterpret_cast<const char*>(&(*it_l)), 8);
        }

        title_offset += (8+8);
        body_offset += (4 + n*8);
    }

    // fill in final zeros in header
    output_file.seekp(title_offset);
    output_file.write(reinterpret_cast<const char*>(&zero), 8);
    output_file.write(reinterpret_cast<const char*>(&zero), 8);
}

// append all documents containing the arg1 word to arg2 list ------- 
void IndexBuilder::append_doc_ids(int64_t target_word_id, 
                                  std::list<int64_t> &doc_ids)
{
    char buff64[8];
    char buff32[4];

    int64_t word_id, offset, doc_id;
    int32_t n;

    // search in every temprary file
    for (size_t part_i = 0; part_i < n_parts; part_i++)
    {
        bool word_found = false;
    
        part_files[part_i].seekg(0, part_files[part_i].beg);
        while ( (!word_found) && 
                (part_files[part_i].read(buff64, 8) != 0) )
        {
            word_id = *(reinterpret_cast<int64_t*>(buff64));  
              
            part_files[part_i].read(buff64, 8);
            offset = *(reinterpret_cast<int64_t*>(buff64));

            if (target_word_id == word_id)
                word_found = true;

            else if (word_id == 0)
                break; 
        }

        if (!word_found)
            continue;

        part_files[part_i].seekg(offset);
        part_files[part_i].read(buff32, 4);

        n = *(reinterpret_cast<int32_t*>(buff32));
        for (int32_t j = 0; j < n; j++)
        {
            part_files[part_i].read(buff64, 8);
            doc_id = *(reinterpret_cast<int64_t*>(buff64));
            doc_ids.push_back(doc_id);
        }
    }
}

// read and store a single part of input data in temporary file -----
index_t IndexBuilder::build_index_part(size_t part_i)
{

    index_t index_part;
    index_part.clear();
 
    char buff64[8]; 
    char buff32[4];

    bool part_finished = false;
    size_t bytes_read = 0;

    while ( (!part_finished) && 
            (input_file.read(buff64, 8) != 0) )
    {

        bytes_read += 8;
        input_file.read(buff32, 4);
        bytes_read += 4;

        int64_t doc_id = *(reinterpret_cast<int64_t*>(buff64));
        int32_t n = *(reinterpret_cast<int32_t*>(buff32));
        for (int32_t i = 0; i < n; i++)
        {
            for (int k = 0; k < 8; k++)
                buff64[k] = 0;

            input_file.read(buff64, 8);
            bytes_read += 8;

            int64_t word_id = *(reinterpret_cast<int64_t*>(buff64));
            if (index_part.find(word_id) == index_part.end())
            {
                std::vector<int64_t> value;
                value.clear();
                value.push_back(doc_id);

                index_part.insert(index_elem_t (word_id, value));
            }
            else
            {
                index_part[word_id].push_back(doc_id);
            }

            unique_word_ids.insert(word_id);
        }

        if (bytes_read >= size_threshold)
            part_finished = true;    

        for (int k = 0; k < 8; k++)
           buff64[k] = 0;
    }
     
    return index_part;
}

// writing index part into file -------------------------------------
void IndexBuilder::dump_index_part_to_file(index_t &index_part, 
                                           size_t part_i)
{
    int64_t zero = 0;
    size_t words_count = index_part.size();

    std::string std_file_name = 
        std::string("ind_part") + std::to_string(part_i);
    std::ofstream part_file(std_file_name.c_str(), 
                            std::ofstream::binary);

    size_t offset = (8+8)*(words_count+1);

    for (index_t::iterator it = index_part.begin();
         it != index_part.end();
         it++)
    {
        part_file.write(
            reinterpret_cast<const char*>(&it->first), 8);
        part_file.write(
            reinterpret_cast<const char*>(&offset), 8);

        offset += (8*it->second.size() + 4);        
    }

    part_file.write(reinterpret_cast<const char*>(&zero), 8);
    part_file.write(reinterpret_cast<const char*>(&zero), 8);

    for (index_t::iterator it_i = index_part.begin();
         it_i != index_part.end();
         it_i++)
    {
        std::vector<int64_t> doc_ids = it_i->second;
        int32_t n = doc_ids.size();
        part_file.write(
            reinterpret_cast<const char*>(&n), 4);
        
        for (std::vector<int64_t>::iterator it_v = doc_ids.begin();
             it_v != doc_ids.end();
             it_v++)
        {
            part_file.write(
                reinterpret_cast<const char*>(&(*it_v)), 8);
        }    

        offset += (8*it_i->second.size() + 4);        
    }

    part_file.close();
    //echo_index_file(std_file_name.c_str());
}

// debug method for printing result file contents -------------------
void IndexBuilder::echo_index_file(const char *file_name)
{
    std::cout << std::endl << "----------" << std::endl;
    std::ifstream f(file_name, std::ifstream::binary);

    char buff64[8];
    char buff32[4];

    bool header_finished = false; 

    while ( (!header_finished) && 
            (f.read(buff64, 8) != 0) )
    {
        int64_t word_id = *(reinterpret_cast<int64_t*>(buff64));
        if (word_id == 0)
            header_finished = true;
        
        f.read(buff64, 8);
        int64_t offset = *(reinterpret_cast<int64_t*>(buff64));

        std::cout << "WordId = [" << word_id << "] " <<
                     "Offset = <" << offset  << '>'  <<  std::endl;

   }
   
   while (f.read(buff32, 4) != 0)
   {
       int32_t n = *(reinterpret_cast<int32_t*>(buff32));
       std::cout << '<' << n << "> ";

       for (int32_t i = 0; i < n; i++)
       {
           f.read(buff64, 8);
           int64_t doc_id = *(reinterpret_cast<int64_t*>(buff64));
           std::cout << '[' << doc_id << "] ";
       }

       std::cout << std::endl;
    }

    std::cout << "----------" << std::endl;
}

// ==================================================================

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cout << "Error: command line must have 2 parameters: " <<
                     "<input file name> <output file name>" << 
                     std::endl;
        return 0;
    }

    IndexBuilder builder(argv[1]);
    builder.build_index(argv[2]);
    
    //builder.echo_index_file(argv[2]);

    return 0;
}
