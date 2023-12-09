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

#include "test.h"
#include <fstream>
#include <vector>
#include "MyMessage_m.h"
#include <bits/stdc++.h>

Define_Module(Test);
std::vector<std::pair<std::string, std::string>> dataVector;
int sf, sn, sl;
int rf, rn, rl;
/*vector<MyMessage_Base *> receiverFinalResult;
vector<MyMessage_Base *> receiverBuffer ;
vector<bool> arrived(3);
vector<int> sequenceArrived(3);
vector<bool> arrivedSender(3);
vector<int> sequenceArrivedSender(3);*/

void modifyMessage(std::string &message, int index, int modificationBit)
{

    message[index] ^= 1 << (7 - modificationBit);
}

std::string byteStuffing(std::string message)
{
    std::string result = "#";
    for (int i = 0; i < message.size(); i++)
    {
        if (message[i] == '#' || message[i] == '/')
        {
            result += '/';
        }
        result += message[i];
    }
    result += '#';
    return result;
}

std::string byteDeStuffing(std::string message)
{
    std::string result = "";
    bool takeNext = false;
    for (int i = 1; i < message.size() - 1; i++)
    {
        if (takeNext)
        {
            result += message[i];
            takeNext = false;
            continue;
        }
        if (message[i] == '/')
        {
            takeNext = true;
            continue;
        }
        result += message[i];
    }
    return result;
}

char calcCheckSum(std::string message)
{
    char result = 0;
    for (int i = 0; i < message.size(); i++)
    {
        result ^= message[i];
        EV << "Character used: " << message[i] << endl;
        EV << "Result: " << result << endl;
    }
    return result;
}
void Test::initialize()
{
    std::ifstream inputFile("input.txt");
    std::vector<std::string> textInput;

    if (!inputFile.is_open())
    {
        EV << "Error opening file!" << endl;
        return;
    }

    std::string line;
    while (std::getline(inputFile, line))
    {
        char number[4];
        char restOfString[256];

        if (std::sscanf(line.c_str(), "%s %255[^\n]", &number, restOfString) == 2)
        {
            dataVector.push_back(std::make_pair(number, std::string(restOfString)));
        }
    }

    // sending the first message
    if (strcmp("sender", getName()) == 0)
    {
        std::string initalString = dataVector[0].second;
        std::string errors = dataVector[0].first;

        char checkSum = calcCheckSum(initalString);
        EV << "checksum is: " << checkSum << endl;

        int index = -1, modificationBit;
        if (errors[3] == '1')
        {
            EV << " introducing modification " << endl;
            modificationBit = int(uniform(0, 7));
            index = int(uniform(0, initalString.size()));
            modifyMessage(initalString, index, modificationBit);
        }

        std::string message = byteStuffing(initalString);
        EV << "sent message is: " << message << endl;

        MyMessage_Base *msg = new MyMessage_Base("message");
        msg->setM_Payload(message.c_str());
        msg->setMycheckbits(checkSum);

        double PT = getParentModule()->par("PT");
        double TD = getParentModule()->par("TD");
        double ED = getParentModule()->par("ED");
        double DD = getParentModule()->par("DD");

        EV << "At time " << simTime() << ", Node " << getName() << ", Introducing channel error with code = " << errors << endl;
        EV << "At time " << simTime() + PT << ", Node " << getName() << " sent frame with seq_num = and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << errors[1] << ", Duplicate " << errors[2] << ", Delay " << (errors[3] ? ED : 0) << "." << endl;
        if (errors[2] == '1')
        {
            EV << "At time " << simTime() + PT + DD << ", Node " << getName() << "sent frame with seq_num = and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << errors[1] << ", Duplicate " << 2 << ", Delay " << (errors[3] ? ED : 0) << "." << endl;
        }

        if (errors[1] == '1')
        {
            //loss
            return;
        }

        double totalDelay = PT + TD;

        if (errors[3] == '1')
        {
            // delay
            totalDelay += ED;
        }
        scheduleAt(simTime() + totalDelay, msg);

        if (errors[2] == '1')
        {
            // duplication
            totalDelay += DD;
            scheduleAt(simTime() + totalDelay, msg->dup());
        }
    }
}

void Test::handleMessage(cMessage *msg)
{
    EV << "I am in handle message" << endl;
    MyMessage_Base *data = check_and_cast<MyMessage_Base *>(msg);
    // TODO - Generated method body
    if (strcmp("receiver", getName()) == 0)
    {
        std::string stuffedPayload = data->getM_Payload();
        std::string payload = byteDeStuffing(stuffedPayload);
        char receiverCheckSum = calcCheckSum(payload);
        EV << "receiver checksum is: " << receiverCheckSum << endl;

        if (receiverCheckSum == data->getMycheckbits())
        {
            EV << "Correct message received !!" << endl;
        }
        else
        {
            EV << "Incorrect message received " << endl;
        }
        EV << "received message before destuffing: " << stuffedPayload << endl;
        EV << "received message after destuffing: " << payload << endl;
    }
    else
    {
        // Sender
        EV << "SELF MESSAGE RECEIVED" << endl;
        send(data, "out");
    }
}
