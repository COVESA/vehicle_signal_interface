**(C) 2016 Jaguar Land Rover - All rights reserved.**

All documents in this repository are licensed under the Creative
Commons Attribution 4.0 International (CC BY 4.0). Click
[here](https://creativecommons.org/licenses/by/4.0/) for details.
All code in this repository is licensed under Mozilla Public License
v2 (MPLv2). Click [here](https://www.mozilla.org/en-US/MPL/2.0/) for
details.

# VEHICLE SIGNAL INTERFACE
This repository will contain the library required to distribute vehicle signals
between components inside the IVI.

At this time, only a stub of the API is available to solicit feedback, but once
the core has arrived it will also belong to this repository.

The 2nd phase of the VSI core code has been added to the repository.  This
version of the core has a statically defined shared memory segment but is
quite usable for many projects.  The default configuration for it is a shared
memory area of approximately 1GB.  The size and location of the image file of
the shared memory segment can be easily customized.  See the README file in
the core directory for more information on this phase 2 code.
