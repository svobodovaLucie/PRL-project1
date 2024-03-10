/**
 * @file      pms.cpp
 *
 * @author    Lucie Svobodova \n
 *            xsvobo1x@stud.fit.vutbr.cz \n
 *            Faculty of Information Technology \n
 *            Brno University of Technology
 * 
 * @date      10.3.2024 (created)
 *
 * @brief     Implementation of Pipeline Merge Sort algorithm using the Open MPI library.
 *            Project for the course Parallel and Distributed Algorithms (PRL) on FIT BUT, 2024.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <cmath>

#include "mpi.h"

#define MPI_ROOT_RANK   0   // constant used for root rank
#define PIPELINE_TAG_0  0   // constant that indicates that the element should be added to the queue 0
#define PIPELINE_TAG_1  1   // constant that indicates that the element should be added to the queue 1

/**
 * Function parses the binary file with filename "numbers" and adds the numbers to the input queue.
 * @return pointer to the input queue, or nullptr in case of error when opening the binary file.
 */
std::queue<uint8_t>* parse() {
    // open the binary file "numbers"
    std::ifstream file("numbers", std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open the file numbers." << std::endl;
        return nullptr;
    }

    // create the input queue
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

/**
 * Function prints the queue and pops all elements from it.
 * @param queue pointer to the queue
 */
void print_queue(std::queue<uint8_t> *queue) {
    // print and pop all elements from the queue
    while (!queue->empty()) { 
        std::cout << static_cast<int>(queue->front()) << std::endl;
        queue->pop();
    }
}

/**
 * Function prints the lower element from two queues.
 * If any queue is empty, function prints first element from the non-empty queue.
 * @param queue_0 pointer to the first queue
 * @param queue_1 pointer to the second queue
 */
void print_lower_number(std::queue<uint8_t> *queue_0, std::queue<uint8_t> *queue_1) {
    // print the lower number if none of the queues is empty
    if (!queue_0->empty() && !queue_1->empty()) {
        if (queue_0->front() < queue_1->front()) {
            std::cout << static_cast<int>(queue_0->front()) << std::endl;
            queue_0->pop();
        } else {
            std::cout << static_cast<int>(queue_1->front()) << std::endl;
            queue_1->pop();
        }
    // print an element from queue_0 if queue_1 is empty
    } else if (!queue_0->empty()) {
        std::cout << static_cast<int>(queue_0->front()) << std::endl;
        queue_0->pop();
    // print an element from queue_1 if queue_0 is empty
    } else if (!queue_1->empty()) {
        std::cout << static_cast<int>(queue_1->front()) << std::endl;
        queue_1->pop();
    }
}

/**
 * Main function
 */
int main(int argc, char* argv[]) {

    // initialize MPI
    MPI_Init(&argc, &argv);

    // get the number of ranks
    int num_of_ranks;
    MPI_Comm_size(MPI_COMM_WORLD, &num_of_ranks);

    // get the current rank id
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // root rank parses the input file and computes the number of elements to be sorted
    int num_of_elements;
    std::queue<uint8_t>* input_queue;
    if (rank == MPI_ROOT_RANK) {
        // create an input queue 
        input_queue = parse();
        if (input_queue == nullptr) {
            return 1;
        }

        // get the number of elements in the queue
        num_of_elements =input_queue->size();

        // handle the scenario when there is only one process
        if (num_of_ranks == 1) {
            // print the input queue
            print_queue(input_queue);
            // delete the input queue
            delete input_queue;
            // finalize MPI and return
            MPI_Finalize();
            return 0;
        }
    }

    // broadcast the number of elements to be sorted
    MPI_Bcast(&num_of_elements, 1, MPI_INT, MPI_ROOT_RANK, MPI_COMM_WORLD);

    /* ************************************ Processor P(0) ************************************ */
    // processing routine for the root rank (0)
    if (rank == MPI_ROOT_RANK) {
        int tag; 
        // process the input queue
        for (size_t i = 0; i < num_of_elements; i++) {
            // send the elements with tags 0,1,0,1,...
            MPI_Send(&(input_queue->front()), 1, MPI_BYTE, 1, (i & 1), MPI_COMM_WORLD);
            input_queue->pop();
        }
        // delete the input queue when all elements are send
        delete input_queue;
    } 

    /* ******************************** Processors P(1) - P(n-2) ******************************** */
    // processing routine for ranks 1 - (n-2)
    else if (rank != (num_of_ranks - 1)) {
        // queues 0 and 1
        std::queue<uint8_t>* queue_0 = new std::queue<uint8_t>();
        std::queue<uint8_t>* queue_1 = new std::queue<uint8_t>();
        // number of received elements
        int num_of_recv_elements = 0;
        // buffer for the received value
        uint8_t recv_value;
        // minimal size of queue_0 to start processing the elements
        int min_num_of_elements_in_queue = (1 << (rank-1));
        // number of sent elements
        int num_of_sent_elements = 0;
        // element to be sent to the next rank
        uint8_t to_be_send;
        // flag indicating that there are enough elements in the queues to start sending
        bool send_started = false;
        // MPI message status used to determine the message tag
        MPI_Status status;
        // number of elements sent from queue_0 and queue_1
        int sent_from_queue_0 = 0;
        int sent_from_queue_1 = 0;
        // maximal number of elements that can be sent from a queue
        int max_sent_from_queue = (1 << (rank - 1));
        // flag indicating whether older elements should be sent first
        bool send_old_elements_first = false;

        // send all received elements forward
        while (num_of_sent_elements < num_of_elements) {
            
            // receive the message
            if (num_of_recv_elements != num_of_elements) {
                MPI_Recv(&recv_value, 1, MPI_BYTE, (rank-1), MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                num_of_recv_elements++;
                
                // push the received value to the corresponding queue based on the message tag
                if (status.MPI_TAG == PIPELINE_TAG_0) {
                    queue_0->push(recv_value);
                } else {
                    queue_1->push(recv_value);
                }
            }

            // check if sending can be started         
            if (!send_started && 
                (queue_0->size() >= min_num_of_elements_in_queue) && (!queue_1->empty())) {
                send_started = true;
            }

            // send the lower element
            if (send_started && !send_old_elements_first) {
                if (!queue_0->empty() && !queue_1->empty()) {
                    if (queue_0->front() < queue_1->front()) {
                        to_be_send = queue_0->front();
                        queue_0->pop();
                        sent_from_queue_0++;
                    } else {
                        to_be_send = queue_1->front();
                        queue_1->pop();
                        sent_from_queue_1++;
                    }
                }

                // determine the queue to send the element to and use it as message tag
                int queue_to_be_send_to = ((num_of_sent_elements & (1 << rank)) ? 1 : 0);
                // send the message
                MPI_Send(&to_be_send, 1, MPI_BYTE, (rank+1), queue_to_be_send_to, MPI_COMM_WORLD);
                num_of_sent_elements++;

                // check if the older elements should be sent first
                if ((sent_from_queue_0) == max_sent_from_queue || (sent_from_queue_1) == max_sent_from_queue) {
                    send_old_elements_first = true;
                }

            // ahndle the case when older elements should be sent first
            } else if (send_old_elements_first) {
                // send an element from the queue if not already sent the maximal number of elements
                if (sent_from_queue_0 < max_sent_from_queue) {
                    to_be_send = queue_0->front();
                    queue_0->pop();
                    sent_from_queue_0++;
                } else if (sent_from_queue_1 < max_sent_from_queue) {
                    to_be_send = queue_1->front();
                    queue_1->pop();
                    sent_from_queue_1++;
                }

                // reset counters and flags if all queues have sent all old elements
                if (sent_from_queue_0 >= max_sent_from_queue && sent_from_queue_1 >= max_sent_from_queue) {
                    sent_from_queue_0 = 0;
                    sent_from_queue_1 = 0;
                    send_old_elements_first = false;
                } 

                // determine the queue to send the element to and use it as message tag
                int queue_to_be_send_to = ((num_of_sent_elements & (1 << rank)) ? 1 : 0);
                // send the message
                MPI_Send(&to_be_send, 1, MPI_BYTE, (rank+1), queue_to_be_send_to, MPI_COMM_WORLD);
                num_of_sent_elements++;
            }
        }
        // delete the queues
        delete queue_0;
        delete queue_1;
    }
    
    /* *********************************** Processor P(n-1) *********************************** */
    // routine of the last rank (n-1)
    else {
        // queues 0 and 1
        std::queue<uint8_t>* queue_0 = new std::queue<uint8_t>();
        std::queue<uint8_t>* queue_1 = new std::queue<uint8_t>();
        // number of received elements
        int num_of_recv_elements = 0;
        // number of processed elements
        int num_of_processed_elements = 0;
        // buffer for the received value
        uint8_t recv_value;
        // flag indicating that processing/printing of elements can be started
        bool processing_started = false;
        // minimal size of queue_0 to start processing the numbers
        int min_num_of_elements_in_queue = (1 << (rank-1));
        // MPI status of received messages
        MPI_Status status;

        // process all elements
        while (num_of_processed_elements < num_of_elements) {

            // receive the message
            if (num_of_recv_elements != num_of_elements) {
                MPI_Recv(&recv_value, 1, MPI_BYTE, (rank-1), MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                num_of_recv_elements++;

                // check the message tag and push the value to the corresponding queue
                if (status.MPI_TAG == PIPELINE_TAG_0) {
                    queue_0->push(recv_value);
                } else {
                    queue_1->push(recv_value);
                }
            }
        
            // check if the processing of elements can be started
            if (!processing_started &&
                (queue_0->size() >= min_num_of_elements_in_queue) && (!queue_1->empty())) {
                processing_started = true;  // indicates whether processing has begun
            } 

            // if processing has started, print one element
            if (processing_started) {
                // print lower element
                print_lower_number(queue_0, queue_1);
                num_of_processed_elements++;
            }
        }
        // delete the queues
        delete queue_0;
        delete queue_1;
    }

    // finalize MPI and return
    MPI_Finalize();
    return 0;
}