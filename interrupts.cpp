/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include "interrupts.hpp"

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string trace;      //!< string to store single line of trace file
    std::string execution = "";  //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;
    std::srand(std::time(nullptr)); // Random number generator


    //parse each line of the input trace file. 'for' loop to keep track of indices.
    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") { //As per Assignment 1
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } else if(activity == "SYSCALL") { //As per Assignment 1
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR (ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR(ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your FORK output here

            int random_execution_time;
            
            // Create the new child PCB
            int child_PID = current.PID + 1;
            PCB child = PCB(child_PID , current.PID, current.program_name, current.size, -1);

            random_execution_time = std::rand() % 10 + 1; // Generate random execution time between 1 and 10 ms
            execution += std::to_string(current_time) + ", " + std::to_string(random_execution_time) + ", cloning the PCB\n";
            current_time += random_execution_time;

            bool allocation_successful = allocate_memory(&child);

            if (!allocation_successful) {
                execution += std::to_string(current_time) + ", 0, Memory allocation unsucessful\n";
                system_status += "ERROR: Memory allocation failed (PID " + std::to_string(child.PID) + ")\n";

                std::vector<PCB> temp_queue = wait_queue;
                system_status += "time: " + std::to_string(current_time) + "; current trace: " + activity + " " +  std::to_string(duration_intr) + "\n";
                system_status += print_PCB(current, temp_queue);
                
            } else {
                wait_queue.push_back(current);

                execution += std::to_string(current_time) + ", " + std::to_string(0) + ", scheduler called\n";
                execution += std::to_string(current_time) + ", " + std::to_string(1) + ", IRET\n";
                current_time += 1;

                std::vector<PCB> display_queue = wait_queue;
                system_status += "time: " + std::to_string(current_time) + "; current trace: " + activity + " " +  std::to_string(duration_intr) + "\n";
                system_status += print_PCB(child, display_queue);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////

            //The following loop helps you do 2 things:
            // * Collect the trace of the child (and only the child, skip parent)
            // * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") {
                    skip = false;
                    continue;
                } else if(_activity == "IF_PARENT"){
                    skip = true;
                    parent_index = j;
                    if(exec_flag) {
                        break;
                    }
                } else if(skip && _activity == "ENDIF") {
                    skip = false;
                    continue;
                } else if(!skip && _activity == "EXEC") {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                
                }

                if(!skip) {
                    child_trace.push_back(trace_file[j]);
                }
            }
            i = parent_index;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the child's trace, run the child (HINT: think recursion)      

            if(!child_trace.empty() && allocation_successful) {
                auto [_execution, _system_status, child_end_time] = simulate_trace(child_trace, current_time, vectors, delays, external_files, child, wait_queue);

                execution += _execution;
                system_status += _system_status;
                current_time = child_end_time;     
            }
            
            if(child.partition_number != -1) {
                free_memory(&child);
            }
            
            wait_queue.erase(
                std::remove_if(wait_queue.begin(), wait_queue.end(), [&](const PCB& p){return p.PID == child.PID; }),
                wait_queue.end());

            ///////////////////////////////////////////////////////////////////////////////////////////


        } else if(activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your EXEC output here

            unsigned int _program_size = get_size(program_name, external_files);

            if (_program_size == (unsigned int) -1) {
                _program_size = 1;
            }

            PCB temp_process = current;
            temp_process.program_name = program_name;
            temp_process.size = _program_size;  

            if(temp_process.partition_number != -1) {
                free_memory(&temp_process);
            }

            current.program_name = program_name;
            current.size = _program_size;

            // Allocate memory partition for new process
            if (!allocate_memory(&current)) {
                execution += std::to_string(current_time) + ", 0, Memory allocation failed for EXEC process\n";
                system_status += "ERROR: Could not allocate memory for EXEC process (PID "
                                + std::to_string(current.PID) + ")\n";
                return {execution, system_status, current_time};
            }                 

            // Random number generator
            int random_execution_time;

            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) 
                        + ", Program is " + std::to_string(get_size(program_name, external_files)) + "Mb large\n";
            current_time += duration_intr;

            unsigned int external_program_size = get_size(program_name, external_files) * 15;
            execution += std::to_string(current_time) + ", " + std::to_string(external_program_size) 
                        + ", loading program into memory\n";
            current_time += external_program_size;

            random_execution_time = std::rand() % 10 + 1; // Generate random execution time between 1 and 10 ms
            execution += std::to_string(current_time)  +  ", " + std::to_string(random_execution_time) +  " , marking partition as occupied\n";
            current_time += random_execution_time;

            random_execution_time = std::rand() % 10 + 1; // Generate random execution time between 1 and 10 ms
            execution += std::to_string(current_time) +  ", " + std::to_string(random_execution_time) + " , updating PCD\n";
            current_time += random_execution_time;

            execution += std::to_string(current_time) + ", " + std::to_string(0) +  ", scheduler called\n"; // Assume 0 execution time for scheduling

            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
            
            std::vector<PCB> temp_queue = wait_queue;
            temp_queue.erase(
                std::remove_if(temp_queue.begin(), temp_queue.end(), [&](const PCB& pcb){return pcb.PID == current.PID; }),
                temp_queue.end());
            system_status += "time: " + std::to_string(current_time) + "; current trace: " + activity + " " + program_name + " " + std::to_string(duration_intr) + "\n";
            system_status += print_PCB(current, temp_queue);

            ///////////////////////////////////////////////////////////////////////////////////////////

            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)

            if (!exec_traces.empty()) {
                auto [_execution, _system_status, child_end_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);
                execution += _execution;
                system_status += _system_status;
                current_time = child_end_time;
            }         

            ///////////////////////////////////////////////////////////////////////////////////////////

            break; //Why is this important? (answer in report)

        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {

    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elemens is the device number, starting from 0
    //external_files is a C++ std::vector of the struct 'external_file'. Check the struct in 
    //interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    //Just a sanity check to know what files you have
    print_external_files(external_files);

    //Make initial PCB (notice how partition is not assigned yet)
    PCB current(0, -1, "init", 1, -1);
    //Update memory (partition is assigned here, you must implement this function)
    if(!allocate_memory(&current)) {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/




    /******************************************************************/

    //Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(   trace_file, 
                                            0, 
                                            vectors, 
                                            delays,
                                            external_files, 
                                            current, 
                                            wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
