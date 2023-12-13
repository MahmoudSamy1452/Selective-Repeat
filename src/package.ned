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
        Node0: Test {
            @display("p=77,88");
        }
        Node1: Test {
            @display("p=169,88");
        }
        coordinator: Coordinator {
            @display("p=116,198");
        }
    connections:
        Node0.out --> Node1.in;
        Node1.out --> Node0.in;
        coordinator.out1 --> Node1.inCor;
        coordinator.out0 --> Node0.inCor;
}

