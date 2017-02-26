

A database for keeping, navigating and manipulating big trees.

The initial idea was to make an 'intelligent' in memory living tree that constantly analyzes itself
and gives user, for example, auto suggestions, or
automatically relates data from different tree parts, or calculates something. 

The OctopusNote concept changed a lot, was simplified and currently
is implemented purely in JavaScript on Node.js.
It uses a totally different approach.
Limited in size, but still very efficient because the most heavy work is done by JSON.parse and JSON.stringify.
Also much easier to scale.
Going to opensource it too in the near future.

Now this database isn't very useful in its current condition,
but can be useful as a base for new projects that:

* use asynchronous event loops based on Libevent
* implement redis subscribe in an event loop
* use Jansson for JSON stringifying and parsing
* use Sqlite full text search and content-less tables
* use red-black trees to index data
* keep incoming operations in queues, distribute to corresponding threads
* import xml files
* use very large tree structures

There are trees and tree views. In RAM only the tree structure is stored.
The actual data is stored on a hard drive.
View is just another tree structure that 'overlays' some area of the actual tree.

Every connected client 'sees' the tree through its view.
When a client requests for a specific part of the tree, 
the view overlays it and loads branch data from a hard drive.

This way a user can navigate in any size tree, remotely, from a limited speed connection, limited memory resources (i.e. web browser).

But in practice, for a normal user its not the case.
An average tree size for a normal user is less than 1Mb.
But this concept could be very useful if working with very big trees.

Also the idea of OctopusNote was to keep only 300 bytes per branch.
In this case storing raw data in a file with fixed length blocks would allow to
easily read and write to any block just by calculating its offset.

It would be a trade-off between CPU overhead (using a traditional db engine) and storage overhead (in this case).

But still economically viable because storage is cheap, and CPU resources aren't.

But after removing the limit, a traditional database engine was used.

Two databases were tried:
* Sqlite
* [ForestDB](https://github.com/couchbase/forestdb)

Another important feature is synchronization.
While each client (i.e. each browser tab) creates a new view and each view can insert,
move or delete branches, it's important to keep them synchronized with the main tree.
This is quite complex. For example a 'move' operation can trigger changes in many views.
And then each view have to pass the changes to its browser tab.

What a user sees in a browser is an identical copy of a view in a server.
Each change that happened in a browser is synchronized with its view in a server.
The recently updated view is synchronized with the main tree and with all other views.

```            
                            -------------------------------------- 
                           |                                      |
-------------+  -------->  +-----------+                          |
browser1 tree|             |server view|                          |
-------------+  <--------  +-----------+                          |
                           |                                      |
-------------+  -------->  +-----------+                          |
browser2 tree|             |server view|          TREE            |
-------------+  <--------  +-----------+                          |
                           |                                      |
-------------+  -------->  +-----------+                          |
browser3 tree|             |server view|                          |
-------------+  <--------  +-----------+                          |
                           |                                      |
                           ----------------------------------------
```

The prototype of this database was successfuly finished and used for a while, but then replaced with much more simpler, practical and scalable system made in JavaScript.

If you need some kind of exotic database designed for a specific purpose, contact me.

## Build

```
apt-get install libevent-dev libsqlite3-dev libhiredis-dev libjansson-dev libxml2-dev

git clone https://github.com/mrtcode/treedb
cd treedb
cmake .
make
```










