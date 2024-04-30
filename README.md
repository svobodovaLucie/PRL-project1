# Pipeline Merge Sort (Project 1, Parallel and Distributed Algorithms)

#### Course Information:
- **Course**: Parallel and Distributed Algorithms (PRL)
- **Author**: Lucie Svobodová
- **Email**:  xsvobo1x@stud.fit.vutbr.cz
- **Year**:   2023/2024
- **University**: Brno University of Technology, Faculty of Information Technology
- Points: 11/10

### Description
This project implements the Pipeline Merge Sort algorithm using the Open MPI library in C++. Pipeline Merge Sort is a parallel sorting algorithm that utilizes multiple processors to sort a sequence of numbers.

#### Usage
**Compilation:**
Compile the source code using the provided script `generate_numbers.sh`. The script compiles the code and generates an executable named `pms`. It also generates random numbers to be sorted and runs the Pipeline Merge Sort (`pms` executable).
`./generate_numbers.sh <number_of_elements>`

- `number_of_elements` - the desired number of numbers to be sorted

The input sequence of numbers is stored in a file named `numbers`. Each number is represented by a single byte, without any separators between them. The program outputs the input sequence followed by the sorted sequence in ascending order. Each number in the sorted sequence is printed on a separate line.

For example, if the input sequence contains 4 elements:
```
54 53 70 25
```
The output of the program will look like:
```
25
53
54
70
```
