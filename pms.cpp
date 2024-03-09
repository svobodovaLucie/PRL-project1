// std::queue


// algo pro prostredni procesory
// while vsechny hodnoty jeste nebyly zpracovany do
    // cekej na zpravu od predchoziho procesu a uloz hodnotu do konkretni fronty
    // if fronty jsou dostatecne naplneny then
        // nastav priznak zapoceti zpracovani
    // end if
    // if je mozne zacit se zpracovavanim then
        // vyber mensi hondotu z obou front a odesli ji nasledujicimu procesu
    // end if 
// end while

// posledni procesor ma v podstate vse stene az na to, ze hodnoty radi do finalni posloupnosti

// komunikace procesu
// tagy tag_pipeline_1, tag_pipeline_2, ktere urcuji, do jake linky prijata hodnota patri
// aneb do jake fronty cilovy proces hodnotu ulozi


#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <cmath>

#include "mpi.h"

#define MPI_ROOT_RANK   0
#define PIPELINE_TAG_0  0
#define PIPELINE_TAG_1  1

// Function parses input file "numbers" and returns a pointer to a queue containing those numbers.
// Returns pointer to a queue filled with numbers from input.
// Returns nullptr if the binary file could not be open.
std::queue<uint8_t>* parse() {
    // open the binary file "numbers"
    std::ifstream file("numbers", std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open the file." << std::endl;
        return nullptr;
    }

    // allocate a new queue on the heap
    std::queue<uint8_t>* input_queue = new std::queue<uint8_t>();
    
    // push the numbers from the input file into the queue and print the contents
    uint8_t num;
    while (file.read(reinterpret_cast<char*>(&num), sizeof(uint8_t))) {
        std::cout << static_cast<int>(num) << " ";
        input_queue->push(num);
    }
    std::cout << std::endl;
    
    // close the input file
    file.close();

    // return a pointer to the new queue
    return input_queue;
}

