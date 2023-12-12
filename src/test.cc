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
std::vector<MyMessage_Base *> time_out;                               //store messages to cancel them if ack is received
std::queue<std::pair<std::string,std::string>> networkLayer;
int WS, WR, max_seq_s, max_seq_r;                               //retrieved as parameter from omnetpp.ini (ws)
int TO;
int ack_expected, next_frame_to_send;
int frame_expected, too_far;
int nbuffered;
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

void Test::sendFrame(int frameKind, int frame_nr, bool resend)      //0 is data, 1 is ack, 2 is nack
{
    if(frameKind == DATA)// data, only sender will enter here 
    {
        //data
        std::string initalString = out_buff[frame_nr % WS].second;
        std::string errors = out_buff[frame_nr % WS].first;

        char checkSum = calcCheckSum(initalString);
        EV << "checksum is: "<<checkSum<<endl;

        int index = -1, modificationBit;

        if(errors[0] == '1' && !resend) // Modification
        {
            EV<<" introducing modification "<<endl;
            modificationBit = int(uniform(0, 7));
            index = int(uniform(0, initalString.size()));
            modifyMessage(initalString,index,modificationBit);
        }

        std::string message = byteStuffing(initalString);
        EV<<"sent message is: " << message<<endl;

        MyMessage_Base * msg = new MyMessage_Base("send");
        msg->setM_Payload(message.c_str());
        msg->setMycheckbits(checkSum);
        msg->setSeq_Num(frame_nr); 

        double PT = getParentModule()->par("PT");
        double DD = getParentModule()->par("DD");
        double TD = getParentModule()->par("TD");
        double ED = getParentModule()->par("ED");

        EV << "At time " << simTime() << ", Node " << getName() << ", Introducing channel error with code = " << errors << endl;
        EV << "At time " << simTime() + PT << ", Node " << getName() << " sent frame with seq_num = and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << errors[1] << ", Duplicate " << errors[2] << ", Delay " << (errors[3] ? ED : 0) << "." << endl;
        if (errors[2] == '1' && !resend)
        {
            EV << "At time " << simTime() + PT + DD << ", Node " << getName() << "sent frame with seq_num = and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << errors[1] << ", Duplicate " << 2 << ", Delay " << (errors[3] ? ED : 0) << "." << endl;
        }

        if (errors[1] == '1' && !resend)
        {
            //loss
            return;
        }

        double totalDelay = PT + TD;

        if (errors[3] == '1' && !resend)
        {
            // delay
            totalDelay += ED;
        }
        scheduleAt(simTime() + totalDelay, msg);

        if (errors[2] == '1' && !resend)
        {
            // duplication
            totalDelay += DD;
            scheduleAt(simTime() + totalDelay, msg->dup());
        }

        // EV << "At time " << simTime() << ", Node " << getName() << ", Introducing channel error with code = " << errors << endl;
        // EV << "At time " << simTime() + PT << ", Node " << getName() << " sent frame with seq_num = and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << errors[1] << ", Duplicate " << errors[2] << ", Delay " << (errors[3] ? ED : 0) << "." << endl;
        
        // if(errors[0] == '1' && !resend)
        // {
        //     //modification

        //     EV <<"PT is: "<<PT<<endl;
        //     scheduleAt(simTime()+PT, msg);
        // }

        // if(errors[1] == '1' && !resend)
        // {
        //     //loss
        //     EV << "At time " << simTime() + PT + DD << ", Node " << getName() << "sent frame with seq_num = and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << errors[1] << ", Duplicate " << 2 << ", Delay " << (errors[3] ? ED : 0) << "." << endl;
        //     return;
        // }
        // if(errors[2] == '1' && !resend)
        // {
        //     // duplication
        //     totalDelay += DD;
        //     scheduleAt(simTime() + totalDelay, msg->dup());
        //     EV << "At time " << simTime() + PT + DD << ", Node " << getName() << "sent frame with seq_num = and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << errors[1] << ", Duplicate " << 2 << ", Delay " << (errors[3] ? ED : 0) << "." << endl;
        // }
        // send(msg, "out");
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
    in_buff.resize(max_seq_r);
    out_buff.resize(max_seq_s);
    ack_expected = 0;
    next_frame_to_send = 0;
    frame_expected = 0;
    too_far = (WR + 1)/2;
    no_nak = true;
    TO = getParentModule()->par("TO");
    nbuffered = 0;

    // sending the first message
    // if (strcmp("sender", getName()) == 0)
    // {
    //     std::string initalString = dataVector[0].second;
    //     std::string errors = dataVector[0].first;

    //     char checkSum = calcCheckSum(initalString);
    //     EV << "checksum is: " << checkSum << endl;

    //     int index = -1, modificationBit;
    //     if (errors[3] == '1')
    //     {
    //         EV << " introducing modification " << endl;
    //         modificationBit = int(uniform(0, 7));
    //         index = int(uniform(0, initalString.size()));
    //         modifyMessage(initalString, index, modificationBit);
    //     }

    //     std::string message = byteStuffing(initalString);
    //     EV << "sent message is: " << message << endl;

    //     MyMessage_Base *msg = new MyMessage_Base("message");
    //     msg->setM_Payload(message.c_str());
    //     msg->setMycheckbits(checkSum);

    //     double PT = getParentModule()->par("PT");
    //     double TD = getParentModule()->par("TD");
    //     double ED = getParentModule()->par("ED");
    //     double DD = getParentModule()->par("DD");

    //     EV << "At time " << simTime() << ", Node " << getName() << ", Introducing channel error with code = " << errors << endl;
    //     EV << "At time " << simTime() + PT << ", Node " << getName() << " sent frame with seq_num = and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << errors[1] << ", Duplicate " << errors[2] << ", Delay " << (errors[3] ? ED : 0) << "." << endl;
    //     if (errors[2] == '1')
    //     {
    //         EV << "At time " << simTime() + PT + DD << ", Node " << getName() << "sent frame with seq_num = and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << errors[1] << ", Duplicate " << 2 << ", Delay " << (errors[3] ? ED : 0) << "." << endl;
    //     }

    //     if (errors[1] == '1')
    //     {
    //         //loss
    //         return;
    //     }

    //     double totalDelay = PT + TD;

    //     if (errors[3] == '1')
    //     {
    //         // delay
    //         totalDelay += ED;
    //     }
    //     scheduleAt(simTime() + totalDelay, msg);

    //     if (errors[2] == '1')
    //     {
    //         // duplication
    //         totalDelay += DD;
    //         scheduleAt(simTime() + totalDelay, msg->dup());
    //     }
    // }
}

void Test::handleMessage(cMessage* dummy)
{
    EV << "I am in handle message" << endl;
    MyMessage_Base *msg = check_and_cast<MyMessage_Base *>(dummy);

    if (strcmp(msg->getName(), getName()) == 0)
    {
        // std::ifstream inputFile("input.txt");
        // std::vector<std::string> textInput;

        // if (!inputFile.is_open())
        // {
        //     EV << "Error opening file!" << endl;
        //     return;
        // }

        // std::string line;
        // while (std::getline(inputFile, line))
        // {
        //     char number[4];
        //     char restOfString[256];

        //     if (std::sscanf(line.c_str(), "%s %255[^\n]", &number, restOfString) == 2)
        //     {
        //         dataVector.push_back(std::make_pair(number, std::string(restOfString)));
        //     }
        // }

        // El Mafrood hena t read el file 3ala 7asab law ana input0 aw input1
        readFile("input1.txt");     //Input0 aw 1 3ala 7asab

        time_out.resize(networkLayer.size());
        for(int i = 0; i < max_seq_s; i++)
        {
            time_out[i] = new MyMessage_Base("timeout");
            time_out[i]->setSeq_Num(i);
        }

        // Emla el buffer bta3 el sender
        for (int i = 0; i < max_seq_s; i++)
        {
            std::pair<std::string, std::string> temp = networkLayer.front();
            networkLayer.pop();
            out_buff[i] = temp;
            nbuffered++;
        }

        // Since the network layer is initially empty, send all the frames in the window
        for (int i = 0; i < WS; i++)
        {
            sendFrame(DATA, next_frame_to_send, false);
            circularSum(next_frame_to_send, max_seq_s);
        }
    }

    if (strcmp(msg->getName(), "send") == 0) // Sender sending a message to itself for introducing delays
    {
        EV << "SELF MESSAGE RECEIVED" << endl;
        msg->setName("message");
        send(msg, "out");
        msg->setName("timeout");
        scheduleAt(simTime() + TO, time_out[msg->getSeq_Num()]);
    }
    else if (strcmp(msg->getName(), "message") == 0) // Receiver receives message from sender
    {
        std::string stuffedPayload = msg->getM_Payload();
        std::string payload = byteDeStuffing(stuffedPayload);
        char receiverCheckSum = calcCheckSum(payload);
        EV << "receiver checksum is: " << receiverCheckSum << endl;

        if (receiverCheckSum == msg->getMycheckbits())
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
    else if (strcmp(msg->getName(), "ack") == 0)
    {
        cancelAndDelete(time_out[msg->getSeq_Num()]);
        nbuffered--;
        if(!networkLayer.empty())
        {
            std::pair<std::string, std::string> temp = networkLayer.front();
            networkLayer.pop();
            out_buff[ack_expected] = temp;
            nbuffered++;
            sendFrame(DATA, next_frame_to_send, false);
        }
        if(nbuffered)
            circularSum(next_frame_to_send, max_seq_s);
        circularSum(ack_expected, max_seq_s);
    }  
    else if (strcmp(msg->getName(), "nack") == 0) //Timeout message
    {
        cancelAndDelete(time_out[ack_expected]);
        sendFrame(DATA, ack_expected, true);
    }
    else
    {
        sendFrame(DATA, msg->getSeq_Num(), true);
    }
}