## Kernel OOPS Trace Analysis

![Alt text](oops-trace.png?raw=true "OOPS Trace")

1. First line in the trace showing the root cause of the oops which is here a null pointer dereference:

    Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000

2. The trace shows all the modules loaded which are:
    Modules linked in: hello(O) faulty(O) 8021q garp mrp stp llc scull(O) ipv6

3. The call trace section shows the faulty function call, the line or the address of the instruction that caused the oops and the module name which is here:

    faulty_write+0x10/0x20 [faulty]

4. Further more the trace shows the systemcalls that followed until the oops happened.