// Main function
int main(int argc, char* argv[]) {

    // Initialize MPI
    MPI_Init(&argc, &argv);

    // Get number of processors
    int num_of_ranks;
    MPI_Comm_size(MPI_COMM_WORLD, &num_of_ranks);

    // Get the number of the current processor (rank)
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int num_of_elements;
    std::queue<uint8_t>* input_queue;
    
    // Parse the input file and get the number of elements to be sorted
    if (rank == MPI_ROOT_RANK) {
        // create an input queue 
        input_queue = parse();
        if (input_queue == nullptr) {
            return 1;
        }

        // get the number of elements in the queue
        num_of_elements =input_queue->size();
        // std::cout << rank << ": Number of elements: " << num_of_elements << std::endl;
    }

    // Broadcast the number of elements to be sorted
    MPI_Bcast(&num_of_elements, 1, MPI_INT, MPI_ROOT_RANK, MPI_COMM_WORLD);

    // The job that root rank does P(0)
    if (rank == MPI_ROOT_RANK) {
        // std::cout << "NR:" << num_of_elements << std::endl;

        int tag; 
        // Process the queue
        for (size_t i = 0; i < num_of_elements; i++) {

            // Send a number to the first pipeline
            // std::cout << rank << ": sending: " << static_cast<int>(input_queue->front()) << ", with tag: " << (i&1) << ", to rank: " << (rank+1) << std::endl;
            
            MPI_Send(&(input_queue->front()), 1, MPI_BYTE, 1, (i & 1), MPI_COMM_WORLD);
            input_queue->pop();
        }

        // delete the dynamically allocated queue
        // std::cout << rank << ": deleting input queue" << std::endl;
        delete input_queue;
    } 
    // The processors P(1) - P(n-1)
    else if (rank != (num_of_ranks - 1)) {
        // Create two queues (the size depends on the rank)
        // TODO may the size be bigger than the value depending on the rank? Like may it be the number of all numbers?
        // TODO maybe just create statically allocated array with num_of_elements elements
        std::queue<uint8_t>* queue_0 = new std::queue<uint8_t>();
        std::queue<uint8_t>* queue_1 = new std::queue<uint8_t>();

        uint8_t to_be_send;
        uint8_t value;
        int recv_in_queue_0 = 0;
        int recv_in_queue_1 = 0;
        int sent = 0;
        int recv = 0;
        bool send_started = false;
        MPI_Status status;
        int max_size = (1 << (rank - 1));
        bool idk = false;
        int qSel = -1;

        while (sent < num_of_elements) {

            if (recv != num_of_elements) {
                MPI_Recv(&value, 1, MPI_BYTE, (rank-1), MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                // std::cout << rank << ": received: " << static_cast<int>(value) << ", tag: " << status.MPI_TAG << std::endl;
                recv++;

                if (status.MPI_TAG == PIPELINE_TAG_0) {
                    queue_0->push(value);
                    // recv_in_queue_0++;
                } else {
                    queue_1->push(value);
                    // recv_in_queue_1++;
                }
            }
            // i++;

            // std::cout << rank << ": Inside: " << static_cast<int>(value) << ", queue_0->front(): " << static_cast<int>(queue_0->front()) <<  std::endl;
        
            // check if something could be sended            
            if (!send_started && (queue_0->size() >= (1 << (rank-1))) && (!queue_1->empty())) {
                send_started = true;
            }

            if (send_started && !idk) {
                if (!queue_0->empty() && !queue_1->empty()) {
                    if (queue_0->front() < queue_1->front()) {
                        to_be_send = queue_0->front();
                        queue_0->pop();
                        qSel = 0;
                    } else {
                        to_be_send = queue_1->front();
                        queue_1->pop();
                        qSel = 1;
                    }
                }

                // send
                int queue_to_be_send_to = ((sent & (1 << rank)) ? 1 : 0);
                // std::cout << rank << ": sending: " << static_cast<int>(to_be_send) << ", with tag: " << queue_to_be_send_to << ", to rank: " << (rank+1) << std::endl;
                MPI_Send(&to_be_send, 1, MPI_BYTE, (rank+1), queue_to_be_send_to, MPI_COMM_WORLD);
                sent++;

                if (qSel == 0) {
                    recv_in_queue_0++;
                } else {
                    recv_in_queue_1++;
                }

                if ((recv_in_queue_0) == max_size || (recv_in_queue_1) == max_size) {
                    idk = true;
                }

            } else if (idk) {
                // old tuple has to be sent first
                if (recv_in_queue_0 < max_size) {
                    // std::cout << "0" << std::endl;
                    to_be_send = queue_0->front();
                    queue_0->pop();
                    recv_in_queue_0++;
                } else if (recv_in_queue_1 < max_size) {
                    // std::cout << "1" << std::endl;
                    to_be_send = queue_1->front();
                    queue_1->pop();
                    recv_in_queue_1++;
                }
                // } else {
                //     // std::cout << "?" << std::endl;
                // }
                if (recv_in_queue_0 >= max_size && recv_in_queue_1 >= max_size) {
                    // std::cout << "2" << std::endl;
                    recv_in_queue_0 = 0;
                    recv_in_queue_1 = 0;
                    idk = false;
                } 
                // else {
                //     std::cout << "!" << std::endl;
                // }

                // send
                int queue_to_be_send_to = ((sent & (1 << rank)) ? 1 : 0);
                // std::cout << rank << ": sending in idk: " << static_cast<int>(to_be_send) << ", with tag: " << queue_to_be_send_to << ", to rank: " << (rank+1) << std::endl;
                MPI_Send(&to_be_send, 1, MPI_BYTE, (rank+1), queue_to_be_send_to, MPI_COMM_WORLD);
                sent++;
            }
        }
        
        // prevent memleak by deleting the queue
        // std::cout << rank << ": deleting queues" << std::endl;
        delete queue_0;
        delete queue_1;
    }
    
    // Last processor P(n-1)
    else {
        // this rank has also queues but it does not send it to any process but it just prints it
        std::queue<uint8_t>* queue_0 = new std::queue<uint8_t>();
        std::queue<uint8_t>* queue_1 = new std::queue<uint8_t>();

        uint8_t to_be_send;
        uint8_t value;
        // int tag;
        int sent = 0;
        // int i = 0;
        int recv = 0;
        bool set = false;
        MPI_Status status;

        while (sent < num_of_elements) {

            // compute the current tag
            // tag = ((i & (1 << (rank-1))) ? 1 : 0);

            // receive the message
            if (recv != num_of_elements) {
                // std::cout << rank << ": waiting for receive, " << queue_0->size() << ", " << queue_1->size() << std::endl;
                MPI_Recv(&value, 1, MPI_BYTE, (rank-1), MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                // std::cout << rank << ": received: " << static_cast<int>(value) << ", tag: " << status.MPI_TAG << std::endl;
                recv++;

                // check the status -> find out the queue to store the value in
                if (status.MPI_TAG == PIPELINE_TAG_0) {
                    queue_0->push(value);
                } else {
                    queue_1->push(value);
                }
            }
            // i++;

            // save the value to the corresponding queue
            // if (tag == PIPELINE_TAG_0) {
            //     // save to the queue_0
            //     MPI_Recv(&value, 1, MPI_BYTE, (rank-1), MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            //     std::cout << rank << ": STATUS: " << status.MPI_TAG << std::endl;
            //     queue_0->push(value);
            //     i++;
            // } 
            // else {
            //     // receive the value and save it to queue1
            //     MPI_Recv(&value, 1, MPI_BYTE, (rank-1), MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            //     std::cout << rank << ": STATUS: " << status.MPI_TAG << std::endl;
            //     queue_1->push(value);
            //     i++;
            // }

            // std::cout << rank << ": Inside the last rank: " << static_cast<int>(value) << ", queue_0->front(): " << static_cast<int>(queue_0->front()) <<  std::endl;
        
            if (!set && (queue_0->size() >= (1 << (rank-1))) && (!queue_1->empty())) {
                set = true;
            } 

            if (set) {
                if (!queue_0->empty() && !queue_1->empty()) {
                    if (queue_0->front() < queue_1->front()) {
                        // to_be_send = queue_0->front();
                        std::cout << static_cast<int>(queue_0->front()) << std::endl;
                        queue_0->pop();
                    } else {
                        // to_be_send = queue_1->front();
                        std::cout << static_cast<int>(queue_1->front()) << std::endl;
                        queue_1->pop();
                    }
                } else if (!queue_0->empty()) {
                    std::cout << static_cast<int>(queue_0->front()) << std::endl;
                    queue_0->pop();
                } else if (!queue_1->empty()) {
                    std::cout << static_cast<int>(queue_1->front()) << std::endl;
                    queue_1->pop();
                }
                sent++;
            }
        }

        // std::cout << rank << ": the End" << std::endl;
        delete queue_0;
        delete queue_1;
    }

    MPI_Finalize();
    return 0;
}