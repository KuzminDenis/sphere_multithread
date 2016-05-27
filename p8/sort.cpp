#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <ctime>
#include <omp.h>

void show(int *array, size_t size)
{
    if (array == NULL)
        std::cout << "(null)" << std::endl;
    else
    {
        std::cout << "[ ";
        for (size_t i = 0; i < size; i++)
            std::cout << array[i] << ' ';
        std::cout << ']' << std::endl;
    }
}

void clear(int *array)
{
    if (array != NULL)
    {
        delete [] array;
        array = NULL;
    }
}

void quick_sort(int* a, const long n) 
{
    long i = 0, j = n;
    int pivot = a[n / 2];
 
    do 
    {
        while (a[i] < pivot) { i++; };
        while (a[j] > pivot) { j--; };
 
        if (i <= j) 
        {
            std::swap(a[i], a[j]);
            i++; 
            j--;
        }

    } while (i <= j);
 
    if (n < 100) 
    {
        if (j > 0) 
            quick_sort(a, j);
        if (n > i)
            quick_sort(a + i, n - i);
        return;
    }
 
    #pragma omp task shared(a)
    if (j > 0) quick_sort(a, j);
    #pragma omp task shared(a)
    if (n > i) quick_sort(a + i, n - i);
    #pragma omp taskwait
}

int random_int(int min, int max)
{
    return min + (rand() % (int)(max - min + 1));
}

int *build_array(size_t size, int seed)
{
    if (size == 0)
        return NULL;

    int *array = new int[size];

    srand(seed);
    for (size_t i = 0; i < size; i++)
        array[i] = random_int(-1000000, 1000000);

    return array;
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cout << "Wrong command line" << std::endl;
        return 0;
    }

    size_t size = atoi(argv[1]);
    int seed = atoi(argv[2]);
    size_t threads = atoi(argv[3]);

    omp_set_num_threads(threads);

    std::cout << "----------------------------------------" 
              << std::endl;
       
    int *array = build_array(size, seed);
    std::cout << "Original array:" << std::endl;
    show(array, size);

    double start_time = omp_get_wtime();

    #pragma omp parallel
    {
        #pragma omp single nowait 
        {
            quick_sort(array, size);
        }
    }

    double end_time = omp_get_wtime();
    double sort_time = end_time - start_time;

    std::cout << "Sorted array:" << std::endl;
    show(array, size);

    std::cout << "Sorting time: " << 1000 * sort_time 
              << " ms" << std::endl;
    std::cout << "----------------------------------------" 
              << std::endl;

    return 0;
} 
