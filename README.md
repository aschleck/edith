edith
=====
edith@exclusivelyducks.com

http://github.com/dschleck/edith

A Dota 2 replay (.dem) parser that understands packet entities. Tested on OS X and Ubuntu.

![Kills in SL2 Na`Vi v Mouz game 1](sl2-navi-mouz-g1-kills.png)

Quick start:
------------
    cd build
    ccmake ../
    make
    ./death_recording <a replay>

If you encounter errors compiling the protocol buffers, make sure the CMake variable
**PROTOBUF_INCLUDE_DIR** points to the directory containing google/protobuf/descriptor.proto.
The other **PROTOBUF_** variables should be set as well.

Your C++ is terrible:
--------------------
This is a reverse engineering project and I stopped caring the moment this worked, sorry. The great
news is others have used this as a reference to write this better!

* [Yasha](https://github.com/dotabuff/yasha)
* [Alice](https://github.com/AliceStats/Alice)
* [Clarity](https://github.com/skadistats/clarity)

Overview:
---------
Parsing packet entities requires several components of knowledge, luckily each replay
contains everything needed to parse it.

The first thing to understand are the send properties (send props). The primary use for these is to
describe a variable in a class. As a simple and imprecise example, suppose Dota had the following
class they needed to synchronize:

    class Ent {
      float x;
      int y;
    };

To describe this class, they could have a send prop named **x** with type 1 (float) and a send prop
named **y** with type 0 (int). These send props could be contained in a send table named **Ent**.

Beyond simply adding additional types (strings, int64s, arrays) send props can also
point at other send tables whose props should also be included. The
primary use of this is for inheritance, most send tables have a send prop named
"baseclass" that points at the send table of the parent class.

The other big source of complexity comes from exclude props. Suppose Dota had some class that
inherited from **Ent**, but no longer used **Ent.y**. To prevent wasting bandwidth, they could
add an exclude prop that will prevent it from being transmitted. The name of this prop would be
**y** and it would also have a field called datatable name set to **Ent**.

After we read in all of the send tables from the replay we have to flatten them. When a table is
flattened, we recursively follow all the links (such as baseclass) to other send tables and copy
all of their send props into the table being flattened.

Before we can parse the packet entities, there is one efficiency optimization. Suppose there's a
class with two hundred integers and every instance sets those to zero. Rather than transferring two
hundred zeroes every time a new instance of this class is created, it would be better to have a
default instance of the class and then transfer only what is changed from the default when a new
instance is created. The **instancebaseline** table accomplishes this, every class has an entry
in the table with a default instance.

Finally we can parse our packet entities. When the server thinks an entity enters the
client's potentially visible set (PVS) it tells the client to make a new entity. Making
a new entity requires first parsing the entry in **instancebaseline**, and then updating the
instance by parsing
the data sent in the packet. Parsing both of these is exactly the same, first you read
the list of properties that they contain and then you read each property.

Entities are updated using the same mechanics. First you read the list of properties in the update
and then you read each property and put them in the entity.

Deleting an entity only requires transferring the ID of the entity.

Understanding the code:
----------------------
**examples/death_recording_visitor** is an example of doing something useful with my code. This
outputs a line whenever a hero dies. This was used to gather data for the tool that produced the image
above.

**src/entity** describes an entity and stores its properties.

**src/property** handles the different types of send props and stores the correct data for
each one.

**src/state** contains a bunch of data structures read from the replay and handles flattening
send tables.

**src/edith** reads the replay, converts it into the internal representation used by the program,
and runs the logic for everything but flattening send tables.

Limitations:
------------
This seems to work on the replays that I have tried, however there are probably a million
different bugs hiding. The read\_float\_coord\_mp is almost certainly completely buggy.

I implemented just enough logic that all the replays I have parse successfully. In
particular the following are not implemented:

* There are a million variations of float serialization. I only implemented the ones
required for the send props in my replays.
* There are a bunch of string tables that I can't read. The ones I've seen appear to be related
to precaching, but there may be more.

If you have a replay that works in the client but doesn't parse correctly, let me know!
