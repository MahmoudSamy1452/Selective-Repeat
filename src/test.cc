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
#define DATA 0
#define ACK 1
#define NACK 2
int WS, WR;
double PT, DD, TD, ED, TO;

// used at sender
std::queue<std::pair<std::string, std::string>> senderNetworkLayer;
std::vector<std::pair<std::string, std::string>> out_buff;
std::vector<MyMessage_Base *> time_out; // store messages to cancel them if ack is received
int ack_expected, next_frame_to_send, max_seq_s;

// used at receiver
std::queue<std::pair<std::string, std::string>> receiverNetworkLayer;
std::vector<std::string> in_buff; // size is retrieved as parameter from omnetpp.ini
std::vector<bool> arrival;
int frame_expected, too_far, max_seq_r;
bool no_nak;

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
            senderNetworkLayer.push(std::make_pair(number, std::string(restOfString)));
        }
    }
    inputFile.close();
}

void modifyMessage(std::string &message, int index, int modificationBit)
{
    message[index] ^= 1 << (7 - modificationBit);
}

bool between(int a, int b, int c)
{
    return (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)));
}

void circularSum(int &value, int size)
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
    }
    return result;
}

void Test::sendFrame(int frameKind, int frame_nr, bool resend, int n)
{
    // for (int i = 0; i < out_buff.size(); i++)
    // {
    //     EV << "out_buff[" << i << "]: " << out_buff[i].second << endl;
    // }
    if (frameKind == DATA) // data, only sender will enter here
    {
        std::string initialString = out_buff[frame_nr % WS].second;
        std::string errors = out_buff[frame_nr % WS].first;

        char checkSum = calcCheckSum(initialString);

        int index = -1, modificationBit;

        if (errors[0] == '1' && !resend) // Modification
        {
            modificationBit = int(uniform(0, 7));
            index = int(uniform(0, initialString.size()));
            modifyMessage(initialString, index, modificationBit);
        }

        std::string message = byteStuffing(initialString);

        MyMessage_Base *msg = new MyMessage_Base("send");
        msg->setM_Type(frameKind);
        msg->setM_Payload(message.c_str());
        msg->setMycheckbits(checkSum);
        msg->setSeq_Num(frame_nr);
        time_out[frame_nr % WS]->setSeq_Num(frame_nr);

        EV << "At time " << simTime() + (n - 1) * 0.5 << ", " << getName() << ", Introducing channel error with code = " << errors << " and msg = " << initialString << endl;
        double totalDelay = PT * n;
        EV << "At time " << simTime() + totalDelay << ", " << getName() << " sent frame with seq_num = " << frame_nr << " and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << ((errors[1] - '0') ? "Yes" : "No") << ", Duplicate " << errors[2] << ", Delay " << (errors[3] - '0' ? ED : 0) << "." << endl;
        if (errors[2] == '1' && !resend)
        {
            EV << "At time " << simTime() + totalDelay + DD << ", Node " << getName() << "sent frame with seq_num = " << frame_nr << " and payload = " << message << " and trailer = " << std::bitset<8>(checkSum) << ", Modified " << ((index == -1) ? -1 : index * 8 + modificationBit + 1) << ", Lost " << errors[1] << ", Duplicate " << 2 << ", Delay " << (errors[3] - '0' ? ED : 0) << "." << endl;
        }

        scheduleAt(simTime() + TO + totalDelay, time_out[frame_nr % WS]);

        if (errors[3] == '1' && !resend)
        {
            // delay
            totalDelay += ED;
        }

        totalDelay += TD;
        if (errors[1] == '1' && !resend)
        {
            // loss
            return;
        }

        scheduleAt(simTime() + totalDelay, msg);

        if (errors[2] == '1' && !resend)
        {
            // duplication
            totalDelay += DD;
            scheduleAt(simTime() + totalDelay, msg->dup());
        }
    }
    else if (frameKind == ACK)
    {
        EV << "At time " << simTime() + PT << ", " << getName() << " Sending Ack with number " << frame_nr << ", loss No";
        MyMessage_Base *msg = new MyMessage_Base("send");
        msg->setM_Payload("ack");
        msg->setM_Type(frameKind);
        msg->setSeq_Num(frame_nr);
        scheduleAt(simTime() + PT + TD, msg);
    }
    else if (frameKind == NACK)
    {
        EV << "At time " << simTime() + PT << ", " << getName() << "Sending Nack with number " << frame_nr << ", loss No";
        MyMessage_Base *msg = new MyMessage_Base("send");
        msg->setM_Payload("nack");
        msg->setM_Type(frameKind);
        msg->setSeq_Num(frame_nr);
        scheduleAt(simTime() + PT + TD, msg);
        no_nak = false;
    }
}

