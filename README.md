## mini-pubsub-broker

A lightweight publish/subscribe server driven by libevent

## Build

You should install [libevent](http://libevent.org/) and [libdatire](http://linux.thai.net/~thep/datrie/datrie.html) first and make sure gcc can search the header file of these libraries.

Then simply run <code>make</code> or <code>make debug</code> under the root path.

## Usage

The subscribe mechanism is topic based and topic means the prefix string of a message.
For example a subscriber has subscribed the topic of ***n***, ***net*** and ***network***, any messages starting with ***n***, ***net*** or ***network*** will be published to this subscriber.

The protocol between server and subscribe client is a subset of [RESP](http://redis.io/topics/protocol)(REdis Serialization Protocol). So you can simply use redis-cli for testing.
The protocol between server and publisher is simply native tcp connecting, sending messages and reading response.
