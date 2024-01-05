#include <algorithm>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <vector>

using std::cout;
using std::endl;
using std::string;
using std::vector;

string inputtracename = "trace.txt";
// remove the ".txt" and add ".out.txt" to the end as output name
string outputtracename = inputtracename.substr(0, inputtracename.length() - 4) + ".out.txt";
string hardwareconfigname = "config.txt";

bool printToConsole = false;

enum Operation
{
    ADD,
    SUB,
    MULT,
    DIV,
    LOAD,
    STORE
};
// The execute cycle of each operation: ADD, SUB, MULT, DIV, LOAD, STORE
const int OperationCycle[6] = {2, 2, 10, 40, 2, 2};

struct HardwareConfig
{
    int LoadRSsize;  // number of load reservation stations
    int StoreRSsize; // number of store reservation stations
    int AddRSsize;   // number of add reservation stations
    int MultRSsize;  // number of multiply reservation stations
    int FRegSize;    // number of fp registers
};

// We use the following structure to record the time of each instruction
struct InstructionStatus
{
    int cycleIssued;
    int cycleExecuted; // execution completed
    int cycleWriteResult;
};

// Register Result Status structure
struct RegisterResultStatus
{
    string ReservationStationName;
    bool dataReady;
};

/*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/
struct Instruction
{
    int instructionID;
    string instructionType;
    string destination;
    string source1, source2;
};

class InstructionQueue
{
  public:
    // create a queue data structure here
    std::queue<Instruction> instructionQueue;

    void readInstructionsFromFile(string inputfilename)
    {
        // function to load the Instruction Queue from the trace file
        if (printToConsole)
            cout << "Loading the instruction queue:\n";

        std::ifstream trace;
        trace.open(inputfilename);

        string line;
        size_t space_location;
        int line_number = 0;

        while (getline(trace, line))
        {
            Instruction newInstruction;
            newInstruction.instructionID = line_number++;

            space_location = line.find(' ');
            newInstruction.instructionType = line.substr(0, space_location);
            line = line.substr(space_location + 1);
            if (newInstruction.instructionType != "STORE")
            {
                space_location = line.find(' ');
                newInstruction.destination = line.substr(0, space_location);
                line = line.substr(space_location + 1);
                space_location = line.find(' ');
                newInstruction.source1 = line.substr(0, space_location);
                newInstruction.source2 = line.substr(space_location + 1);
            }
            else
            {
                // STORE is a special instruction where the destination is the operand
                // STORE F6 40 0
                // Here, the data from F6 is written at memory 40+0
                // In these instructions we are swapping the destination and source1 location
                // Ignoring source2 as we will not be offsetting our address in our code example
                space_location = line.find(' ');
                newInstruction.source1 = line.substr(0, space_location);
                line = line.substr(space_location + 1);
                space_location = line.find(' ');
                newInstruction.destination = line.substr(0, space_location);
                newInstruction.source2 = line.substr(space_location + 1);
            }
            // Printing Instructions to Console
            if (printToConsole)
            {
                cout << "Line (" << line_number << ")\n";
                __printInstruction(newInstruction);
                cout << "\n";
            }

            instructionQueue.push(newInstruction);
        }
        trace.close();
    }

    void __printInstruction(Instruction instruction)
    {
        cout << "Instruction ID     : " << instruction.instructionID << "\n";
        cout << "Instruction String : " << instruction.instructionType << "\n";
        cout << "Destination String : " << instruction.destination << "\n";
        cout << "Source 1 String    : " << instruction.source1 << "\n";
        cout << "Source 2 String    : " << instruction.source2 << "\n";
    }
};

// Creating a struct to track all active instructions...
struct activeInstructionDetails
{
    string reservationStationName;
    int stationType, reservationStationID, FRegID;
    int executionComplete;
    // no active instruction = -1;
    // execution not complete = 0;
    // execution complete, but data not sent on data bus = 1;
    // execution complete, and data sent on data bus = 2;
    // all steps completed = 3;
};

class CommonDataBus
{
  public:
    bool data;
    string source;
    int FDestination;

    CommonDataBus()
    {
        clear();
    }

    void broadcast(bool broadcastData, string broadcastSource, int broadcastDestination)
    {
        data = broadcastData;
        source = broadcastSource;
        FDestination = broadcastDestination;
    }

    void clear()
    {
        data = 0;
        source = "";
        FDestination = 0;
    }
};

class RegisterResultStatuses
{
  public:
    // constructor for RegisterResultStatuses class
    RegisterResultStatuses(HardwareConfig config)
    {
        _registers.resize(config.FRegSize);

        for (int i = 0; i < _registers.size(); i++)
        {
            setFRegReservationStationName(i, "");
        }
    }

    string getFRegReservationStationName(int index)
    {
        return _registers[index].ReservationStationName;
    }

    void setFRegReservationStationName(int index, string name)
    {
        _registers[index].ReservationStationName = name;
        _registers[index].dataReady = 0;
    }

    // int getFRegIndex(string stationName)
    // {
    //     for (int i = 0; i < _registers.size(); i++)
    //     {
    //         if (_registers[i].ReservationStationName == stationName)
    //             return i;
    //     }
    //     return -1;
    // }

    bool isDataReady(string reservationStationName)
    {
        for (int index = 0; index < _registers.size(); index++)
        {
            if (_registers[index].ReservationStationName == reservationStationName &&
                _registers[index].ReservationStationName != "")
            {
                if (printToConsole)
                    cout << "F" << index << ", " << _registers[index].ReservationStationName << ", "
                         << _registers[index].dataReady << "\n";

                return _registers[index].dataReady;
            }
        }
        return false;
    }

    void setDataReady(string cdbSource)
    {
        for (int index = 0; index < _registers.size(); index++)
        {
            if (_registers[index].ReservationStationName == cdbSource && _registers[index].ReservationStationName != "")
            {
                _registers[index].dataReady = 1;
                if (printToConsole)
                    cout << "Data is ready for F" << index << "!\n";
            }
        }
    }

    void clearDataReady(int index)
    {
        if (printToConsole)
            cout << "Clearing dataReady for F" << index << "\n";
        _registers[index].dataReady = 0;
    }

    // void unassignFReg(string stationName)
    // {
    //     int idx = getFRegIndex(stationName);

    //     _registers[idx].ReservationStationName = "";
    //     _registers[idx].dataReady = false;
    // }

    /*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/
    /*
    Print all register result status. It is called by PrintRegisterResultStatus4Grade() for grading.
    If you don't want to write such a class, then make sure you call the same function and print the register
    result status in the same format.
    */
    string _printRegisterResultStatus() const
    {
        std::ostringstream result;
        for (int idx = 0; idx < _registers.size(); idx++)
        {
            result << "F" + std::to_string(idx) << ": ";
            result << _registers[idx].ReservationStationName << ", ";
            result << "dataRdy: " << (_registers[idx].dataReady ? "Y" : "N") << ", ";
            result << "\n";
        }
        return result.str();
    }
    /*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/
  private:
    vector<RegisterResultStatus> _registers;
};

// Define your Reservation Station structure
struct ReservationStation
{
    string reservationStationName;
    bool reservationStationBusy;
    int Op;
    bool Vj;   // using booleans to indicate whether data is ready or not
    bool Vk;   // using booleans to indicate whether data is ready or not
    string Qj; // instead of using pointer, I'm putting the Source FReg number here...
    string Qk; // instead of using pointer, I'm putting the Source FReg number here...
    int remaining_cycle;
    int instructionID;
    // check if you can add result ready boolean variable, that will indicate that the result is ready, but
    // it is not published on the common data bus... and the counter will keep on counting down until the
    // result ready is set...
};
class ReservationStations
{
  public:
    ReservationStations(HardwareConfig config)
    {
        loadSize = config.LoadRSsize;
        storeSize = config.StoreRSsize;
        addSize = config.AddRSsize;
        multSize = config.MultRSsize;

        load_stations.resize(loadSize);
        store_stations.resize(storeSize);
        add_stations.resize(addSize);
        mult_stations.resize(multSize);

        // Initializing load stations:
        for (int i = 0; i < loadSize; i++)
        {
            load_stations[i].reservationStationName = "Load" + std::to_string(i);
            load_stations[i].reservationStationBusy = 0;
            load_stations[i].Vj = 0;
            load_stations[i].Vk = 0;
            load_stations[i].Qj = ""; // Initialising as "", as it doesnt point to any FReg
            load_stations[i].Qk = ""; // Initialising as "", as it doesnt point to any FReg
            load_stations[i].remaining_cycle = 0;
            load_stations[i].instructionID = -1;
        }

        // Initializing store stations:
        for (int i = 0; i < storeSize; i++)
        {
            store_stations[i].reservationStationName = "Store" + std::to_string(i);
            store_stations[i].reservationStationBusy = 0;
            store_stations[i].Vj = 0;
            store_stations[i].Vk = 0;
            store_stations[i].Qj = ""; // Initialising as "", as it doesnt point to any FReg
            store_stations[i].Qk = ""; // Initialising as "", as it doesnt point to any FReg
            store_stations[i].remaining_cycle = 0;
            store_stations[i].instructionID = -1;
        }

        // Initializing add stations:
        for (int i = 0; i < addSize; i++)
        {
            add_stations[i].reservationStationName = "Add" + std::to_string(i);
            add_stations[i].reservationStationBusy = 0;
            add_stations[i].Vj = 0;
            add_stations[i].Vk = 0;
            add_stations[i].Qj = ""; // Initialising as "", as it doesnt point to any FReg
            add_stations[i].Qk = ""; // Initialising as "", as it doesnt point to any FReg
            add_stations[i].remaining_cycle = 0;
            add_stations[i].instructionID = -1;
        }

        // Initializing mult stations:
        for (int i = 0; i < multSize; i++)
        {
            mult_stations[i].reservationStationName = "Mult" + std::to_string(i);
            mult_stations[i].reservationStationBusy = 0;
            mult_stations[i].Vj = 0;
            mult_stations[i].Vk = 0;
            mult_stations[i].Qj = ""; // Initialising as "", as it doesnt point to any FReg
            mult_stations[i].Qk = ""; // Initialising as "", as it doesnt point to any FReg
            mult_stations[i].remaining_cycle = 0;
            mult_stations[i].instructionID = -1;
        }
    }

    bool isLoadStationAvailable()
    {
        for (int i = 0; i < loadSize; i++)
        {
            // if reservation station is not busy, return true
            if (!load_stations[i].reservationStationBusy)
                return true;
        }
        // if all stations are busy, return false
        return false;
    }

    bool isStoreStationAvailable()
    {
        for (int i = 0; i < storeSize; i++)
        {
            // if reservation station is not busy, return true
            if (!store_stations[i].reservationStationBusy)
                return true;
        }
        // if all stations are busy, return false
        return false;
    }

    bool isAddStationAvailable()
    {
        for (int i = 0; i < addSize; i++)
        {
            // if reservation station is not busy, return true
            if (!add_stations[i].reservationStationBusy)
                return true;
        }
        // if all stations are busy, return false
        return false;
    }

    bool isMultStationAvailable()
    {
        for (int i = 0; i < multSize; i++)
        {
            // if reservation station is not busy, return true
            if (!mult_stations[i].reservationStationBusy)
                return true;
        }
        // if all stations are busy, return false
        return false;
    }

    void checkLoadStationOperands(int i)
    {
        if (load_stations[i].Vj && load_stations[i].remaining_cycle == 0)
        {
            load_stations[i].remaining_cycle = OperationCycle[load_stations[i].Op];
        }
    }

    void checkAddStationOperands(int i, RegisterResultStatuses &registerResultStatus)
    {
        if (add_stations[i].Qj != "")
        {
            if (registerResultStatus.isDataReady(add_stations[i].Qj))
            {
                if (printToConsole)
                    cout << "Operand 1 ready...\n";
                add_stations[i].Vj = 1;
                add_stations[i].Qj = ""; // clearing Qj
            }
            else
            {
                add_stations[i].Vj = 0;
            }
        }

        if (add_stations[i].Qk != "")
        {
            if (registerResultStatus.isDataReady(add_stations[i].Qk))
            {
                if (printToConsole)
                    cout << "Operand 2 ready...\n";
                add_stations[i].Vk = 1;
                add_stations[i].Qk = ""; // clearing Qk
            }
            else
            {
                add_stations[i].Vk = 0;
            }
        }

        if (add_stations[i].Vj && add_stations[i].Vk && add_stations[i].remaining_cycle == 0)
        {
            add_stations[i].remaining_cycle = OperationCycle[add_stations[i].Op] + 1;
            if (printToConsole)
                cout << "Starting counter: " << add_stations[i].remaining_cycle << "\n";
        }
    }

    void checkMultStationOperands(int i, RegisterResultStatuses &registerResultStatus)
    {
        if (mult_stations[i].Qj != "")
        {
            if (registerResultStatus.isDataReady(mult_stations[i].Qj))
            {
                if (printToConsole)
                    cout << "Operand 1 ready...\n";
                mult_stations[i].Vj = 1;
                mult_stations[i].Qj = ""; // clearing Qj
            }
            else
            {
                mult_stations[i].Vj = 0;
            }
        }

        if (mult_stations[i].Qk != "")
        {
            if (registerResultStatus.isDataReady(mult_stations[i].Qk))
            {
                if (printToConsole)
                    cout << "Operand 2 ready...\n";
                mult_stations[i].Vk = 1;
                mult_stations[i].Qk = ""; // clearing Qk
            }
            else
            {
                mult_stations[i].Vk = 0;
            }
        }

        if (mult_stations[i].Vj && mult_stations[i].Vk && mult_stations[i].remaining_cycle == 0)
        {
            mult_stations[i].remaining_cycle = OperationCycle[mult_stations[i].Op] + 1;
            if (printToConsole)
                cout << "Starting counter: " << mult_stations[i].remaining_cycle << "\n";
        }
    }

    void checkStoreStationOperand(int i, RegisterResultStatuses &registerResultStatus)
    {
        if (store_stations[i].Qj != "")
        {
            if (registerResultStatus.isDataReady(store_stations[i].Qj))
            {
                if (printToConsole)
                    cout << "Operand 1 ready...\n";
                store_stations[i].Vj = 1;
                store_stations[i].Qj = "";
            }
            else
            {
                store_stations[i].Vj = 0;
            }
        }

        if (store_stations[i].Vj && store_stations[i].remaining_cycle == 0)
        {
            store_stations[i].remaining_cycle = OperationCycle[store_stations[i].Op];
            if (printToConsole)
                cout << "Starting counter: " << store_stations[i].remaining_cycle << "\n";
        }
    }

    string assignLoadStation(string memory_address, int instructionID,
                             vector<activeInstructionDetails> &activeInstructions, int FRegID)
    {
        for (int i = 0; i < loadSize; i++)
        {
            if (!load_stations[i].reservationStationBusy)
            {
                load_stations[i].reservationStationBusy = 1;
                load_stations[i].Op = LOAD;
                load_stations[i].Qj = memory_address;
                load_stations[i].instructionID = instructionID;

                // Load is a special case... in this code, we are assuming that load's operand:
                // memory address, is always ready.. so we just assign data values while assigning
                load_stations[i].Vj = 1;
                // load_stations[i].remaining_cycle = OperationCycle[load_stations[i].Op];

                activeInstructionDetails newActiveInstruction;

                newActiveInstruction.reservationStationName = load_stations[i].reservationStationName;
                newActiveInstruction.stationType = load_stations[i].Op;
                newActiveInstruction.reservationStationID = i;
                newActiveInstruction.executionComplete = 0;
                newActiveInstruction.FRegID = FRegID;

                activeInstructions[load_stations[i].instructionID] = newActiveInstruction;

                if (printToConsole)
                    cout << "Instruction Assigned : ";
                if (printToConsole)
                    cout << load_stations[i].instructionID << "\t";
                if (printToConsole)
                    cout << activeInstructions[load_stations[i].instructionID].reservationStationName << "\t";
                if (printToConsole)
                    cout << activeInstructions[load_stations[i].instructionID].executionComplete << "\n";

                return load_stations[i].reservationStationName;
            }
        }
        if (printToConsole)
            cout << "All load stations are busy right now...\n";
        return "";
    }

    string assignStoreStation(string operand1, int instructionID, vector<activeInstructionDetails> &activeInstructions)
    {
        for (int i = 0; i < storeSize; i++)
        {
            if (!store_stations[i].reservationStationBusy)
            {
                store_stations[i].reservationStationBusy = 1;
                store_stations[i].Op = STORE;
                store_stations[i].Qj = operand1;
                store_stations[i].instructionID = instructionID;

                activeInstructionDetails newActiveInstruction;
                newActiveInstruction.reservationStationName = store_stations[i].reservationStationName;
                newActiveInstruction.stationType = store_stations[i].Op;
                newActiveInstruction.reservationStationID = i;
                newActiveInstruction.executionComplete = 0;
                newActiveInstruction.FRegID = -1; // Special for store instruction

                activeInstructions[store_stations[i].instructionID] = newActiveInstruction;

                if (printToConsole)
                    cout << "Instruction Assigned : ";
                if (printToConsole)
                    cout << store_stations[i].instructionID << "\t";
                if (printToConsole)
                    cout << activeInstructions[store_stations[i].instructionID].reservationStationName << "\t";
                if (printToConsole)
                    cout << activeInstructions[store_stations[i].instructionID].executionComplete << "\n";

                return store_stations[i].reservationStationName;
            }
        }
        if (printToConsole)
            cout << "All store stations are busy right now...\n";
        return "";
    }

    string assignAddStation(string subtype, string operand1, string operand2, int instructionID,
                            vector<activeInstructionDetails> &activeInstructions, int FRegID)
    {
        for (int i = 0; i < addSize; i++)
        {
            if (!add_stations[i].reservationStationBusy)
            {
                add_stations[i].reservationStationBusy = 1;
                if (subtype == "ADD")
                    add_stations[i].Op = ADD;
                else
                    add_stations[i].Op = SUB;

                add_stations[i].Qj = operand1;
                add_stations[i].Qk = operand2;
                add_stations[i].instructionID = instructionID;

                activeInstructionDetails newActiveInstruction;
                newActiveInstruction.reservationStationName = add_stations[i].reservationStationName;
                newActiveInstruction.stationType = add_stations[i].Op;
                newActiveInstruction.reservationStationID = i;
                newActiveInstruction.executionComplete = 0;
                newActiveInstruction.FRegID = FRegID;

                activeInstructions[add_stations[i].instructionID] = newActiveInstruction;

                if (printToConsole)
                    cout << "Instruction Assigned : ";
                if (printToConsole)
                    cout << add_stations[i].instructionID << "\t";
                if (printToConsole)
                    cout << activeInstructions[add_stations[i].instructionID].reservationStationName << "\t";
                if (printToConsole)
                    cout << activeInstructions[add_stations[i].instructionID].executionComplete << "\n";

                return add_stations[i].reservationStationName;
            }
        }
        if (printToConsole)
            cout << "All add stations are busy right now...\n";
        return "";
    }

    string assignMultStation(string subtype, string operand1, string operand2, int instructionID,
                             vector<activeInstructionDetails> &activeInstructions, int FRegID)
    {
        for (int i = 0; i < multSize; i++)
        {
            if (!mult_stations[i].reservationStationBusy)
            {
                mult_stations[i].reservationStationBusy = 1;
                if (subtype == "MULT")
                    mult_stations[i].Op = MULT;
                else
                    mult_stations[i].Op = DIV;

                mult_stations[i].Qj = operand1;
                mult_stations[i].Qk = operand2;
                mult_stations[i].instructionID = instructionID;

                activeInstructionDetails newActiveInstruction;
                newActiveInstruction.reservationStationName = mult_stations[i].reservationStationName;
                newActiveInstruction.stationType = mult_stations[i].Op;
                newActiveInstruction.reservationStationID = i;
                newActiveInstruction.executionComplete = 0;
                newActiveInstruction.FRegID = FRegID;

                activeInstructions[mult_stations[i].instructionID] = newActiveInstruction;

                if (printToConsole)
                    cout << "Instruction Assigned : ";
                if (printToConsole)
                    cout << mult_stations[i].instructionID << "\t";
                if (printToConsole)
                    cout << activeInstructions[mult_stations[i].instructionID].reservationStationName << "\t";
                if (printToConsole)
                    cout << activeInstructions[mult_stations[i].instructionID].executionComplete << "\n";

                return mult_stations[i].reservationStationName;
            }
        }
        if (printToConsole)
            cout << "All mult stations are busy right now...\n";
        return "";
    }

    void unassignLoadStation(int i)
    {
        // Unassign Reservation Station
        load_stations[i].reservationStationBusy = 0;
        load_stations[i].Qj = "";
        load_stations[i].Vj = 0;
    }

    void unassignStoreStation(int i)
    {
        // Unassign Reservation Station
        store_stations[i].reservationStationBusy = 0;
        store_stations[i].Qj = "";
        store_stations[i].Vj = 0;
    }

    void unassingAddStation(int i)
    {
        // Unassign Reservation Station
        add_stations[i].reservationStationBusy = 0;
        add_stations[i].Qj = "";
        add_stations[i].Qk = "";
        add_stations[i].Vj = 0;
        add_stations[i].Vk = 0;
    }

    void unassingMultStation(int i)
    {
        // Unassign Reservation Station
        mult_stations[i].reservationStationBusy = 0;
        mult_stations[i].Qj = "";
        mult_stations[i].Qk = "";
        mult_stations[i].Vj = 0;
        mult_stations[i].Vk = 0;
    }

    int executeLoadStation(int i)
    {
        if (load_stations[i].reservationStationBusy)
        {
            if (load_stations[i].Vj)
            {
                if (load_stations[i].remaining_cycle > 0)
                {
                    // Reservation Station has an active instruction, the data is ready,
                    // and the remaining cycle is not zero, so we reduce the cycle by 1
                    load_stations[i].remaining_cycle--;

                    // The execution is completed if cycle count is 0...
                    if (load_stations[i].remaining_cycle == 0)
                    {
                        // Returning 1, as execution is complete, but not broadcasted on data bus...
                        return 1;
                    }
                    else
                        return 0;
                }
                else
                    return -1;
            }
            else
            {
                // Reservation Station has an active instruction, but data is not ready...
                if (printToConsole)
                    cout << load_stations[i].reservationStationName << " waiting for operands...\n";
                return 0;
            }
        }
        else
            return -1;
    }

    int executeStoreStation(int i)
    {
        if (store_stations[i].reservationStationBusy)
        {
            if (store_stations[i].Vj)
            {
                if (store_stations[i].remaining_cycle > 0)
                {
                    // Reservation Station has an active instruction, the data is ready,
                    // and the remaining cycle is not zero, so we reduce the cycle by 1
                    store_stations[i].remaining_cycle--;

                    // The execution is completed if cycle count is 0...
                    if (store_stations[i].remaining_cycle == 0)
                    {
                        // Returning 1, as execution is complete, but not broadcasted on data bus...
                        return 1;
                    }
                    else
                        return 0;
                }
                else
                    return 0;
            }
            else
            {
                // Reservation Station has an active instruction, but data is not ready...
                if (printToConsole)
                    cout << store_stations[i].reservationStationName << " waiting for operands...\n";
                return 0;
            }
        }
        else
            return -1;
    }

    int executeAddStation(int i)
    {
        if (add_stations[i].reservationStationBusy)
        {
            if (add_stations[i].Vj && add_stations[i].Vk)
            {
                if (add_stations[i].remaining_cycle > 0)
                {
                    // Reservation Station has an active instruction, the data is ready,
                    // and the remaining cycle is not zero, so we reduce the cycle by 1
                    add_stations[i].remaining_cycle--;

                    // The execution is completed if cycle count is 0...
                    if (add_stations[i].remaining_cycle == 0)
                    {
                        // Returning 1, as execution is complete, but not broadcasted on data bus...
                        return 1;
                    }
                    else
                        return 0;
                }
                else
                    return -1;
            }
            else
            {
                // Reservation Station has an active instruction, but data is not ready...
                if (printToConsole)
                    cout << add_stations[i].reservationStationName << " waiting for operands...\n";
                return 0;
            }
        }
        else
            return -1;
    }

    int executeMultStation(int i)
    {
        if (mult_stations[i].reservationStationBusy)
        {
            if (mult_stations[i].Vj && mult_stations[i].Vk)
            {
                if (mult_stations[i].remaining_cycle > 0)
                {
                    // Reservation Station has an active instruction, the data is ready,
                    // and the remaining cycle is not zero, so we reduce the cycle by 1
                    mult_stations[i].remaining_cycle--;

                    // The execution is completed if cycle count is 0...
                    if (mult_stations[i].remaining_cycle == 0)
                    {
                        // Returning 1, as execution is complete, but not broadcasted on data bus...
                        return 1;
                    }
                    else
                        return 0;
                }
                else
                    return -1;
            }
            else
            {
                // Reservation Station has an active instruction, but data is not ready...
                if (printToConsole)
                    cout << mult_stations[i].reservationStationName << " waiting for operands...\n";
                return 0;
            }
        }
        else
            return -1;
    }

  private:
    int addSize, multSize, loadSize, storeSize;

    vector<ReservationStation> load_stations;
    vector<ReservationStation> store_stations;
    vector<ReservationStation> add_stations;
    vector<ReservationStation> mult_stations;
};

/*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/

/*
print the instruction status, the reservation stations and the register result status
@param filename: output file name
@param instructionStatus: instruction status
*/
void PrintResult4Grade(const string &filename, const vector<InstructionStatus> &instructionStatus)
{
    std::ofstream outfile(filename, std::ios_base::app); // append result to the end of file
    outfile << "Instruction Status:\n";
    for (int idx = 0; idx < instructionStatus.size(); idx++)
    {
        outfile << "Instr" << idx << ": ";
        outfile << "Issued: " << instructionStatus[idx].cycleIssued << ", ";
        outfile << "Completed: " << instructionStatus[idx].cycleExecuted << ", ";
        outfile << "Write Result: " << instructionStatus[idx].cycleWriteResult << ", ";
        outfile << "\n";
    }
    outfile.close();
}

