edith
=====

edith@exclusivelyducks.com

A Dota 2 replay (.dem) parser that understands packet entities. Tested on OS X.

Quick start:
------------
    cd build
    ccmake ../src
    make
    ./edith <a replay>

If make can't generate the protocol buffers then you may need to find the directory
containing google/protobuf/descriptor.proto and do:

    cmake -DPROTOBUF_IMPORT_DIRS=/usr/local/include ../src

replacing /usr/local/include.

Your C++ is terrible:
--------------------
I am very bad at C++ and have done just enough that this works, sorry.

Limitations:
------------
I doubt this actually compiles on anything but OS X.

This seems to work on the replays that I have tried, however there are probably a
million different bugs hiding. I'm particularily scared of a lot of the float
deserializations.

I implemented just enough logic that all the replays I have parse successfully. In
particular I didn't fully implement:

* There's a million variations of float serialization. I only implemented the ones
required for the send props in my replays.
* There's several complex string table parsing paths and some flags I don't understand.
I only implemented the path required by the instancebaseline table, and I don't parse
any other tables.
