//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include "Coordinator.h"
#include <fstream>
#include <vector>
#include "MyMessage_m.h"
#include <bits/stdc++.h>

Define_Module(Coordinator);

int node_id, start_time;

void readFile()
{
    std::ifstream inputFile("coordinator.txt");
    std::vector<std::string> textInput;

    if (!inputFile.is_open())
    {
        EV << "Error opening file!" << endl;
        return;
    }

    std::string line;
    std::getline(inputFile, line);
    std::sscanf(line.c_str(), "%d %d]", &node_id, &start_time);

    inputFile.close();
}

void Coordinator::initialize()
{
    // TODO - Generated method body
    readFile();
    MyMessage_Base *msg = new MyMessage_Base(("Node" + std::to_string(node_id)).c_str());
    scheduleAt(simTime() + start_time, msg);
}

void Coordinator::handleMessage(cMessage *dummy)
{
    // TODO - Generated method body
    std::ofstream file;
    std::cout << "attempting to open file\n";
    file.open("output.txt", std::ios::out | std::ios::trunc);
    if (!file.is_open())
    {
        std::cout << "Did not open file!\n";
        printf("Error in opening output.txt in %s", getName());
        exit(-1);
    }
    std::cout << "File opened successfully!\n";
    file.close();
    MyMessage_Base *msg = check_and_cast<MyMessage_Base *>(dummy);
    if (strcmp(msg->getName(), "Node0") == 0)
        send(msg, "out0");
    else
        send(msg, "out1");
}