void Test::initialize()
{
    // Resize buffers according to parameter WS
    WS = getParentModule()->par("WS");
    WR = getParentModule()->par("WR");
    PT = getParentModule()->par("PT");
    DD = getParentModule()->par("DD");
    TD = getParentModule()->par("TD");
    ED = getParentModule()->par("ED");
    TO = getParentModule()->par("TO");
    max_seq_s = (WS * 2);
    max_seq_r = (WR * 2);
    in_buff.resize(WR);
    arrival.resize(WR);
    out_buff.resize(WS);
    ack_expected = 0;
    next_frame_to_send = 0;
    frame_expected = 0;
    too_far = WR;
    no_nak = true;
}

void Test::handleMessage(cMessage *dummy)
{
    MyMessage_Base *msg = check_and_cast<MyMessage_Base *>(dummy);

    if (strcmp(msg->getName(), getName()) == 0)
    {
        readFile("input" + std::to_string((msg->getName())[4] - '0') + ".txt");

        time_out.resize(WS);
        for (int i = 0; i < WS; i++)
        {
            time_out[i] = new MyMessage_Base("timeout");
        }

        for (int i = 0; senderNetworkLayer.size() && i < WS; i++)
        {
            std::pair<std::string, std::string> temp = senderNetworkLayer.front();
            senderNetworkLayer.pop();
            out_buff[i] = temp;
            sendFrame(DATA, next_frame_to_send, false, i + 1);
            circularSum(next_frame_to_send, max_seq_s);
        }
    }
    else if (strcmp(msg->getName(), "send") == 0) // Sender sending a message to itself for introducing delays
    {
        msg->setName("message");
        send(msg, "out");
    }
    else if (strcmp(msg->getName(), "message") == 0) // Receiver receives message from sender
    {
        if (msg->getM_Type() == DATA)
        {
            std::string stuffedPayload = msg->getM_Payload();
            std::string payload = byteDeStuffing(stuffedPayload);
            char receiverCheckSum = calcCheckSum(payload);

            EV << "At time " << simTime() << ", " << getName() << " received frame with seq_num = " << msg->getSeq_Num() << " and payload = " << msg->getM_Payload() << " and trailer = " << std::bitset<8>(msg->getMycheckbits()) << ", Modified " << ((receiverCheckSum == msg->getMycheckbits()) ? -1 : 1) << " Lost No, Duplicate " << (arrival[msg->getSeq_Num()] ? 2 : 1) << ", Delay 0\n";

            if (receiverCheckSum == msg->getMycheckbits())
            {
                if (msg->getSeq_Num() != frame_expected && no_nak)
                    sendFrame(NACK, frame_expected);
                if (between(frame_expected, msg->getSeq_Num(), too_far) && arrival[msg->getSeq_Num() % WR] == false)
                {
                    in_buff[msg->getSeq_Num() % WR] = payload;
                    arrival[msg->getSeq_Num() % WR] = true;
                }
            }
            else
            {
                if (msg->getSeq_Num() == frame_expected)
                {
                    sendFrame(NACK, frame_expected);
                }
            }

            bool send_ack = false;
            while (arrival[frame_expected % WR])
            {
                receiverNetworkLayer.push(std::make_pair("", in_buff[frame_expected % WR]));
                arrival[frame_expected % WR] = false;
                no_nak = true;
                circularSum(frame_expected, max_seq_r);
                circularSum(too_far, max_seq_r);
                send_ack = true;
            }
            if (send_ack)
                sendFrame(ACK, frame_expected);
        }
        else if (msg->getM_Type() == ACK)
        {
            int count = 1;
            while (between(ack_expected, (msg->getSeq_Num() + max_seq_s) % (max_seq_s + 1), next_frame_to_send))
            {
                cancelEvent(time_out[ack_expected % WS]);
                if (!senderNetworkLayer.empty())
                {
                    std::pair<std::string, std::string> temp = senderNetworkLayer.front();
                    senderNetworkLayer.pop();
                    out_buff[ack_expected % WS] = temp; // next_frame_to_send % WS
                    sendFrame(DATA, next_frame_to_send, false, count);
                    count++;
                    circularSum(next_frame_to_send, max_seq_s);
                }
                circularSum(ack_expected, max_seq_s);
            }
        }
        else if (msg->getM_Type() == NACK)
        {
            cancelEvent(time_out[msg->getSeq_Num() % WS]);
            sendFrame(DATA, msg->getSeq_Num(), true);
        }
    }
    else if (strcmp(msg->getName(), "timeout") == 0)
    {
        EV << "Time out event at time " << simTime() << ", at " << getName() << " for frame with seq_num = " << msg->getSeq_Num();
        sendFrame(DATA, msg->getSeq_Num(), true);
    }
}