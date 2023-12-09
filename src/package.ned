package networks_project;

import networks_project.Test;


@license(LGPL);
//
// TODO documentation
//
network Network
{
    parameters:
        double PT;
    	double TD;
        double ED;
        double DD;
        int WS;
        int WR;
        double TO;
    submodules:
        sender: Test {
            @display("p=77,88");
        }
        receiver: Test {
            @display("p=169,88");
        }
    connections:
        sender.out --> receiver.in;
        receiver.out --> sender.in;
}