/*
print the register result status each 5 cycles
@param filename: output file name
@param registerResultStatus: register result status
@param thiscycle: current cycle
*/
void PrintRegisterResultStatus4Grade(const string &filename, const RegisterResultStatuses &registerResultStatus,
                                     const int thiscycle)
{
    if (thiscycle % 5 != 0)
        return;
    std::ofstream outfile(filename, std::ios_base::app); // append result to the end of file
    outfile << "Cycle " << thiscycle << ":\n";
    outfile << registerResultStatus._printRegisterResultStatus() << "\n";
    outfile.close();
}

/*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/
// Function to check whether we can issue an instruction
bool canIssueInstruction(InstructionQueue &iq, RegisterResultStatuses &registerResultStatus,
                         ReservationStations &reservationStations)
{
    if (iq.instructionQueue.empty())
    {
        // Instruction Queue is empty, no new instruction to issue...
        return false;
    }
    else
    {
        // Instruction Queue is not empty, checking if FReg is available...

        // Get the details of the next instruction in queue
        Instruction nextInstruction = iq.instructionQueue.front();

        if (nextInstruction.instructionType == "ADD" || nextInstruction.instructionType == "SUB")
        {
            int destinationRegister = std::stoi(nextInstruction.destination.substr(1));
            string FRegReservationStationName = registerResultStatus.getFRegReservationStationName(destinationRegister);

            if (FRegReservationStationName == "" ||
                (FRegReservationStationName != "" && registerResultStatus.isDataReady(FRegReservationStationName)))
            {
                // Checking if the reservation station is avaiable...
                if (reservationStations.isAddStationAvailable())
                    return true;
                else
                    return false;
            }
            else
                return false;
        }
        else if (nextInstruction.instructionType == "MULT" || nextInstruction.instructionType == "DIV")
        {
            int destinationRegister = std::stoi(nextInstruction.destination.substr(1));
            string FRegReservationStationName = registerResultStatus.getFRegReservationStationName(destinationRegister);

            if (FRegReservationStationName == "" ||
                (FRegReservationStationName != "" && registerResultStatus.isDataReady(FRegReservationStationName)))
            {
                if (reservationStations.isMultStationAvailable())
                    return true;
                else
                    return false;
            }
            else
                return false;
        }
        else if (nextInstruction.instructionType == "LOAD")
        {
            int destinationRegister = std::stoi(nextInstruction.destination.substr(1));
            string FRegReservationStationName = registerResultStatus.getFRegReservationStationName(destinationRegister);

            if (FRegReservationStationName == "" ||
                (FRegReservationStationName != "" && registerResultStatus.isDataReady(FRegReservationStationName)))
            {
                if (reservationStations.isLoadStationAvailable())
                    return true;
                else
                    return false;
            }
            else
                return false;
        }
        else if (nextInstruction.instructionType == "STORE")
        {
            int destinationRegister = std::stoi(nextInstruction.source1.substr(1));
            string FRegReservationStationName = registerResultStatus.getFRegReservationStationName(destinationRegister);

            // Not checking whether destination is available or not as destination is a memory address,
            // We will check whether the data from FReg is available in operands...
            if (FRegReservationStationName != "" && registerResultStatus.isDataReady(FRegReservationStationName))
            {
                if (reservationStations.isStoreStationAvailable())
                    return true;
                else
                    return false;
            }
            else
                return false;
        }
        else
        {
            if (printToConsole)
                cout << "Invalid Instruction!\n";
            return false;
        }
    }
}

// Function to issue a new instruction
bool issueNextInstruction(InstructionQueue &iq, vector<activeInstructionDetails> &activeInstructions,
                          RegisterResultStatuses &registerResultStatus, ReservationStations &reservationStations)
{
    if (canIssueInstruction(iq, registerResultStatus, reservationStations))
    {
        if (printToConsole)
            cout << "Issuing new instruction...\n";

        Instruction newInstruction = iq.instructionQueue.front();

        // iq.printInstruction(newInstruction);
        if (printToConsole)
            cout << "\n";

        if (newInstruction.instructionType == "ADD" || newInstruction.instructionType == "SUB")
        {
            string operand1 =
                registerResultStatus.getFRegReservationStationName(std::stoi(newInstruction.source1.substr(1)));

            string operand2 =
                registerResultStatus.getFRegReservationStationName(std::stoi(newInstruction.source2.substr(1)));

            int destinationRegister = std::stoi(newInstruction.destination.substr(1));

            string FRegReservationStationName = registerResultStatus.getFRegReservationStationName(destinationRegister);

            if (printToConsole)
                cout << "F" << destinationRegister << " : " << FRegReservationStationName << ", "
                     << (registerResultStatus.isDataReady(FRegReservationStationName) ? "Y" : "N") << "\n";

            if (FRegReservationStationName == "" ||
                (FRegReservationStationName != "" && registerResultStatus.isDataReady(FRegReservationStationName)))
            {
                string assignedStation = reservationStations.assignAddStation(newInstruction.instructionType, operand1,
                                                                              operand2, newInstruction.instructionID,
                                                                              activeInstructions, destinationRegister);

                if (printToConsole)
                    cout << "Assigning F" << destinationRegister << "...\n\n";
                registerResultStatus.setFRegReservationStationName(destinationRegister, assignedStation);
            }
            else
            {
                if (printToConsole)
                    cout << "F" << destinationRegister << " is busy right now with "
                         << registerResultStatus.getFRegReservationStationName(destinationRegister) << "...\n\n";
            }
        }
        else if (newInstruction.instructionType == "MULT" || newInstruction.instructionType == "DIV")
        {
            string operand1 =
                registerResultStatus.getFRegReservationStationName(std::stoi(newInstruction.source1.substr(1)));

            string operand2 =
                registerResultStatus.getFRegReservationStationName(std::stoi(newInstruction.source2.substr(1)));

            int destinationRegister = std::stoi(newInstruction.destination.substr(1));

            string FRegReservationStationName = registerResultStatus.getFRegReservationStationName(destinationRegister);

            if (printToConsole)
                cout << "F" << destinationRegister << " : " << FRegReservationStationName << ", "
                     << (registerResultStatus.isDataReady(FRegReservationStationName) ? "Y" : "N") << "\n";

            if (FRegReservationStationName == "" ||
                (FRegReservationStationName != "" && registerResultStatus.isDataReady(FRegReservationStationName)))
            {
                string assignedStation = reservationStations.assignMultStation(newInstruction.instructionType, operand1,
                                                                               operand2, newInstruction.instructionID,
                                                                               activeInstructions, destinationRegister);

                if (printToConsole)
                    cout << "Assigning F" << destinationRegister << "...\n\n";
                registerResultStatus.setFRegReservationStationName(destinationRegister, assignedStation);
            }
            else
            {
                if (printToConsole)
                    cout << "F" << destinationRegister << " is busy right now with "
                         << registerResultStatus.getFRegReservationStationName(destinationRegister) << "...\n\n";
            }
        }
        else if (newInstruction.instructionType == "LOAD")
        {
            string operand1 =
                registerResultStatus.getFRegReservationStationName(std::stoi(newInstruction.source1.substr(1)));

            int destinationRegister = std::stoi(newInstruction.destination.substr(1));

            string FRegReservationStationName = registerResultStatus.getFRegReservationStationName(destinationRegister);

            if (printToConsole)
                cout << "F" << destinationRegister << " : " << FRegReservationStationName << ", "
                     << (registerResultStatus.isDataReady(FRegReservationStationName) ? "Y" : "N") << "\n";

            if (FRegReservationStationName == "" ||
                (FRegReservationStationName != "" && registerResultStatus.isDataReady(FRegReservationStationName)))
            {
                string assignedStation = reservationStations.assignLoadStation(operand1, newInstruction.instructionID,
                                                                               activeInstructions, destinationRegister);

                if (printToConsole)
                    cout << "Assigning F" << destinationRegister << "...\n\n";
                registerResultStatus.setFRegReservationStationName(destinationRegister, assignedStation);
            }
            else
            {
                if (printToConsole)
                    cout << "F" << destinationRegister << " is busy right now with "
                         << registerResultStatus.getFRegReservationStationName(destinationRegister) << "...\n\n";
            }
        }
        else if (newInstruction.instructionType == "STORE")
        {
            // Here the source is a FReg
            string operand1 =
                registerResultStatus.getFRegReservationStationName(std::stoi(newInstruction.source1.substr(1)));

            // Just to check whether the result is ready, I'm assigning destination as source
            int destinationRegister = std::stoi(newInstruction.source1.substr(1));

            string FRegReservationStationName = registerResultStatus.getFRegReservationStationName(destinationRegister);

            if (printToConsole)
                cout << "F" << destinationRegister << " : " << FRegReservationStationName << ", "
                     << (registerResultStatus.isDataReady(FRegReservationStationName) ? "Y" : "N") << "\n";

            // if the result is ready at the destination register... issue the instruction
            if ((FRegReservationStationName != "" && registerResultStatus.isDataReady(FRegReservationStationName)))
            {
                string assignedStation =
                    reservationStations.assignStoreStation(operand1, newInstruction.instructionID, activeInstructions);

                if (printToConsole)
                    cout << "Store Instruction.... no FReg is assigned...\n\n";
                // Invalid FReg (-1) is assigned, as it is waiting for a result from FReg....
                // registerResultStatus.setFRegReservationStationName(destinationRegister, assignedStation);
            }
        }
        else
        {
            if (printToConsole)
                cout << "Invalid Instruction!\n";
            // Popping invalid instruction from the queue...
            iq.instructionQueue.pop();
            return false;
        }

        // Popping instruction from the queue...
        iq.instructionQueue.pop();
        return true;
    }
    else
    {
        if (printToConsole)
            cout << "Instruction cannot be issued right now...\n\n";
        return false;
    }
}

void __printActiveInstructionTable(vector<activeInstructionDetails> &activeInstructions, int totalInstructionCount)
{
    // active instruction details
    cout << "Active Instructions Table: \n";
    for (int idx = 0; idx < totalInstructionCount; idx++)
    {
        cout << idx << "\t";
    }
    cout << "\n";

    for (int idx = 0; idx < totalInstructionCount; idx++)
    {
        cout << activeInstructions[idx].executionComplete << "\t";
    }
    cout << "\n\n";
}

// Function to simulate the Tomasulo algorithm
void simulateTomasulo(InstructionQueue &iq, RegisterResultStatuses &registerResultStatus,
                      ReservationStations &reservationStations, CommonDataBus &cdb,
                      vector<InstructionStatus> &instructionStatus)
{
    if (printToConsole)
        cout << "--------------------------------------------------\n";
    if (printToConsole)
        cout << "Simulating Tomasulo's Algorithm...\n";
    if (printToConsole)
        cout << "--------------------------------------------------\n";

    int thiscycle = 1; // start cycle: 1

    int totalInstructionCount = iq.instructionQueue.size();

    vector<activeInstructionDetails> activeInstructions;
    activeInstructions.resize(totalInstructionCount);

    for (int i = 0; i < totalInstructionCount; i++)
    {
        activeInstructions[i].executionComplete = -1;
        activeInstructions[i].FRegID = -1;
    }

    while (thiscycle < 100000000)
    {
        if (printToConsole)
            cout << "Cycle (" << thiscycle << ")\n";
        if (printToConsole)
            cout << "---------------\n";
        // if (thiscycle > 100)
        //     break;

        int instructionsCompleted = 0;
        // Reservation Stations should be updated every cycle, and broadcast to Common Data Bus

        // Reading CDB... check if this is even required!!!!!
        if (cdb.data == 1)
        {
            if (printToConsole)
                cout << "Data from " << cdb.source << " present on common data bus...\n";
            if (cdb.FDestination >= 0)
            {
                // Setting data ready in registers where the name matches the cdb source
                registerResultStatus.setDataReady(cdb.source);
                if (printToConsole)
                    cout << "Data is for FReg: F" << cdb.FDestination << "\n\n";
            }
        }
        else
        {
            if (printToConsole)
                cout << "No data on common data bus...\n\n";
        }

        if (printToConsole)
            __printActiveInstructionTable(activeInstructions, totalInstructionCount);

        // Execute all active stations that have active instructions
        for (int idx = 0; idx < totalInstructionCount; idx++)
        {
            if (activeInstructions[idx].executionComplete == 0) // Active Instruction Present
            {
                if (printToConsole)
                    cout << "Executing Active Instruction ID: " << idx << "\n";
                if (activeInstructions[idx].stationType == LOAD)
                {
                    reservationStations.checkLoadStationOperands(activeInstructions[idx].reservationStationID);
                    // Load is a special case, here we assume that the operands are always ready,
                    // so we go straight to execution...
                    int executionStatus =
                        reservationStations.executeLoadStation(activeInstructions[idx].reservationStationID);
                    if (executionStatus == 1)
                    {
                        if (printToConsole)
                            cout << activeInstructions[idx].reservationStationName << " execution completed at "
                                 << thiscycle << " cycle...\n";
                        instructionStatus[idx].cycleExecuted = thiscycle;
                        activeInstructions[idx].executionComplete = 1;
                    }
                }
                else if (activeInstructions[idx].stationType == STORE)
                {
                    // Checking for operands...
                    reservationStations.checkStoreStationOperand(activeInstructions[idx].reservationStationID,
                                                                 registerResultStatus);

                    int executionStatus =
                        reservationStations.executeStoreStation(activeInstructions[idx].reservationStationID);
                    if (executionStatus == 1)
                    {
                        if (printToConsole)
                            cout << activeInstructions[idx].reservationStationName << " execution completed at "
                                 << thiscycle << " cycle...\n";
                        instructionStatus[idx].cycleExecuted = thiscycle;
                        activeInstructions[idx].executionComplete = 1;
                    }
                }
                else if (activeInstructions[idx].stationType == ADD || activeInstructions[idx].stationType == SUB)
                {
                    // Checking for operands...
                    reservationStations.checkAddStationOperands(activeInstructions[idx].reservationStationID,
                                                                registerResultStatus);

                    int executionStatus =
                        reservationStations.executeAddStation(activeInstructions[idx].reservationStationID);
                    if (executionStatus == 1)
                    {
                        if (printToConsole)
                            cout << activeInstructions[idx].reservationStationName << " execution completed at "
                                 << thiscycle << " cycle...\n";
                        instructionStatus[idx].cycleExecuted = thiscycle;
                        activeInstructions[idx].executionComplete = 1;
                    }
                }
                else if (activeInstructions[idx].stationType == MULT || activeInstructions[idx].stationType == DIV)
                {
                    // Checking for operands...
                    reservationStations.checkMultStationOperands(activeInstructions[idx].reservationStationID,
                                                                 registerResultStatus);

                    int executionStatus =
                        reservationStations.executeMultStation(activeInstructions[idx].reservationStationID);
                    if (executionStatus == 1)
                    {
                        if (printToConsole)
                            cout << activeInstructions[idx].reservationStationName << " execution completed at "
                                 << thiscycle << " cycle...\n";
                        instructionStatus[idx].cycleExecuted = thiscycle;
                        activeInstructions[idx].executionComplete = 1;
                    }
                }
            }
        }

        if (printToConsole)
            cout << "All active instructions executed...\n\n";

        // setting common data bus free for broadcasting...
        if (printToConsole)
            cout << "Clearing the common data bus...\n\n";
        cdb.clear();

        // Execute all active stations that have active instructions that have been executed...
        for (int idx = 0; idx < totalInstructionCount; idx++)
        {
            if (activeInstructions[idx].executionComplete == 1)
            {
                if (printToConsole)
                    cout << "Executing Completed Instruction ID: " << idx << "\n";
                if (cdb.data == 0)
                {
                    if (activeInstructions[idx].stationType != STORE)
                    {
                        if (printToConsole)
                            cout << activeInstructions[idx].reservationStationName << " data broadcasted at "
                                 << thiscycle << " cycle...\n";
                        cdb.broadcast(1, activeInstructions[idx].reservationStationName,
                                      activeInstructions[idx].FRegID);
                        activeInstructions[idx].executionComplete = 2;
                    }
                    else
                    {
                        // In store instruction we don't broadcast anything to common data bus...
                        activeInstructions[idx].executionComplete = 2;
                    }
                }
                else
                {
                    if (printToConsole)
                        cout << "common data bus is busy...\n";
                }
            }
            else if (activeInstructions[idx].executionComplete == 2)
            {
                if (printToConsole)
                    cout << "All steps completed for Instruction ID: " << idx << "\n";

                // Unassignining reservation station...
                if (activeInstructions[idx].stationType == LOAD)
                {
                    reservationStations.unassignLoadStation(activeInstructions[idx].reservationStationID);
                }
                else if (activeInstructions[idx].stationType == STORE)
                {
                    reservationStations.unassignStoreStation(activeInstructions[idx].reservationStationID);
                }
                else if (activeInstructions[idx].stationType == ADD || activeInstructions[idx].stationType == SUB)
                {
                    reservationStations.unassingAddStation(activeInstructions[idx].reservationStationID);
                }
                else if (activeInstructions[idx].stationType == MULT || activeInstructions[idx].stationType == DIV)
                {
                    reservationStations.unassingMultStation(activeInstructions[idx].reservationStationID);
                }

                instructionStatus[idx].cycleWriteResult = thiscycle;
                activeInstructions[idx].executionComplete = 3;
            }
            else if (activeInstructions[idx].executionComplete == 3)
            {
                instructionsCompleted++;
            }
        }

        // Issue new instruction in each cycle
        if (issueNextInstruction(iq, activeInstructions, registerResultStatus, reservationStations))
        {
            // if a new instruction is issued, update the status for new instruction in the status table...
            InstructionStatus newInstructionStatus;

            newInstructionStatus.cycleIssued = thiscycle;
            newInstructionStatus.cycleExecuted = 0;
            newInstructionStatus.cycleWriteResult = 0;

            instructionStatus.push_back(newInstructionStatus);
        }

        if (instructionsCompleted == totalInstructionCount)
            break;

        // At the end of this cycle, we need this function to print all registers status for grading
        PrintRegisterResultStatus4Grade(outputtracename, registerResultStatus, thiscycle);

        ++thiscycle;

        // The simulator should stop when all instructions are finished.
        // ...
    }
};

/*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/

void __printHardwareConfig(HardwareConfig config)
{
    cout << "# Load Reservation Stations     : " << config.LoadRSsize << "\n";
    cout << "# Store Reservation Stations    : " << config.StoreRSsize << "\n";
    cout << "# Add Reservation Stations      : " << config.AddRSsize << "\n";
    cout << "# Multiply Reservation Stations : " << config.MultRSsize << "\n";
    cout << "# Floating Point Registers      : " << config.FRegSize << "\n";
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        hardwareconfigname = argv[1];
        inputtracename = argv[2];
    }

    HardwareConfig hardwareConfig;
    std::ifstream config;
    config.open(hardwareconfigname);
    config >> hardwareConfig.LoadRSsize;  // number of load reservation stations
    config >> hardwareConfig.StoreRSsize; // number of store reservation stations
    config >> hardwareConfig.AddRSsize;   // number of add reservation stations
    config >> hardwareConfig.MultRSsize;  // number of multiply reservation stations
    config >> hardwareConfig.FRegSize;    // number of fp registers
    config.close();

    /*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/
    // Printing hardware config to console...
    if (printToConsole)
    {
        cout << "----- Tomasulo's Algorithm -----\n";
        __printHardwareConfig(hardwareConfig);
        cout << "\n";
    }
    // Read instructions from a file (replace 'instructions.txt' with your file name)

    // Initializing an InstructionQueue Object
    InstructionQueue iq;

    // Reading the instructions from the file and populating the queue
    iq.readInstructionsFromFile(inputtracename);

    // Initializing the register result status
    RegisterResultStatuses registerResultStatus = RegisterResultStatuses(hardwareConfig);

    // Initializing the reservation stations
    ReservationStations reservationStations = ReservationStations(hardwareConfig);

    // Initialisze the common data bus
    CommonDataBus cdb = CommonDataBus();

    // Initialize the instruction status table
    vector<InstructionStatus> instructionStatus;

    // Simulate Tomasulo:
    simulateTomasulo(iq, registerResultStatus, reservationStations, cdb, instructionStatus);

    /*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/

    // At the end of the program, print Instruction Status Table for grading
    PrintResult4Grade(outputtracename, instructionStatus);

    return 0;
}
