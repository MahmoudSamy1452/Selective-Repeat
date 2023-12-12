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
std::vector<std::pair<std::string, std::string>> in_buff;       //size is retrieved as parameter from omnetpp.ini
std::vector<std::pair<std::string, std::string>> out_buff;      //size is retrieved as parameter from omnetpp.ini
std::queue<std::pair<std::string,std::string>> networkLayer;
int WS, WR, max_seq_s, max_seq_r;                               //retrieved as parameter from omnetpp.ini (ws)
int ack_expected, next_frame_to_send;
int frame_expected, too_far;
bool no_nak;                                                    //at receiver side
double PT, DD, TD, ED;
#define DATA 0
#define ACK 1
#define NACK 2

/*vector<MyMessage_Base *> receiverFinalResult;
vector<MyMessage_Base *> receiverBuffer ;
vector<bool> arrived(3);
vector<int> sequenceArrived(3);
vector<bool> arrivedSender(3);
vector<int> sequenceArrivedSender(3);*/

void readFile(std::string file_name)
{
    std::ifstream inputFile(file_name);
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
            networkLayer.push(std::make_pair(number, std::string(restOfString)));
        }
    }

    inputFile.close();
}

void readFile(std::string file_name)
{
    std::ifstream inputFile(file_name);
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
            networkLayer.push(std::make_pair(number, std::string(restOfString)));
        }
    }

    inputFile.close();
}

void modifyMessage(std::string &message,int index,int modificationBit)
{
    message[index] ^= 1 << (7 - modificationBit);
}

bool between(int a, int b, int c)
{
    return (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)));
}

void circularSum (int& value, int size)
{
    value = (value + 1) % size;
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

void sendFrame(int frameKind, int frame_nr, bool resend = false)//0 is data, 1 is ack, 2 is nack
{
    if(frameKind == DATA)// data, only sender will enter here 
    {
        //data
        std::string initalString = out_buff[frame_nr].second;
        std::string errors = out_buff[frame_nr].first;

        char checkSum = calcCheckSum(initalString);
        EV << "checksum is: "<<checkSum<<endl;

        if(errors[3] == '1' && !resend)
        {
            EV<<" introducing modification "<<endl;
            int modificationBit = int(uniform(0, 7));
            int index = int(uniform(0, initalString.size()));
            modifyMessage(initalString,index,modificationBit);
        }

        std::string message = byteStuffing(initalString);
        EV<<"sent message is: " << message<<endl;

        MyMessage_Base * msg = new MyMessage_Base("message");
        msg->setM_Payload(message.c_str());
        msg->setMycheckbits(checkSum);
        storedMsg = msg;

        double PT = getParentModule()->par("PT");
        double DD = getParentModule()->par("DD");
        double TD = getParentModule()->par("TD");
        double ED = getParentModule()->par("ED");

        if(errors[0] == '1' && !resend)
        {
            //delay

            EV <<"PT is: "<<PT<<endl;
            scheduleAt(simTime()+PT, msg);
        } else

        if(errors[1] == '1' && !resend)
        {
            EV << "At time " << simTime() + PT + DD << ", Node " << getName() << "sent frame with seq_num = and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << errors[1] << ", Duplicate " << 2 << ", Delay " << (errors[3] ? ED : 0) << "." << endl;
        }
        if(errors[2] == '1' && !resend)
        {
            //loss
        }
        send(msg, "out");
    }
    else if(frameKind == ACK) 
    {
        //ack
        MyMessage_Base * msg = new MyMessage_Base("ack");
        msg->setM_Payload("ack");
        send(msg, "out");
    }
    else if(frameKind == NACK)
    {
        //nack
        MyMessage_Base * msg = new MyMessage_Base("nack");
        msg->setM_Payload("nack");
        send(msg, "out");
    }
}

void Test::initialize()
{
    // Resize buffers according to parameter WS
    WS = getParentModule()->par("WS");     
    WR = getParentModule()->par("WR");
    max_seq_s = (WS * 2);       //max sequence number for sender
    max_seq_r = (WR * 2);       //max sequence number for receiver
    inBuffer.resize(WS);
    outBuffer.resize(WR);
    ack_expected = 0;
    next_frame_to_send = 0;
    frame_expected = 0;
    too_far = (WR + 1)/2;
    no_nak = true;

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
