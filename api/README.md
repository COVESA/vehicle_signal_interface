**(C) 2016 Jaguar Land Rover - All rights reserved.**<br>

All documents in this repository are licensed under the Creative
Commons Attribution 4.0 International (CC BY 4.0). Click
[here](https://creativecommons.org/licenses/by/4.0/) for details.
All code in this repository is licensed under Mozilla Public License
v2 (MPLv2). Click [here](https://www.mozilla.org/en-US/MPL/2.0/) for
details.

# VEHICLE SIGNAL INTERFACE API
This repository contains the API required to interact with the Vehicle Signal
Interface, a software bus designed for rapid signal propagation.

All signals to be used with this API are specified by the Vehicle Signal
Specification, located **[here](https://github.com/PDXostc/vehicle_signal_specification)**.

## BASIC INTERACTION FLOW
Users of the API have two basic mechanisms available to them:
1. Signals.
2. Groups of signals.

Signals can logically be thought of as OR operations in terms of how the calling
applications will be awoken. For example, suppose an application has subscribed
to exactly two signals, SIG0 and SIG1:
```
   VSI HANDLE
    /      \
  SIG0    SIG1
```

In this example, once the application begins to listen for signals, it will be
awoken if either SIG0 or SIG1 fire.

Groups can logically be thought of as AND operations in terms of how the calling
applications will be awoken. For example, suppose an application has subscribed
to exactly two signals, SIG0 and SIG1, and placed them in GROUP 1.
```
 VSI HANDLE
     |
  GROUP 1
  /     \
SIG0   SIG1
```

In this example, once the application begins to listen for signals, it will
be awoken only if both SIG0 *and* SIG1 fire.

Signals outside of groups are always considered to be OR conditions; signals
inside of groups are always considered to be AND conditions. The API allows
for both methods of operation simultaneously, but does not allow combinations
within groups. Representing this logically:
```
A || B - Valid
A && B - Valid
A || (B && C) - Valid
A || (B && (C || D)) - Invalid
```

An example with valid signals:
```
 VSI HANDLE
  /   |   \
SIG0 SIG1 GROUP 1
           /   \
         SIG2 SIG3
```

One additional consideration is that the API will allow signals to exist in
multiple groups simultaneously. Leveraging the example above, the API permits
the creation of a GROUP 2 containing a signal from GROUP 1, such as SIG2.

Users are to refer to signals by either the signal name as specified by the
Vehicle Signal Specification or by the unique ID associated with that signal.
The unique ID is composed of two parts: a domain ID and a signal ID within that
domain.

The VSI API will provide functions to convert between a named signal and its
IDs. It will only use the ID internally, so applications interested in maximum
performance are encouraged to use the signal IDs directly.

## INTERACTION FLOWS
There are two basic flows through the API depending on whether the user is
processing signals or groups.

For signals:
1. vsi_subscribe_to_signal
2. vsi_listen
3. vsi_get_oldest_signal

For groups:
1. vsi_create_signal_group
2. vsi_add_signal_to_group
3. vsi_listen
4. vsi_get_oldest_group

It is possible to work with both signals and groups of signals simultaneously.
In that situation, the user performs all of the configuration steps for both
flows listed above, calls vsi_listen, and calls vsi_get_oldest_signal or
vsi_get_oldest_group based on the results of vsi_listen.

# BUILDING...
The API code in this repository comes bundled with a sample application for ease
of understanding. By default, only the API is built.

## ...THE API
Compiling the API is straightforward:

    make

## ...THE SAMPLE APPLICATION
The sample application is as simple as a separate make target:

    make sample

By default, ```make all``` will compile both the API and the sample application.

## ...THE DOCUMENTATION
Doxygen is used to generate the human-readable documentation from the [API
header file](vsi.h). To generate this documentation, run:

    make docs

# NOTES
All data flows from producers to consumers. All communication from a consumer to
a producer (for example, an application configuring the HVAC) will be through
CommonAPI/D-Bus/RPCnotyetinvented. This is to prevent producers from having to
share any control for the firing of signals.

Optional register/unregister calls have been provided to ensure sole ownership
of a signal. If a signal has not been registered, any application may fire
that signal.

The calling application is responsible for passing in buffers and the length
of the buffers for the API to store the resultant data in.

# FUTURE CONSIDERATIONS
1. Additional helper functions for quality of life improvements.
2. Security controls to restrict what signals an application can consume.
